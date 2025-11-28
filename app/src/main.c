/*
 * main.c
 */

#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "BTN.h"
#include "LED.h"

#include "my_state_machine.h"

#define SLEEP_MS 1

int main(void) {

  if (0 > BTN_init()) {
    return 0;       // if init fails, just stop the program
  }
  if (0 > LED_init()) {
    return 0;       // if init fails, just stop the program
  }
  //Initialize our state machine (sets initial state, clears counters, etc)
  state_machine_init();
  //Main loop: repeatedly "tick" the state machine and sleep a bit
  while(1) {
    int ret = state_machine_run();  //run one iteration of the FSM

    if (0 > ret) {  // convention: non-zero or negative can mean "stop"
      return 0;
    }

    // This below controls how often state_machine_run() is called.
    // With SLEEP_MS = 1, we *attempt* to run it about once per millisecond.
    
    k_msleep(SLEEP_MS);
  }
	return 0;
}
