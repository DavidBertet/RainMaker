// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "websocket.h"

#include "freertos/task.h"
#include <sys/unistd.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_timer.h>

#include "webserver.h"

// Local variables

static const char *TAG = "websocket";

#define MAX_CLIENTS 4
static int clients_fd[MAX_CLIENTS];

// Ping-pong mechanism
#define CLIENT_TIMEOUT_MS 60000      // 60 seconds timeout
#define PING_CHECK_INTERVAL_MS 30000 // Check every 30 seconds

typedef struct
{
  int fd;
  int64_t last_activity;
  bool ping_sent;
  char session_token[64];
} client_info_t;

static client_info_t clients_info[MAX_CLIENTS];
static esp_timer_handle_t ping_timer = NULL;

#define MAX_CALLBACKS 20

static char json[1024];

typedef struct
{
  char type[32];
  wsserver_receive_callback callback;
} ws_callback_entry;

static ws_callback_entry receive_callbacks[MAX_CALLBACKS];

static httpd_handle_t server = NULL;

// Implementations

struct async_resp_arg
{
  httpd_handle_t hd;
  int fd;
};

// Forward declarations
static void ping_timer_callback(void *arg);
static void send_ping_to_client(int sockfd);

// Manage clients

static esp_err_t on_client_connected(httpd_handle_t hd, int sockfd)
{
  ESP_LOGI(TAG, "WS Client Connected %i", sockfd);
  int available_index = -1;

  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] == sockfd)
    {
      return ESP_FAIL;
    }
    if (available_index == -1 && clients_fd[i] == -1)
    {
      available_index = i;
    }
  }

  if (available_index == -1)
  {
    ESP_LOGI(TAG, "No more space available for client %i", sockfd);
    return ESP_FAIL;
  }

  clients_fd[available_index] = sockfd;

  // Initialize client info for ping-pong
  clients_info[available_index].fd = sockfd;
  clients_info[available_index].last_activity = esp_timer_get_time() / 1000; // Convert to ms
  clients_info[available_index].ping_sent = false;
  clients_info[available_index].session_token[0] = '\0';

  return ESP_OK;
}

void on_ws_client_disconnected(int sockfd)
{
  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] == sockfd)
    {
      close(sockfd);
      clients_fd[i] = -1;

      // Clear client info
      clients_info[i].fd = -1;
      clients_info[i].last_activity = 0;
      clients_info[i].ping_sent = false;

      return;
    }
  }

  close(sockfd);

  return;
}

// Update client activity timestamp
static void update_client_activity(int sockfd)
{
  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] == sockfd)
    {
      clients_info[i].last_activity = esp_timer_get_time() / 1000; // Convert to ms
      clients_info[i].ping_sent = false;                           // Reset ping flag on any activity
      break;
    }
  }
}

// Ping-pong timer callback
static void ping_timer_callback(void *arg)
{
  int64_t current_time = esp_timer_get_time() / 1000; // Convert to ms

  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] == -1)
      continue;

    int64_t time_since_activity = current_time - clients_info[i].last_activity;

    // If client hasn't responded to ping, disconnect
    if (clients_info[i].ping_sent && time_since_activity > CLIENT_TIMEOUT_MS)
    {
      ESP_LOGW(TAG, "Client %d ping timeout, disconnecting", clients_fd[i]);
      on_ws_client_disconnected(clients_fd[i]);
      continue;
    }

    // If no activity for half the timeout period, send ping
    if (!clients_info[i].ping_sent && time_since_activity > (CLIENT_TIMEOUT_MS / 2))
    {
      ESP_LOGI(TAG, "Sending ping to client %d", clients_fd[i]);
      send_ping_to_client(clients_fd[i]);
      clients_info[i].ping_sent = true;
    }
  }
}

static void send_ping_to_client(int sockfd)
{
  if (server == NULL)
    return;

  snprintf(json, sizeof(json), "{\"type\":\"ping\",\"timestamp\":%lld}", esp_timer_get_time() / 1000);

  send_message_sockfd(json, sockfd);
}

