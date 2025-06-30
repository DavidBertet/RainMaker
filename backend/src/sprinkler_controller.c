// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "sprinkler_controller.h"

#include "ws_sprinkler.h"
#include "days_utils.h"
#include "sprinkler_repository.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char *TAG = "SPRINKLER_CTRL";

#define EXECUTOR_TASK_STACK_SIZE 12288
#define EXECUTION_QUEUE_SIZE 10
#define ZONE_TIMER_PERIOD_MS 100

// Task handles
static TaskHandle_t executor_task_handle = NULL;
static QueueHandle_t execution_queue = NULL;
static TimerHandle_t zone_timer = NULL;

static bool controller_running = false;

// Execution command structure
typedef struct
{
    uint8_t program_id;
    uint8_t zone_index;
    uint16_t duration_minutes;
    bool is_program_start;
    bool is_program_end;
} execution_cmd_t;

// Current execution state
typedef struct
{
    bool is_running; // currently watering
    uint8_t current_program_id;
    uint8_t current_zone_id;
    uint8_t current_zone_index;
    time_t zone_start_time;
    uint16_t zone_duration_seconds;
} execution_state_t;

// Param used when safely accessing the sprinkler data structure
typedef struct
{
    int zone_id;
    bool turn_on;
} update_gpio_context_t;

// Structure to pass data for executor operations
typedef struct
{
    uint8_t program_id;
    int zone_index;

    // Output data
    bool program_found;
    char program_name[MAX_PROGRAM_NAME_LEN];
    int zone_count;
    uint8_t zone_id;
    int zone_duration_minutes;
    bool zone_enabled;
} executor_operation_data_t;

typedef struct
{
    bool should_resume;
    uint8_t program_id;
    uint8_t zone_index;
    uint16_t remaining_minutes;
} program_recovery_t;

static execution_state_t exec_state = {0};

esp_err_t init_zone_gpio(const zone_t *zone)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << zone->output),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPIO %d for zone %d", zone->output, zone->id);
        return ret;
    }

    // Initialize to OFF state
    gpio_set_level(zone->output, 0);
    ESP_LOGI(TAG, "Initialized GPIO %d for zone %d (%s)", zone->output, zone->id, zone->name);

    return ESP_OK;
}

// Function to initialize GPIO pins for zones
static esp_err_t init_all_zones_gpio(const sprinkler_data_t *sprinkler_data, void *user_data)
{
    for (int i = 0; i < sprinkler_data->zone_count; i++)
    {
        init_zone_gpio(&sprinkler_data->zones[i]);
    }

    return ESP_OK;
}

static esp_err_t update_gpio_state(const sprinkler_data_t *sprinkler_data, void *user_data)
{
    update_gpio_context_t *gpio_context = (update_gpio_context_t *)user_data;
    zone_t zone = sprinkler_data->zones[gpio_context->zone_id - 1];

    if (!zone.id)
    {
        ESP_LOGE(TAG, "Zone %d not found", gpio_context->zone_id);
        return ESP_ERR_NOT_FOUND;
    }

    exec_state.is_running = gpio_context->turn_on;
    exec_state.current_zone_id = gpio_context->turn_on ? gpio_context->zone_id : 0;
    exec_state.zone_start_time = time(NULL);
    gpio_set_level(zone.output, gpio_context->turn_on ? 1 : 0);

    ESP_LOGI(TAG, "Zone %d (%s) turned %s", gpio_context->zone_id, zone.name, gpio_context->turn_on ? "ON" : "OFF");

    return ESP_OK;
}

