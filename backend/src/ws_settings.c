// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "ws_settings.h"

#include "websocket.h"

#include "lwip/sockets.h"
#include "errno.h"
#include "esp_log.h"
#include "sys/time.h"
#include <time.h>
#include <string.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>
// SPIFFS includes
#include "esp_spiffs.h"
#include "esp_partition.h"
#include <sys/stat.h>
#include <dirent.h>
#include "esp_wifi.h"

#include "wifi.h"
#include "sprinkler_controller.h"
#include "constants.h"

const char *TAG = "WS_SETTINGS";

static char json[2176];
static char system_info[2048]; // Increased size for SPIFFS info

void get_settings_info(char *buffer, size_t buffer_size)
{
    bool isWifiConnected = is_wifi_connected();
    bool isWifiSetup = is_wifi_setup();
    bool requiresOTAPassword = strlen(OTA_PASSWORD) != 0;
    snprintf(buffer, buffer_size,
             "{\"type\":\"settings\",\"ota\":{\"requiresPassword\":%s},\"wifi\":{\"connected\":%s,\"setup\":%s}}",
             requiresOTAPassword ? "true" : "false", isWifiConnected ? "true" : "false", isWifiSetup ? "true" : "false");
}

void ws_handle_get_settings(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received get_settings request");

    get_settings_info(json, sizeof(json));

    send_message_sockfd(json, sockfd);
    ESP_LOGI(TAG, "Sent settings: %s", json);
}

void broadcast_get_settings(void)
{
    get_settings_info(json, sizeof(json));

    broadcast_message(json);
    ESP_LOGI(TAG, "Sent settings: %s", json);
}

void ws_handle_time_update(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received time_update request");

    // Extract time from JSON payload
    cJSON *time_node = cJSON_GetObjectItem(root, "time");
    if (!cJSON_IsNumber(time_node))
    {
        ESP_LOGE(TAG, "Missing 'time' field in JSON");

        // Send error response
        snprintf(json, sizeof(json),
                 "{\"type\":\"time_update_response\",\"success\":false,\"error\":\"Missing time field\"}");
        broadcast_message(json);
        return;
    }

    // Parse time value (expecting Unix timestamp)
    time_t new_time = (time_t)time_node->valueint;

    // Validate timestamp (basic sanity check)
    if (new_time < 1000000000 || new_time > 2147483647)
    { // Roughly 2001-2038 range
        ESP_LOGE(TAG, "Time value out of reasonable range: %lld", new_time);
        snprintf(json, sizeof(json),
                 "{\"type\":\"time_update_response\",\"success\":false,\"error\":\"Time out of valid range\"}");
        broadcast_message(json);
        return;
    }

    // Update system time
    struct timeval tv;
    tv.tv_sec = new_time;
    tv.tv_usec = 0;

    if (settimeofday(&tv, NULL) == 0)
    {
        // Get updated time for confirmation
        time_t current_time;
        struct tm timeinfo;
        char time_str[64];

        time(&current_time);
        localtime_r(&current_time, &timeinfo);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        ESP_LOGI(TAG, "System time updated successfully to: %s", time_str);

        // Start sprinkler controller
        sprinkler_controller_start();

        // Send success response with updated time
        snprintf(json, sizeof(json),
                 "{\"type\":\"time_update_response\",\"success\":true,\"current_time\":%lld,\"formatted_time\":\"%.63s\"}",
                 current_time, time_str);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to set system time");
        snprintf(json, sizeof(json),
                 "{\"type\":\"time_update_response\",\"success\":false,\"error\":\"Failed to set system time\"}");
    }

    broadcast_message(json);
    ESP_LOGI(TAG, "Sent time_update_response: %s", json);
}

// Helper function to format uptime
void format_uptime(int64_t uptime_us, char *buffer, size_t buffer_size)
{
    int64_t uptime_sec = uptime_us / 1000000;
    int days = uptime_sec / 86400;
    int hours = (uptime_sec % 86400) / 3600;
    int minutes = (uptime_sec % 3600) / 60;
    int seconds = uptime_sec % 60;

    if (days > 0)
    {
        snprintf(buffer, buffer_size, "%d days, %d hours, %d minutes", days, hours, minutes);
    }
    else if (hours > 0)
    {
        snprintf(buffer, buffer_size, "%d hours, %d minutes", hours, minutes);
    }
    else if (minutes > 0)
    {
        snprintf(buffer, buffer_size, "%d minutes, %d seconds", minutes, seconds);
    }
    else
    {
        snprintf(buffer, buffer_size, "%d seconds", seconds);
    }
}

void format_time(time_t timestamp, char *buffer, size_t buffer_size)
{
    struct tm *tm_info = localtime(&timestamp);
    strftime(buffer, buffer_size, "%Y-%m-%d %I:%M %p", tm_info);
}

