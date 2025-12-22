/*
 * main.c
 */
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
  uint32_t breathe_acc_ms;
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
    if (a->led0_fb_ms <= TICK_MS) {
      a->led0_fb_ms = 0;
      LED_set(LED0, LED_OFF);
    } else {
      a->led0_fb_ms -= TICK_MS;
    }
  }

  if (a->led1_fb_ms > 0) {
    if (a->led1_fb_ms <= TICK_MS) {
      a->led1_fb_ms = 0;
      LED_set(LED1, LED_OFF);
    } else {
      a->led1_fb_ms -= TICK_MS;
    }
  }
}

// Hold BTN0 + BTN1 for 3s anytime --> go to S3
// Returns true if it "triggered" the transition
static bool check_hold_to_standby(app_t *a, const struct smf_state *return_state, const struct smf_state *s3_state)
{
  if (BTN_is_pressed(BTN0) && BTN_is_pressed(BTN1)) {
    if (a->both_hold_ms < HOLD_TO_STANDBY_MS) {
      a->both_hold_ms += TICK_MS;
    }
    if (a->both_hold_ms >= HOLD_TO_STANDBY_MS) {
      a->both_hold_ms = 0;
      a->prev_state = return_state;
      smf_set_state(&a->smf, s3_state);
      return true;
    }
  } else {
    a->both_hold_ms = 0;
  }
  return false;
}

//* ----- SMF state function prototypes ----- *//
static void s0_entry(void *o);
static void s0_run(void *o);

static void s1_entry(void *o);
static void s1_run(void *o);

static void s2_entry(void *o);
static void s2_run(void *o);

static void s3_entry(void *o);
static void s3_run(void *o);

//* ----- SMF state table ----- *//
static const struct smf_state states[] = {
  [ST_S0] = SMF_CREATE_STATE(s0_entry, s0_run, NULL, NULL, NULL),
  [ST_S1] = SMF_CREATE_STATE(s1_entry, s1_run, NULL, NULL, NULL),
  [ST_S2] = SMF_CREATE_STATE(s2_entry, s2_run, NULL, NULL, NULL),
  [ST_S3] = SMF_CREATE_STATE(s3_entry, s3_run, NULL, NULL, NULL),
};

//* ----- S0: 1hz entry ----- *//
static void s0_entry(void *o)
{
  app_t *a = (app_t *)o;
  LED_blink(LED3, LED_1HZ);
  LED_set(LED0, LED_OFF);
  LED_set(LED1, LED_OFF);
  LED_set(LED2, LED_OFF);

  reset_current_code(a);
  a->both_hold_ms = 0;

  printk("S0: Enter first ASCII char (BTN0 = 0, BTN1 = 1). BTN2 reset, BTN3 save -> S1.\n");
}

static void s0_run(void *o) {
  app_t *a = (app_t *)o;

  // global services
  service_feedback_leds(a);

  // global hold-to-standby (S0 --> S3)
  if (check_hold_to_standby(a, &states[ST_S0], &states[ST_S3])) {
    return;
  }

  if (BTN_check_clear_pressed(BTN0)) {
    append_bit(a, 0);
    blink_feedback_led0(a);
    printk("S0: bit = 0, count = %u, byte = 0x%02X\n", a->bit_count, a->current_byte);
  }

  if (BTN_check_clear_pressed(BTN1)) {
    append_bit(a, 1);
    blink_feedback_led1(a);
    printk("S0: bit = 1, count = %u, byte = 0x%02X\n", a->bit_count, a->current_byte);
  }

  if (BTN_check_clear_pressed(BTN2)) {
    reset_current_code(a);
    printk("S0: reset current code\n");
  }

  if (BTN_check_clear_pressed(BTN3)) {
    if (!current_code_ready(a)) {
      printk("S0: need 8 bits before save (currently %u)\n", a->bit_count);
      return;
    }

    // Save first character into string
    a->str_len = 0;
    a->str[a->str_len++] = (char)a->current_byte;
    a->str[a->str_len] = '\0';

    printk("S0: saved first char '%c' (0x%02X). -> S1\n", a->str[0], (uint8_t)a->str[0]);

    reset_current_code(a);
    smf_set_state(&a->smf, &states[ST_S1]);
  }

}

