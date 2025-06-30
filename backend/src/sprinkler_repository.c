// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "sprinkler_repository.h"

#include "sprinkler_storage.h"
#include "sprinkler_controller.h"
#include "ws_sprinkler.h"
#include "days_utils.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "SPRINKLER_REPOSITORY";
sprinkler_data_t sprinkler_data;

static SemaphoreHandle_t sprinkler_data_mutex = NULL;

static esp_err_t sprinkler_remove_zone_from_program(uint8_t program_id,
                                                    uint8_t zone_id);

esp_err_t sprinkler_repository_init(void)
{
    // Create mutex for protecting sprinkler_data
    sprinkler_data_mutex = xSemaphoreCreateMutex();
    if (sprinkler_data_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create sprinkler data mutex");
        return ESP_ERR_NO_MEM;
    }

    sprinkler_load_all_data(&sprinkler_data);

    return ESP_OK;
}

esp_err_t sprinkler_create_or_update_zone(uint8_t zone_id, const char *name, uint8_t output)
{
    // Update existing zone
    if (zone_id)
    {
        if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to take mutex in sprinkler_remove_zone");
            return ESP_ERR_TIMEOUT;
        }

        zone_t *zone = &sprinkler_data.zones[zone_id - 1];
        if (zone->id == 0)
        {
            xSemaphoreGive(sprinkler_data_mutex);
            return ESP_ERR_INVALID_STATE;
        }
        strncpy(zone->name, name, MAX_ZONE_NAME_LEN - 1);
        zone->output = output;
        esp_err_t ret = sprinkler_save_zone(zone);
        xSemaphoreGive(sprinkler_data_mutex);

        init_zone_gpio(zone);

        if (ret == ESP_OK)
        {
            broadcast_zone_update();
        }
        return ret;
    }

    if (sprinkler_data.zone_count >= MAX_ZONES)
    {
        return ESP_ERR_NO_MEM;
    }

    // Find the first available slot (could have holes from deleted zones)
    int available_slot = -1;

    for (int i = 0; i < MAX_ZONES; i++)
    {
        if (sprinkler_data.zones[i].id == 0) // Empty slot
        {
            available_slot = i;
            break;
        }
    }

    if (available_slot == -1)
    {
        return ESP_ERR_NO_MEM;
    }

    zone_t *zone = &sprinkler_data.zones[available_slot];
    zone->id = available_slot + 1;
    strncpy(zone->name, name, MAX_ZONE_NAME_LEN - 1);
    zone->name[MAX_ZONE_NAME_LEN - 1] = '\0';
    zone->output = output;
    zone->enabled = true;
    zone->last_run = 0;

    // Save the new zone immediately
    esp_err_t err = sprinkler_save_zone(zone);
    if (err == ESP_OK)
    {
        init_zone_gpio(zone);
        sprinkler_data.zone_count++;
        xSemaphoreGive(sprinkler_data_mutex);
        broadcast_zone_update();
    }
    else
    {
        xSemaphoreGive(sprinkler_data_mutex);
    }

    return err;
}

esp_err_t sprinkler_remove_zone(uint8_t zone_id)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_remove_zone");
        return ESP_ERR_TIMEOUT;
    }

    // Remove zone from memory
    memset(&sprinkler_data.zones[zone_id - 1], 0, sizeof(zone_t));
    sprinkler_data.zone_count--;

    // Remove zone from storage
    esp_err_t err = sprinkler_delete_zone(zone_id);
    if (err != ESP_OK)
    {
        xSemaphoreGive(sprinkler_data_mutex);
        ESP_LOGI(TAG, "Failed to delete zone %d", zone_id);
        return err;
    }

    // Remove zone if used in a program and reorder the zones in the program to fill the gap
    for (int i = 1; i <= MAX_PROGRAMS; i++)
    {
        sprinkler_remove_zone_from_program(i, zone_id);
    }

    xSemaphoreGive(sprinkler_data_mutex);

    broadcast_zone_update();
    broadcast_program_update();

    return err;
}

esp_err_t sprinkler_enable_zone(uint8_t zone_id, bool is_enabled)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_enable_zone");
        return ESP_ERR_TIMEOUT;
    }

    zone_t *zone = &sprinkler_data.zones[zone_id - 1];
    zone->enabled = is_enabled;

    esp_err_t err = sprinkler_save_zone(zone);
    xSemaphoreGive(sprinkler_data_mutex);

    if (err == ESP_OK)
    {
        broadcast_zone_update();
    }

    return err;
}

