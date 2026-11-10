#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "http_server.h"
#include "uart_read.h"
#include "wifi_config.h"
#include <stdlib.h>
#include <string.h>

#define HTTP_TAG "HTTP"
#define MAX_COORDINATES 100

static float latitude[MAX_COORDINATES];
static float longitude[MAX_COORDINATES];
static float speed[MAX_COORDINATES];
static int coord_count = 0;

// Identifiant unique réinitialisé à chaque démarrage
static int session_id = 0;

void update_gps_data(float new_lat, float new_lon, float new_speed) {
    if (coord_count >= MAX_COORDINATES) {
        for (int i = 1; i < MAX_COORDINATES; i++) {
            latitude[i - 1] = latitude[i];
            longitude[i - 1] = longitude[i];
            speed[i - 1] = speed[i];
        }
        coord_count--;
    }
    latitude[coord_count] = new_lat;
    longitude[coord_count] = new_lon;
    speed[coord_count] = new_speed;
    coord_count++;
}

esp_err_t cors_options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

// GET: Renvoie le session_id et le tableau des coordonnées au format JSON
esp_err_t gps_data_handler(httpd_req_t *req) {
    if (session_id == 0) {
        session_id = (rand() % 900000) + 100000;
    }
    
    int buffer_size = (coord_count > 0) ? (80 * coord_count + 64) : 128;
    char *json_response = (char *)malloc(buffer_size);
    if (json_response == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    char temp[80];
    snprintf(json_response, buffer_size, "{\"session_id\": %d, \"data\": [", session_id);
    
    for (int i = 0; i < coord_count; i++) {
        snprintf(temp, sizeof(temp), "{\"latitude\": %.6f, \"longitude\": %.6f, \"speed\": %.2f}", 
                 latitude[i], longitude[i], speed[i]);
        strcat(json_response, temp);
        if (i < coord_count - 1) strcat(json_response, ",");
    }
    strcat(json_response, "]}");
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    free(json_response);
    return ESP_OK;
}

esp_err_t gps_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    float new_lat, new_lon;
    if (sscanf(buf, "{\"latitude\": %f, \"longitude\": %f}", &new_lat, &new_lon) == 2) {
        update_gps_data(new_lat, new_lon, 0.0);
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

void wifi_init_sta() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(HTTP_TAG, "Initializing Wi-Fi...");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(HTTP_TAG, "Connecting to Wi-Fi...");
    while (esp_wifi_connect() != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "Failed to connect, retrying...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI(HTTP_TAG, "Connected to Wi-Fi!");

    // Attente de l'attribution de l'IP via DHCP et affichage dans les logs
    esp_netif_ip_info_t ip_info;
    int retry_count = 0;
    while (retry_count < 10) {
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            break;
        }
        ESP_LOGW(HTTP_TAG, "Waiting for IP address... (%d)", retry_count + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }

    if (ip_info.ip.addr == 0) {
        ESP_LOGE(HTTP_TAG, "Failed to get IP address!");
    } else {
        ESP_LOGI(HTTP_TAG, "========================================");
        ESP_LOGI(HTTP_TAG, "  ESP32 IP Address : " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(HTTP_TAG, "========================================");
    }
}

void check_wifi_connection(void *pvParameters) {
    while (1) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGW(HTTP_TAG, "WiFi disconnected! Reconnecting...");
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void start_wifi_monitor() {
    xTaskCreate(check_wifi_connection, "wifi_monitor", 3072, NULL, 5, NULL);
}

static bool http_server_started = false;

void start_http_server(void *pvParameters) {
    if (http_server_started) {
        ESP_LOGW(HTTP_TAG, "Server already running, skipping...");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(HTTP_TAG, "Starting HTTP server...");
    http_server_started = true;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t options_uri = {
            .uri = "/gps", .method = HTTP_OPTIONS, .handler = cors_options_handler, .user_ctx = NULL
        };
        httpd_uri_t gps_data_uri = {
            .uri = "/gps", .method = HTTP_GET, .handler = gps_data_handler, .user_ctx = NULL
        };
        httpd_uri_t gps_post_uri = {
            .uri = "/gps", .method = HTTP_POST, .handler = gps_post_handler, .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &options_uri);
        httpd_register_uri_handler(server, &gps_data_uri);
        httpd_register_uri_handler(server, &gps_post_uri);
    }
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}