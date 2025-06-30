// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "sntp.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "lwip/sys.h"

#include "wifi.h"

static const char *TAG = "NTP_TIME";

static esp_err_t (*user_time_sync_callback)(void) = NULL;

static void get_and_print_time(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    // Get current time
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format and print the time
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current local time: %s", strftime_buf);

    // Also print Unix timestamp
    ESP_LOGI(TAG, "Unix timestamp: %lld", now);
}

// Callback function called when time is synchronized
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    get_and_print_time();

    if (user_time_sync_callback != NULL)
    {
        user_time_sync_callback();
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");

    // Set the callback function for time synchronization
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // Set the operating mode
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set the SNTP server
    esp_sntp_setservername(0, NTP_SERVER);

    // Initialize SNTP service
    esp_sntp_init();
}

static void ntp_time_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    while (!wait_wifi_connection())
        ESP_LOGI(TAG, "Waiting for WiFi connection...");

    ESP_LOGI(TAG, "WiFi connected, starting NTP sync");

    setenv("TZ", NTP_TIMEZONE, 1);
    tzset();

    initialize_sntp();

    vTaskDelete(NULL);
}

void start_ntp_sync(void)
{
    xTaskCreate(ntp_time_task, "ntp_time_task", 4096, NULL, 5, NULL);
}

bool is_time_set(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);

    // Check if year is reasonable (greater than 2020)
    return (timeinfo.tm_year > (2020 - 1900));
}

void register_time_sync_callback(esp_err_t (*callback)(void))
{
    user_time_sync_callback = callback;
}