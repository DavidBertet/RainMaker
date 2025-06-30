// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_event.h>
#include "esp_netif.h"

#include "storage.h"
#include "captdns.h"
#include "wifi.h"
#include "webserver.h"
#include "spiffs.h"
#include "sntp.h"

#include "websocket.h"
#include "ws_wifi.h"
#include "ws_sprinkler.h"
#include "ws_settings.h"

#include "sprinkler_controller.h"
#include "sprinkler_repository.h"

static const char *TAG = "main";

void app_main()
{
  // To disable all logs, use ESP_LOG_NONE
  esp_log_level_set("*", ESP_LOG_DEBUG);

  ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

  // Init NVS storage
  setup_storage();

  // Init TCP/IP stack
  ESP_ERROR_CHECK(esp_netif_init());
  // Init event mechanism
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Init file storage
  ESP_ERROR_CHECK(setup_spiffs());

  // Setup captive portal - automatically opens the page when we connect to the wifi
  // setup_captive_dns();

  // Setup wifi access point
  setup_wifi();

  // Setup HTTP server
  setup_server();

  sprinkler_repository_init();
  ws_update_system_init();

  // Register websocket callbacks
  register_callback("wifi_status", ws_handle_wifi_status);
  register_callback("wifi_scan", ws_handle_wifi_scan);
  register_callback("wifi_connect", ws_handle_wifi_connect);
  register_callback("wifi_disconnect", ws_handle_wifi_disconnect);

  register_callback("get_zones", ws_handle_get_zones);
  register_callback("get_programs", ws_handle_get_programs);

  register_callback("create_or_update_zone", ws_handle_create_or_update_zone);
  register_callback("delete_zone", ws_handle_delete_zone);
  register_callback("test_manual", ws_handle_test_manual);

  register_callback("create_or_update_program", ws_handle_create_or_update_program);
  register_callback("delete_program", ws_handle_delete_program);

  register_callback("enable", ws_handle_enable);

  register_callback("get_settings", ws_handle_get_settings);
  register_callback("time_update", ws_handle_time_update);
  register_callback("get_system_info", ws_handle_system_info);

  // Init sprinkler controller
  sprinkler_controller_init();

  // Retrieve time from network and start sprinkler
  start_ntp_sync();
  register_time_sync_callback(sprinkler_controller_start);
}