esp_err_t sprinkler_create_or_update_program(uint8_t program_id, const char *name, uint8_t days,
                                             uint8_t start_hour, uint8_t start_minute, const program_zone_t *zones, uint8_t zone_count)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_create_or_update_program");
        return ESP_ERR_TIMEOUT;
    }

    // Update existing zone
    if (program_id)
    {
        program_t *program = &sprinkler_data.programs[program_id - 1];
        if (program->id == 0)
        {
            xSemaphoreGive(sprinkler_data_mutex);
            return ESP_ERR_INVALID_STATE;
        }
        strncpy(program->name, name, MAX_ZONE_NAME_LEN - 1);
        program->schedule.days = days;
        program->schedule.start_hour = start_hour;
        program->schedule.start_minute = start_minute;

        // Update zones
        program->zone_count = zone_count;
        for (int i = 0; i < zone_count; i++)
        {
            program_zone_t *zone = &program->zones[i];
            zone->zone_id = zones[i].zone_id;
            zone->order = zones[i].order;
            zone->duration = zones[i].duration;
        }

        program->next_run = calculate_next_run(days, start_hour, start_minute);

        esp_err_t ret = sprinkler_save_program(program);
        xSemaphoreGive(sprinkler_data_mutex);

        if (ret == ESP_OK)
        {
            broadcast_program_update();
        }
        return ret;
    }

    if (sprinkler_data.program_count >= MAX_PROGRAMS)
    {
        xSemaphoreGive(sprinkler_data_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Find the first available slot (could have holes from deleted zones)
    int available_slot = -1;

    for (int i = 0; i < MAX_PROGRAMS; i++)
    {
        if (sprinkler_data.programs[i].id == 0) // Empty slot
        {
            available_slot = i;
            break;
        }
    }

    if (available_slot == -1)
    {
        xSemaphoreGive(sprinkler_data_mutex);
        return ESP_ERR_NO_MEM;
    }

    program_t *program = &sprinkler_data.programs[available_slot];
    program->id = available_slot + 1;
    strncpy(program->name, name, MAX_PROGRAM_NAME_LEN - 1);
    program->name[MAX_PROGRAM_NAME_LEN - 1] = '\0';
    program->enabled = true;

    program->schedule.days = days;
    program->schedule.start_hour = start_hour;
    program->schedule.start_minute = start_minute;

    // Update zones
    program->zone_count = zone_count;
    for (int i = 0; i < zone_count; i++)
    {
        program_zone_t *zone = &program->zones[i];
        zone->zone_id = zones[i].zone_id;
        zone->order = zones[i].order;
        zone->duration = zones[i].duration;
    }

    program->last_run = 0;
    program->next_run = calculate_next_run(days, start_hour, start_minute);

    // Save the new program immediately
    esp_err_t err = sprinkler_save_program(program);

    if (err == ESP_OK)
    {
        sprinkler_data.program_count++;
        xSemaphoreGive(sprinkler_data_mutex);
        broadcast_program_update();
    }
    else
    {
        xSemaphoreGive(sprinkler_data_mutex);
    }

    return err;
}

esp_err_t sprinkler_remove_program(uint8_t program_id)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_remove_program");
        return ESP_ERR_TIMEOUT;
    }

    // Remove program from memory
    memset(&sprinkler_data.programs[program_id - 1], 0, sizeof(program_t));
    sprinkler_data.program_count--;

    // Remove zone from storage
    esp_err_t err = sprinkler_delete_program(program_id);
    xSemaphoreGive(sprinkler_data_mutex);

    if (err == ESP_OK)
    {
        broadcast_program_update();
    }

    return err;
}

esp_err_t sprinkler_enable_program(uint8_t program_id, bool is_enabled)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_enable_program");
        return ESP_ERR_TIMEOUT;
    }

    program_t *program = &sprinkler_data.programs[program_id - 1];
    program->enabled = is_enabled;

    esp_err_t err = sprinkler_save_program(program);
    xSemaphoreGive(sprinkler_data_mutex);

    if (err == ESP_OK)
    {
        broadcast_program_update();
    }

    return err;
}

