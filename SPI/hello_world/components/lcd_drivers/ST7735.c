#include "ST7735.h"

typedef struct {
  uint8_t cmd;
  uint8_t data[16];
  uint8_t databytes;  
} lcd_init_cmd_t;


static spi_device_handle_t* spi_dev;

PINS* lcd_pins;

static uint8_t display_buff[LCD_WIDTH * LCD_HEIGHT * 2];

DRAM_ATTR static const lcd_init_cmd_t st7735_init_cmds[] = {

    // software reset with delay
    {ST7735_SWRESET, {0}, ST_CMD_DELAY},
    // Out of sleep mode with delay
    {ST7735_SLPOUT, {0}, ST_CMD_DELAY},
    // Framerate ctrl - normal mode. Rate = fosc/(1x2+40) * (LINE+2C+2D)
    {ST7735_FRMCTR1, {0x01, 0x2C, 0x2D}, 3},
    // Framerate ctrl - idle mode.  Rate = fosc/(1x2+40) * (LINE+2C+2D)
    {ST7735_FRMCTR2, {0x01, 0x2C, 0x2D}, 3},
    // Framerate - partial mode. Dot/Line inversion mode
    {ST7735_FRMCTR3, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6},
    // Display inversion ctrl: No inversion
    {ST7735_INVCTR, {0x07}, 1},
    // Power control1 set GVDD: -4.6V, AUTO mode.
    {ST7735_PWCTR1, {0xA2, 0x02, 0x84}, 3},
    // Power control2 set VGH/VGL: VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
    {ST7735_PWCTR2, {0xC5}, 1},
    // Power control3 normal mode(Full color): Op-amp current small, booster voltage
    {ST7735_PWCTR3, {0x0A, 0x00}, 2},
    // Power control4 idle mode(8-colors): Op-amp current small & medium low
    {ST7735_PWCTR4, {0x8A, 0x2A}, 2},
    // Power control5 partial mode + full colors
    {ST7735_PWCTR5, {0x8A, 0xEE}, 2},
    // VCOMH VoltageVCOM control 1: VCOMH=0x0E=2.850
    {ST7735_VMCTR1, {0x0E}, 1},
    // Display Inversion Off
    {ST7735_INVOFF, {0}, 0},
    // Memory Data Access Control: Horizontal
    {ST7735_MADCTL, {0xA8}, 1}, 
    // Color mode, Interface Pixel Format: RGB-565, 16-bit/pixel
    {ST7735_COLMOD, {0x05}, 1},
    // Rows 0–128
    {ST7735_CASET, {0x00, 0x00, 0x00, 0x7F + 0x01}, 4}, 
    // Columns 0–160
    {ST7735_RASET, {0x00, 0x00, 0x00, 0x9F + 0x02}, 4}, 
    // Gamma Adjustments (pos. polarity). Provides accurate colors.
    {ST7735_GMCTRP1,
     {0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22, 0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10}, 16},
    // Gamma Adjustments (neg. polarity). Provides accurate colors.
    {ST7735_GMCTRN1,
     {0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e, 0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10,}, 16},
    // Normal Display Mode On
    {ST7735_NORON, {0}, ST_CMD_DELAY},
    // Display On
    {ST7735_DISPON, {0}, ST_CMD_DELAY},
    // Command to close sending operations
    {0, {0}, 0xFF},
};

// Sets the level of the DC pin (Data/Command) to specify whether data or a command is being sent.
void st7735_set_dc_pin(int value) {
    gpio_set_level(lcd_pins->PIN_NUM_DC, value);
};

// Sends a single command to the ST7735 display.
static void st7735_cmd(const uint8_t cmd) {
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));                       
  t.length = 8;                                    
  t.tx_buffer = &cmd;                              
  t.user = (void *)0;                              
  ret = spi_device_polling_transmit(*spi_dev, &t);  
  assert(ret == ESP_OK);                           
}

// Sends a data block to the ST7735 display. The length of the data is specified.
static void st7735_data(const uint8_t *data, int len) {
  esp_err_t ret;
  spi_transaction_t t;
  if (len == 0) return;                            
  memset(&t, 0, sizeof(t));                        
  t.length = len * 8;                              
  t.tx_buffer = data;                              
  t.user = (void *)1;                              
  ret = spi_device_polling_transmit(*spi_dev, &t);  
  assert(ret == ESP_OK);                           
}

// Sets the address window for pixel drawing on the ST7735 display.
static void st7735_set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  uint8_t data[4];
  st7735_cmd(ST7735_CASET);
  data[0] = 0x00;
  data[1] = x0 + 0x01;
  data[2] = 0x00;
  data[3] = x1 + 0x01;
  st7735_data(data, 4);

  st7735_cmd(ST7735_RASET);
  data[0] = 0x00;
  data[1] = y0 + 0x02;
  data[2] = 0x00;
  data[3] = y1 + 0x02;
  st7735_data(data, 4);

  st7735_cmd(ST7735_RAMWR);
}

