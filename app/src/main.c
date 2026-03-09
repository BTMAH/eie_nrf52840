/*
 * main.c
 */

#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 1

#define ARDUINO_SPI_NODE DT_NODELABEL(arduino_spi)
#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#define CMD_SOFTWARE_RESET 0x01
#define CMD_SLEEP_OUT 0x11
#define CMD_DISPLAY_ON 0x29
#define CMD_COLUMN_ADDRESS_SET 0x2A
#define CMD_ROW_ADDRESS_SET 0x2B
#define CMD_MEMORY_WRITE 0x2C

static const struct gpio_dt_spec dcx_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, dcx_gpios);
static const struct spi_cs_control cs_ctrl = (struct spi_cs_control) {
  .gpio = GPIO_DT_SPEC_GET(ARDUINO_SPI_NODE, cs_gpios),
  .delay = 0u,
};

static const struct device *dev = DEVICE_DT_GET(ARDUINO_SPI_NODE);
static const struct spi_config spi_cfg = {
  .frequency = 1000000,
  // SPI mode is not explicitly set in the operations bitfield as we are using
  /// mode 0 which corresponds to a 0 value
  .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
  .slave = 0,
  .cs = cs_ctrl
};

static void lcd_cmd(uint8_t cmd, struct spi_buf *data) {
  struct spi_buf cmd_buf[1] = {[0]={.buf=&cmd, .len=1}};
  struct spi_buf_set cmd_set = {.buffers=cmd_buf, .count=1};

  // D/C select must be low to send command
  gpio_pin_set_dt(&dcx_gpio, 0);
  spi_write(dev, &spi_cfg, &cmd_set);

  if (data != NULL) {
    struct spi_buf_set data_set = {.buffers=data, .count=1};

    // D/C select must be high to send command
    gpio_pin_set_dt(&dcx_gpio, 1);
    spi_write(dev, &spi_cfg, &data_set);
  }
}

int main(void) {
  if(!device_is_ready(dev)) {
    return 0;
  }

  if(!gpio_is_ready_dt(&dcx_gpio)) {
    return 0;
  }
  if(gpio_pin_configure_dt(&dcx_gpio, GPIO_OUTPUT_LOW)) {
    return 0;
  }

  if (0 > BTN_init()) {
    return 0;
  }
  if (0 > LED_init()) {
    return 0;
  }

  lcd_cmd(CMD_SOFTWARE_RESET, NULL);
  k_msleep(120); // The software reset command can take up to 120 ms
                     // before the next command can be processed
  lcd_cmd(CMD_SLEEP_OUT, NULL);
  lcd_cmd(CMD_DISPLAY_ON, NULL);

  uint8_t column_data[] = {[0]=0x00, [1]=0x95, [2]=0x00, [3]=0x9f}; // Column 149 to 159
  uint8_t row_data[] = {[0]=0x00, [1]=0x75, [2]=0x00, [3]=0x7F};
 // Row 117 to 127
  uint8_t color_data[300];
  for (int i = 0; i < 300; i+= 3) {
    color_data[i] = 0xFC;
    color_data[i+1] = 0;
    color_data[i+2] = 0;
  }
  struct spi_buf column_data_buf = {.buf=column_data, .len=4};
  struct spi_buf row_data_buf = {.buf=row_data, .len=4};
  struct spi_buf color_data_buf = {.buf=color_data, .len=300};

  lcd_cmd(CMD_COLUMN_ADDRESS_SET, &column_data_buf);
  lcd_cmd(CMD_ROW_ADDRESS_SET, &row_data_buf);
  lcd_cmd(CMD_MEMORY_WRITE, &color_data_buf);

  while(1) {
    k_msleep(SLEEP_MS);
  }
	return 0;
}
