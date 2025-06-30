// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "cJSON.h"

void ws_handle_get_settings(const cJSON *root, int sockfd);
void broadcast_get_settings(void);
void ws_handle_time_update(const cJSON *root, int sockfd);
void ws_handle_system_info(const cJSON *root, int sockfd);