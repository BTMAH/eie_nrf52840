/**
 * @file main.c
 */

#include <inttypes.h>


#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "BTN.h"
#include "LED.h"
#include "lv_data_obj.h"

#define SLEEP_MS 1

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;


int main(void) {
  if (!device_is_ready(display_dev)) {
    return 0;
  }
  screen = lv_screen_active();
  if (screen == NULL) {
    return 0;
  }
  if (0 > BTN_init()) {
    return 0;
  }
  if (0 > LED_init()) {
    return 0;
  }

  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, "Hello World!");

  display_blanking_off(display_dev);
  while (1) {
    lv_timer_handler();
    k_msleep(SLEEP_MS);
  }
  return 0;
}
