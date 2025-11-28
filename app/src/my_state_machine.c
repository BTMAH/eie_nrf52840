/**
 * @file my_state_machine.c
 *
 * This file implements the 5-state FSM from the diagram:
 * 
 * S0: all LEDs off
 * S1: LED1 blinks at 4 Hz
 * S2: LED1 & 3 on, LED2 & 4 off
 * S3: LED1 & 3 off, LED2 & 4 on
 * S4: all LEDs blink at 16 Hz
 * 
 */

#include <zephyr/kernel.h>
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
static void s0_entry(void *o);
static enum smf_state_result s0_run(void *o);
static void s0_exit(void *o);

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

/* 
 * Enum that gives an index for each state
 * These indices will be used to index into the "sm_states" table below.
*/
typedef enum {
    SM_S0_ALL_OFF = 0,          // S0 - All LEDs off
    SM_S1_LED1_BLINK_4HZ,       // S1 - LED1 blink at 4hz
    SM_S2_LEDS_1_3_ON,          // S2 - LED1 & 3 on
    SM_S3_LEDS_2_4_ON,          // S3 - LED2 & 4 on
    SM_S4_ALL_BLINK_16HZ,       // S4 - ALL LEDs blink at 16 Hz
} sm_state_t;

/*
 * This struct is our *state machine object*
 *
 * VERY IMPORTANT: the first member MUST be 'struct smf_ctx ctx;'
 *  - SMF_CTX(&sm_obj) casts '&sm_obj' to a 'struct smf_ctx *'
 *  - SMF internally assumes that 'ctx' is at the beginning of the struct
 *
 * After 'ctx', we can put any data we want (counters, flags, etc.)
 */
typedef struct {
    struct smf_ctx ctx;         // SMF context (must be first)
    uint32_t ticks;             // used in S2/S3 to track elapsed time
} sm_object_t;

/*-----------------------------------------------------------
 * State Table
 *-----------------------------------------------------------*/

/*
 * This is the core of SMF: a table of 'struct smf_state'
 *
 * Each entry is created with SMF_CREATE_STATE(entry, run, exit, parent, initial)
 * 
 * - entry: function called ONCE when entering the state
 * - run: function called very time we call sm_run_state()
 * - exit: function called ONCE when leaving the state
 * - parent: pointer to parent state's struct smf_state (for hierarchial SMs)
 * - initial: point to child's initial state (for hierarchial SMs)
 * 
 * For a flat FSM (no hierarchy), we just pass NULL for parent and initial.
 */

static const struct smf_state sm_states[] = {
    [SM_S0_ALL_OFF] = SMF_CREATE_STATE(s0_entry, s0_run, s0_exit, NULL, NULL),
    [SM_S1_LED1_BLINK_4HZ] = SMF_CREATE_STATE(s1_entry, s1_run, s1_exit, NULL, NULL),
    [SM_S2_LEDS_1_3_ON] = SMF_CREATE_STATE(s2_entry, s2_run, s2_exit, NULL, NULL),
    [SM_S3_LEDS_2_4_ON] = SMF_CREATE_STATE(s3_entry, s3_run, s3_exit, NULL, NULL),
    [SM_S4_ALL_BLINK_16HZ] = SMF_CREATE_STATE(s4_entry, s4_run, s4_exit, NULL, NULL),
};

/*
 * Our single global instance of the state machine object.
 * Could have more than one if multiple independent FSMs were needed.
 */

static sm_object_t sm_obj;

/*-----------------------------------------------------------
 * Public API called from main.c
 *----------------------------------------------------------*/

/*
 * Initialize the FSM:
 *   - clear any counters/flags
 *   - tell SMF what the initial state is
 */
void state_machine_init(void)
{
    sm_obj.ticks = 0;   // start with timer at 0

    // smf_set_initial(ctx, initial_state)
    //
    // SMF_CTX(&sm_obj) turns '&sm_obj' into 'struct smf_ctx *'
    // &sm_states[SM_S0_ALL_OFF] is a pointer to the S0 state entry in our table.
    smf_set_initial(SMF_CTX(&sm_obj), &sm_states[SM_S0_ALL_OFF]);
}

/*
 * Run one "step" of the FSM.
 * This is called in a loop from main.c.
 *
 * smf_run_state() will:
 *   - call the current state's run() function
 *   - if run() calls smf_set_state(), it handles exit() and entry() as needed
 */
int state_machine_run(void)
{
    return smf_run_state(SMF_CTX(&sm_obj));
}

/*-----------------------------------------------------------
 * State S0 – All LEDs off
 *
 * Transitions:
 *   - Button 1 pressed -> S1
 *   - Button 4 pressed -> S4
 *----------------------------------------------------------*/

static void s0_entry(void *o)
{
    ARG_UNUSED(o);  // we don't use the 'o' parameter in this function

    printk("Enter S0: all LEDs off\n");

    // Ensure all LEDs are off when we enter S0
    LED_set(LED0, LED_OFF);
    LED_set(LED1, LED_OFF);
    LED_set(LED2, LED_OFF);
    LED_set(LED3, LED_OFF);
}

static enum smf_state_result s0_run(void *o)
{
    ARG_UNUSED(o);  // we don't need 'o' in this simple state

    // BTN_check_clear_pressed() returns true ONCE when a debounced press is
    // detected, and also clears its internal flag so it won't repeat.
    if (BTN_check_clear_pressed(BUTTON1)) {
        // Button 1 pressed → transition to S1
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S1_LED1_BLINK_4HZ]);

    } else if (BTN_check_clear_pressed(BUTTON4)) {
        // Button 4 pressed → transition to S4
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S4_ALL_BLINK_16HZ]);
    }

    // Flat FSM, so we just say "event handled"
    return SMF_EVENT_HANDLED;
}

