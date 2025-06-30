// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "webserver.h"

#include "freertos/task.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include "esp_netif.h"

#include "websocket.h"
#include "webfile.h"

// Local variables

static const char *TAG = "webserver";

static httpd_handle_t server = NULL;

// Implementation

static void on_client_disconnected(httpd_handle_t hd, int sockfd)
{
  on_ws_client_disconnected(sockfd);
}

static httpd_handle_t start_webserver(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.close_fn = on_client_disconnected;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.lru_purge_enable = true;
  config.enable_so_linger = true;

  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  esp_err_t ret = httpd_start(&server, &config);
  if (ret != ESP_OK)
  {
    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
  }
  // Set URI handlers
  ESP_LOGI(TAG, "Registering URI handlers");

  start_websocket(server);
  start_web_file(server);

  return server;
}
void setup_server(void)
{
  // Start the server
  server = start_webserver();
}

// Utils

esp_err_t get_session_from_cookies(httpd_req_t *req, char *session_token, size_t max_len)
{
  char cookie_header[256];
  size_t cookie_len = httpd_req_get_hdr_value_len(req, "Cookie") + 1;

  if (cookie_len <= 1 || cookie_len > sizeof(cookie_header))
  {
    return ESP_FAIL;
  }

  if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header, cookie_len) != ESP_OK)
  {
    return ESP_FAIL;
  }

  // Parse "session_id=token" from cookies
  char *session_start = strstr(cookie_header, "session_id=");
  if (!session_start)
  {
    return ESP_FAIL;
  }

  session_start += strlen("session_id=");
  char *session_end = strchr(session_start, ';');
  size_t token_len = session_end ? (session_end - session_start) : strlen(session_start);

  if (token_len >= max_len)
  {
    return ESP_FAIL;
  }

  strncpy(session_token, session_start, token_len);
  session_token[token_len] = '\0';

  return ESP_OK;
}