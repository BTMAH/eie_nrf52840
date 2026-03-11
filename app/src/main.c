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

#define PADDLE_W 30
#define PADDLE_H 5
#define BALL_SIZE 16

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

// The goat
LV_IMAGE_DECLARE(TheRealGoat);
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
    ball.vx = 3;

    if (toward_player1) {
        ball.vy = 4;
    } else {
        ball.vy = -4;
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
    // Ball Movement
    ball.x += ball.vx;
    ball.y += ball.vy;

    // Side Walls
    if (ball.x <= 0) {
        ball.x = 0;
        ball.vx = -ball.vx;
    }
    if (ball.x >= (SCREEN_W - BALL_SIZE)) {
        ball.x = SCREEN_W - BALL_SIZE;
        ball.vx = -ball.vx;
    }

    int ball_center = ball.x + (BALL_SIZE / 2);

    // Top paddle collision
    if ((ball.y <= (PLAYER2_Y + PADDLE_H)) &&
        ((ball.y + BALL_SIZE) >= PLAYER2_Y) &&
        ((ball.x + BALL_SIZE) >= player2_x) &&
        (ball.x <= (player2_x + PADDLE_W)) &&
        (ball.vy < 0)) {

        ball.y = PLAYER2_Y + PADDLE_H;
        ball.vy = -ball.vy;

        int offset = ball_center - (player2_x + (PADDLE_W / 2));
        if (offset < -10) {
            ball.vx = -3;
        } else if (offset > 10) {
            ball.vx = 3;
        }
    }

    // Bottom paddle collision
    if (((ball.y + BALL_SIZE) >= PLAYER1_Y) &&
        (ball.y <= (PLAYER1_Y + PADDLE_H)) &&
        ((ball.x + BALL_SIZE) >= player1_x) &&
        (ball.x <= (player1_x + PADDLE_W)) &&
        (ball.vy > 0)) {

        ball.y = PLAYER1_Y - BALL_SIZE;
        ball.vy = -ball.vy;

        int offset = ball_center - (player1_x + (PADDLE_W / 2));
        if (offset < -10) {
            ball.vx = -3;
        } else if (offset > 10) {
            ball.vx = 3;
        }
    }

    // Score Conditions
    if (ball.y < 0) {
        player1_score++;
        update_score_labels();

        if (player1_score >= WIN_SCORE) {
            show_game_over(1);
            return;
        }

        reset_ball(false);
    } else if (ball.y > SCREEN_H) {
        player2_score++;
        update_score_labels();

        if (player2_score >= WIN_SCORE) {
            show_game_over(2);
            return;
        }

        reset_ball(true);
    }

    update_ui_positions();

}
/*----------------------------------------------------------------------------
 * Create all LVGL objects
 *---------------------------------------------------------------------------*/
static void create_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    lv_obj_t *serve_line = lv_obj_create(screen);
    lv_obj_set_size(serve_line, 2, SCREEN_H - 60);
    lv_obj_set_pos(serve_line, 25, 30);
    lv_obj_set_style_bg_color(serve_line, lv_color_white(), 0);
    lv_obj_set_style_border_width(serve_line, 0, 0);
    lv_obj_set_style_radius(serve_line, 0, 0);

    // Top score
    player2_score_label = lv_label_create(screen);
    lv_obj_set_style_text_color(player2_score_label, lv_color_white(), 0);
    lv_obj_align(player2_score_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_transform_rotation(player2_score_label, 1800, 0);

    // Bottom score
    player1_score_label = lv_label_create(screen);
    lv_obj_set_style_text_color(player1_score_label, lv_color_white(), 0);
    lv_obj_align(player1_score_label, LV_ALIGN_BOTTOM_MID, 0, -28);

    // Top paddle
    player2_paddle = lv_obj_create(screen);
    lv_obj_set_size(player2_paddle, PADDLE_W, PADDLE_H);
    lv_obj_set_style_bg_color(player2_paddle, lv_color_white(), 0);
    lv_obj_set_style_border_width(player2_paddle, 0, 0);
    lv_obj_set_style_radius(player2_paddle, 0, 0);

    // Bottom paddle
    player1_paddle = lv_obj_create(screen);
    lv_obj_set_size(player1_paddle, PADDLE_W, PADDLE_H);
    lv_obj_set_style_bg_color(player1_paddle, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(player1_paddle, 0, 0);
    lv_obj_set_style_radius(player1_paddle, 0, 0);

    // Ball
    ball_obj = lv_image_create(screen);
    lv_image_set_src(ball_obj, &TheRealGoat);

    // Winner label
    winner_label = lv_label_create(screen);
    lv_obj_set_style_text_color(winner_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_text_font(winner_label, &lv_font_montserrat_14, 0);
    lv_obj_align(winner_label, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_flag(winner_label, LV_OBJ_FLAG_HIDDEN);

    // Restart label
    restart_label = lv_label_create(screen);
    lv_label_set_text(restart_label, "Press BTN1 to restart");
    lv_obj_set_style_text_color(restart_label, lv_color_white(), 0);
    lv_obj_align(restart_label, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(restart_label, LV_OBJ_FLAG_HIDDEN);

    update_score_labels();
    reset_ball(true);
    update_ui_positions();
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