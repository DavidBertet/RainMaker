// Copyright (c) 2025 David Bertet. Licensed under the MIT License.

#include "webfile.h"

#include "freertos/task.h"
#include <sys/unistd.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "esp_ota_ops.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_random.h"

#include "utils.h"
#include "websocket.h"
#include "webserver.h"
#include "spiffs.h"
#include "constants.h"

static const char *TAG = "webfile";

#define OTA_PASSWORD_HEADER "X-OTA-Password"
#define MAX_PASSWORD_LEN 64

#define SCRATCH_BUFSIZE 8192
// Max length a file path can have on storage
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
// Max size of an individual file. Make sure this value is consistent with webpage
#define MAX_FILE_SIZE (400 * 1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

// Temporary file suffix for atomic operations
#define TEMP_FILE_SUFFIX ".tmp"

// Progress update threshold (avoid spamming)
#define PROGRESS_UPDATE_THRESHOLD 4096

static esp_ota_handle_t ota_handle;

// Buffer for temporary storage during file transfer
static char scratch_buffer[SCRATCH_BUFSIZE];

#define IS_FILE_EXTENSION(filename, ext) \
  (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

// Set HTTP content type from file extension
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
  if (IS_FILE_EXTENSION(filename, ".js.gz"))
  {
    return httpd_resp_set_type(req, "text/javascript");
  }
  else if (IS_FILE_EXTENSION(filename, ".css.gz"))
  {
    return httpd_resp_set_type(req, "text/css");
  }
  else if (IS_FILE_EXTENSION(filename, ".html.gz"))
  {
    return httpd_resp_set_type(req, "text/html");
  }
  else if (IS_FILE_EXTENSION(filename, ".pdf.gz"))
  {
    return httpd_resp_set_type(req, "application/pdf");
  }
  else if (IS_FILE_EXTENSION(filename, ".jpeg.gz"))
  {
    return httpd_resp_set_type(req, "image/jpeg");
  }
  else if (IS_FILE_EXTENSION(filename, ".ico.gz"))
  {
    return httpd_resp_set_type(req, "image/x-icon");
  }
  // For any other type always set as plain text
  return httpd_resp_set_type(req, "text/plain");
}

// Redirect to /
static esp_err_t redirect_root(httpd_req_t *req)
{
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);

  return ESP_OK;
}

// Copies the full path into destination buffer and returns
// pointer to path (skipping the preceding base path)
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest)
  {
    pathlen = min(pathlen, quest - uri);
  }
  const char *hash = strchr(uri, '#');
  if (hash)
  {
    pathlen = min(pathlen, hash - uri);
  }

  if (base_pathlen + pathlen + 1 > destsize)
  {
    // Full path string won't fit into destination buffer
    return NULL;
  }

  // Construct full path (base + path)
  strcpy(dest, base_path);
  strlcpy(dest + base_pathlen, uri, pathlen + 1);

  // Return pointer to path, skipping the base
  return dest + base_pathlen;
}

// Delayed restart by 1s
static void restart_task(void *pvParameter)
{
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}

static void send_upload_progress(int loaded, int total, char *token)
{
  char message[128];
  int ret = snprintf(message, sizeof(message),
                     "{\"type\":\"upload_progress\",\"loaded\":\"%d\",\"total\":\"%d\"}",
                     loaded, total);

  if (ret > 0 && ret < sizeof(message))
  {
    ESP_LOGI(TAG, "%s", message);
    send_message_token(message, token);
  }
  else
  {
    ESP_LOGE(TAG, "Failed to format upload progress message");
  }
}