//* ----- S1: 4hz entry ----- *//
static void s1_entry(void *o) 
{
  app_t *a = (app_t *)o;
  LED_blink(LED3, LED_4HZ);
  LED_set(LED0, LED_OFF);
  LED_set(LED1, LED_OFF);
  LED_set(LED2, LED_OFF);

  reset_current_code(a);
  a->both_hold_ms = 0;

  printk("S1: Build string. Enter next ASCII (BTN0/BTN1) or BTN2 reset code or BTN3 save --> S2.\n");
}
static void s1_run(void *o)
{
  app_t *a = (app_t *)o;

  service_feedback_leds(a);

  // global hold-to-standby (S1 -> S3)
  if (check_hold_to_standby(a, &states[ST_S1], &states[ST_S3])) {
    return;
  }

  if (BTN_check_clear_pressed(BTN0)) {
    append_bit(a, 0);
    blink_feedback_led0(a);
    printk("S1: bit = 0, count = %u, byte = 0x%02X\n", a->bit_count, a->current_byte);
  }

  if (BTN_check_clear_pressed(BTN1)) {
    append_bit(a, 1);
    blink_feedback_led1(a);
    printk("S1: bit = 1, count = %u, byte = 0x%02X\n", a->bit_count, a->current_byte);
  }

  if (BTN_check_clear_pressed(BTN2)) {
    reset_current_code(a);
    printk("S1: reset current code\n");
  }

  if (BTN_check_clear_pressed(BTN3)) {
    // save input code and go to S2
    // if 8-bits are ready, append that char before finalizing.
    if (current_code_ready(a)) {
      if (a->str_len < MAX_STR_LEN) {
        a->str[a->str_len++] = (char)a->current_byte;
        a->str[a->str_len] = '\0';
        printk("S1: appended '%c' (0x%02X)\n", a->str[a->str_len - 1], (uint8_t)a->str[a->str_len - 1]);
      } else {
        printk("S1: string full, cannot append\n");
      }
    } else {
      printk("S1: finalizing without new char (only %u bits entered)\n", a->bit_count);
    }

    reset_current_code(a);
    printk("S1: -> S2 (ready to send). Current string: %s\n", a->str);
    smf_set_state(&a->smf, &states[ST_S2]);
  }
}

//* ----- S2: 16hz entry ----- *//
static void s2_entry(void *o)
{
  app_t *a = (app_t *)o;
  LED_blink(LED3, LED_16HZ);
  LED_set(LED0, LED_OFF);
  LED_set(LED1, LED_OFF);
  LED_set(LED2, LED_OFF);

  a->both_hold_ms = 0;

  printk("S2: BTN3 sends to serial. BTN2 -> S0.\n");
}

static void s2_run(void*o)
{
  app_t *a = (app_t *)o;

  // global hold-to-standby (S2 --> S3)
  if (check_hold_to_standby(a, &states[ST_S2], &states[ST_S3])) {
    return;
  }

  if (BTN_check_clear_pressed(BTN2)) {
    printk("S2: -> S0\n");
    reset_current_code(a);
    smf_set_state(&a->smf, &states[ST_S0]);
  }

  if (BTN_check_clear_pressed(BTN3)) {
    printk("S2: SEND to serial monitor: %s\n", a->str);
  }
}

//* ----- S3: standby PWM breathing ----- *//
static void s3_entry(void *o)
{
  app_t *a = (app_t *)o;

  // Stop blink on LED3 and use PWM on all LEDs
  LED_pwm(LED0, 0);
  LED_pwm(LED1, 0);
  LED_pwm(LED2, 0);
  LED_pwm(LED3, 0);

  a->duty = 0;
  a->duty_dir = +1;
  a->breathe_acc_ms = 0;

  printk("S3: Standby breathing. Any button press returns to previous state.\n");

}

static void s3_run(void *o)
{
  app_t *a = (app_t *)o;

  // Any button press exits to previous state
  if (BTN_check_clear_pressed(BTN0) || BTN_check_clear_pressed(BTN1) ||
      BTN_check_clear_pressed(BTN2) || BTN_check_clear_pressed(BTN3)) {
        const struct smf_state *back = (a->prev_state != NULL) ? a->prev_state : &states[ST_S0];
        printk("S3: exit -> previous state\n");
        smf_set_state(&a->smf, back);
        return;
      }
  
  // Gently pulse and pace it so it looks smooth
  a->breathe_acc_ms += TICK_MS;
  // update duty every 15 ms
  if (a->breathe_acc_ms >= 15u) {
    a->breathe_acc_ms = 0;

    int16_t next = (int16_t)a->duty + a->duty_dir;
    if (next >= 100) {
      next = 100;
      a->duty_dir = -1;
    } else if (next <= 0) {
      next = 0;
      a->duty_dir = +1;
    }

    a->duty = (uint8_t)next;

    LED_pwm(LED0, a->duty);
    LED_pwm(LED1, a->duty);
    LED_pwm(LED2, a->duty);
    LED_pwm(LED3, a->duty);
  }
}

//* ----- main ----- *//
int main(void) {

  if (0 > LED_init()) {
    return 0;
  }
  if (0 > BTN_init()) {
    return 0;
  }

  app_t app = {0};

  // start in S0
  smf_set_initial(&app.smf, &states[ST_S0]);

  while (1) {
    smf_run_state(&app.smf);
    k_msleep(TICK_MS);
  }
	return 0;
}
