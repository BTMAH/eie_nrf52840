/**
 * @file my_state_machine.c
 *
 * This file implements the 5-state FMS from the diagram:
 * 
 * S0: all LEDs off
 * S1: LED1 blinks at 4 Hz
 * S2: LED1 & 3 on, LED2 & 4 off
 * S3: LED1 & 3 off, LED2 & 4 on
 * S4: all LEDs blink at 16 Hz
 * 
 */

#include <zephyr/kernel.h)
#include <zephyr/sys/printk.h>
#include <zephyr/smf.h> // SMF types, macros, and functions

#include "BTN.h"        // EiE button API
#include "LED.h"        // EiE LED API
#include "my_state_machine.h"

/*-----------------------------------------------------------
 * Timing Configuration
 *-----------------------------------------------------------*/

// Our main loop in main.c sleeps for SLEEP_MS_1, so each call to
// state_machine_run() happens roughly every 1ms.
#define TICK_MS 1u

//How many "ticks" to get 1 second, assuming 1 ms per tick.
#define TICKS_PER_SEC (1000u / TICK_MS)

/*-----------------------------------------------------------
 * Button Mapping
 *-----------------------------------------------------------*/

/*
* In the diagram they say "Button 1-4"
* on the nRF board and EiE APIs we have BTN0-BTN3
* We'll map them like this:
*
* Diagram Button 1 -> BTN0
* Diagram Button 2 -> BTN1
* Diagram Button 3 -> BTN2
* Diagram Button 4 -> BTN3
*/

#define BUTTON1 BTN0
#define BUTTON2 BTN1
#define BUTTON3 BTN2
#define BUTTON4 BTN3

/*-----------------------------------------------------------
 * Function Prototypes for each state's entry/run/exit
 *
 * SMF expects:
 *  entry: void state_entry(void *obj)
 *  run: enum smf_state_result state_run(void *obj)
 *  exit: void state_exit(void *obj)
 * 
 * 'obj' is a pointer to *our* state machine object (defined later).
 *-----------------------------------------------------------*/

// S0: All LEDs off
static void s0_entry(void *0);
static enum smf_state_result s0_run(void *o);
static void s0_exit(void *0);

// S1: LED1 blink at 4 Hz
static void s1_entry(void *o);
static enum smf_state_result s1_run(void *o);
static void s1_exit(void *o);

// S2: LED1 & 3 on, LED2 & 4 off
static void s2_entry(void *o);
static enum smf_state_result s2_run(void *o);
static void s2_exit(void *o);

// S3: LED1 & 3 off, LED2 & 4 on
static void s3_entry(void *o);
static enum smf_state_result s3_run(void *o);
static void s3_exit(void *o);

// S4: All LEDs blink at 16 Hz
static void s4_entry(void *o);
static enum smf_state_result s4_run(void *o);
static void s4_exit(void *o);

/*-----------------------------------------------------------
 * State IDs and state machine "object" type
 *-----------------------------------------------------------*/


 
/*-----------------------------------------------------------
 * Typedefs
 *-----------------------------------------------------------*/
enum led_state_machine_states {
    LED_ON_STATE,
    LED_OFF_STATE
};
typedef struct {
    // Context variable used by zephyr to track state machine state. Must be first
    struct smf_ctx ctx;

    uint16_t count;
} led_state_object_t;

/*-----------------------------------------------------------
 * Local Variables
 *-----------------------------------------------------------*/
static const struct smf_state led_states[] = {
    [LED_ON_STATE] = SMF_CREATE_STATE(led_on_state_exit, led_on_state_run, NULL, NULL, NULL),
    [LED_OFF_STATE] = SMF_CREATE_STATE(led_off_state_exit, led_off_state_run, NULL, NULL, NULL)
};

static led_state_object_t led_state_object;

void state_machine_init() {
    led_state_object.count = 0;
    smf_set_initial(SMF_CTX(&led_state_object), &led_states[LED_ON_STATE]);
}

int state_machine_run() {
    return smf_run_state(SMF_CTX(&led_state_object));
}

static void led_on_state_exit(void* o) {
    LED_set(LED0, LED_OFF);
}

static enum smf_state_result led_on_state_run(void* o) {
    if (led_state_object.count > 500) {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LED_OFF_STATE]);
    } else {
        led_state_object.count++;
    }

    return SMF_EVENT_HANDLED;
}

static void led_off_state_exit(void* o) {
    LED_set(LED0, LED_ON);
}

static enum smf_state_result led_off_state_run(void* o) {
    if (led_state_object.count > 500) {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LED_ON_STATE]);
    } else {
        led_state_object.count++;
    }

    return SMF_EVENT_HANDLED;
}