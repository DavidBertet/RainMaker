// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "sprinkler_storage.h"

#include "ws_sprinkler.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPRINKLER_STORAGE";

esp_err_t sprinkler_load_all_data(sprinkler_data_t *data)
{
    esp_err_t err = ESP_OK;

    // Initialize data structure
    memset(data, 0, sizeof(sprinkler_data_t));

    // Load all zones
    for (uint8_t i = 1; i <= MAX_ZONES; i++)
    {
        zone_t zone;
        if (sprinkler_load_zone(i, &zone) == ESP_OK)
        {
            data->zones[zone.id - 1] = zone;
            data->zone_count++;
        }
    }

    // Load all programs
    for (uint8_t i = 1; i <= MAX_PROGRAMS; i++)
    {
        program_t program;
        if (sprinkler_load_program(i, &program) == ESP_OK)
        {
            data->programs[program.id - 1] = program;
            data->program_count++;
        }
    }

    ESP_LOGI(TAG, "Loaded %d zones and %d programs", data->zone_count, data->program_count);
    return err;
}

void sprinkler_delete_all_data(void)
{
    // Delete all zones
    for (uint8_t i = 1; i <= MAX_ZONES; i++)
    {
        sprinkler_delete_zone(i);
    }

    // Delete all programs
    for (uint8_t i = 1; i <= MAX_PROGRAMS; i++)
    {
        sprinkler_delete_program(i);
    }

    ESP_LOGI(TAG, "Deleted all zones and all programs");
}

esp_err_t sprinkler_save_zone(const zone_t *zone)
{
    char key[16];
    snprintf(key, sizeof(key), "zone_%d", zone->id);
    return write_blob(key, zone, sizeof(zone_t));
}

esp_err_t sprinkler_load_zone(uint8_t zone_id, zone_t *zone)
{
    char key[16];
    snprintf(key, sizeof(key), "zone_%d", zone_id);
    size_t required_size = sizeof(zone_t);
    return read_blob(key, zone, &required_size);
}

esp_err_t sprinkler_delete_zone(uint8_t zone_id)
{
    char key[16];
    snprintf(key, sizeof(key), "zone_%d", zone_id);
    return delete_blob(key);
}

esp_err_t sprinkler_save_program(const program_t *program)
{
    char key[16];
    snprintf(key, sizeof(key), "prog_%d", program->id);
    return write_blob(key, program, sizeof(program_t));
}

esp_err_t sprinkler_load_program(uint8_t program_id, program_t *program)
{
    char key[16];
    snprintf(key, sizeof(key), "prog_%d", program_id);
    size_t required_size = sizeof(program_t);
    return read_blob(key, program, &required_size);
}

esp_err_t sprinkler_delete_program(uint8_t program_id)
{
    char key[16];
    snprintf(key, sizeof(key), "prog_%d", program_id);
    return delete_blob(key);
}
