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
#define MAX_PASSWORD_LEN 16

typedef enum {
    STATE_BOOT,         // LED3 ON for 3s, option to enter password-entry mode
    STATE_ENTRY,        // user is recording a new password
    STATE_LOCKED,       // LED0 ON, entering password
    STATE_WAITING       // result shown, waiting for any button to reset
} app_state_t;

// Password storage: start with a default password
// 0 = BTN0, 1 = BTN1, 2 = BTN2

static uint8_t password[MAX_PASSWORD_LEN] = {0, 1, 2, 1};
static uint8_t password_len = 4;

// Buffer for the user's current attempt in LOCKED state
static uint8_t input_buf[MAX_PASSWORD_LEN];
static uint8_t input_len = 0;

// Buffer used when user is defining a new password in ENTRY mode
static uint8_t entry_buf[MAX_PASSWORD_LEN];
static uint8_t entry_len = 0;


// Helper: add a button "value" (0/1/2) to input buffer safely (LOCKED state)
static void buffer_add(uint8_t value) {
    if (input_len >= MAX_PASSWORD_LEN) {
        printk("Input buffer full, ignoring BTN%u\n", value);
        return;
    }

    input_buf[input_len] = value;
    input_len++;

    printk("Added BTN%u, length now %u\n", value, input_len);
}

static void buffer_add_to_entry(uint8_t value) {
    if (entry_len >= MAX_PASSWORD_LEN) {
        printk("Entry buffer full, ignoring BTN%u\n", value);
        return;
    }

    entry_buf[entry_len] = value;
    entry_len++;
    
    printk("Added BTN%u to entry, length now %u\n", value, entry_len);
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

    app_state_t state = STATE_BOOT;
    int64_t boot_start_ms = k_uptime_get();

// Initial LED setup: LED3 on during BOOT, others off
    LED_set(LED0, LED_OFF);
    LED_set(LED1, LED_OFF);
    LED_set(LED2, LED_OFF);
    LED_set(LED3, LED_ON);

    printk("BOOT: LED3 ON for 3 seconds.\n");
    printk("Press BTN3 now to enter password ENTRY mode.\n");

    
    while (1) {
        // Debounce button checks
        bool b0 = BTN_check_clear_pressed(BTN0);
        bool b1 = BTN_check_clear_pressed(BTN1);
        bool b2 = BTN_check_clear_pressed(BTN2);
        bool b3 = BTN_check_clear_pressed(BTN3);

        switch (state) {
        case STATE_BOOT: {
            // If user presses BTN3 during the first 3 seconds --> ENTRY mode
            if (b3) {
                entry_len = 0;
                printk("ENTRY MODE: Press BTN0/1/2 to enter password, BTN3 to save.\n");
                state = STATE_ENTRY;
                break;
            }

            // Check 3-second timeout
            int64_t now = k_uptime_get();
            if (now - boot_start_ms >= 3000) {
                LED_set(LED3, LED_OFF);
                // Move straight to LOCKED (Challenge 1 behaviour)
                go_to_locked(&state);
                printk("BOOT timeout --> entering LOCKED state.\n");
            }
            break;
        }

        case STATE_ENTRY: {
            if (b0) {
                buffer_add_to_entry(0);
            }
            if (b1) {
                buffer_add_to_entry(1);
            }
            if (b2) {
                buffer_add_to_entry(2);
            }
            if (b3) {
                if (entry_len > 0) {
                    password_len = entry_len;
                    for (uint8_t i = 0; i < entry_len; i++) {
                        password[i] = entry_buf[i];
                    }
                    printk("New password saved (length = %u). Going to LOCKED.\n", password_len);
                } else {
                    printk("ENTRY ended with empty password, keeping old password.\n");
                }
                LED_set(LED3, LED_OFF);
                go_to_locked(&state);
            }
            break;
        }
        case STATE_LOCKED: {
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
        }

        case STATE_WAITING: {
            // Any button press resets the system to LOCKED
            if (b0 || b1 || b2 || b3) {
                go_to_locked(&state);
            }
            break;
        }
        }
        k_msleep(SLEEP_MS);
    }

}