// Function to turn zone on/off
static esp_err_t control_zone(uint8_t zone_id, bool turn_on)
{
    if (zone_id > MAX_ZONES || zone_id == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    update_gpio_context_t params = {
        .zone_id = zone_id,
        .turn_on = turn_on};
    esp_err_t ret = safe_sprinklerdata_operation(update_gpio_state, &params);
    if (ret == ESP_OK)
    {
        ret = sprinkler_update_zone_status(zone_id, turn_on);
    }

    return ret;
}

// Zone timer callback
static void zone_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Zone timer callback triggered");

    // Handle manual zone operation (program_id = 255)
    if (exec_state.current_program_id == 255)
    {
        // Manual zone - just turn off the current zone
        if (exec_state.current_zone_id != 0)
        {
            control_zone(exec_state.current_zone_id, false);
            ESP_LOGI(TAG, "Manual zone %d timer expired, turning off", exec_state.current_zone_id);
        }
        // Reset execution state
        memset(&exec_state, 0, sizeof(exec_state));
        return;
    }

    execution_cmd_t cmd = {0};
    cmd.program_id = exec_state.current_program_id;
    cmd.zone_index = exec_state.current_zone_index + 1; // Move to next zone
    cmd.is_program_end = false;

    if (xQueueSend(execution_queue, &cmd, 0) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to queue next zone command");
    }
}

static esp_err_t executor_get_program_zone_operation(const sprinkler_data_t *data, void *user_data)
{
    executor_operation_data_t *op_data = (executor_operation_data_t *)user_data;

    // Initialize output
    op_data->program_found = false;
    op_data->zone_enabled = false;

    // Validate program ID
    if (op_data->program_id == 0 || op_data->program_id > MAX_PROGRAMS)
    {
        return ESP_OK;
    }

    const program_t *program = &data->programs[op_data->program_id - 1];
    if (!program->id)
    {
        return ESP_OK;
    }

    // Program found, copy basic info
    op_data->program_found = true;
    strncpy(op_data->program_name, program->name, sizeof(op_data->program_name) - 1);
    op_data->program_name[sizeof(op_data->program_name) - 1] = '\0';
    op_data->zone_count = program->zone_count;

    // Find the next enabled zone starting from the requested index
    for (int i = op_data->zone_index; i < program->zone_count; i++)
    {
        const program_zone_t *pz = &program->zones[i];

        // Check if zone is enabled
        if (pz->zone_id > 0 && pz->zone_id <= MAX_ZONES)
        {
            const zone_t *zone = &data->zones[pz->zone_id - 1];

            if (zone->enabled)
            {
                // Found an enabled zone
                op_data->zone_index = i; // Update to the actual zone index we found
                op_data->zone_id = pz->zone_id;
                op_data->zone_duration_minutes = pz->duration;
                op_data->zone_enabled = true;
                break;
            }
            else
            {
                ESP_LOGW(TAG, "Zone %d is disabled, skipping", pz->zone_id);
            }
        }
    }

    return ESP_OK;
}

