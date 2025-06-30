// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "sprinkler_storage.h"

extern sprinkler_data_t sprinkler_data;

esp_err_t sprinkler_repository_init(void);

esp_err_t sprinkler_create_or_update_zone(uint8_t zone_id, const char *name, uint8_t output);
esp_err_t sprinkler_remove_zone(uint8_t zone_id);
esp_err_t sprinkler_enable_zone(uint8_t zone_id, bool is_enabled);
esp_err_t sprinkler_create_or_update_program(uint8_t program_id, const char *name, uint8_t days,
                                             uint8_t start_hour, uint8_t start_minute, const program_zone_t *zones, uint8_t zone_count);
esp_err_t sprinkler_remove_program(uint8_t program_id);
esp_err_t sprinkler_enable_program(uint8_t program_id, bool is_enabled);
esp_err_t sprinkler_add_zone_to_program(uint8_t program_id,
                                        uint8_t zone_id, uint16_t duration, uint8_t order);
esp_err_t sprinkler_update_zone_status(uint8_t zone_id, bool turn_on);
esp_err_t sprinkler_update_program_next_run(uint8_t program_id);
esp_err_t sprinkler_update_program_last_run(uint8_t program_id);

esp_err_t safe_sprinklerdata_operation(esp_err_t (*operation)(const sprinkler_data_t *, void *), void *user_data);