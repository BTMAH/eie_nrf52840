/**
 * @file main.c
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <lvgl.h>

#include "BTN.h"

#define SCREEN_W 320
#define SCREEN_H 240

#define PADDLE_W 60
#define PADDLE_H 10
#define BALL_SIZE 10

#define PLAYER1_Y (SCREEN_H - 20)
#define PLAYER2_Y 25

#define PLAYER_SPEED 10
#define FRAME_MS 16

typedef struct {
    int x;
    int y;
    int vx;
    int vy;
} ball_t;

static const struct device *display_dev;

/* LVGL objects */
static lv_obj_t *player1_paddle;
static lv_obj_t *player2_paddle;
static lv_obj_t *ball_obj;
static lv_obj_t *player1_score_label;
static lv_obj_t *player2_score_label;

/* Game state */
static ball_t ball;
static int player1_x = (SCREEN_W - PADDLE_W) / 2;
static int player2_x = (SCREEN_W - PADDLE_W) / 2;
static int player1_score = 0;
static int player2_score = 0;

static void update_ui_positions(void);
static void reset_ball(bool toward_player1);
static void update_game(void);
static void update_score_labels(void);

/*----------------------------------------------------------------------------
 * Helper: update score text
 *---------------------------------------------------------------------------*/
static void update_score_labels(void)
{
    static char top_text[24];
    static char bottom_text[24];

    snprintk(top_text, sizeof(top_text), "P2: %d", player2_score);
    snprintk(bottom_text, sizeof(bottom_text), "P1: %d", player1_score);

    lv_label_set_text(player2_score_label, top_text);
    lv_label_set_text(player1_score_label, bottom_text);
}

/*----------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/
int main(void)
{
    int ret;

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return 0;
    }

    ret = BTN_init();
    if (ret < 0) {
        printk("BTN_init failed: %d\n", ret);
        return 0;
    }

    create_ui();
    display_blanking_off(display_dev);

    printk("2-player Pong starting...\n");
    printk("P2(top): BTN1 left, BTN2 right\n");
    printk("P1(bot): BTN3 left, BTN4 right\n");

    while (true) {
        update_game();
        lv_timer_handler();
        k_msleep(FRAME_MS);
    }

    return 0;
}