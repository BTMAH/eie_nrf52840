/*
 * main.c
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#include <stdbool.h>
#include <stdint.h>

#include "BTN.h"
#include "LED.h"

#define SLEEP_MS  1

// ---- Configure your password here (bitmask over BTN2..BTN1..BTN0) ----
// e.g., 0b101 means BTN2 and BTN0 must be pressed sometime before ENTER.
#define PASS_MASK  0b101u

typedef enum {
    STATE_LOCKED = 0,     // LED0 ON, waiting for any of BTN0..BTN2 to begin entry
    STATE_ENTRY,          // collecting presses on BTN0..BTN2 until BTN3 (Enter)
    STATE_RESULT_WAIT,    // print result, turn LED0 OFF, then go to WAITING
    STATE_WAITING         // any button press resets to LOCKED (and clears state)
} app_state_t;

static inline bool all_buttons_released(void) {
    return !BTN_is_pressed(BTN0) &&
           !BTN_is_pressed(BTN1) &&
           !BTN_is_pressed(BTN2) &&
           !BTN_is_pressed(BTN3);
}

static inline void note_btn_press(uint8_t *mask) {
    bool changed = false;
    if (BTN_check_clear_pressed(BTN0)) { *mask |= (1u << 0); changed = true; }
    if (BTN_check_clear_pressed(BTN1)) { *mask |= (1u << 1); changed = true; }
    if (BTN_check_clear_pressed(BTN2)) { *mask |= (1u << 2); changed = true; }

static inline void clear_all_latches(void) {
    for (int i = 0; i < NUM_BTNS; ++i) BTN_clear_pressed((btn_id)i);
}

static inline void reset_to_locked(uint8_t *entry_mask, app_state_t *st) {
    *entry_mask = 0u;
    clear_all_latches();
    LED_set(LED0, true);
    *st = STATE_LOCKED;
}

int main(void) {
    if (BTN_init() < 0) return 0;
    if (LED_init() < 0) return 0;

    uint8_t     entry_mask = 0u;
    app_state_t st = STATE_LOCKED;

    LED_set(LED0, true);

    while (1) {
        switch (st) {

        case STATE_LOCKED:
            note_btn_press(&entry_mask);
            if (entry_mask != 0u) {
                st = STATE_ENTRY;
            }
            break;

        case STATE_ENTRY:
            note_btn_press(&entry_mask);

            if (BTN_check_clear_pressed(BTN3)) {
                if (entry_mask == PASS_MASK) {
                    printk("Correct!\n");
                } else {
                    printk("Incorrect!\n");
                }
                LED_set(LED0, false);
                st = STATE_RESULT_WAIT;
            }
            break;

        case STATE_RESULT_WAIT:
            if (all_buttons_released()) {
                clear_all_latches();
                st = STATE_WAITING;
            }
            break;

        case STATE_WAITING:
            if (BTN_check_clear_pressed(BTN0) ||
                BTN_check_clear_pressed(BTN1) ||
                BTN_check_clear_pressed(BTN2) ||
                BTN_check_clear_pressed(BTN3)) {
                reset_to_locked(&entry_mask, &st);
            }
            break;
        }

        k_msleep(SLEEP_MS);
    }

    return 0;
}