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

enum system_state {
    STATE_IDLE = 0,
    STATE_ARMED, 
    STATE_ACTIVE
};
typedef enum system_state system_state_t;

// Add the 4-bit counter state
static uint8_t cnt = 0;

static void show_nibble(uint8_t v) {
    LED_set(LED0, (v & 0x1) ? LED_ON : LED_OFF);
    LED_set(LED1, (v & 0x2) ? LED_ON : LED_OFF);
    LED_set(LED2, (v & 0x4) ? LED_ON : LED_OFF);
    LED_set(LED3, (v & 0x8) ? LED_ON : LED_OFF);
}

static void enter_state(system_state_t s) {
    switch(s) {
        case STATE_IDLE:
            show_nibble(cnt);
            break;
        
        case STATE_ARMED:
            LED_blink(LED0, LED_1HZ);
            LED_set(LED1, LED_OFF);
            LED_set(LED2, LED_OFF);
            LED_set(LED3, LED_OFF);
            break;
        
        case STATE_ACTIVE:
            LED_set(LED0, LED_ON);
            LED_set(LED1, LED_OFF);
            LED_set(LED2, LED_OFF);
            LED_set(LED3, LED_OFF);
            break;
    }
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

    system_state_t state = STATE_IDLE;
    enter_state(state);

    while (1) {
        bool b0 = BTN_check_clear_pressed(BTN0);
        bool b1 = BTN_check_clear_pressed(BTN1);

        if (0 < b0) {
            cnt = (uint8_t)((cnt + 1) & 0x0F);
            if (state == STATE_IDLE) {
                show_nibble(cnt);
            }
            printk("cnt = %u\n", cnt);
        }

        switch (state) {
            case STATE_IDLE:
                if (0 < b0) {
                    state = STATE_ARMED;
                    enter_state(state);
                    printk("-> ARMED\n");
                }
                break;
            
            case STATE_ARMED:
                if (0 < b0) {
                    state = STATE_ACTIVE;
                    enter_state(state);
                    printk("-> ACTIVE\n");
                }

                else if (0 < b1) {
                    state = STATE_IDLE;
                    enter_state(state);
                    printk("-> IDLE\n");
                }
                break;
            
            case STATE_ACTIVE:
                if (0 < b1) {
                    state = STATE_IDLE;
                    enter_state(state);
                    printk("-> IDLE\n");
                }
                break;
        }
        k_msleep(SLEEP_MS);
    }
}