// This is used for the etag
static uint32_t hash_filename(const char *filename)
{
  uint32_t hash = 5381; // Prime number
  int c;

  while ((c = *filename++))
  {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

// Generate temporary file path by appending .tmp suffix
static esp_err_t generate_temp_filepath(char *temp_filepath, const char *original_filepath, size_t max_len)
{
  size_t original_len = strlen(original_filepath);
  size_t suffix_len = strlen(TEMP_FILE_SUFFIX);

  if (original_len + suffix_len + 1 > max_len)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  strcpy(temp_filepath, original_filepath);
  strcat(temp_filepath, TEMP_FILE_SUFFIX);

  return ESP_OK;
}

// Atomically replace original file with temporary file
static esp_err_t atomic_file_replace(const char *temp_filepath, const char *original_filepath)
{
  // First, try to remove the original file if it exists
  // It's okay if this fails as file might not exist
  unlink(original_filepath);

  // Rename temporary file to final name
  if (rename(temp_filepath, original_filepath) != 0)
  {
    ESP_LOGE(TAG, "Failed to rename %s to %s", temp_filepath, original_filepath);
    // Clean up temporary file
    unlink(temp_filepath);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Successfully replaced %s", original_filepath);
  return ESP_OK;
}

// Clean up temporary file in case of error
static void cleanup_temp_file(const char *temp_filepath)
{
  if (temp_filepath && strlen(temp_filepath) > 0)
  {
    unlink(temp_filepath);
    ESP_LOGI(TAG, "Cleaned up temporary file: %s", temp_filepath);
  }
}

// Handler to download a file from the server
static esp_err_t download_get_handler(httpd_req_t *req)
{
  int sockfd = httpd_req_to_sockfd(req);

  ESP_LOGI(TAG, "Request received for %s", req->uri);

  char filepath[FILE_PATH_MAX];
  FILE *fd = NULL;
  struct stat file_stat;
  esp_err_t ret = ESP_OK;

  const char *filename = get_path_from_uri(filepath, SPIFFS_BASE_PATH, req->uri, sizeof(filepath));

  if (!filename)
  {
    ESP_LOGE(TAG, "Filename is too long");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  if (strcmp(filename, "/") == 0 || strcmp(filename, "/hotspot-detect.html") == 0)
  {
    strcpy(filepath, "/spiffs/index.html.gz");
    filename = "/index.html.gz";
  }
  else
  {
    // Safe concatenation with bounds checking
    size_t current_len = strlen(filepath);
    size_t gz_len = strlen(".gz");

    if (current_len + gz_len + 1 > sizeof(filepath))
    {
      ESP_LOGE(TAG, "Filepath too long to append .gz suffix: %s", filepath);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filepath too long");
      return ESP_FAIL;
    }

    // Always serve gzip version
    strcat(filepath, ".gz");
  }

  if (stat(filepath, &file_stat) == -1)
  {
    ESP_LOGE(TAG, "Failed to stat file: %s", filepath);
    return redirect_root(req);
  }

  fd = fopen(filepath, "r");
  if (!fd)
  {
    ESP_LOGE(TAG, "Failed to read existing file: %s", filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
    return ESP_FAIL;
  }

  // Add session token cookie if not present
  char session_token[64];
  if (get_session_from_cookies(req, session_token, sizeof(session_token)) != ESP_OK)
  {
    // Generate unique session token
    uint32_t random1 = esp_random();
    uint32_t random2 = esp_random();
    char session_token[32];
    snprintf(session_token, sizeof(session_token), "%08lx%08lx", random1, random2);

    // Set session cookie
    char cookie_header[64];
    snprintf(cookie_header, sizeof(cookie_header), "session_id=%s; Path=/; HttpOnly", session_token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie_header);
  }

  // Use etag to determine if the file has changed
  char etag[32];
  char if_none_match[32];
  snprintf(etag, sizeof(etag), "\"%08x\"", (unsigned int)(file_stat.st_size ^ hash_filename(filename)));

  if ((httpd_req_get_hdr_value_str(req, "If-None-Match", if_none_match, sizeof(if_none_match)) == ESP_OK) &&
      strcmp(if_none_match, etag) == 0)
  {
    // File hasn't changed, send 304 Not Modified
    httpd_resp_set_status(req, "304 Not Modified");
    httpd_resp_set_hdr(req, "ETag", etag);
    httpd_resp_send(req, NULL, 0);
    fclose(fd);
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Sending file: %s (%ld bytes)...", filename, file_stat.st_size);
  set_content_type_from_file(req, filename);

  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "ETag", etag);
  httpd_resp_set_hdr(req, "Cache-Control", "private, must-revalidate");

  size_t chunksize;
  do
  {
    // Read file in chunks into the scratch buffer
    chunksize = fread(scratch_buffer, 1, SCRATCH_BUFSIZE, fd);

    if (chunksize > 0)
    {
      // Send the buffer contents as HTTP response chunk
      if (httpd_resp_send_chunk(req, scratch_buffer, chunksize) != ESP_OK)
      {
        ESP_LOGE(TAG, "File sending failed!");
        ret = ESP_FAIL;
        break; // Break instead of multiple returns
      }
    }

    // Keep looping till the whole file is sent
  } while (chunksize != 0);

  // Always close file
  fclose(fd);

  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "File sending complete");
    // Respond with an empty chunk to signal HTTP response completion
    httpd_resp_send_chunk(req, NULL, 0);
  }
  else
  {
    // Abort sending file
    httpd_resp_sendstr_chunk(req, NULL);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
  }

  return ret;
}

// Handler to upload a new binary onto the chip
static esp_err_t upload_ota_handler(httpd_req_t *req, const char *session_token)
{
  int received;
  bool ota_started = false;
  int last_progress_update = 0;

  esp_err_t ret = esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &ota_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(ret));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to begin OTA");
    return ESP_FAIL;
  }
  ota_started = true;

  // Content length of the request gives the size of the file being uploaded
  int remaining = req->content_len;

  while (remaining > 0)
  {
    // Throttle progress updates to avoid spam
    int current_progress = req->content_len - remaining;
    if (current_progress - last_progress_update >= PROGRESS_UPDATE_THRESHOLD || remaining == req->content_len)
    {
      send_upload_progress(current_progress, req->content_len, session_token);
      last_progress_update = current_progress;
    }

    // Receive the file part by part into a buffer
    if ((received = httpd_req_recv(req, scratch_buffer, min(remaining, SCRATCH_BUFSIZE))) <= 0)
    {
      if (received == HTTPD_SOCK_ERR_TIMEOUT)
      {
        // Retry if timeout occurred
        continue;
      }

      // In case of unrecoverable error, clean up
      if (ota_started)
      {
        esp_ota_abort(ota_handle);
      }

      ESP_LOGE(TAG, "File reception failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
      return ESP_FAIL;
    }

    // Write buffer content to OTA partition
    if (received && esp_ota_write(ota_handle, scratch_buffer, received) != ESP_OK)
    {
      // Couldn't write everything to OTA partition!
      esp_ota_abort(ota_handle);

      ESP_LOGE(TAG, "OTA write failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to OTA");
      return ESP_FAIL;
    }

    // Keep track of remaining size of the file left to be uploaded
    remaining -= received;
  }

  send_upload_progress(req->content_len, req->content_len, session_token);

  // End OTA upon upload completion
  ret = esp_ota_end(ota_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(ret));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to finalize OTA");
    return ESP_FAIL;
  }

  // Set new boot partition
  ret = esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Set new boot partition failed: %s", esp_err_to_name(ret));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set new boot partition");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "OTA update complete");

  // Respond with success message
  httpd_resp_sendstr(req, "Firmware uploaded successfully. Restarting now!");

  xTaskCreate(&restart_task, "restart_task", 2048, NULL, 10, NULL);
  return ESP_OK;
}

