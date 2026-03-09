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

#define SLEEP_MS 100

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;

extern const lv_image_dsc_t TheGoat;

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
  
  lv_obj_t *image = lv_image_create(screen);
  lv_image_set_src(image, &TheGoat);
  lv_obj_align(image, LV_ALIGN_CENTER, 0, 0);

  display_blanking_off(display_dev);
  while (1) {
    lv_timer_handler();
    k_msleep(SLEEP_MS);
  }
  return 0;
}
