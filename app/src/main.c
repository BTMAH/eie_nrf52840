/*
 * main.c
 */

#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include "BTN.h"
#include "LED.h"

#define ARDUINO_SPI_NODE DT_NODELABEL(arduino_i2c)
#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#define SLEEP_MS 100
#define ADDR 0x38
#define TD_STATUS 0x02
#define P1_XH 0x03
#define P1_XL 0x04
#define P1_YH 0x05
#define P1_YL 0x06

#define TOUCH_EVENT_MASK 0xC0
#define TOUCH_EVENT_SHIFT 6

#define TOUCH_POS_MSB_MASK 0x0F

typedef enum {
  TOUCH_EVENT_PRESS_DOWN = 0b00u,
  TOUCH_EVENT_LIFT_UP = 0b01u,
  TOUCH_EVENT_CONTACT = 0b10u,
  TOUCH_EVENT_NO_EVENT = 0b11u
} touch_event_t;

#define ARDUINO_I2C_NODE DT_NODELABEL(arduino_i2c)
static const struct device* dev = DEVICE_DT_GET(ARDUINO_I2C_NODE);

void touch_control_cmd_rsp(uint8_t cmd, uint8_t * rsp) {
  struct i2c_msg cmd_rsp_msg[2] = {
    {&cmd, 1, I2C_MSG_WRITE},
    {rsp, 1, I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP}
  };
  int ret = i2c_transfer(dev, cmd_rsp_msg, 2, ADDR);
  if (ret < 0) {
    printk("I2C transfer failed: %d\n", ret);
}
}

int main(void) {
  if(!device_is_ready(dev)) {
    return 0;
  }

  if(0 > i2c_configure(dev, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_CONTROLLER)) {
    return 0;
  }

  if (0 > BTN_init()) {
    return 0;
  }
  if (0 > LED_init()) {
    return 0;
  }

 
  while(1) {
    uint8_t touch_status;
    touch_control_cmd_rsp(TD_STATUS, &touch_status);
    if(touch_status == 1) {
      uint8_t x_pos_h;
      uint8_t x_pos_l;
      uint8_t y_pos_h;
      uint8_t y_pos_l;

      touch_control_cmd_rsp(P1_XH, &x_pos_h);
      touch_control_cmd_rsp(P1_XL, &x_pos_l);
      touch_control_cmd_rsp(P1_YH, &y_pos_h);
      touch_control_cmd_rsp(P1_YL, &y_pos_l);

      uint16_t x_pos = ((x_pos_h & TOUCH_POS_MSB_MASK) << 8) + x_pos_l;
      uint16_t y_pos = ((y_pos_h & TOUCH_POS_MSB_MASK) << 8) + y_pos_l;

      printk("Touch at %u, %u\n", x_pos, y_pos);
    }
    k_msleep(SLEEP_MS);
  }
	return 0;
}
