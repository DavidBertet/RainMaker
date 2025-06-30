// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "cJSON.h"
#include "esp_err.h"

esp_err_t ws_update_system_init(void);

void ws_handle_get_zones(const cJSON *root, int sockfd);
void ws_handle_get_programs(const cJSON *root, int sockfd);
void ws_handle_get_all_data(const cJSON *root, int sockfd);

void ws_handle_create_or_update_zone(const cJSON *root, int sockfd);
void ws_handle_delete_zone(const cJSON *root, int sockfd);

void ws_handle_create_or_update_program(const cJSON *root, int sockfd);
void ws_handle_delete_program(const cJSON *root, int sockfd);

void ws_handle_test_manual(const cJSON *root, int sockfd);
void ws_handle_enable(const cJSON *root, int sockfd);

void broadcast_zone_update(void);
void broadcast_program_update(void);