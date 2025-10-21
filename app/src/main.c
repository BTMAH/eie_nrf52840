/*
 * main.c
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS  1

// ---- Configure your password here (bitmask over BTN2..BTN1..BTN0) ----
// e.g., 0b101 means BTN2 and BTN0 must be pressed sometime before ENTER.
#define PASS_MASK  0b101

typedef enum {
    STATE_LOCKED = 0,     // LED0 ON, waiting for any of BTN0..BTN2 to begin entry
    STATE_ENTRY,          // collecting presses on BTN0..BTN2 until BTN3 (Enter)
    STATE_RESULT_WAIT,    // print result, turn LED0 OFF, then go to WAITING
    STATE_WAITING         // any button press resets to LOCKED (and clears state)
} app_state_t;

static inline void reset_to_locked(uint8_t *entry_mask, app_state_t *st)
{
