// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "wifi.h"

#include "ws_wifi.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define AP_WIFI_SSID "RainMaker"
#define AP_WIFI_PASS "Arrosage!"
#define AP_WIFI_CHANNEL 10
#define MAX_AP_CONN 2

static const char *TAG = "wifi";

typedef struct
{
    char ssid[32];
    char password[64];
} wifi_credentials_t;

// WiFi event group
static EventGroupHandle_t s_wifi_event_group = NULL;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;

// Connection state
static bool wifi_connecting = false;
static bool wifi_connected = false;
static int s_retry_num = 0;
static const int WIFI_MAXIMUM_RETRY = 5;

static void setup_apsta(void)
{
    // Check if AP is already setup
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"))
    {
        return;
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_WIFI_SSID,
            .ssid_len = strlen(AP_WIFI_SSID),
            .channel = AP_WIFI_CHANNEL,
            .password = AP_WIFI_PASS,
            .max_connection = MAX_AP_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(AP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    ESP_LOGI(TAG, "WiFi AP init finished. SSID:%s password:%s channel:%d", AP_WIFI_SSID, AP_WIFI_PASS, AP_WIFI_CHANNEL);
}

static esp_err_t setup_sta(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA init finished.");
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        printf("WiFi AP started successfully!\n");
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        const wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED reason %d", event->reason);
        // Don't retry if it's intented disconnection
        if (event->reason < 200) // see wifi_err_reason_t
        {
            return;
        }

        if (s_retry_num < WIFI_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            wifi_connecting = false;
        }
        ESP_LOGI(TAG, "Connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_connected = true;
        wifi_connecting = false;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connect_task(void *pvParameters)
{
    wifi_credentials_t *creds = (wifi_credentials_t *)pvParameters;

    ESP_LOGI(TAG, "Starting WiFi connection to SSID: %s", creds->ssid);

    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connection result
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    // Send connection result
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", creds->ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", creds->ssid);
    }

    // Broadcast new values
    ws_handle_wifi_status(NULL, 0);

    // Clean up
    free(creds);
    vTaskDelete(NULL);
}

bool wait_wifi_connection(void)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(10000)); // Wait 10 seconds

    wifi_connecting = false;

    if (bits & WIFI_CONNECTED_BIT)
    {
        return true;
    }
    else
    {
        return false;
    }
}

esp_err_t setup_wifi(void)
{
    ESP_LOGI(TAG, "Attempting auto-connect with stored credentials");

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &event_handler,
                                               NULL));

    if (s_wifi_event_group == NULL)
    {
        s_wifi_event_group = xEventGroupCreate();
    }

    // Initialize WiFi
    esp_err_t ret = setup_sta();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize STA WiFi");
        setup_apsta(); // Fall back to AP mode
        return ret;
    }

    // ESP-IDF will automatically try to connect with stored credentials
    // If connection fails, we'll get a disconnect event

    // Wait a bit to see if connection succeeds
    bool is_connected = wait_wifi_connection();

    if (is_connected)
    {
        ESP_LOGI(TAG, "Auto-connect successful");
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "Auto-connect failed or timed out, starting AP mode");
        setup_apsta(); // Fall back to AP mode
        wait_wifi_connection();
        return ESP_FAIL;
    }
}

bool is_wifi_connected(void)
{
    return wifi_connected;
}

bool is_wifi_connecting(void)
{
    return wifi_connecting;
}

bool is_wifi_setup(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS namespace
    err = nvs_open("nvs.net80211", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        return false;
    }

    // Check if WiFi SSID is stored
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, "sta.ssid", NULL, &required_size);
    nvs_close(nvs_handle);

    if (err == ESP_OK && required_size > 0)
    {
        return true;
    }

    return false;
}

esp_err_t wifi_start_sta_connection(const char *ssid, const char *password)
{
    wifi_connecting = true;

    wifi_credentials_t *creds = malloc(sizeof(wifi_credentials_t));
    if (!creds)
    {
        wifi_connecting = false;
        return ESP_ERR_NO_MEM;
    }
    strncpy(creds->ssid, ssid, sizeof(creds->ssid) - 1);
    strncpy(creds->password, password, sizeof(creds->password) - 1);
    creds->ssid[sizeof(creds->ssid) - 1] = '\0';
    creds->password[sizeof(creds->password) - 1] = '\0';

    // Reset retry counter and event group
    s_retry_num = 0;
    if (s_wifi_event_group)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    // Create task to handle WiFi connection
    BaseType_t result = xTaskCreate(wifi_connect_task, "wifi_connect_task", 4096, creds, 5, NULL);
    if (result != pdPASS)
    {
        wifi_connecting = false;
        free(creds);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void wifi_stop_sta_connection(void)
{
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disconnect from WiFi: %s", esp_err_to_name(ret));
    }

    wifi_config_t wifi_config = {0}; // Zero out the config
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clear WiFi config: %s", esp_err_to_name(ret));
    }

    nvs_handle_t nvs_handle;
    if (nvs_open("nvs.net80211", NVS_READWRITE, &nvs_handle) == ESP_OK)
    {
        nvs_erase_all(nvs_handle); // Clear all WiFi related NVS data
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    // Restart in AP mode
    setup_apsta();
    wait_wifi_connection();

    // Broadcast new values
    ws_handle_wifi_status(NULL, 0);
}