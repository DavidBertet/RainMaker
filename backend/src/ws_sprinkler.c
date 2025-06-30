// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "ws_sprinkler.h"

#include "websocket.h"
#include "sprinkler_repository.h"
#include "sprinkler_serialization.h"
#include "sprinkler_controller.h"
#include "esp_wifi.h"
#include "days_utils.h"

#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SPRINKLER_WS";
static char json[JSON_BUFFER_SIZE];

// Queue system for handling broadcasts
#define WS_QUEUE_SIZE 10
#define WS_TASK_STACK_SIZE 4096
#define WS_TASK_PRIORITY 5
#define WS_UPDATE_TYPE_COUNT 2

// Function pointer type for serialization functions
typedef esp_err_t (*serializer_func_t)(const sprinkler_data_t *data, char *json, size_t size);

// Mapping of update types to their serialization functions and descriptions
typedef struct
{
    serializer_func_t serializer;
    const char *name;
} update_info_t;

typedef struct
{
    serializer_func_t serializer;
    char *json_buffer;
    size_t buffer_size;
} serialize_json_info_t;

typedef enum
{
    WS_UPDATE_ZONES,
    WS_UPDATE_PROGRAMS,
} ws_update_type_t;

static const update_info_t update_handlers[WS_UPDATE_TYPE_COUNT] = {
    [WS_UPDATE_ZONES] = {
        .serializer = sprinkler_zones_to_json,
        .name = "zones"},
    [WS_UPDATE_PROGRAMS] = {.serializer = sprinkler_programs_to_json, .name = "programs"}};

static esp_err_t process_serializer(const sprinkler_data_t *data, void *user_data)
{
    serialize_json_info_t *info = (serialize_json_info_t *)user_data;

    return info->serializer(data, json, sizeof(json));
}

typedef struct
{
    ws_update_type_t type;
    uint32_t timestamp; // For deduplication
} ws_update_message_t;

static QueueHandle_t ws_update_queue = NULL;
static TaskHandle_t ws_task_handle = NULL;

// Task that processes update queue and sends broadcasts
static void ws_update_task(void *pvParameters)
{
    ws_update_message_t msg;
    bool has_pending[WS_UPDATE_TYPE_COUNT] = {false};
    TickType_t last_process_time = 0;
    const TickType_t min_interval = pdMS_TO_TICKS(50); // Minimum 50ms between broadcasts

    ESP_LOGI(TAG, "WebSocket update task started");

    while (1)
    {
        // Wait for update message or timeout
        if (xQueueReceive(ws_update_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            // Mark that we have a pending update of this type
            has_pending[msg.type] = true;
        }

        // Check if enough time has passed since last processing
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_process_time) >= min_interval)
        {
            bool processed_any = false;

            // Process any pending updates
            for (int i = 0; i < WS_UPDATE_TYPE_COUNT; i++)
            {
                if (has_pending[i])
                {
                    const update_info_t *info = &update_handlers[i];

                    serialize_json_info_t params = {
                        .serializer = info->serializer,
                        .json_buffer = json,
                        sizeof(json)};
                    esp_err_t ret = safe_sprinklerdata_operation(process_serializer, &params);

                    if (ret == ESP_OK)
                    {
                        broadcast_message(json);
                        ESP_LOGD(TAG, "Broadcasted %s update", info->name);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to serialize %s", info->name);
                        snprintf(json, sizeof(json),
                                 "{\"type\":\"error\",\"message\":\"Failed to serialize %s\"}",
                                 info->name);
                        broadcast_message(json);
                    }

                    has_pending[i] = false;
                    processed_any = true;

                    // Process one update per cycle to avoid holding mutex too long
                    break;
                }
            }

            if (processed_any)
            {
                last_process_time = current_time;
            }
        }
    }
}