static void ws_handle_ping_message(const cJSON *root, int sockfd)
{
  ESP_LOGI(TAG, "Received ping from client %d", sockfd);
  update_client_activity(sockfd);

  // Echo back timestamp if present
  cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
  if (timestamp && cJSON_IsNumber(timestamp))
  {
    snprintf(json, sizeof(json), "{\"type\":\"pong\",\"timestamp\":%f}", timestamp->valuedouble);
  }
  else
  {
    snprintf(json, sizeof(json), "{\"type\":\"pong\"}");
  }

  send_message_sockfd(json, sockfd);
}

static void ws_handle_pong_message(const cJSON *root, int sockfd)
{
  ESP_LOGI(TAG, "Received pong from client %d", sockfd);
  update_client_activity(sockfd);

  // Calculate RTT if timestamp is present
  cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
  if (timestamp && cJSON_IsNumber(timestamp))
  {
    int64_t current_time = esp_timer_get_time() / 1000; // Convert to ms
    int64_t rtt = current_time - (int64_t)timestamp->valuedouble;
    ESP_LOGI(TAG, "Client %d RTT: %lld ms", sockfd, rtt);
  }
}

// Token / cookie

static void extract_session_from_ws_request(httpd_req_t *req, int sockfd)
{
  char session_token[64];
  if (get_session_from_cookies(req, session_token, sizeof(session_token)) == ESP_OK)
  {
    // Store session for this client
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
      if (clients_fd[i] == sockfd)
      {
        strncpy(clients_info[i].session_token, session_token, sizeof(clients_info[i].session_token) - 1);
        clients_info[i].session_token[sizeof(clients_info[i].session_token) - 1] = '\0';
        ESP_LOGI(TAG, "WebSocket client %d session: %s", sockfd, session_token);
        break;
      }
    }
  }
}

static int find_client_by_session_token(const char *token)
{
  if (!token || strlen(token) == 0)
    return -1;

  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] != -1 &&
        strcmp(clients_info[i].session_token, token) == 0)
    {
      return clients_fd[i];
    }
  }
  return -1;
}

// Manage messages

esp_err_t send_message_token(char *msg, char *token)
{
  if (server == NULL)
  {
    ESP_LOGE(TAG, "Tried to send a message while server down");
    return ESP_FAIL;
  }

  int sockfd = find_client_by_session_token(token);
  if (sockfd == -1)
  {
    ESP_LOGE(TAG, "Client not found");
    return ESP_FAIL;
  }

  return send_message_sockfd(msg, sockfd);
}

esp_err_t send_message_sockfd(char *msg, int sockfd)
{
  if (server == NULL)
  {
    ESP_LOGE(TAG, "Tried to send a message while server down");
    return ESP_FAIL;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t *)msg;
  ws_pkt.len = strlen(msg);
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  ESP_LOGI(TAG, "Send message to %i", sockfd);
  esp_err_t ret = httpd_ws_send_frame_async(server, sockfd, &ws_pkt);
  if (ret != ESP_OK)
  {
    on_ws_client_disconnected(sockfd);
  }
  return ret;
}

esp_err_t broadcast_message(char *msg)
{
  if (server == NULL)
  {
    ESP_LOGE(TAG, "Tried to broadcast a message while server down");
    return ESP_FAIL;
  }

  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] == -1)
    {
      continue;
    }

    send_message_sockfd(msg, clients_fd[i]);
  }

  return ESP_OK;
}

