// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#pragma once

#include <esp_http_server.h>
#include "esp_err.h"

void setup_server(void);

esp_err_t get_session_from_cookies(httpd_req_t *req, char *session_token, size_t max_len);