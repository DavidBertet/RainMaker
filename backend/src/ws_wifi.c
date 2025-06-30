// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "ws_wifi.h"

#include "wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>

#include "ws_settings.h"

#define MAX_APs 10
static const char *TAG = "ws_wifi_scan";
static char json[2048];
static char entry[128];
static char status_info[512];

static const char *wifi_mode_to_string(wifi_mode_t mode)
{
    switch (mode)
    {
    case WIFI_MODE_NULL:
        return "OFF";
    case WIFI_MODE_STA:
        return "STA";
    case WIFI_MODE_AP:
        return "AP";
    case WIFI_MODE_APSTA:
        return "AP+STA";
    default:
        return "UNKNOWN";
    }
}

static const char *wifi_auth_mode_to_string(wifi_auth_mode_t auth_mode)
{
    switch (auth_mode)
    {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2_WPA3_PSK";
    default:
        return "UNKNOWN";
    }
}

static void get_wifi_status_info(char *buffer, size_t buffer_size)
{
    wifi_mode_t mode;
    wifi_config_t wifi_config;
    wifi_ap_record_t ap_info;
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif;
    uint8_t mac[6];
    char ip_str[16] = "0.0.0.0";
    char gateway_str[16] = "0.0.0.0";
    char netmask_str[16] = "0.0.0.0";
    char mac_str[18] = "00:00:00:00:00:00";

    // Get WiFi mode
    esp_wifi_get_mode(&mode);

    // Get MAC address
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Initialize status string
    snprintf(buffer, buffer_size,
             "\"status\":{\"mode\":\"%s\",\"mac\":\"%s\"",
             wifi_mode_to_string(mode), mac_str);

    // Get STA specific information if in STA mode
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
    {
        // Get STA configuration
        esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

        // Get connection info
        esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
        if (ret == ESP_OK)
        {
            // Connected to AP - get detailed info
            netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif)
            {
                esp_netif_get_ip_info(netif, &ip_info);
                esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
                esp_ip4addr_ntoa(&ip_info.gw, gateway_str, sizeof(gateway_str));
                esp_ip4addr_ntoa(&ip_info.netmask, netmask_str, sizeof(netmask_str));
            }

            char temp_buffer[256];
            snprintf(temp_buffer, sizeof(temp_buffer),
                     ",\"sta\":{\"connected\":true,\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"auth_mode\":\"%s\",\"ip\":\"%s\",\"gateway\":\"%s\",\"netmask\":\"%s\"}",
                     (char *)ap_info.ssid, ap_info.rssi, ap_info.primary,
                     wifi_auth_mode_to_string(ap_info.authmode),
                     ip_str, gateway_str, netmask_str);
            strncat(buffer, temp_buffer, buffer_size - strlen(buffer) - 1);
        }
        else
        {
            // Not connected but STA mode is active
            char temp_buffer[128];
            snprintf(temp_buffer, sizeof(temp_buffer),
                     ",\"sta\":{\"connected\":false,\"configured_ssid\":\"%s\"}",
                     (char *)wifi_config.sta.ssid);
            strncat(buffer, temp_buffer, buffer_size - strlen(buffer) - 1);
        }
    }

    // Get AP specific information if in AP mode
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
    {
        esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
        uint8_t ap_mac[6];
        esp_wifi_get_mac(WIFI_IF_AP, ap_mac);
        char ap_mac_str[18];
        snprintf(ap_mac_str, sizeof(ap_mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5]);

        // Get AP IP info
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (netif)
        {
            esp_netif_get_ip_info(netif, &ip_info);
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
        }

        // Get number of connected stations
        wifi_sta_list_t sta_list;
        esp_wifi_ap_get_sta_list(&sta_list);

        char temp_buffer[256];
        snprintf(temp_buffer, sizeof(temp_buffer),
                 ",\"ap\":{\"ssid\":\"%s\",\"channel\":%d,\"auth_mode\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\",\"connected_stations\":%d,\"max_connections\":%d}",
                 (char *)wifi_config.ap.ssid, wifi_config.ap.channel,
                 wifi_auth_mode_to_string(wifi_config.ap.authmode),
                 ip_str, ap_mac_str, sta_list.num, wifi_config.ap.max_connection);
        strncat(buffer, temp_buffer, buffer_size - strlen(buffer) - 1);
    }

    strncat(buffer, "}", buffer_size - strlen(buffer) - 1);
}

