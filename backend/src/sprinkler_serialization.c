// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "sprinkler_serialization.h"

#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SPRINKLER_JSON";

static sprinkler_controller_status_t sprinkler_status;

int days_to_json(uint8_t days, char *buffer, size_t buffer_size)
{
    uint8_t count = __builtin_popcount(days) + 3; // + 3 for "[]" + null
    if (!buffer || buffer_size < count)
        return 0;

    char *ptr = buffer;
    bool first = true;

    // Start JSON array
    *ptr++ = '[';

    // Add each active day
    for (int day = 0; day < 7; day++)
    {
        if (days & (1 << day))
        {
            // Add comma separator if not first element
            if (!first)
            {
                *ptr++ = ',';
            }
            *ptr++ = '0' + day;
            first = false;
        }
    }

    // Close JSON array
    *ptr++ = ']';
    *ptr = '\0';

    return count;
}

void format_time_string(time_t timestamp, char *time_str, size_t size)
{
    if (timestamp == 0)
    {
        strncpy(time_str, "Never", size - 1);
        time_str[size - 1] = '\0';
        return;
    }

    struct tm *tm_info = localtime(&timestamp);
    strftime(time_str, size, "%Y-%m-%d %I:%M %p", tm_info);
}

const char *zone_status_to_string(zone_t *zone, sprinkler_controller_status_t *sprinkler_status)
{
    if (!zone->enabled)
    {
        return "disabled";
    }
    if (sprinkler_status->is_running && zone->id == sprinkler_status->current_zone_id)
    {
        if (sprinkler_status->current_program_id == 255)
        {
            return "testing";
        }
        return "running";
    }
    return "idle";
}

const char *program_status_to_string(program_t *program, sprinkler_controller_status_t *sprinkler_status)
{
    if (!program->enabled)
    {
        return "disabled";
    }
    if (sprinkler_status->is_running && program->id == sprinkler_status->current_program_id)
    {
        return "running";
    }
    return "scheduled";
}

esp_err_t sprinkler_zones_to_json(const sprinkler_data_t *data, char *json_buffer, size_t buffer_size)
{
    char entry[JSON_ENTRY_SIZE];

    sprinkler_controller_get_status(&sprinkler_status);

    // Start JSON structure
    strcpy(json_buffer, "{\"type\":\"zone_list\",\"zones\":[");

    bool first = true;
    for (int zone_id = 1; zone_id <= MAX_ZONES; zone_id++)
    {
        if (data->zones[zone_id - 1].id == 0) // Empty slot
            continue;

        zone_t *zone = (zone_t *)&data->zones[zone_id - 1];
        snprintf(entry, sizeof(entry),
                 "%s{\"id\":%d,\"name\":\"%s\",\"output\":%d,\"enabled\":%s,\"lastRun\":%lld,\"status\":\"%s\"}",
                 (first ? "" : ","),
                 zone->id,
                 zone->name,
                 zone->output,
                 zone->enabled ? "true" : "false",
                 zone->last_run,
                 zone_status_to_string(zone, &sprinkler_status));

        // Check if we have enough space
        if (strlen(json_buffer) + strlen(entry) + 3 >= buffer_size)
        {
            ESP_LOGE(TAG, "JSON buffer too small for zones");
            return ESP_ERR_NO_MEM;
        }

        strncat(json_buffer, entry, buffer_size - strlen(json_buffer) - 1);

        first = false;
    }

    strncat(json_buffer, "]}", buffer_size - strlen(json_buffer) - 1);

    ESP_LOGI(TAG, "Generated zones JSON (%d bytes)", strlen(json_buffer));
    return ESP_OK;
}

esp_err_t sprinkler_programs_to_json(const sprinkler_data_t *data, char *json_buffer, size_t buffer_size)
{
    char entry[JSON_ENTRY_SIZE];
    char zone_entry[64];
    char days_buffer[16];

    sprinkler_controller_get_status(&sprinkler_status);

    // Start JSON structure
    strcpy(json_buffer, "{\"type\":\"program_list\",\"programs\":[");

    bool first = true;
    for (int program_id = 1; program_id <= MAX_PROGRAMS; program_id++)
    {
        if (data->programs[program_id - 1].id == 0) // Empty slot
            continue;

        program_t *prog = (program_t *)&data->programs[program_id - 1];
        days_to_json(prog->schedule.days, days_buffer, sizeof(days_buffer));

        // Start program entry
        snprintf(entry, sizeof(entry),
                 "%s{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"schedule\":{\"days\":%s,\"startTime\":\"%02d:%02d\"},\"zones\":[",
                 (first ? "" : ","),
                 prog->id,
                 prog->name,
                 prog->enabled ? "true" : "false",
                 days_buffer,
                 prog->schedule.start_hour,
                 prog->schedule.start_minute);

        if (strlen(json_buffer) + strlen(entry) >= buffer_size - 100)
        {
            ESP_LOGE(TAG, "JSON buffer too small for programs");
            return ESP_ERR_NO_MEM;
        }

        strncat(json_buffer, entry, buffer_size - strlen(json_buffer) - 1);

        // Add zones for this program
        for (int j = 0; j < prog->zone_count; j++)
        {
            program_zone_t *pz = (program_zone_t *)&prog->zones[j];
            snprintf(zone_entry, sizeof(zone_entry),
                     "%s{\"id\":%d,\"duration\":%d,\"order\":%d}",
                     (j > 0 ? "," : ""),
                     pz->zone_id,
                     pz->duration,
                     pz->order);

            if (strlen(json_buffer) + strlen(zone_entry) >= buffer_size - 50)
            {
                ESP_LOGE(TAG, "JSON buffer too small for program zones");
                return ESP_ERR_NO_MEM;
            }

            strncat(json_buffer, zone_entry, buffer_size - strlen(json_buffer) - 1);
        }

        // Close zones array and add program metadata
        snprintf(entry, sizeof(entry),
                 "],\"lastRun\":%lld,\"nextRun\":%lld,\"status\":\"%s\"}",
                 prog->last_run,
                 prog->next_run,
                 program_status_to_string(prog, &sprinkler_status));

        if (strlen(json_buffer) + strlen(entry) >= buffer_size - 10)
        {
            ESP_LOGE(TAG, "JSON buffer too small for program end");
            return ESP_ERR_NO_MEM;
        }

        strncat(json_buffer, entry, buffer_size - strlen(json_buffer) - 1);

        first = false;
    }

    strncat(json_buffer, "]}", buffer_size - strlen(json_buffer) - 1);

    ESP_LOGI(TAG, "Generated programs JSON (%d bytes)", strlen(json_buffer));
    return ESP_OK;
}
