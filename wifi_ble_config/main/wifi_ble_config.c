#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_http_client.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define BUTTON_GPIO GPIO_NUM_0       // Button connected to GPIO0
#define BUTTON_HOLD_TIME 3000        // Time in milliseconds to hold the button
#define BLINK_GPIO GPIO_NUM_2        // LED connected to GPIO2

#define HTTP_URL "http://example.com"
#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x55
#define SAMPLE_DEVICE_NAME "ESP32_I2C_Config"
#define SVC_INST_ID 0

static const char *TAG = "WiFi_BLE_Config";

static bool isConnected = false;
static bool isConfigMode = false;
static bool credentials_received = false;
static bool ble_server_stopping = false;
static bool ble_connected = false;
static bool ble_initialized = false;

static uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

static const uint16_t wifi_service_uuid = 0x00FF;
static const uint16_t wifi_ssid_uuid = 0xFF01;
static const uint16_t wifi_pass_uuid = 0xFF02;
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;

// Wi-Fi credentials structure
typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

wifi_credentials_t wifi_credentials = {0};

// Function prototypes
void start_ble_server(void);
void stop_ble_server(void);
void wifi_init(void);
void save_wifi_credentials(const wifi_credentials_t *credentials);
void load_wifi_credentials(wifi_credentials_t *credentials);
void check_button_press(void *arg);
void blink_led_task(void *pvParameter);
void fetch_webpage_task(void *pvParameter);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void wifi_status_logger_task(void *pvParameter);

// BLE variables and functions
static uint8_t adv_config_done = 0;

#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// Advertising Service UUID (128-bit)
static uint8_t adv_service_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // First UUID, 16-bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 
    (wifi_service_uuid & 0xFF), 
    ((wifi_service_uuid >> 8) & 0xFF), 
    0x00, 0x00,
};

// Advertising data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,  // slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010,  // slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = 0,       // No manufacturer data
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(adv_service_uuid128),
    .p_service_uuid      = adv_service_uuid128,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Handle table to keep track of attribute handles
enum {
    IDX_SVC,
    IDX_CHAR_SSID_DECL,
    IDX_CHAR_SSID_VAL,
    IDX_CHAR_PASS_DECL,
    IDX_CHAR_PASS_VAL,
    HRS_IDX_NB,
};

static uint16_t handle_table[HRS_IDX_NB];

// GATT Attribute Database
static esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] = {
    // Service Declaration
    [IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(uint16_t), sizeof(wifi_service_uuid), (uint8_t *)&wifi_service_uuid
        }
    },

    // Wi-Fi SSID Characteristic Declaration
    [IDX_CHAR_SSID_DECL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
            1, 1, (uint8_t *)&char_prop_read_write
        }
    },

    // Wi-Fi SSID Characteristic Value
    [IDX_CHAR_SSID_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, (uint8_t *)&wifi_ssid_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(wifi_credentials.ssid), 0, NULL
        }
    },

    // Wi-Fi Password Characteristic Declaration
    [IDX_CHAR_PASS_DECL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
            1, 1, (uint8_t *)&char_prop_read_write
        }
    },

    // Wi-Fi Password Characteristic Value
    [IDX_CHAR_PASS_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {
            ESP_UUID_LEN_16, (uint8_t *)&wifi_pass_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(wifi_credentials.password), 0, NULL
        }
    },
};

// GATT Profile Instance Structure
typedef struct {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
} gatts_profile_inst_t;

// Initialize GATT profile table
static gatts_profile_inst_t gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

// GAP Event Handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch(event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~adv_config_flag);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if(param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE(TAG, "Advertising start failed");
            } else {
                ESP_LOGI(TAG, "Advertising started successfully");
            }
            break;
        default:
            break;
    }
}

