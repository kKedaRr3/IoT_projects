#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GATTS_TAG "SERVER_DEMO"

#define GATTS_SERVICE_UUID_TEST   0x00FF
#define GATTS_CHAR_UUID_TEST      0xFF01
#define GATTS_DESCR_UUID_TEST     0x2902  // UUID for Client Characteristic Configuration Descriptor
#define GATTS_NUM_HANDLE_TEST     4

#define TEST_DEVICE_NAME            "ESP32_GATT_SERVER"
#define TEST_MANUFACTURER_DATA_LEN  17

#define NVS_NAMESPACE "storage"
#define NVS_KEY       "stored_value"

static uint8_t char_value[128];  // Increased size to accommodate larger values
static uint8_t adv_config_done = 0;

uint16_t gatt_service_handle_table[GATTS_NUM_HANDLE_TEST];

static esp_gatt_char_prop_t char_property = 0;

static esp_attr_value_t gatts_char_val = {
    .attr_max_len = sizeof(char_value),
    .attr_len     = sizeof(char_value),
    .attr_value   = char_value,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 16,
    .p_service_uuid      = NULL,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Scan response data (optional)
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, 
                                        esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    uint16_t descr_handle;
    esp_bt_uuid_t char_uuid;
    bool notifications_enabled;
} gl_profile = {
    .gatts_cb = gatts_profile_event_handler,
    .gatts_if = ESP_GATT_IF_NONE,
};

static nvs_handle_t my_nvs_handle;

static void gap_event_handler(esp_gap_ble_cb_event_t event, 
                              esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~0x01);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~0x02);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // Advertising started
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Advertising started");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        // Advertising stopped
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(GATTS_TAG, "Advertising stopped");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "Connection parameters updated");
        break;
    default:
        break;
    }
}

