#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "ST7735.h"

char* TAG = "ESP_ST7753";

static spi_device_handle_t spi_dev;

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
  st7735_set_dc_pin((int)t->user);
}

void spi_init(PINS* pins){

  spi_bus_config_t buscfg = {.miso_io_num = pins->PIN_NUM_MISO,
                             .mosi_io_num = pins->PIN_NUM_MOSI,
                             .sclk_io_num = pins->PIN_NUM_CLK,
                             .quadwp_io_num = -1,  // unused
                             .quadhd_io_num = -1,  // unused
                             .max_transfer_sz = 160 * 128 * 2};

  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 10 * 1000 * 1000,       // Clock out at 10 MHz
      .mode = 0,                                // SPI mode 0
      .spics_io_num = pins->PIN_NUM_CS,         // CS pin
      .queue_size = 7,                          // We want to be able to queue 7 transactions at a time
      .pre_cb = lcd_spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
  };

  spi_host_device_t spi_host = VSPI_HOST;

  ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &buscfg, 1));

  ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &devcfg, &spi_dev));

}

void app_main(void)
{ 
    PINS pins = {
      .PIN_NUM_MISO = -1,
      .PIN_NUM_MOSI = 23,
      .PIN_NUM_CLK = 18,
      .PIN_NUM_CS = 5,
      .PIN_NUM_DC = 21,
      .PIN_NUM_RST = 22,
      .PIN_NUM_BCKL = 17
    };

    ESP_LOGI(TAG, "SPI INIT...");
    spi_init(&pins);
    ESP_LOGI(TAG, "SPI INIT done");
    
    ESP_LOGI(TAG, "LCD INIT...");
    st7735_init(&spi_dev, &pins);
    ESP_LOGI(TAG, "LCD INIT done");

    st7735_fill_screen(COLOR_WHITE);

    char temp[128];
    bool inside = true;


    while(1) {
        if(inside) {
            snprintf(temp, sizeof(temp), "Jestes w obszarze");
            st7735_draw_string(1, 1, temp, COLOR_BLACK, COLOR_MAGENTA, 3);
            inside = false;
        } else {
            snprintf(temp, sizeof(temp), "Jestes poza obszarem!!!");
            st7735_draw_string(1, 1, temp, COLOR_BLACK, COLOR_MAGENTA, 3);
            inside = true;
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        st7735_fill_screen(COLOR_WHITE);  
    }
}