// GATT Profile Event Handler
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "ESP_GATTS_REG_EVT");
            gl_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;  // Store gatts_if
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
            if (set_dev_name_ret) {
                ESP_LOGE(TAG, "Set device name failed, error code = %x", set_dev_name_ret);
            }

            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret) {
                ESP_LOGE(TAG, "Config adv data failed, error code = %x", ret);
            }
            adv_config_done |= adv_config_flag;

            // Create attribute table
            esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            break;
        }

        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            } else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
                    param->add_attr_tab.num_handle, HRS_IDX_NB);
            } else {
                memcpy(handle_table, param->add_attr_tab.handles, sizeof(handle_table));
                esp_ble_gatts_start_service(handle_table[IDX_SVC]);
            }
            break;
        }

        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, handle = %d", param->write.handle);
            ESP_LOGI(TAG, "IDX_CHAR_SSID_VAL handle = %d", handle_table[IDX_CHAR_SSID_VAL]);
            ESP_LOGI(TAG, "IDX_CHAR_PASS_VAL handle = %d", handle_table[IDX_CHAR_PASS_VAL]);

            if (!param->write.is_prep){
                esp_gatt_status_t status = ESP_GATT_OK;
                if (param->write.handle == handle_table[IDX_CHAR_SSID_VAL]) {  // SSID
                    // Save SSID
                    memset(wifi_credentials.ssid, 0, sizeof(wifi_credentials.ssid));
                    memcpy(wifi_credentials.ssid, param->write.value, param->write.len);
                    if (param->write.len < sizeof(wifi_credentials.ssid)) {
                        wifi_credentials.ssid[param->write.len] = '\0'; // Null-terminate the string
                    } else {
                        wifi_credentials.ssid[sizeof(wifi_credentials.ssid) - 1] = '\0';
                    }
                    ESP_LOGI(TAG, "Received SSID: %s", wifi_credentials.ssid);
                } else if (param->write.handle == handle_table[IDX_CHAR_PASS_VAL]) {  // Password
                    // Save Password
                    memset(wifi_credentials.password, 0, sizeof(wifi_credentials.password));
                    memcpy(wifi_credentials.password, param->write.value, param->write.len);
                    if (param->write.len < sizeof(wifi_credentials.password)) {
                        wifi_credentials.password[param->write.len] = '\0'; // Null-terminate the string
                    } else {
                        wifi_credentials.password[sizeof(wifi_credentials.password) - 1] = '\0';
                    }
                    ESP_LOGI(TAG, "Received Password: %s", wifi_credentials.password);

                    // Set the flag to indicate credentials are received
                    credentials_received = true;
                }
                // Send response
                if (param->write.need_rsp) {
                    esp_gatt_rsp_t rsp;
                    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                    rsp.attr_value.handle = param->write.handle;
                    rsp.attr_value.len = param->write.len;
                    rsp.attr_value.offset = param->write.offset;
                    rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                    memcpy(rsp.attr_value.value, param->write.value, param->write.len);
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                                status, &rsp);
                }
            }
            break;
        }

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            gl_profile_tab[PROFILE_APP_IDX].conn_id = param->connect.conn_id;
            ble_connected = true; // Update connection status
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = %d", param->disconnect.reason);
            ble_connected = false; // Update connection status
            if (!ble_server_stopping) {
                // Start advertising again
                esp_ble_gap_start_advertising(&adv_params);
            } else {
                ESP_LOGI(TAG, "BLE server is stopping, not restarting advertising");
            }
            break;
        default:
            break;
    }
}

// Credentials Handling Task
void credentials_task(void *pvParameter) {
    while (true) {
        if (credentials_received) {
            credentials_received = false;  // Reset the flag

            ESP_LOGI(TAG, "Credentials received, saving and initializing Wi-Fi");

            // Save credentials
            save_wifi_credentials(&wifi_credentials);

            // Stop BLE server
            stop_ble_server();
            ESP_LOGI(TAG, "Configuration successfully received, stopping BLE server");
            isConfigMode = false;

            // Initialize Wi-Fi
            wifi_init();

            // Restart the credentials task to handle future credential updates
            vTaskDelete(NULL);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Button Press Handling Task
void check_button_press(void *arg) {
    while (1) {
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            // Button is pressed
            vTaskDelay(BUTTON_HOLD_TIME / portTICK_PERIOD_MS);

            if (gpio_get_level(BUTTON_GPIO) == 0) {
                if(!isConfigMode){
                    ESP_LOGI(TAG, "Entering configuration mode...");
                    isConfigMode = true;
                    start_ble_server();
                } else {
                    ESP_LOGI(TAG, "Exiting configuration mode...");
                    isConfigMode = false;
                    stop_ble_server();
                }
                // Wait until button is released
                while (gpio_get_level(BUTTON_GPIO) == 0) {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Start BLE Server
void start_ble_server(void) {
    // Initialize BLE
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller memory release failed: %s", esp_err_to_name(ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller initialization failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid initialization failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register GAP and GATT callbacks
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_profile_event_handler);
    esp_ble_gatts_app_register(ESP_APP_ID);
    
    ble_initialized = true;
}

// Stop BLE Server
void stop_ble_server(void) {
    ble_server_stopping = true; // Set flag to indicate BLE server is stopping
    esp_err_t ret;

    // Stop advertising
    ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
    }

    // Close GATT server connection if connected
    if (ble_connected) {
        ret = esp_ble_gatts_close(gl_profile_tab[PROFILE_APP_IDX].gatts_if, gl_profile_tab[PROFILE_APP_IDX].conn_id);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close GATT server connection: %s", esp_err_to_name(ret));
        }
        ble_connected = false;
    } else {
        ESP_LOGI(TAG, "No active BLE connection to close.");
    }

    // Unregister GATT application
    ret = esp_ble_gatts_app_unregister(gl_profile_tab[PROFILE_APP_IDX].gatts_if);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister GATT application: %s", esp_err_to_name(ret));
    }

    // Disable and deinitialize Bluedroid
    ret = esp_bluedroid_disable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable Bluedroid: %s", esp_err_to_name(ret));
    }

    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinit Bluedroid: %s", esp_err_to_name(ret));
    }

    // Do NOT disable or deinitialize the BT controller here, since Wi-Fi needs it.
    
    ble_initialized = false;
    ESP_LOGI(TAG, "BLE server stopped successfully.");
}

// Save Wi-Fi Credentials to NVS
void save_wifi_credentials(const wifi_credentials_t *credentials) {
    ESP_LOGI(TAG, "Saving Wi-Fi credentials: SSID: %s, Password: %s", credentials->ssid, credentials->password);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, "ssid", credentials->ssid);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set SSID in NVS: %s", esp_err_to_name(err));
        }
        err = nvs_set_str(nvs_handle, "password", credentials->password);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set Password in NVS: %s", esp_err_to_name(err));
        }
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Wi-Fi credentials saved to NVS.");
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
    }
}