static void queue_update(ws_update_type_t type)
{
    ws_update_message_t msg = {
        .type = type};

    if (xQueueSend(ws_update_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Update queue full, dropping update request");
    }
}

esp_err_t ws_update_system_init(void)
{
    // Create queue for update messages
    ws_update_queue = xQueueCreate(WS_QUEUE_SIZE, sizeof(ws_update_message_t));
    if (ws_update_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create WebSocket update queue");
        return ESP_ERR_NO_MEM;
    }

    // Create task to handle updates
    BaseType_t ret = xTaskCreate(
        ws_update_task,
        "ws_update_task",
        WS_TASK_STACK_SIZE,
        NULL,
        WS_TASK_PRIORITY,
        &ws_task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create WebSocket update task");
        vQueueDelete(ws_update_queue);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "WebSocket update system initialized");
    return ESP_OK;
}

void ws_handle_get_zones(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received get_zones request");
    broadcast_zone_update();
}

void ws_handle_get_programs(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received get_programs request");
    broadcast_program_update();
}

void ws_handle_create_or_update_zone(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received add_zone request");

    // Expected format: {"type":"create_or_update_zone","id":1,"name":"New Zone","output":4}
    // No id means creation
    cJSON *zone_id_node = cJSON_GetObjectItem(root, "zone_id");
    cJSON *zone_name_node = cJSON_GetObjectItem(root, "name");
    cJSON *output_node = cJSON_GetObjectItem(root, "output");
    if (!cJSON_IsString(zone_name_node) || !cJSON_IsNumber(output_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    const char *zone_name = zone_name_node->valuestring;
    uint8_t output = output_node->valueint;

    // zone_id is used to update an existing zone
    int zone_id = 0;
    if (cJSON_IsNumber(zone_id_node))
    {
        zone_id = zone_id_node->valueint;
    }

    esp_err_t ret = sprinkler_create_or_update_zone(zone_id, zone_name, output);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add zone: %s", zone_name);
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to add zone\"}");
        broadcast_message(json);
    }
}

void ws_handle_delete_zone(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received delete_zone request");

    // Expected format: {"type":"delete_zone","zone_id":2}
    cJSON *zone_id_node = cJSON_GetObjectItem(root, "zone_id");
    if (!cJSON_IsNumber(zone_id_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    const int zone_id = zone_id_node->valueint;

    esp_err_t ret = sprinkler_remove_zone(zone_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete zone");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to delete zone\"}");
        broadcast_message(json);
        return;
    }
}

void ws_handle_create_or_update_program(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received create_or_update_program request");

    // Expected format: {"type":"create_or_update_program","id":1,"name":"Evening","schedule"{"days":[1,3,5],"start_time":"18:00"},"zones":[{"id":1,"order":1,"duration":30},{"id":2,"order":2,"duration":60}]}
    cJSON *program_id_node = cJSON_GetObjectItem(root, "id");
    cJSON *name_node = cJSON_GetObjectItem(root, "name");

    cJSON *schedule_node = cJSON_GetObjectItem(root, "schedule");
    cJSON *days_node = cJSON_GetObjectItem(schedule_node, "days");
    cJSON *start_time_node = cJSON_GetObjectItem(schedule_node, "start_time");

    cJSON *zones_node = cJSON_GetObjectItem(root, "zones");
    // program_id is used to update an existing program
    int program_id = 0;
    if (cJSON_IsNumber(program_id_node))
    {
        program_id = program_id_node->valueint;
    }

    if (!cJSON_IsString(name_node) || !cJSON_IsArray(days_node) || !cJSON_IsString(start_time_node) || !cJSON_IsArray(zones_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    // parse days
    uint8_t days = 0;
    cJSON *day;
    for (uint8_t i = 0; i < cJSON_GetArraySize(days_node); i++)
    {
        day = cJSON_GetArrayItem(days_node, i);
        if (!cJSON_IsNumber(day))
        {
            continue;
        }
        add_day(&days, day->valueint);
    }
    // split start_time_node "12:30" into 12 and 30
    char *start_time_str = start_time_node->valuestring;
    char *start_hour_str = strtok(start_time_str, ":");
    char *start_minute_str = strtok(NULL, ":");
    if (!start_hour_str || !start_minute_str)
    {
        ESP_LOGE(TAG, "Invalid start time format");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }
    uint8_t start_hour = atoi(start_hour_str);
    uint8_t start_minute = atoi(start_minute_str);

    // Parse zones
    uint8_t zone_count = 0;
    cJSON *zone_node;
    cJSON *zone_id_node;
    cJSON *zone_order_node;
    cJSON *zone_duration_node;
    program_zone_t zones[MAX_ZONES_PER_PROGRAM];
    for (uint8_t i = 0; i < cJSON_GetArraySize(zones_node); i++)
    {
        zone_node = cJSON_GetArrayItem(zones_node, i);
        if (!cJSON_IsObject(zone_node) || i >= MAX_ZONES_PER_PROGRAM)
        {
            continue;
        }
        zone_id_node = cJSON_GetObjectItem(zone_node, "id");
        zone_order_node = cJSON_GetObjectItem(zone_node, "order");
        zone_duration_node = cJSON_GetObjectItem(zone_node, "duration");
        if (!cJSON_IsNumber(zone_id_node) || !cJSON_IsNumber(zone_order_node) || !cJSON_IsNumber(zone_duration_node))
        {
            continue;
        }
        zone_count++;
        zones[i].zone_id = zone_id_node->valueint;
        zones[i].order = zone_order_node->valueint;
        zones[i].duration = zone_duration_node->valueint;
    }

    esp_err_t ret = sprinkler_create_or_update_program(program_id, name_node->valuestring, days, start_hour, start_minute, zones, zone_count);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create program: %s", name_node->valuestring);
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to create program\"}");
        broadcast_message(json);
    }
}

void ws_handle_delete_program(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received delete_program request");

    // Expected format: {"type":"delete_program","program_id":2}
    cJSON *program_id_node = cJSON_GetObjectItem(root, "program_id");
    if (!cJSON_IsNumber(program_id_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    const int program_id = program_id_node->valueint;

    esp_err_t ret = sprinkler_remove_program(program_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete program");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to delete program\"}");
        broadcast_message(json);
        return;
    }
}

void ws_handle_enable(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received enable request");

    cJSON *zone_id_node = cJSON_GetObjectItem(root, "zone_id");
    cJSON *program_id_node = cJSON_GetObjectItem(root, "program_id");
    cJSON *is_enabled_node = cJSON_GetObjectItem(root, "is_enabled");

    if ((zone_id_node && !cJSON_IsNumber(zone_id_node)) || (program_id_node && !cJSON_IsNumber(program_id_node)) || !cJSON_IsBool(is_enabled_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    if (zone_id_node)
    {
        sprinkler_enable_zone(zone_id_node->valueint, is_enabled_node->valueint);
    }
    else if (program_id_node)
    {
        sprinkler_enable_program(program_id_node->valueint, is_enabled_node->valueint);
    }
}

void ws_handle_test_manual(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received test_manual request");

    // Expected format: {"type":"test_manual","zone_id":1,"action":"start"|"stop"}
    cJSON *zone_id_node = cJSON_GetObjectItem(root, "zone_id");
    cJSON *program_id_node = cJSON_GetObjectItem(root, "program_id");
    cJSON *action_node = cJSON_GetObjectItem(root, "action");
    if ((zone_id_node && !cJSON_IsNumber(zone_id_node)) || (program_id_node && !cJSON_IsNumber(program_id_node)) || !cJSON_IsString(action_node))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        broadcast_message(json);
        return;
    }

    const char *action = action_node->valuestring;
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    if (!strcmp(action, "start"))
    {
        if (zone_id_node)
        {
            ret = sprinkler_controller_manual_zone(zone_id_node->valueint, 30); // 30 seconds
        }
        else if (program_id_node)
        {
            ret = sprinkler_controller_manual_program(program_id_node->valueint);
        }
    }
    else if (!strcmp(action, "stop"))
    {
        ret = sprinkler_controller_stop_pending();
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to run manual test");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to run manual test\"}");
        broadcast_message(json);
        return;
    }
}

// Utility function to broadcast zones status updates
void broadcast_zone_update(void)
{
    queue_update(WS_UPDATE_ZONES);
}

// Utility function to broadcast programs status updates
void broadcast_program_update(void)
{
    queue_update(WS_UPDATE_PROGRAMS);
}
