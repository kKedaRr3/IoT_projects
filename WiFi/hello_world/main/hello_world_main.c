#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_http_client.h"

#define WIFI_SSID "Galaxy_A52s_5G"
#define WIFI_PASS "Radekkozak"

#define BLINK_GPIO GPIO_NUM_2

#define HTTP_URL "http://example.com" 

static const char *TAG = "WiFi_STA";

static bool isConnected = false;

void fetch_webpage_task(void *pvParameter);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            isConnected = false;  
            ESP_LOGI(TAG, "Połączenie WiFi utracone, ponawianie próby...");
            ESP_LOGI(TAG, "isConnected: %d ", isConnected);
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        isConnected = true;
        ESP_LOGI(TAG, "isConnected: %d ", isConnected);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Uzyskano IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xTaskCreate(&fetch_webpage_task, "fetch_webpage_task", 4096, NULL, 5, NULL);
        
    }
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));


    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi w trybie stacji zainicjalizowane.");
}

void blink_led_task(void *pvParameter) {
    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (true) {
        if (isConnected) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 1);  
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            gpio_set_level(BLINK_GPIO, 1);
            if(isConnected){
                continue;
            }  
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            if(isConnected){
                continue;
            }  
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "Połączono z serwerem");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "Nagłówki zostały wysłane");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "Otrzymano dane: %.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Zakończono żądanie HTTP");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Rozłączono z serwerem");
            break;
        default:
            ESP_LOGW(TAG, "Nieobsługiwane zdarzenie HTTP: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}
void fetch_webpage_task(void *pvParameter) {
    while (!isConnected) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }

    esp_http_client_config_t config = {
        .url = HTTP_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 20000, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status Code: %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Błąd pobierania strony: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    vTaskDelete(NULL); 
}

void app_main(void) {

    esp_log_level_set("task_wdt", ESP_LOG_NONE);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    xTaskCreate(&blink_led_task, "blink_led_task", 1024, NULL, 5, NULL);

}
