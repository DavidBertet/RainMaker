// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "sprinkler_storage.h"
#include "sprinkler_controller.h"

#define JSON_BUFFER_SIZE 2048
#define JSON_ENTRY_SIZE 256

// Function prototypes for JSON serialization
esp_err_t sprinkler_zones_to_json(const sprinkler_data_t *data, char *json_buffer, size_t buffer_size);
esp_err_t sprinkler_programs_to_json(const sprinkler_data_t *data, char *json_buffer, size_t buffer_size);

// Helper functions for time formatting
int days_to_json(uint8_t days, char *buffer, size_t buffer_size);
void format_time_string(time_t timestamp, char *time_str, size_t size);
const char *zone_status_to_string(zone_t *zone, sprinkler_controller_status_t *sprinkler_status);
const char *program_status_to_string(program_t *program, sprinkler_controller_status_t *sprinkler_status);
