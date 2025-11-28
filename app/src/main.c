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

#define SLEEP_MS  10

typedef enum {
    STATE_LOCKED,       // LED0 ON, entering password
    STATE_WAITING       // result shown, waiting for any button to reset
} app_state_t;

#define MAX_PASSWORD_LEN 16
// Fixed password for Challenge 1: sequence of BTN0, BTN1, BTN2, etc.
// 0 = BTN0, 1 = BTN1, 2 = BTN2

static const uint8_t password[] = {0, 1, 2, 1, 0, 2, 1};
static const uint8_t password_len = 7;

// Buffer for the user's current attempt in LOCKED state
static uint8_t input_buf[MAX_PASSWORD_LEN];
static uint8_t input_len = 0;

// Helper: add a button "value" (0/1/2) to input buffer safely
static void buffer_add(uint8_t value) {
    if (input_len >= MAX_PASSWORD_LEN) {
        printk("Input buffer full, ignoring BTN%u\n", value);
        return;
    }

    input_buf[input_len] = value;
    input_len++;

    printk("Added BTN%u, length now %u\n", value, input_len);
}

static bool check_password(void) {
    if (input_len != password_len) {
        return false;
    }

    for (uint8_t i = 0; i < password_len; i++) {
        if (input_buf[i] != password[i]) {
            return false;
        }
    }
    return true;
}

// Helper: reset to locked state (LED ON, clear input)
static void go_to_locked(app_state_t *state) {
    input_len = 0;
    LED_set(LED0, LED_ON);
    *state = STATE_LOCKED;
    printk("Back to LOCKED state\n");
}

int main(void) {
    if (0 > BTN_init()) {
        printk("BTN_init failed!\n");
        return -1;
    }

    if (0 > LED_init()) {
        printk("LED_init failed!\n");
        return -1;
    }

    app_state_t state = STATE_LOCKED;

    LED_set(LED0, LED_ON);
    LED_set(LED1, LED_OFF);
    LED_set(LED2, LED_OFF);
    LED_set(LED3, LED_OFF);

    printk("System LOCKED. Enter password with BTN0/BTN1/BTN2, press BTN3 to check.\n");
    
    while (1) {
        // Debounce button checks
        bool b0 = BTN_check_clear_pressed(BTN0);
        bool b1 = BTN_check_clear_pressed(BTN1);
        bool b2 = BTN_check_clear_pressed(BTN2);
        bool b3 = BTN_check_clear_pressed(BTN3);

        switch (state) {
        case STATE_LOCKED:
            // Record password attempt using BTN0 to BTN2
            if (b0) {
                buffer_add(0);
            }
            if (b1) {
                buffer_add(1);
            }
            if (b2) {
                buffer_add(2);
            }

            // BTN3 = "enter" / check password
            if(b3) {
                bool ok = check_password();
                if (ok) {
                    printk("Correct!\n");
                } else {
                    printk("Incorrect!\n");
                }

                // Move into WAITING state, turn LED0 off
                LED_set(LED0, LED_OFF);
                state = STATE_WAITING;
                printk("WAITING state: press ANY button to reset to LOCKED.\n");
            }
            break;

        case STATE_WAITING:
            // Any button press resets the system to LOCKED
            if (b0 || b1 || b2 || b3) {
                go_to_locked(&state);
            }
            break;
        }
        k_msleep(SLEEP_MS);
    }

}