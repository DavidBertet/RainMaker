// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "sprinkler_storage.h"

typedef struct
{
    bool is_running;
    uint8_t current_program_id; // 255 = manual mode
    uint8_t current_zone_id;
    time_t zone_start_time;
    uint16_t zone_duration_seconds;
    uint16_t zone_remaining_seconds;
} sprinkler_controller_status_t;

/**
 * @brief Initialize the sprinkler controller
 * Sets up GPIO pins, creates FreeRTOS resources
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_init();

/**
 * @brief Initialize the GPIO for a specific zone
 *
 * @param zone Pointer to zone
 * @return esp_err_t ESP_OK on success
 */
esp_err_t init_zone_gpio(const zone_t *zone);

/**
 * @brief Start the sprinkler controller tasks
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_start(void);

/**
 * @brief Stop the sprinkler controller and all zones
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_stop(void);

/**
 * @brief Manually run a specific zone for a duration
 *
 * @param zone_id Zone to run
 * @param duration_seconds Duration in seconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_manual_zone(uint8_t zone_id, uint16_t duration_seconds);

/**
 * @brief Manually run a specific program immediately
 *
 * @param program_id Program to run
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_manual_program(uint8_t program_id);

/**
 * @brief Stop all zones/program immediately
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_stop_pending(void);

/**
 * @brief Get current controller status
 *
 * @param status Pointer to status structure to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sprinkler_controller_get_status(sprinkler_controller_status_t *status);