static void s0_exit(void *o)
{
    ARG_UNUSED(o);
    printk("Exit S0\n");
}

/*-----------------------------------------------------------
 * State S1 – LED1 blink at 4 Hz
 *
 * Transitions:
 *   - Button 2 pressed -> S2
 *   - Button 4 pressed -> S4
 *----------------------------------------------------------*/

static void s1_entry(void *o)
{
    ARG_UNUSED(o);
    printk("Enter S1: LED1 blink 4 Hz\n");

    // Turn off all LEDs except LED1
    LED_set(LED1, LED_OFF);
    LED_set(LED2, LED_OFF);
    LED_set(LED3, LED_OFF);

    // LED_blink(led_id, led_frequency) is provided by the EiE LED API.
    // It sets up a non-blocking timer to toggle the LED at the given rate.
    LED_blink(LED0, LED_4HZ);
}

static enum smf_state_result s1_run(void *o)
{
    ARG_UNUSED(o);

    if (BTN_check_clear_pressed(BUTTON2)) {
        // Move to S2 when Button 2 is pressed
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S2_LEDS_1_3_ON]);

    } else if (BTN_check_clear_pressed(BUTTON4)) {
        // Jump to S0 if Button 4 is pressed
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S0_ALL_OFF]);
    } else if (BTN_check_clear_pressed(BUTTON3)) {
        // Button 3 -> S4
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S4_ALL_BLINK_16HZ]);
    }

    return SMF_EVENT_HANDLED;
}

static void s1_exit(void *o)
{
    ARG_UNUSED(o);
    printk("Exit S1\n");
}

static void s2_entry(void *o)
{
    sm_object_t *self = o;  // cast the generic void* back to our type
    printk("Enter S2: LED1 & LED3 on\n");

    // Start timing from 0 when we enter this state
    self->ticks = 0;

    // Set LEDs according to the diagram
    LED_set(LED0, LED_ON);      // "LED 1" on
    LED_set(LED1, LED_OFF);  // "LED 2" off
    LED_set(LED2, LED_ON);   // "LED 3" on
    LED_set(LED3, LED_OFF);  // "LED 4" off    
}

static enum smf_state_result s2_run(void *o)
{
    sm_object_t *self = o;

    // We assume state_machine_run() is called every 1 ms (TICK_MS),
    // so each time run() executes we add 1 ms to our timer.
    self->ticks += TICK_MS;

    if (BTN_check_clear_pressed(BUTTON4)) {
        // Button 4 pressed → immediate transition back to S0
        smf_set_state(SMF_CTX(self), &sm_states[SM_S0_ALL_OFF]);

    } else if (self->ticks >= 1u * TICKS_PER_SEC) {
        // 1 second has passed → transition to S3
        smf_set_state(SMF_CTX(self), &sm_states[SM_S3_LEDS_2_4_ON]);
    }

    return SMF_EVENT_HANDLED;
}

static void s2_exit(void *o)
{
    ARG_UNUSED(o);
    printk("Exit S2\n");
}

/*-----------------------------------------------------------
 * State S3 – LED1 & LED3 off, LED2 & LED4 on
 *
 * Transitions:
 *   - 2 seconds passed -> S2
 *   - Button 4 pressed -> S0
 *----------------------------------------------------------*/

static void s3_entry(void *o)
{
    sm_object_t *self = o;

    printk("Enter S3: LED2 & LED4 on\n");

    // Reset timer on entry
    self->ticks = 0;

    // LED pattern for S3:
    LED_set(LED0, LED_OFF);  // LED 1 off
    LED_set(LED1, LED_ON);   // LED 2 on
    LED_set(LED2, LED_OFF);  // LED 3 off
    LED_set(LED3, LED_ON);   // LED 4 on
}

static enum smf_state_result s3_run(void *o)
{
    sm_object_t *self = o;

    // Count time in milliseconds again
    self->ticks += TICK_MS;

    if (BTN_check_clear_pressed(BUTTON4)) {
        // Button 4 → back to S0
        smf_set_state(SMF_CTX(self), &sm_states[SM_S0_ALL_OFF]);

    } else if (self->ticks >= 2u * TICKS_PER_SEC) {
        // 2 seconds have passed → go back to S2
        smf_set_state(SMF_CTX(self), &sm_states[SM_S2_LEDS_1_3_ON]);
    }

    return SMF_EVENT_HANDLED;
}

static void s3_exit(void *o)
{
    ARG_UNUSED(o);
    printk("Exit S3\n");
}

/*-----------------------------------------------------------
 * State S4 – All LEDs blink at 16 Hz
 *
 * Transitions:
 *   - Button 4 pressed -> S0
 *----------------------------------------------------------*/

static void s4_entry(void *o)
{
    ARG_UNUSED(o);
    printk("Enter S4: all LEDs blink 16 Hz\n");

    // Blink all LEDs at 16 Hz using the EiE LED API
    LED_blink(LED0, LED_16HZ);
    LED_blink(LED1, LED_16HZ);
    LED_blink(LED2, LED_16HZ);
    LED_blink(LED3, LED_16HZ);
}

static enum smf_state_result s4_run(void *o)
{
    ARG_UNUSED(o);

    // Only transition condition for S4: Button 4 pressed -> S0
    if(BTN_check_clear_pressed(BUTTON4)) {
        smf_set_state(SMF_CTX(&sm_obj), &sm_states[SM_S0_ALL_OFF]);
    }

    return SMF_EVENT_HANDLED;
}

static void s4_exit(void *o)
{
    ARG_UNUSED(o);
    printk("Exit S4\n");
}