// Helper function to format bytes
void format_bytes(size_t bytes, char *buffer, size_t buffer_size)
{
    if (bytes >= 1024 * 1024)
    {
        snprintf(buffer, buffer_size, "%.1f MB", (float)bytes / (1024 * 1024));
    }
    else if (bytes >= 1024)
    {
        snprintf(buffer, buffer_size, "%.1f KB", (float)bytes / 1024);
    }
    else
    {
        snprintf(buffer, buffer_size, "%zu bytes", bytes);
    }
}

// Helper function to count files in SPIFFS
int count_spiffs_files(const char *path, size_t *total_size)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    int file_count = 0;
    *total_size = 0;
    char full_path[257];

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        { // Regular file
            file_count++;
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0)
            {
                *total_size += file_stat.st_size;
            }
        }
    }

    closedir(dir);
    return file_count;
}

// Helper function to get SPIFFS information
void get_spiffs_info(char *buffer, size_t buffer_size)
{
    // Find SPIFFS partition
    const esp_partition_t *spiffs_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);

    if (spiffs_partition == NULL)
    {
        snprintf(buffer, buffer_size,
                 "\"spiffs\": {"
                 "\"status\": \"No SPIFFS partition found\","
                 "}");
        return;
    }

    // Get SPIFFS info
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);

    if (ret != ESP_OK)
    {
        char partition_size_str[32];
        format_bytes(spiffs_partition->size, partition_size_str, sizeof(partition_size_str));

        snprintf(buffer, buffer_size,
                 "\"spiffs\": {"
                 "\"status\": \"SPIFFS not mounted or error\","
                 "\"partition_size\": \"%s\","
                 "\"partition_label\": \"%s\","
                 "\"partition_address\": \"0x%lu\""
                 "}",
                 partition_size_str,
                 spiffs_partition->label,
                 spiffs_partition->address);
        return;
    }

    // Count files and calculate file sizes
    size_t files_total_size = 0;
    int file_count = count_spiffs_files("/spiffs", &files_total_size);

    // Calculate usage statistics
    size_t free_space = total - used;
    int usage_percent = total > 0 ? (used * 100) / total : 0;

    // Format sizes
    char total_str[32], used_str[32], free_str[32], files_size_str[32];
    char partition_size_str[32];

    format_bytes(total, total_str, sizeof(total_str));
    format_bytes(used, used_str, sizeof(used_str));
    format_bytes(free_space, free_str, sizeof(free_str));
    format_bytes(files_total_size, files_size_str, sizeof(files_size_str));
    format_bytes(spiffs_partition->size, partition_size_str, sizeof(partition_size_str));

    snprintf(buffer, buffer_size,
             "\"spiffs\": {"
             "\"status\": \"Mounted and operational\","
             "\"partition_size\": \"%s\","
             "\"partition_label\": \"%s\","
             "\"partition_address\": \"0x%lx\","
             "\"total_space\": \"%s\","
             "\"used_space\": \"%s\","
             "\"free_space\": \"%s\","
             "\"usage\": \"%d%%\","
             "\"files_count\": %d,"
             "\"total_size\": \"%s\""
             "}",
             partition_size_str,
             spiffs_partition->label,
             spiffs_partition->address,
             total_str,
             used_str,
             free_str,
             usage_percent,
             file_count >= 0 ? file_count : 0,
             files_size_str);
}

