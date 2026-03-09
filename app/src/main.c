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

#define WIN_SCORE 10

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
static lv_obj_t *winner_label;
static lv_obj_t *restart_label;

/* Game state */
static ball_t ball;
static int player1_x = (SCREEN_W - PADDLE_W) / 2;
static int player2_x = (SCREEN_W - PADDLE_W) / 2;
static int player1_score = 0;
static int player2_score = 0;
static bool game_over = false;

static void update_ui_positions(void);
static void reset_ball(bool toward_player1);
static void update_game(void);
static void update_score_labels(void);
static void show_game_over(uint8_t winner);
static void reset_match(void);

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
 * Helper: reset ball
 *---------------------------------------------------------------------------*/
static void reset_ball(bool toward_player1)
{
    ball.x = 30;
    ball.y = (SCREEN_H - BALL_SIZE) / 2;
    ball.vx = 2;

    if (toward_player1) {
        ball.vy = 3;
    } else {
        ball.vy = -3;
    }
}

/*----------------------------------------------------------------------------
 * Helper: reset full match
 * BTN0 resets after game over
 *---------------------------------------------------------------------------*/
static void reset_match(void)
{
    player1_score = 0;
    player2_score = 0;
    player1_x = (SCREEN_W - PADDLE_W) / 2;
    player2_x = (SCREEN_W - PADDLE_W) / 2;
    game_over = false;

    update_score_labels();
    reset_ball(true);
    update_ui_positions();

    lv_obj_add_flag(winner_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(restart_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(player1_paddle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(player2_paddle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
}

/*----------------------------------------------------------------------------
 * Helper: show game over screen
 *---------------------------------------------------------------------------*/
static void show_game_over(uint8_t winner)
{
    static char win_text[40];

    game_over = true;

    snprintk(win_text, sizeof(win_text), "Congrats Player %d!", winner);
    lv_label_set_text(winner_label, win_text);

    lv_obj_clear_flag(winner_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(restart_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(player1_paddle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(player2_paddle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
}

/*----------------------------------------------------------------------------
 * Helper: push state to LVGL objects
 *---------------------------------------------------------------------------*/
static void update_ui_positions(void)
{
    lv_obj_set_pos(player1_paddle, player1_x, PLAYER1_Y);
    lv_obj_set_pos(player2_paddle, player2_x, PLAYER2_Y);
    lv_obj_set_pos(ball_obj, ball.x, ball.y);
}

/*----------------------------------------------------------------------------
 * Main game step
 * Player 2 (top):    BTN0 left, BTN1 right
 * Player 1 (bottom): BTN2 left, BTN3 right
 *---------------------------------------------------------------------------*/
static void update_game(void)
{
    if (game_over) {
        if (BTN_is_pressed(BTN0)) {
            k_msleep(150);
            reset_match();
        }
        return;
    }

    /* Player 2 movement */
    if (BTN_is_pressed(BTN0) && !BTN_is_pressed(BTN1)) {
        player2_x -= PLAYER_SPEED;
    } else if (BTN_is_pressed(BTN1) && !BTN_is_pressed(BTN0)) {
        player2_x += PLAYER_SPEED;
    }

    /* Player 1 movement */
    if (BTN_is_pressed(BTN2) && !BTN_is_pressed(BTN3)) {
        player1_x -= PLAYER_SPEED;
    } else if (BTN_is_pressed(BTN3) && !BTN_is_pressed(BTN2)) {
        player1_x += PLAYER_SPEED;
    }

    if (player1_x < 0) {
        player1_x = 0;
    }
    if (player1_x > (SCREEN_W - PADDLE_W)) {
        player1_x = SCREEN_W - PADDLE_W;
    }

    if (player2_x < 0) {
        player2_x = 0;
    }
    if (player2_x > (SCREEN_W - PADDLE_W)) {
        player2_x = SCREEN_W - PADDLE_W;
    }

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