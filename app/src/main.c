/*
 * main.c
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

// Helper function to fade a diagonal ON then OFF
void fade_diagonal(const struct gpio_dt_spec *a, const struct gpio_dt_spec *b)
{
    // Fade in
    for (int brightness = 0; brightness <= 100; brightness += 5) {
        gpio_pin_set_dt(a, 1);
        gpio_pin_set_dt(b, 1);
        k_busy_wait(brightness * 40);   // ON time grows with brightness
        gpio_pin_set_dt(a, 0);
        gpio_pin_set_dt(b, 0);
        k_busy_wait((100 - brightness) * 40); // OFF time shortens
    }

    // Fade out
    for (int brightness = 100; brightness >= 0; brightness -= 5) {
        gpio_pin_set_dt(a, 1);
        gpio_pin_set_dt(b, 1);
        k_busy_wait(brightness * 100);
        gpio_pin_set_dt(a, 0);
        gpio_pin_set_dt(b, 0);
        k_busy_wait((100 - brightness) * 100);
    }
}

int main(void)
{
    if (!gpio_is_ready_dt(&led0) ||
        !gpio_is_ready_dt(&led1) ||
        !gpio_is_ready_dt(&led2) ||
        !gpio_is_ready_dt(&led3)) {
        return -1;
    }

    gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE);

    while (1) {
        // Fade diagonal 1 (top-left, bottom-right)
        fade_diagonal(&led0, &led3);
        k_msleep(400);

        // Fade diagonal 2 (top-right, bottom-left)
        fade_diagonal(&led1, &led2);
        k_msleep(400);
    }

    return 0;
}