// Function to get comprehensive system information
void get_system_info(char *buffer, size_t buffer_size)
{
    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    // Get memory information
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t used_heap = total_heap - free_heap;
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t min_free_heap = esp_get_minimum_free_heap_size();

    // Get internal memory stats
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // Get PSRAM info (if available)
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    // Get flash information
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);

    // Get uptime
    int64_t uptime_us = esp_timer_get_time();
    char uptime_str[64];
    format_uptime(uptime_us, uptime_str, sizeof(uptime_str));

    // Get time
    time_t time_us = time(NULL);
    char time_str[64];
    format_time(time_us, time_str, sizeof(time_str));

    // Get reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char *reset_reason_str;
    switch (reset_reason)
    {
    case ESP_RST_POWERON:
        reset_reason_str = "Power-on reset";
        break;
    case ESP_RST_EXT:
        reset_reason_str = "External reset";
        break;
    case ESP_RST_SW:
        reset_reason_str = "Software reset";
        break;
    case ESP_RST_PANIC:
        reset_reason_str = "Panic reset";
        break;
    case ESP_RST_INT_WDT:
        reset_reason_str = "Interrupt watchdog";
        break;
    case ESP_RST_TASK_WDT:
        reset_reason_str = "Task watchdog";
        break;
    case ESP_RST_WDT:
        reset_reason_str = "Other watchdog";
        break;
    case ESP_RST_DEEPSLEEP:
        reset_reason_str = "Deep sleep reset";
        break;
    case ESP_RST_BROWNOUT:
        reset_reason_str = "Brownout reset";
        break;
    case ESP_RST_SDIO:
        reset_reason_str = "SDIO reset";
        break;
    default:
        reset_reason_str = "Unknown";
        break;
    }

    // Format memory usage percentages
    int heap_usage_percent = (used_heap * 100) / total_heap;
    int internal_usage_percent = internal_total > 0 ? ((internal_total - internal_free) * 100) / internal_total : 0;
    int psram_usage_percent = psram_total > 0 ? ((psram_total - psram_free) * 100) / psram_total : 0;

    // Format sizes
    char total_heap_str[32], free_heap_str[32], used_heap_str[32];
    char largest_block_str[32], min_free_str[32];
    char internal_total_str[32], internal_free_str[32];
    char psram_total_str[32], psram_free_str[32];
    char flash_size_str[32];

    format_bytes(total_heap, total_heap_str, sizeof(total_heap_str));
    format_bytes(free_heap, free_heap_str, sizeof(free_heap_str));
    format_bytes(used_heap, used_heap_str, sizeof(used_heap_str));
    format_bytes(largest_free_block, largest_block_str, sizeof(largest_block_str));
    format_bytes(min_free_heap, min_free_str, sizeof(min_free_str));
    format_bytes(internal_total, internal_total_str, sizeof(internal_total_str));
    format_bytes(internal_free, internal_free_str, sizeof(internal_free_str));
    format_bytes(psram_total, psram_total_str, sizeof(psram_total_str));
    format_bytes(psram_free, psram_free_str, sizeof(psram_free_str));
    format_bytes(flash_size, flash_size_str, sizeof(flash_size_str));

    // Get SPIFFS information
    char spiffs_info[512];
    get_spiffs_info(spiffs_info, sizeof(spiffs_info));

    // Build the JSON response with grouped sections
    snprintf(buffer, buffer_size,
             "\"device\": {"
             "\"status\": \"Online and operational\","
             "\"reset_reason\": \"%s\","
             "\"uptime\": \"%s\","
             "\"time\": \"%s\""
             "},"
             "\"system\": {"
             "\"idf_version\": \"%s\","
             "\"freertos_tasks\": %d"
             "},"
             "\"hardware\": {"
             "\"chip_model\": \"%s\","
             "\"chip_revision\": %d,"
             "\"cpu_cores\": %d,"
             "\"flash_size\": \"%s\""
             "},"
             "\"memory\": {"
             "\"heap_total\": \"%s\","
             "\"heap_free\": \"%s\","
             "\"heap_used\": \"%s\","
             "\"heap_usage\": \"%d%%\","
             "\"heap_largest_free_block\": \"%s\","
             "\"heap_min_free_ever\": \"%s\","
             "\"internal_total\": \"%s\","
             "\"internal_free\": \"%s\","
             "\"internal_usage\": \"%d%%\""
             "},"
             "\"psram\": {"
             "\"psram_total\": \"%s\","
             "\"psram_free\": \"%s\","
             "\"psram_usage\": \"%d%%\""
             "},"
             "%s",
             // Device section
             reset_reason_str,
             uptime_str,
             time_str,
             // System section
             esp_get_idf_version(),
             uxTaskGetNumberOfTasks(),
             // Hardware section
             chip_info.model == CHIP_ESP32
                 ? "ESP32"
             : chip_info.model == CHIP_ESP32S2 ? "ESP32-S2"
             : chip_info.model == CHIP_ESP32S3 ? "ESP32-S3"
             : chip_info.model == CHIP_ESP32C3 ? "ESP32-C3"
             : chip_info.model == CHIP_ESP32C2 ? "ESP32-C2"
             : chip_info.model == CHIP_ESP32C6 ? "ESP32-C6"
             : chip_info.model == CHIP_ESP32H2 ? "ESP32-H2"
                                               : "Unknown",
             chip_info.revision,
             chip_info.cores,
             flash_size_str,
             // Memory section
             total_heap_str,
             free_heap_str,
             used_heap_str,
             heap_usage_percent,
             largest_block_str,
             min_free_str,
             internal_total_str,
             internal_free_str,
             internal_usage_percent,
             psram_total_str,
             psram_free_str,
             psram_usage_percent,
             // Storage section
             spiffs_info);
}

// Main WebSocket handler function
void ws_handle_system_info(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received system_info request");

    // Get comprehensive system information
    get_system_info(system_info, sizeof(system_info));

    // Build JSON response with system information
    snprintf(json, sizeof(json), "{\"type\":\"system_info\",\"settings\":{%s}}", system_info);

    send_message_sockfd(json, sockfd);
}