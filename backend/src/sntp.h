// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include <stdbool.h>

#include "lwip/err.h"

// NTP server configuration
#define NTP_SERVER "pool.ntp.org"
// Set timezone. Examples:
// "EST5EDT,M3.2.0/2,M11.1.0" - Eastern Time
// "PST8PDT,M3.2.0,M11.1.0" - Pacific Time
// "CET-1CEST,M3.5.0,M10.5.0/3" - Central European Time
// "JST-9" - Japan Standard Time
// "UTC-0" - UTC
#define NTP_TIMEZONE "PST8PDT,M3.2.0,M11.1.0"

void start_ntp_sync(void);

// Utility function to check if time is set
bool is_time_set(void);

// Function to register a callback for when time is synchronized
void register_time_sync_callback(esp_err_t (*callback)(void));