static void executor_task(void *pvParameters)
{
    execution_cmd_t cmd;
    ESP_LOGI(TAG, "Executor task started");

    while (controller_running)
    {
        if (xQueueReceive(execution_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            executor_operation_data_t op_data = {
                .program_id = cmd.program_id,
                .zone_index = cmd.is_program_start ? 0 : (cmd.zone_index + 1), // Start from next zone for continuation
                .program_found = false,
                .program_name = {0},
                .zone_count = 0,
                .zone_id = 0,
                .zone_duration_minutes = 0,
                .zone_enabled = false};

            // Get program and zone info safely
            esp_err_t ret = safe_sprinklerdata_operation(executor_get_program_zone_operation, &op_data);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to safely access program data: %s", esp_err_to_name(ret));
                continue;
            }

            if (!op_data.program_found)
            {
                ESP_LOGE(TAG, "Program %d not found", cmd.program_id);
                continue;
            }

            if (cmd.is_program_start)
            {
                ESP_LOGI(TAG, "Executing program %d (%s)", op_data.program_id, op_data.program_name);
                exec_state.current_program_id = op_data.program_id;
                exec_state.current_zone_index = op_data.zone_index; // Use the actual found zone index

                // Start first enabled zone if found
                if (op_data.zone_enabled)
                {
                    exec_state.zone_duration_seconds = op_data.zone_duration_minutes * 60;
                    exec_state.current_zone_id = op_data.zone_id;

                    control_zone(op_data.zone_id, true);

                    // Start timer for this zone
                    xTimerChangePeriod(zone_timer, pdMS_TO_TICKS(exec_state.zone_duration_seconds * 1000), 0);
                    xTimerStart(zone_timer, 0);

                    ESP_LOGI(TAG, "Started zone %d (index %d) for %d minutes", op_data.zone_id, op_data.zone_index, op_data.zone_duration_minutes);
                }
                else
                {
                    // No enabled zones found in entire program
                    ESP_LOGW(TAG, "No enabled zones found in program %d", op_data.program_id);
                    exec_state.current_program_id = 0;
                    exec_state.current_zone_index = 0;
                }
            }
            else
            {
                // Turn off current zone
                control_zone(exec_state.current_zone_id, false);

                // Check if we found another enabled zone
                if (op_data.zone_enabled)
                {
                    exec_state.current_zone_index = op_data.zone_index; // Use the actual found zone index
                    exec_state.zone_duration_seconds = op_data.zone_duration_minutes * 60;
                    exec_state.current_zone_id = op_data.zone_id;

                    control_zone(op_data.zone_id, true);

                    // Start timer for this zone
                    xTimerChangePeriod(zone_timer, pdMS_TO_TICKS(exec_state.zone_duration_seconds * 1000), 0);
                    xTimerStart(zone_timer, 0);

                    ESP_LOGI(TAG, "Running zone %d (index %d) for %d minutes", op_data.zone_id, op_data.zone_index, op_data.zone_duration_minutes);
                }
                else
                {
                    // Program completed - no more enabled zones
                    ESP_LOGI(TAG, "Program %d completed", op_data.program_id);
                    sprinkler_update_program_next_run(op_data.program_id);

                    // Update state
                    exec_state.current_program_id = 0;
                    exec_state.current_zone_index = 0;
                }
            }
        }
    }

    ESP_LOGI(TAG, "Executor task stopped");
    vTaskDelete(NULL);
}

static esp_err_t check_program_recovery_operation(const sprinkler_data_t *data, void *user_data)
{
    program_recovery_t *recovery = (program_recovery_t *)user_data;
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    recovery->should_resume = false;

    // Check each program
    for (int i = 0; i < MAX_PROGRAMS; i++)
    {
        const program_t *program = &data->programs[i];
        if (!program->id || !program->enabled)
            continue;

        // Check if program should run today
        int today_bit = 1 << tm_now->tm_wday;
        if (!(program->schedule.days & today_bit))
            continue;

        // Calculate today's start time for this program
        struct tm tm_start = *tm_now;
        tm_start.tm_hour = program->schedule.start_hour;
        tm_start.tm_min = program->schedule.start_minute;
        tm_start.tm_sec = 0;
        time_t program_start = mktime(&tm_start);

        // Calculate total program duration
        uint16_t total_duration_minutes = 0;
        for (int j = 0; j < program->zone_count; j++)
        {
            const program_zone_t *pz = &program->zones[j];
            if (pz->zone_id > 0 && pz->zone_id <= MAX_ZONES)
            {
                const zone_t *zone = &data->zones[pz->zone_id - 1];
                if (zone->enabled)
                {
                    total_duration_minutes += pz->duration;
                }
            }
        }

        time_t program_end = program_start + (total_duration_minutes * 60);

        // Check if we're currently within program execution window
        if (now >= program_start && now < program_end)
        {
            // Calculate elapsed time and find current zone
            uint16_t elapsed_minutes = (now - program_start) / 60;
            uint16_t cumulative_minutes = 0;

            for (int j = 0; j < program->zone_count; j++)
            {
                const program_zone_t *pz = &program->zones[j];
                if (pz->zone_id > 0 && pz->zone_id <= MAX_ZONES)
                {
                    const zone_t *zone = &data->zones[pz->zone_id - 1];
                    if (zone->enabled)
                    {
                        if (elapsed_minutes < cumulative_minutes + pz->duration)
                        {
                            // Found the zone we should be running
                            recovery->should_resume = true;
                            recovery->program_id = program->id;
                            recovery->zone_index = j;
                            recovery->remaining_minutes = cumulative_minutes + pz->duration - elapsed_minutes;
                            return ESP_OK;
                        }
                        cumulative_minutes += pz->duration;
                    }
                }
            }
        }
    }

    return ESP_OK;
}

