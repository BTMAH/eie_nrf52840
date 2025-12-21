/*
 * main.c
 */

#include <inttypes.h>

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/smf.h>

#include "BTN.h"
#include "LED.h"

#define TICK_MS 1u
#define HOLD_TO_STANDBY_MS 3000u

#define MAX_STR_LEN 64u
#define FEEDBACK_BLINK_MS 120u

typedef enum {
  ST_S0 = 0,    // entry state (1hz)
  ST_S1,        // entry state (4hz)
  ST_S2,        // send state (16hz)
  ST_S3         // standby PWM breathing
} app_state_id_t;

typedef struct {
  struct smf_ctx smf;   // required by SMF

  // Go back to previous state after S3 return
  const struct smf_state *prev_state;

  // 3s hold detection
  uint32_t both_hold_ms;

  // LED0 and LED1 "blink on click" response
  uint32_t led0_fb_ms;
  uint32_t led1_fb_ms;

  // Current 8-bit entry
  uint8_t bit_count;        // 0 -> 8
  uint8_t current_byte;     // what the bits assemble to

  // Saved string
  char str[MAX_STR_LEN + 1];
  uint8_t str_len;

  // PWM breathing for S3
  uint8_t duty;
  int8_t duty_dir;
  uint32 breathe_acc_ms;
} app_t;

//* ----- Helper Functions ----- *//
static void reset_current_code(app_t *a)
{
  a->bit_count = 0;
  a->current_byte = 0;
}

static void append_bit(app_t *a, uint8_t bit)
{
  if (a->bit_count >= 8) {
    // If already full, ignore the bits
    return;
  }
  // Take the bits we’ve entered so far, 
  // shift them left by one, 
  // and append the new bit (0 or 1) to the right
  a->current_byte = (uint8_t)((a->current_byte << 1) | (bit & 0x1u));
  a->bit_count++;
}

static bool current_code_ready(const app_t *a)
{
  return (a->bit_count == 8);
}

static void blink_feedback_led0(app_t *a)
{
  LED_blink(LED0, LED_16HZ);
  a->led0_fb_ms = FEEDBACK_BLINK_MS;
}

static void blink_feedback_led1(app_t *a)
{
  LED_blink(LED1, LED_16HZ);
  a->led1_fb_ms = FEEDBACK_BLINK_MS;
}

static void service_feedback_leds(app_t *a)
{
  // Called every tick to end the temporary blink and restore OFF
  if (a->led0_fb_ms > 0) {
    a->led0_fb_ms -= TICK_MS;
    if (a->led0_fb_ms == 0)
      LED_set(LED0, LED_OFF);
  }

  if (a->led1_fb_ms > 0) {
    a->led1_fb_ms -= TICK_MS;
    if (a->led1_fb_ms == 0)
      LED_set(LED1, LED_OFF);
  }
}



int main(void) {

  if (0 > LED_init()) {
    return 0;
  }
  if (0 > BTN_init()) {
    return 0;
  }

  uint8_t current_duty_cycle = 0;
  LED_pwm(LED0, current_duty_cycle);
  LED_pwm(LED1, current_duty_cycle);
  LED_pwm(LED2, current_duty_cycle);
  LED_pwm(LED3, current_duty_cycle);

  while(1) {
    if (BTN_check_clear_pressed(BTN0) || BTN_check_clear_pressed(BTN1) ||
        BTN_check_clear_pressed(BTN2) || BTN_check_clear_pressed(BTN3)) {
      current_duty_cycle = (current_duty_cycle >= 100) ? 0 : (current_duty_cycle + 10);
      printk("Setting all LEDs to %d%% brightness.\n", current_duty_cycle);
      LED_pwm(LED0, current_duty_cycle);
      LED_pwm(LED1, current_duty_cycle);
      LED_pwm(LED2, current_duty_cycle);
      LED_pwm(LED3, current_duty_cycle);
    }
    k_msleep(SLEEP_TIME_MS);
  }
	return 0;
}
