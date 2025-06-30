// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t setup_wifi(void);

bool wait_wifi_connection(void);
bool is_wifi_connected(void);
bool is_wifi_connecting(void);
bool is_wifi_setup(void);
esp_err_t wifi_start_sta_connection(const char *ssid, const char *password);
void wifi_stop_sta_connection(void);