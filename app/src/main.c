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

static void enter_state(system_state_t s) {
    switch(s) {
        case STATE_IDLE:
            LED_set(LED0, LED_OFF);
            break;
        
        case STATE_ARMED:
            LED_blink(LED0, LED_1HZ);
            break;
        
        case STATE_ACTIVE:
            LED_set(LED0, LED_ON);
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