// Initializes the ST7735 display and configures all necessary parameters.
void st7735_init(spi_device_handle_t* spi_dev_, PINS* pins_) {

  spi_dev = spi_dev_;
  lcd_pins = pins_;

  gpio_set_direction(lcd_pins->PIN_NUM_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(lcd_pins->PIN_NUM_RST, GPIO_MODE_OUTPUT);
  gpio_set_direction(lcd_pins->PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

  // Reset the display
  gpio_set_level(lcd_pins->PIN_NUM_RST, 0);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  gpio_set_level(lcd_pins->PIN_NUM_RST, 1);
  vTaskDelay(150 / portTICK_PERIOD_MS);

  // Send all the init commands
  int cmd = 0;
  while (st7735_init_cmds[cmd].databytes != 0xff) {
    st7735_cmd(st7735_init_cmds[cmd].cmd);
    st7735_data(st7735_init_cmds[cmd].data, st7735_init_cmds[cmd].databytes & 0x1F);
    if (st7735_init_cmds[cmd].databytes & ST_CMD_DELAY) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    cmd++;
  }

  /// Enable backlight
  gpio_set_level(lcd_pins->PIN_NUM_BCKL, 1);
}

// Fills the entire display with a specified color.
void st7735_fill_screen(uint16_t color) {
  for (int i = 0; i < (LCD_WIDTH * LCD_HEIGHT * 2); i = i + 2) {
    display_buff[i] = color & 0xFF;
    display_buff[i + 1] = color >> 8;
  }

  st7735_set_address_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
  st7735_data(display_buff, LCD_WIDTH * LCD_HEIGHT * 2);
}

// Draws a filled rectangle on the display with the specified color, position, and size.
void st7735_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) return;
  if ((x + w - 1) >= LCD_WIDTH) w = LCD_WIDTH - x;
  if ((y + h - 1) >= LCD_HEIGHT) h = LCD_HEIGHT - y;

  st7735_set_address_window(x, y, x + w - 1, y + h - 1);

  st7735_cmd(ST7735_RAMWR);

  for (int i = 0; i < (w * h * 2); i = i + 2) {
    display_buff[i] = color & 0xFF;
    display_buff[i + 1] = color >> 8;
  }
  st7735_data(display_buff, w * h * 2);
}

// Toggles color inversion mode on the display.
void st7735_invert_color(int i) {
  if (i) {
    st7735_cmd(ST7735_INVON);
  } else {
    st7735_cmd(ST7735_INVOFF);
  }
}

// Draws a single pixel at the specified position with the given color.
void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= LCD_WIDTH) || (y < 0) || (y >= LCD_HEIGHT)) return;

  display_buff[0] = color & 0xFF;
  display_buff[1] = color >> 8;
  st7735_set_address_window(x, y, x, y);
  st7735_data(display_buff, 2);
}

// Draws a single character at the specified position with a given size, foreground, and background color.
void st7735_draw_char(int16_t x, int16_t y, char c, int16_t color, int16_t bg_color, uint8_t size) {
  if ((x >= LCD_WIDTH - 5 * size + 1) ||          
      (y >= LCD_HEIGHT - 7 * size + 1) ||        
      ((x + 5 * size + 1) < 0) ||                 
      ((y + 7 * size + 1) < 0))                   
    return;

  for (int8_t i = 0; i < 5; i++) {  
    uint8_t line = std_font[c * 5 + i];
    for (int8_t j = 0; j < 8; j++, line >>= 1) {
      if (line & 1) {
        if (size == 1) {
          st7735_draw_pixel(x + i, y + j, color);
        } else {
          st7735_rect(x + (i * size), y + (j * size), size, size, color);
        }
      } else if (bg_color != color) {
        if (size == 1) {
          st7735_draw_pixel(x + i, y + j, bg_color);
        } else {
          st7735_rect(x + (i * size), y + (j * size), size, size, bg_color);
        }
      }
    }
  }
}

// Draws a string of characters on the display and returns the number of characters drawn.
uint32_t st7735_draw_string(uint16_t x_, uint16_t y, const char *pt, int16_t color, int16_t bg_color, uint8_t size) {
  uint32_t x_offset = 5 + 1 , y_offset = 7;  // font size 5x7.
  uint16_t x = x_;

  uint32_t count = 0;
  if (y > 15) {
    return 0;
  }
  while (*pt) {
    if (y > 15 - size + 2) return 0; 
    st7735_draw_char(x * x_offset + 1, y * y_offset + 1, *pt, color, bg_color, size);
    pt++;
    x = x + size;
    if (x > 24 - size + 2) {
      x = x_;
      y = y + size + 1; 
      if(*pt == ' '){
        pt++;
      }
    }
    count++;
  }
  return count;  // number of characters printed
}