esp_err_t sprinkler_add_zone_to_program(uint8_t program_id,
                                        uint8_t zone_id, uint16_t duration, uint8_t order)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_add_zone_to_program");
        return ESP_ERR_TIMEOUT;
    }

    if (program_id > sprinkler_data.program_count || program_id == 0)
    {
        xSemaphoreGive(sprinkler_data_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    program_t *program = &sprinkler_data.programs[program_id - 1];
    if (program->zone_count >= MAX_ZONES_PER_PROGRAM)
    {
        xSemaphoreGive(sprinkler_data_mutex);
        return ESP_ERR_NO_MEM;
    }

    program_zone_t *prog_zone = &program->zones[program->zone_count];
    prog_zone->zone_id = zone_id;
    prog_zone->duration = duration;
    prog_zone->order = order;

    program->zone_count++;

    // Save the updated program immediately
    esp_err_t ret = sprinkler_save_program(program);
    xSemaphoreGive(sprinkler_data_mutex);

    if (ret == ESP_OK)
    {
        broadcast_program_update();
    }
    return ret;
}

esp_err_t sprinkler_update_zone_status(uint8_t zone_id, bool turn_on)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_update_zone_status");
        return ESP_ERR_TIMEOUT;
    }

    if (turn_on)
    {
        time(&sprinkler_data.zones[zone_id - 1].last_run);
    }

    // Save the updated zone immediately
    esp_err_t ret = sprinkler_save_zone(&sprinkler_data.zones[zone_id - 1]);
    xSemaphoreGive(sprinkler_data_mutex);

    if (ret == ESP_OK)
    {
        broadcast_zone_update();
    }
    return ret;
}

esp_err_t sprinkler_update_program_next_run(uint8_t program_id)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_update_program_next_run");
        return ESP_ERR_TIMEOUT;
    }

    program_t *program = &sprinkler_data.programs[program_id - 1];
    program->next_run = calculate_next_run(program->schedule.days, program->schedule.start_hour, program->schedule.start_minute);

    esp_err_t ret = sprinkler_save_program(&sprinkler_data.programs[program_id - 1]);
    xSemaphoreGive(sprinkler_data_mutex);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save program next run");
        return ret;
    }

    broadcast_program_update();
    return ESP_OK;
}

esp_err_t sprinkler_update_program_last_run(uint8_t program_id)
{
    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex in sprinkler_update_program_last_run");
        return ESP_ERR_TIMEOUT;
    }

    time(&sprinkler_data.programs[program_id - 1].last_run);
    esp_err_t ret = sprinkler_save_program(&sprinkler_data.programs[program_id - 1]);
    xSemaphoreGive(sprinkler_data_mutex);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save program last run");
        return ret;
    }

    broadcast_program_update();
    return ESP_OK;
}

// Note: This function is called from sprinkler_remove_zone which already holds the mutex
// So we don't need to acquire it again here
static esp_err_t sprinkler_remove_zone_from_program(uint8_t program_id,
                                                    uint8_t zone_id)
{
    if (program_id > sprinkler_data.program_count || program_id == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    program_t *program = &sprinkler_data.programs[program_id - 1];
    bool program_modified = false;

    // Find and remove the zone from this program
    for (int j = 0; j < program->zone_count; j++)
    {
        if (program->zones[j].zone_id == zone_id)
        {
            // Shift all subsequent zones left to fill the gap
            for (int k = j; k < program->zone_count - 1; k++)
            {
                program->zones[k] = program->zones[k + 1];
                program->zones[k].order -= 1;
            }
            program->zone_count--;
            program_modified = true;
            j--; // Decrement j to check the same position again (now contains the next zone)
        }
    }

    // Save the modified program if it was changed
    if (program_modified)
    {
        esp_err_t ret = sprinkler_save_program(program);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "Failed to save program %d", program->id);
        }
        else
        {
            ESP_LOGI(TAG, "Removed zone %d from program %d", zone_id, program->id);
        }
        return ret;
    }

    return ESP_OK;
}

esp_err_t safe_sprinklerdata_operation(esp_err_t (*operation)(const sprinkler_data_t *, void *), void *user_data)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(sprinkler_data_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        ret = operation(&sprinkler_data, user_data);
        xSemaphoreGive(sprinkler_data_mutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to acquire sprinkler data mutex");
    }

    return ret;
}