// Public API functions
esp_err_t sprinkler_controller_init()
{
    // Initialize GPIO for zones
    esp_err_t ret = safe_sprinklerdata_operation(init_all_zones_gpio, NULL);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Create execution queue
    execution_queue = xQueueCreate(EXECUTION_QUEUE_SIZE, sizeof(execution_cmd_t));
    if (!execution_queue)
    {
        ESP_LOGE(TAG, "Failed to create execution queue");
        return ESP_ERR_NO_MEM;
    }

    // Create zone timer
    zone_timer = xTimerCreate("ZoneTimer", pdMS_TO_TICKS(1000), pdFALSE, NULL, zone_timer_callback);
    if (!zone_timer)
    {
        ESP_LOGE(TAG, "Failed to create zone timer");
        vQueueDelete(execution_queue);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Sprinkler controller initialized");
    return ESP_OK;
}

static esp_err_t update_all_programs_next_run_operation(const sprinkler_data_t *data, void *user_data)
{
    bool *any_updated = (bool *)user_data;
    *any_updated = false;

    for (int i = 0; i < MAX_PROGRAMS; i++)
    {
        program_t *program = &sprinkler_data.programs[i];
        if (!program->id || !program->enabled)
            continue;

        // Calculate what the next run should be
        time_t new_next_run = calculate_next_run(program->schedule.days,
                                                 program->schedule.start_hour,
                                                 program->schedule.start_minute);

        // Update if it's different from stored value
        if (program->next_run != new_next_run)
        {
            program->next_run = new_next_run;
            esp_err_t ret = sprinkler_save_program(program);
            if (ret == ESP_OK)
            {
                *any_updated = true;
                ESP_LOGI(TAG, "Updated next run for program %d (%s)", program->id, program->name);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to save updated next run for program %d", program->id);
            }
        }
    }

    return ESP_OK;
}

esp_err_t sprinkler_controller_update_all_next_runs(void)
{
    bool any_updated = false;
    esp_err_t ret = safe_sprinklerdata_operation(update_all_programs_next_run_operation, &any_updated);

    if (ret == ESP_OK && any_updated)
    {
        broadcast_program_update();
    }

    return ret;
}

esp_err_t sprinkler_controller_start(void)
{
    if (controller_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    controller_running = true;

    // Update all program next runs first (in case ESP32 was off for a while)
    esp_err_t ret = sprinkler_controller_update_all_next_runs();
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to update program next runs on startup");
    }

    // Create executor task
    if (xTaskCreate(executor_task, "sprinkler_executor", EXECUTOR_TASK_STACK_SIZE, NULL, 6, &executor_task_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create executor task");
        controller_running = false;
        return ESP_ERR_NO_MEM;
    }

    // Check if we need to resume a program
    program_recovery_t recovery = {0};
    ret = safe_sprinklerdata_operation(check_program_recovery_operation, &recovery);

    if (ret == ESP_OK && recovery.should_resume)
    {
        ESP_LOGI(TAG, "Resuming program %d at zone index %d for %d minutes",
                 recovery.program_id, recovery.zone_index, recovery.remaining_minutes);

        // Set up execution state for recovery
        exec_state.current_program_id = recovery.program_id;
        exec_state.current_zone_index = recovery.zone_index;

        // Start the zone directly (bypass normal program start)
        executor_operation_data_t op_data = {
            .program_id = recovery.program_id,
            .zone_index = recovery.zone_index,
        };

        ret = safe_sprinklerdata_operation(executor_get_program_zone_operation, &op_data);
        if (ret == ESP_OK && op_data.program_found && op_data.zone_enabled)
        {
            exec_state.zone_duration_seconds = recovery.remaining_minutes * 60;
            exec_state.current_zone_id = op_data.zone_id;

            control_zone(op_data.zone_id, true);

            // Start timer for remaining time
            xTimerChangePeriod(zone_timer, pdMS_TO_TICKS(exec_state.zone_duration_seconds * 1000), 0);
            xTimerStart(zone_timer, 0);

            sprinkler_update_program_last_run(recovery.program_id);

            ESP_LOGI(TAG, "Resumed zone %d for %d minutes", op_data.zone_id, recovery.remaining_minutes);
        }
    }

    ESP_LOGI(TAG, "Sprinkler controller started");
    return ESP_OK;
}

esp_err_t sprinkler_controller_stop(void)
{
    if (!controller_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    controller_running = false;

    // Stop any running zones
    if (exec_state.is_running && exec_state.current_zone_id != 0)
    {
        control_zone(exec_state.current_zone_id, false);
        xTimerStop(zone_timer, 0);
    }

    // Reset execution state
    memset(&exec_state, 0, sizeof(exec_state));

    // Tasks will self-delete when controller_running becomes false

    ESP_LOGI(TAG, "Sprinkler controller stopped");
    return ESP_OK;
}

esp_err_t sprinkler_controller_manual_zone(uint8_t zone_id, uint16_t duration_seconds)
{
    if (!controller_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Stop any current program execution
    sprinkler_controller_stop_pending();

    // Set up manual execution state
    exec_state.zone_duration_seconds = duration_seconds;
    exec_state.current_program_id = 255; // Special value for manual

    // Start manual zone
    esp_err_t ret = control_zone(zone_id, true);
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Start timer for manual zone
    xTimerChangePeriod(zone_timer, pdMS_TO_TICKS(duration_seconds * 1000), 0);
    xTimerStart(zone_timer, 0);

    ESP_LOGI(TAG, "Manual zone %d started for %d seconds", zone_id, duration_seconds);
    return ESP_OK;
}

esp_err_t sprinkler_controller_manual_program(uint8_t program_id)
{
    if (!controller_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Stop any current program execution
    sprinkler_controller_stop_pending();

    // Queue program start command
    execution_cmd_t cmd = {
        .program_id = program_id,
        .zone_index = 0,
        .is_program_start = true,
        .is_program_end = false};

    if (xQueueSend(execution_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        sprinkler_update_program_last_run(program_id);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to queue program %d", program_id);
    }

    return ESP_OK;
}

esp_err_t sprinkler_controller_stop_pending(void)
{
    if (!controller_running)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t zone_id = exec_state.current_zone_id;
    uint8_t program_id = exec_state.current_program_id;

    // Reset execution state
    memset(&exec_state, 0, sizeof(exec_state));

    // Stop running zone
    if (zone_id != 0)
    {
        control_zone(zone_id, false);
        xTimerStop(zone_timer, 0);
    }

    // Update program status
    if (program_id != 0)
    {
        sprinkler_update_program_next_run(program_id);
    }

    ESP_LOGI(TAG, "All zones stopped");
    return ESP_OK;
}

esp_err_t sprinkler_controller_get_status(sprinkler_controller_status_t *status)
{
    if (!status)
    {
        return ESP_ERR_INVALID_ARG;
    }

    status->is_running = exec_state.is_running;
    status->current_program_id = exec_state.current_program_id;
    status->current_zone_id = exec_state.current_zone_id;
    status->zone_start_time = exec_state.zone_start_time;
    status->zone_duration_seconds = exec_state.zone_duration_seconds;

    if (exec_state.is_running && exec_state.zone_start_time > 0)
    {
        time_t now = time(NULL);
        uint16_t elapsed = now - exec_state.zone_start_time;
        status->zone_remaining_seconds = (elapsed < exec_state.zone_duration_seconds) ? (exec_state.zone_duration_seconds - elapsed) : 0;
    }
    else
    {
        status->zone_remaining_seconds = 0;
    }

    return ESP_OK;
}
