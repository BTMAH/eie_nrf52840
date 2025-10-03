/*
 * main.c
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

int main(void) {
    int ret;
    int ret1;
    int ret2;
    int ret3;

    if(!gpio_is_ready_dt(&led0) || !gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) || !gpio_is_ready_dt(&led3)){
        return -1;
    }

    ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
    ret1 = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    ret2 = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
    ret3 = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);

    if (ret < 0) {
        return ret;
    }
    if (ret1 < 0) {
        return ret1;
    }
    if (ret2 < 0) {
        return ret2;
    }
    if (ret3 < 0) {
        return ret3;
    }

    while (1) {
        for (int brightness = 0; brightness < 100; brightness++) {
            gpio_pin_set_dt(&led0, 1);
            gpio_pin_set_dt(&led1, 1);
            gpio_pin_set_dt(&led2, 1);
            gpio_pin_set_dt(&led3, 1);
            k_busy_wait(brightness * 100);  // ON time
            gpio_pin_set_dt(&led0, 0);
            gpio_pin_set_dt(&led1, 0);
            gpio_pin_set_dt(&led2, 0);
            gpio_pin_set_dt(&led3, 0);
            k_busy_wait((100 - brightness) * 100); // OFF time
        }

        for (int brightness = 100; brightness > 0; brightness--) {
            gpio_pin_set_dt(&led0, 1);
            gpio_pin_set_dt(&led1, 1);
            gpio_pin_set_dt(&led2, 1);
            gpio_pin_set_dt(&led3, 1);
            k_busy_wait(brightness * 100);  // ON time
            gpio_pin_set_dt(&led0, 0);
            gpio_pin_set_dt(&led1, 0);
            gpio_pin_set_dt(&led2, 0);
            gpio_pin_set_dt(&led3, 0);
            k_busy_wait((100 - brightness) * 100); // OFF time
        }

    }
    return 0;
}