// Handle received messages and forward to listener meaningful messages
esp_err_t receive_ws_message(httpd_req_t *req)
{
  int sockfd = httpd_req_to_sockfd(req);

  // Check for handshake
  if (req->method == HTTP_GET)
  {
    if (on_client_connected(server, sockfd) != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to create new client");
      return ESP_FAIL;
    }
    extract_session_from_ws_request(req, sockfd);
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  uint8_t *buffer = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  // Set max_len = 0 to get the frame len
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

  if (ws_pkt.len)
  {
    // ws_pkt.len + 1 is for NULL termination as we are expecting a string
    buffer = calloc(1, ws_pkt.len + 1);
    if (buffer == NULL)
    {
      ESP_LOGE(TAG, "Failed to calloc memory for buffer");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buffer;

    // Set max_len = ws_pkt.len to get the frame payload
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buffer);
      return ret;
    }

    // Update client activity for any received message
    update_client_activity(sockfd);

    // Parse JSON and extract type
    cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    if (root)
    {
      cJSON *type = cJSON_GetObjectItem(root, "type");
      if (type && cJSON_IsString(type))
      {
        for (int i = 0; i < MAX_CALLBACKS; ++i)
        {
          if (receive_callbacks[i].callback &&
              strcmp(receive_callbacks[i].type, type->valuestring) == 0)
          {
            receive_callbacks[i].callback(root, sockfd);
          }
        }
      }
      cJSON_Delete(root);
    }
    free(buffer);
  }

  ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);

  return ret;
}

// Manage consumer callbacks

void register_callback(const char *type, wsserver_receive_callback callback)
{
  int available_index = -1;
  for (int i = 0; i < MAX_CALLBACKS; ++i)
  {
    if (receive_callbacks[i].callback == callback && strcmp(receive_callbacks[i].type, type) == 0)
    {
      return;
    }
    if (receive_callbacks[i].callback == NULL && available_index == -1)
    {
      available_index = i;
    }
  }
  if (available_index != -1)
  {
    ESP_LOGI(TAG, "Register callback for type %s in the first available place", type);
    strncpy(receive_callbacks[available_index].type, type, sizeof(receive_callbacks[available_index].type) - 1);
    receive_callbacks[available_index].type[sizeof(receive_callbacks[available_index].type) - 1] = '\0';
    receive_callbacks[available_index].callback = callback;
  }
  else
  {
    ESP_LOGI(TAG, "Register callback has no available place");
  }
}

void unregister_callback(const char *type, wsserver_receive_callback callback)
{
  for (int i = 0; i < MAX_CALLBACKS; ++i)
  {
    if (receive_callbacks[i].callback == callback && strcmp(receive_callbacks[i].type, type) == 0)
    {
      receive_callbacks[i].callback = NULL;
      receive_callbacks[i].type[0] = '\0';
    }
  }
}

// Websocket lifecycle

void start_websocket(httpd_handle_t new_server)
{
  ESP_LOGI(TAG, "Start websocket");

  server = new_server;

  // Init clients
  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    clients_fd[i] = -1;
    clients_info[i].fd = -1;
    clients_info[i].last_activity = 0;
    clients_info[i].ping_sent = false;
  }

  // Init callbacks
  for (int i = 0; i < MAX_CALLBACKS; ++i)
  {
    receive_callbacks[i].callback = NULL;
  }

  // Start ping timer
  esp_timer_create_args_t timer_args = {
      .callback = ping_timer_callback,
      .arg = NULL,
      .name = "ws_ping_timer"};

  esp_err_t ret = esp_timer_create(&timer_args, &ping_timer);
  if (ret == ESP_OK)
  {
    esp_timer_start_periodic(ping_timer, PING_CHECK_INTERVAL_MS * 1000); // Convert to microseconds
    ESP_LOGI(TAG, "Ping timer started");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to create ping timer: %s", esp_err_to_name(ret));
  }

  // URI handler for websockets to server
  static const httpd_uri_t ws = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = receive_ws_message,
      .user_ctx = NULL,
      .is_websocket = true};
  httpd_register_uri_handler(server, &ws);

  register_callback("ping", ws_handle_ping_message);
  register_callback("pong", ws_handle_pong_message);
}

void stop_websocket(void)
{
  ESP_LOGI(TAG, "Stop websocket");

  // Stop and delete ping timer
  if (ping_timer)
  {
    esp_timer_stop(ping_timer);
    esp_timer_delete(ping_timer);
    ping_timer = NULL;
  }

  // Close all client connections
  for (int i = 0; i < MAX_CLIENTS; ++i)
  {
    if (clients_fd[i] != -1)
    {
      close(clients_fd[i]);
      clients_fd[i] = -1;
      clients_info[i].fd = -1;
      clients_info[i].last_activity = 0;
      clients_info[i].ping_sent = false;
    }
  }

  server = NULL;
}