// Handler to upload a file onto the filesystem with atomic replacement
static esp_err_t upload_file_handler(httpd_req_t *req, const char *session_token, const char *filepath, const char *filename)
{
  FILE *fd = NULL;
  char temp_filepath[FILE_PATH_MAX];
  esp_err_t ret = ESP_OK;
  int last_progress_update = 0;

  // File cannot be larger than a limit
  if (req->content_len > MAX_FILE_SIZE)
  {
    ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " MAX_FILE_SIZE_STR "!");
    return ESP_FAIL;
  }

  // Generate temporary file path
  if (generate_temp_filepath(temp_filepath, filepath, sizeof(temp_filepath)) != ESP_OK)
  {
    ESP_LOGE(TAG, "Temporary filepath too long for : %s", filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Temporary filepath too long");
    return ESP_FAIL;
  }

  // Create temporary file for atomic write
  fd = fopen(temp_filepath, "w");
  if (!fd)
  {
    ESP_LOGE(TAG, "Failed to create temporary file : %s", temp_filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create temporary file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Receiving file : %s to temporary location: %s", filename, temp_filepath);

  int received;
  int remaining = req->content_len;

  while (remaining > 0)
  {
    // Throttle progress updates
    int current_progress = req->content_len - remaining;
    if (current_progress - last_progress_update >= PROGRESS_UPDATE_THRESHOLD || remaining == req->content_len)
    {
      send_upload_progress(current_progress, req->content_len, session_token);
      last_progress_update = current_progress;
    }

    // Receive the file part by part into a buffer
    if ((received = httpd_req_recv(req, scratch_buffer, min(remaining, SCRATCH_BUFSIZE))) <= 0)
    {
      if (received == HTTPD_SOCK_ERR_TIMEOUT)
      {
        // Retry if timeout occurred
        continue;
      }

      // In case of unrecoverable error, close and delete the temporary file
      ret = ESP_FAIL;
      ESP_LOGE(TAG, "File reception failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
      break;
    }

    // Write buffer content to temporary file
    if (received && (received != fwrite(scratch_buffer, 1, received, fd)))
    {
      // Couldn't write everything to file!
      ret = ESP_FAIL;
      ESP_LOGE(TAG, "File write failed!");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
      break;
    }

    // Keep track of remaining size of the file left to be uploaded
    remaining -= received;
  }

  // Close the file
  fclose(fd);

  if (ret != ESP_OK)
  {
    cleanup_temp_file(temp_filepath);
    return ESP_FAIL;
  }

  send_upload_progress(req->content_len, req->content_len, session_token);

  // Now atomically replace the original file with the temporary file
  if (atomic_file_replace(temp_filepath, filepath) != ESP_OK)
  {
    cleanup_temp_file(temp_filepath);
    ESP_LOGE(TAG, "Failed to atomically replace file: %s", filepath);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to replace original file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "File reception and replacement complete: %s", filename);

  // Respond with success message
  httpd_resp_sendstr(req, "File uploaded successfully");
  return ESP_OK;
}

// Helper to check for passowrd
static bool check_password(httpd_req_t *req)
{
  // If no password is defined, allow access
  if (strlen(OTA_PASSWORD) == 0)
  {
    return true;
  }

  char received_password[MAX_PASSWORD_LEN];

  // Check for password in HTTP header
  if (httpd_req_get_hdr_value_str(req, OTA_PASSWORD_HEADER, received_password, sizeof(received_password)) == ESP_OK)
  {
    if (strcmp(received_password, OTA_PASSWORD) == 0)
    {
      ESP_LOGI(TAG, "OTA password verified via header");
      return true;
    }
  }

  ESP_LOGW(TAG, "Access denied: invalid or missing password");
  return false;
}

// Handler to upload something onto the server
static esp_err_t upload_post_handler(httpd_req_t *req)
{
  int sockfd = httpd_req_to_sockfd(req);

  char filepath[FILE_PATH_MAX];

  // Check password before allowing upload
  if (!check_password(req))
  {
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication required for OTA updates");
    return ESP_FAIL;
  }

  // Skip leading "/upload" from URI to get filename
  // Note sizeof() counts NULL termination hence the -1
  const char *filename = get_path_from_uri(filepath, SPIFFS_BASE_PATH, req->uri + sizeof("/upload") - 1, sizeof(filepath));
  if (!filename)
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  // Filename cannot have a trailing '/'
  if (filename[strlen(filename) - 1] == '/')
  {
    ESP_LOGE(TAG, "Invalid filename : %s", filename);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
    return ESP_FAIL;
  }

  char session_token[64] = {0};
  get_session_from_cookies(req, session_token, sizeof(session_token));

  if (IS_FILE_EXTENSION(filename, ".bin"))
  {
    return upload_ota_handler(req, session_token);
  }
  else
  {
    return upload_file_handler(req, session_token, filepath, filename);
  }
}

// Clean up any leftover temporary files from interrupted uploads
static void cleanup_temp_files_on_startup(void)
{
  DIR *dir;
  struct dirent *entry;
  char filepath[FILE_PATH_MAX];

  dir = opendir(SPIFFS_BASE_PATH);
  if (dir == NULL)
  {
    ESP_LOGE(TAG, "Failed to open SPIFFS directory for temp file cleanup");
    return;
  }

  ESP_LOGI(TAG, "Cleaning up temporary files from previous session...");
  int cleanup_count = 0;

  while ((entry = readdir(dir)) != NULL)
  {
    // Check if file ends with .tmp suffix
    size_t name_len = strlen(entry->d_name);
    size_t suffix_len = strlen(TEMP_FILE_SUFFIX);

    if (name_len > suffix_len &&
        strcmp(entry->d_name + name_len - suffix_len, TEMP_FILE_SUFFIX) == 0)
    {

      // Check if the full path would fit in our buffer
      size_t base_len = strlen(SPIFFS_BASE_PATH);
      if (base_len + 1 + name_len + 1 > sizeof(filepath))
      {
        ESP_LOGW(TAG, "Temporary filename too long to process: %s", entry->d_name);
        continue;
      }

      // Construct full path safely
      int ret = snprintf(filepath, sizeof(filepath), "%s/%s", SPIFFS_BASE_PATH, entry->d_name);
      if (ret >= sizeof(filepath))
      {
        ESP_LOGW(TAG, "Path truncated for temporary file: %s", entry->d_name);
        continue;
      }

      // Remove the temporary file
      if (unlink(filepath) == 0)
      {
        ESP_LOGI(TAG, "Removed leftover temporary file: %s", entry->d_name);
        cleanup_count++;
      }
      else
      {
        ESP_LOGW(TAG, "Failed to remove temporary file: %s", entry->d_name);
      }
    }
  }

  closedir(dir);

  if (cleanup_count > 0)
  {
    ESP_LOGI(TAG, "Cleaned up %d temporary files", cleanup_count);
  }
  else
  {
    ESP_LOGI(TAG, "No temporary files found to clean up");
  }
}

void start_web_file(httpd_handle_t server)
{
  ESP_LOGI(TAG, "Start web file");

  // Clean up any leftover temporary files from interrupted uploads
  cleanup_temp_files_on_startup();

  // URI handler for accessing files from server
  httpd_uri_t file_download = {
      .uri = "/*", // Match all URIs of type /path/to/file
      .method = HTTP_GET,
      .handler = download_get_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(server, &file_download);

  // URI handler for uploading files to server
  httpd_uri_t file_upload = {
      .uri = "/upload/*", // Match all URIs of type /upload/path/to/file
      .method = HTTP_POST,
      .handler = upload_post_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(server, &file_upload);
}