// Returns current device WiFi configuration and connection status
void ws_handle_wifi_status(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received wifi_status request");

    // Get current WiFi status
    get_wifi_status_info(status_info, sizeof(status_info));

    // Build JSON response with just status information
    snprintf(json, sizeof(json), "{\"type\":\"wifi_status\",%s}", status_info);

    broadcast_message(json);
    ESP_LOGI(TAG, "Sent wifi_status: %s", json);
}

// ServiceReturns available networks
void ws_handle_wifi_scan(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received wifi_scan request");

    // Perform WiFi scan
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    uint16_t ap_num = MAX_APs;
    wifi_ap_record_t ap_records[MAX_APs];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    // Build JSON response with just scan results
    strcpy(json, "{\"type\":\"wifi_list\",\"networks\":[");

    for (int i = 0; i < ap_num; ++i)
    {
        snprintf(entry, sizeof(entry),
                 "%s{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"auth_mode\":\"%s\",\"secure\":%s}",
                 (i > 0 ? "," : ""),
                 (char *)ap_records[i].ssid,
                 ap_records[i].rssi,
                 ap_records[i].primary,
                 wifi_auth_mode_to_string(ap_records[i].authmode),
                 (ap_records[i].authmode == WIFI_AUTH_OPEN ? "false" : "true"));
        strncat(json, entry, sizeof(json) - strlen(json) - 1);
    }
    strncat(json, "]}", sizeof(json) - strlen(json) - 1);

    send_message_sockfd(json, sockfd);
    ESP_LOGI(TAG, "Sent wifi_list: %s", json);
}

void ws_handle_wifi_connect(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received wifi_connect request");

    // Check if already connecting
    if (is_wifi_connecting())
    {
        strcpy(json, "{\"type\":\"error\",\"message\":\"WiFi connection already in progress\"}");
        send_message_sockfd(json, sockfd);
        return;
    }

    // Parse SSID and password from JSON
    const cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    const cJSON *password_json = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json))
    {
        ESP_LOGE(TAG, "Invalid JSON");
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
        send_message_sockfd(json, sockfd);
        return;
    }

    const char *ssid = ssid_json->valuestring;
    const char *password = password_json->valuestring;

    // Validate input
    if (strlen(ssid) == 0 || strlen(ssid) >= 32 || strlen(password) >= 64)
    {
        strcpy(json, "{\"type\":\"error\",\"message\":\"Invalid inputs\"}");
        send_message_sockfd(json, sockfd);
        return;
    }

    esp_err_t ret = wifi_start_sta_connection(ssid, password);
    if (ret != ESP_OK)
    {
        strcpy(json, "{\"type\":\"error\",\"message\":\"Failed to start connection\"}");
        send_message_sockfd(json, sockfd);
        return;
    }

    // Wait a bit to see if connection succeeds
    bool is_connected = wait_wifi_connection();

    if (!is_connected)
    {
        strcpy(json, "{\"type\":\"error\",\"message\":\"Couldn't connect, check that your are in range and that your password is correct\"}");
        send_message_sockfd(json, sockfd);
        return;
    }

    strcpy(json, "{\"type\":\"wifi_connect_success\"}");
    send_message_sockfd(json, sockfd);

    broadcast_get_settings();
}

void ws_handle_wifi_disconnect(const cJSON *root, int sockfd)
{
    ESP_LOGI(TAG, "Received wifi_disconnect request");
    wifi_stop_sta_connection();

    broadcast_get_settings();
}