// Function to read value from NVS
static esp_err_t read_value_from_nvs(char *value, size_t max_len, size_t *length)
{
    esp_err_t err;
    size_t required_size = 0;

    err = nvs_get_str(my_nvs_handle, NVS_KEY, NULL, &required_size);
    if (err == ESP_OK && required_size <= max_len) {
        err = nvs_get_str(my_nvs_handle, NVS_KEY, value, &required_size);
        if (err == ESP_OK) {
            *length = required_size - 1; // Exclude null terminator
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Key not found, initialize with default value
        strcpy(value, "Default Value");
        *length = strlen(value);
        err = nvs_set_str(my_nvs_handle, NVS_KEY, value);
        nvs_commit(my_nvs_handle);
    }
    return err;
}

// Function to write value to NVS
static esp_err_t write_value_to_nvs(const char *value)
{
    esp_err_t err;
    err = nvs_set_str(my_nvs_handle, NVS_KEY, value);
    if (err == ESP_OK) {
        err = nvs_commit(my_nvs_handle);
    }
    return err;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, 
                                        esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "Application registered, app_id %d", param->reg.app_id);
        gl_profile.gatts_if = gatts_if;
        gl_profile.service_id.is_primary = true;
        gl_profile.service_id.id.inst_id = 0x00;
        gl_profile.service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST;

        esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        adv_config_done |= 0x01;
        esp_ble_gap_config_adv_data(&scan_rsp_data);
        adv_config_done |= 0x02;

        esp_ble_gatts_create_service(gatts_if, &gl_profile.service_id, GATTS_NUM_HANDLE_TEST);
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "Service created, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile.service_handle = param->create.service_handle;
        gl_profile.char_uuid.len = ESP_UUID_LEN_16;
        gl_profile.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST;

        esp_ble_gatts_start_service(gl_profile.service_handle);

        char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

        esp_attr_control_t control = {
            .auto_rsp = ESP_GATT_RSP_BY_APP,
        };

        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile.service_handle, 
                                                        &gl_profile.char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        char_property,
                                                        &gatts_char_val,
                                                        &control);
        if (add_char_ret) {
            ESP_LOGE(GATTS_TAG, "Adding characteristic failed");
        }
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "Characteristic added, status %d, attr_handle %d", param->add_char.status, param->add_char.attr_handle);
        gl_profile.char_handle = param->add_char.attr_handle;

        // Add Descriptor for Notifications
        {
            esp_bt_uuid_t descr_uuid;
            descr_uuid.len = ESP_UUID_LEN_16;
            descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG; // 0x2902

            esp_attr_control_t control = {
                .auto_rsp = ESP_GATT_AUTO_RSP,  // Changed to AUTO_RSP
            };

            uint8_t descr_val[2] = {0x00, 0x00};
            esp_attr_value_t descr_val_attr = {
                .attr_max_len = sizeof(descr_val),
                .attr_len = sizeof(descr_val),
                .attr_value = descr_val,
            };

            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile.service_handle,
                                                                   &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                   &descr_val_attr,
                                                                   &control);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "Adding descriptor failed");
            }
        }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile.descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGI(GATTS_TAG, "Descriptor added, handle = %d", gl_profile.descr_handle);
        break;
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(GATTS_TAG, "Characteristic read, conn_id %d, trans_id %lu, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
        // Read value from NVS
        size_t length = 0;
        esp_err_t err = read_value_from_nvs((char *)char_value, sizeof(char_value), &length);
        if (err != ESP_OK) {
            ESP_LOGE(GATTS_TAG, "Failed to read from NVS: %s", esp_err_to_name(err));
        }

        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = length;
        memcpy(rsp.attr_value.value, char_value, length);
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "Write event: handle=%d, value len=%d", param->write.handle, param->write.len);
        ESP_LOG_BUFFER_CHAR(GATTS_TAG, param->write.value, param->write.len);
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                        ESP_GATT_OK, NULL);
        }
        // Check if write to CCCD
        if (param->write.handle == gl_profile.descr_handle && param->write.len == 2) {
            uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
            if (descr_value == 0x0001) {
                gl_profile.notifications_enabled = true;
                ESP_LOGI(GATTS_TAG, "Notifications enabled");
            } else if (descr_value == 0x0000) {
                gl_profile.notifications_enabled = false;
                ESP_LOGI(GATTS_TAG, "Notifications disabled");
            }
        } else if (param->write.handle == gl_profile.char_handle) {
            if (param->write.len < sizeof(char_value)) {
                // Update characteristic value
                memset(char_value, 0, sizeof(char_value));
                memcpy(char_value, param->write.value, param->write.len);
                gatts_char_val.attr_len = param->write.len;

                // Write value to NVS
                char temp_value[sizeof(char_value)];
                memcpy(temp_value, param->write.value, param->write.len);
                temp_value[param->write.len] = '\0'; // Null-terminate
                esp_err_t err = write_value_to_nvs(temp_value);
                if (err != ESP_OK) {
                    ESP_LOGE(GATTS_TAG, "Failed to write to NVS: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGI(GATTS_TAG, "Value saved to NVS");
                }
            } else {
                ESP_LOGE(GATTS_TAG, "Write value too long");
            }
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Device connected, conn_id %d", param->connect.conn_id);
        gl_profile.conn_id = param->connect.conn_id;
        gl_profile.notifications_enabled = false;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Device disconnected, restarting advertising");
        esp_ble_gap_start_advertising(&adv_params);
        gl_profile.notifications_enabled = false;
        break;
    default:
        break;
    }
}

void notification_task(void *pvParameter)
{
    while (1) {
        if (gl_profile.notifications_enabled) {
            // Read the latest value from NVS
            size_t length = 0;
            esp_err_t err = read_value_from_nvs((char *)char_value, sizeof(char_value), &length);
            if (err != ESP_OK) {
                ESP_LOGE(GATTS_TAG, "Failed to read from NVS: %s", esp_err_to_name(err));
            } else {
                gatts_char_val.attr_len = length;
            }

            err = esp_ble_gatts_send_indicate(gl_profile.gatts_if, gl_profile.conn_id, gl_profile.char_handle,
                                              gatts_char_val.attr_len, gatts_char_val.attr_value, false);
            if (err != ESP_OK) {
                ESP_LOGE(GATTS_TAG, "Failed to send notification: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(GATTS_TAG, "Notification sent successfully");
            }
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for 2 seconds
    }
}

void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Open NVS handle
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(GATTS_TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
    } else {
        ESP_LOGI(GATTS_TAG, "NVS handle opened");
    }

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    // Register callbacks
    ret = esp_ble_gatts_register_callback(gatts_profile_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_app_register(0);
    ESP_ERROR_CHECK(ret);

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(GATTS_TAG, "Setting local MTU failed, error code = %x", local_mtu_ret);
    }

    // Create a task to send notifications
    xTaskCreate(notification_task, "notification_task", 4096, NULL, 5, NULL);
}
