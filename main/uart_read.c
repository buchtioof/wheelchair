#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "http_server.h"
#include "uart_read.h"

static const char *UART_TAG = "UART";

int parse_gpgga(const char *nmea_sentence, float *latitude, float *longitude) {
    char buffer[128];
    strncpy(buffer, nmea_sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char *token = strtok(buffer, ",");
    int field_index = 0;

    float lat = 0.0, lon = 0.0;
    char lat_dir = 'N', lon_dir = 'E';
    int fix_quality = 0;

    while (token != NULL) {
        field_index++;
        if (field_index == 3) lat = atof(token);
        else if (field_index == 4) lat_dir = token[0];
        else if (field_index == 5) lon = atof(token);
        else if (field_index == 6) lon_dir = token[0];
        else if (field_index == 7) fix_quality = atoi(token);
        
        token = strtok(NULL, ",");
    }

    if (fix_quality == 0 || lat == 0.0f || lon == 0.0f) {
        *latitude = 0.0f;
        *longitude = 0.0f;
        return 0; 
    }

    float parsed_lat = ((int)(lat / 100) + (lat - ((int)(lat / 100) * 100)) / 60.0) * (lat_dir == 'S' ? -1 : 1);
    float parsed_lon = ((int)(lon / 100) + (lon - ((int)(lon / 100) * 100)) / 60.0) * (lon_dir == 'W' ? -1 : 1);

    if (parsed_lat < -90.0f || parsed_lat > 90.0f || parsed_lon < -180.0f || parsed_lon > 180.0f) {
        return 0;
    }

    *latitude = parsed_lat;
    *longitude = parsed_lon;

    return 1;
}

int parse_gprmc(const char *nmea_sentence, float *latitude, float *longitude, float *speed_kmh) {
    char buffer[128];
    strncpy(buffer, nmea_sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char *token = strtok(buffer, ",");
    int field_index = 0;
    float lat = 0.0, lon = 0.0, speed_knots = 0.0;
    char lat_dir = 'N', lon_dir = 'E';
    char status = 'V';

    while (token != NULL) {
        field_index++;
        if (field_index == 3) status = token[0];               // 3ème champ : Statut ('A' = Valid, 'V' = Invalid)
        else if (field_index == 4) lat = atof(token);          // 4ème champ : Latitude
        else if (field_index == 5) lat_dir = token[0];         // 5ème champ : N/S
        else if (field_index == 6) lon = atof(token);          // 6ème champ : Longitude
        else if (field_index == 7) lon_dir = token[0];         // 7ème champ : E/W
        else if (field_index == 8) speed_knots = atof(token); // 8ème champ : Vitesse en nœuds
        
        token = strtok(NULL, ",");
    }

    if (status != 'A' || lat == 0.0f || lon == 0.0f) {
        *latitude = 0.0f;
        *longitude = 0.0f;
        *speed_kmh = 0.0f;
        return 0;
    }

    float parsed_lat = ((int)(lat / 100) + (lat - ((int)(lat / 100) * 100)) / 60.0) * (lat_dir == 'S' ? -1 : 1);
    float parsed_lon = ((int)(lon / 100) + (lon - ((int)(lon / 100) * 100)) / 60.0) * (lon_dir == 'W' ? -1 : 1);

    *latitude = parsed_lat;
    *longitude = parsed_lon;
    *speed_kmh = speed_knots * 1.852f; // Conversion nœuds -> km/h

    return 1;
}

void uart_task(void *arg) {
    static uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            
            // On cherche GPRMC (ou GNRMC selon le module)
            char *gprmc_start = strstr((char *)data, "$GPRMC");
            if (!gprmc_start) gprmc_start = strstr((char *)data, "$GNRMC");

            if (gprmc_start) {
                char *end = strchr(gprmc_start, '\n');
                if (end) {
                    *end = '\0';
                    float latitude = 0.0, longitude = 0.0, speed = 0.0;
                    
                    if (parse_gprmc(gprmc_start, &latitude, &longitude, &speed)) {
                        ESP_LOGI(UART_TAG, "GPS Fix OK - Lat: %.6f, Lon: %.6f, Vit: %.2f km/h", latitude, longitude, speed);
                        gpio_set_level(LED_GPIO, 1);
                        update_gps_data(latitude, longitude, speed);
                    } else {
                        ESP_LOGW(UART_TAG, "Pas de Fix GPS valide...");
                        gpio_set_level(LED_GPIO, 0);
                    }
                }
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void uart_task_test(void *arg) {
    ESP_LOGI(UART_TAG, "Starting test mode: Sending fake GPS coordinates...");
    while (1) {
        float latitude = 60.1699 + ((rand() % 1000) / 100000.0);
        float longitude = 24.9384 + ((rand() % 1000) / 100000.0);
        float speed = (rand() % 500) / 10.0; // Vitesse entre 0.0 et 50.0 km/h
        
        gpio_set_level(LED_GPIO, 1);
        ESP_LOGI(UART_TAG, "Simulated GPS Data - Lat: %.6f, Lon: %.6f, Vit: %.2f km/h", latitude, longitude, speed);
        update_gps_data(latitude, longitude, speed);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}