// Load Wi-Fi Credentials from NVS
void load_wifi_credentials(wifi_credentials_t *credentials) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_creds", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t ssid_size = sizeof(credentials->ssid);
        size_t pass_size = sizeof(credentials->password);
        err = nvs_get_str(nvs_handle, "ssid", credentials->ssid, &ssid_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to get SSID from NVS: %s", esp_err_to_name(err));
            memset(credentials->ssid, 0, sizeof(credentials->ssid));
        }
        err = nvs_get_str(nvs_handle, "password", credentials->password, &pass_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to get Password from NVS: %s", esp_err_to_name(err));
            memset(credentials->password, 0, sizeof(credentials->password));
        }
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Wi-Fi credentials loaded from NVS. SSID: %s, Password: %s", credentials->ssid, credentials->password);
    } else {
        ESP_LOGE(TAG, "No Wi-Fi credentials found in NVS.");
    }
}

// Initialize Wi-Fi Driver and Connect
void wifi_init(void) {
    // Initialize Wi-Fi driver with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register Wi-Fi and IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        return;
    }

    // Configure Wi-Fi with SSID and password
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    // Copy SSID and password from loaded credentials
    strncpy((char*)wifi_config.sta.ssid, wifi_credentials.ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_credentials.password, sizeof(wifi_config.sta.password));

    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi initialized with SSID: %s", wifi_credentials.ssid);
}

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            isConnected = false;
            ESP_LOGI(TAG, "Wi-Fi disconnected, attempting to reconnect...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        isConnected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xTaskCreate(&fetch_webpage_task, "fetch_webpage_task", 4096, NULL, 5, NULL);
    }
}

// Wi-Fi Status Logger Task
void wifi_status_logger_task(void *pvParameter) {
    while (1) {
        if (isConnected) {
            ESP_LOGI(TAG, "Wi-Fi Status: CONNECTED");
        } else {
            ESP_LOGI(TAG, "Wi-Fi Status: DISCONNECTED");
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);  // Wait for 3 seconds
    }
}

// LED Blinking Task
void blink_led_task(void *pvParameter) {
    esp_rom_gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (true) {
        if (isConnected && !isConfigMode) {
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else if (isConfigMode) {
            // Blink rapidly when in configuration mode
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        } else if(ble_initialized){
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else {
            // Blink slowly when disconnected
            gpio_set_level(BLINK_GPIO, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

// HTTP Event Handler
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "Connected to server");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP headers sent");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "Received data: %.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP request finished");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from server");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled HTTP event: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

// Fetch Webpage Task
void fetch_webpage_task(void *pvParameter) {
    esp_http_client_config_t config = {
        .url = HTTP_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 20000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    vTaskDelete(NULL);
}

// Main Application
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    // Handle NVS errors
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS Flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default Wi-Fi station
    esp_netif_create_default_wifi_sta();

    // Load Wi-Fi credentials from NVS
    load_wifi_credentials(&wifi_credentials);

    // Initialize button GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    // Initialize Wi-Fi
    if (strlen(wifi_credentials.ssid) > 0) {
        ESP_LOGI(TAG, "Wi-Fi credentials found. Initializing Wi-Fi...");
        wifi_init();
    } else {
        ESP_LOGI(TAG, "No Wi-Fi credentials found. Entering configuration mode.");
        isConfigMode = true;
        start_ble_server();
    }

    // Start FreeRTOS tasks
    xTaskCreate(&check_button_press, "check_button_press", 2048, NULL, 5, NULL);
    xTaskCreate(&blink_led_task, "blink_led_task", 1024, NULL, 5, NULL);
    xTaskCreate(&credentials_task, "credentials_task", 4096, NULL, 5, NULL);
    xTaskCreate(&wifi_status_logger_task, "wifi_status_logger_task", 2048, NULL, 5, NULL);
}
