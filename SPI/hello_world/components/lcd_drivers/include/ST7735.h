#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "ascii_font.h"

#define CONFIG_USE_COLOR_RBG565 

// Some ready-made 16-bit (RBG-565) color settings:
#define	COLOR_BLACK      0x0000
#define COLOR_WHITE      0xFFFF
#define	COLOR_RED        0xF800
#define	COLOR_GREEN      0x001F
#define	COLOR_BLUE       0x07E0
#define COLOR_CYAN       0x07FF
#define COLOR_MAGENTA    0xFFE0
#define COLOR_YELLOW     0xF81F

#define ST7735_TFTHEIGHT_160 160  // for 1.8" and mini display

#define LCD_WIDTH 160
#define LCD_HEIGHT 128


/* ST7735 COMMANDS */
#define ST_CMD_DELAY 0x80  // special signifier for command lists

#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID 0x04
#define ST7735_RDDST 0x09

#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13

#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR 0x30
#define ST7735_TEOFF 0x34
#define ST7735_TEON 0x35
#define ST7735_MADCTL 0x36
#define ST7735_COLMOD 0x3A

#define ST7735_MADCTL_MY 0x80
#define ST7735_MADCTL_MX 0x40
#define ST7735_MADCTL_MV 0x20
#define ST7735_MADCTL_ML 0x10
#define ST7735_MADCTL_RGB 0x00

#define ST7735_RDID1 0xDA
#define ST7735_RDID2 0xDB
#define ST7735_RDID3 0xDC
#define ST7735_RDID4 0xDD

#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH 0x04

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1
#define ST7735_TEMP    ff

typedef struct {
    int PIN_NUM_MISO;
    int PIN_NUM_MOSI;
    int PIN_NUM_CLK;
    int PIN_NUM_CS;
    int PIN_NUM_DC;
    int PIN_NUM_RST;
    int PIN_NUM_BCKL;
} PINS;

void st7735_set_dc_pin(int value);
void st7735_fill_screen(uint16_t color);
void st7735_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void st7735_invert_color(int i);
void st7735_init(spi_device_handle_t* spi_dev_, PINS* pins);
void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color);
void st7735_draw_char(int16_t x, int16_t y, char c, int16_t color, int16_t bg_color, uint8_t size);
uint32_t st7735_draw_string(uint16_t x_, uint16_t y, const char *pt, int16_t color, int16_t bg_color, uint8_t size);
  