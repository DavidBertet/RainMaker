// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include "websocket.h"
#include "cJSON.h"

void ws_handle_wifi_status(const cJSON *root, int sockfd);
void ws_handle_wifi_scan(const cJSON *root, int sockfd);
void ws_handle_wifi_connect(const cJSON *root, int sockfd);
void ws_handle_wifi_disconnect(const cJSON *root, int sockfd);