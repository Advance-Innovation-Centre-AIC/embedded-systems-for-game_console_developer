/*
 * pong_step4.c -- สร้าง Pong #4 (คาบ 13): นับคะแนน + เสิร์ฟใหม่ + first-to-N
 *
 * Twin of pong_step4.py
 * (embedded-systems-for-game_console_developer/developer-i/solution_codes/pong_step4.py).
 *
 * หน้าเริ่ม PONG รอกด Start จากนั้นไม้ซ้ายของเราบังคับด้วย UP/DOWN (เร่งและไถล)
 * ไม้ขวาเป็น AI ง่าย ๆ ที่วิ่งตามลูก ตีโต้กันได้สองฝั่ง ลูกหลุดซ้ายหรือขวา = อีกฝั่ง
 * ได้แต้ม ป้าย "0 : 0" บนจอนับคะแนน แล้วเสิร์ฟลูกใหม่ ใครถึง 7 แต้มก่อนชนะ (เสียง win
 * หรือ lose และข้อความ YOU WIN! หรือ AI WINS) แล้วเกมจบ กด Start ระหว่างเล่นก็จบเกม
 *
 * The PONG title waits for Start. Our left paddle moves with UP/DOWN
 * (speeding up and sliding); the right paddle is a simple AI that
 * follows the ball, so both sides return it. A ball out on one side
 * scores for the other, the "0 : 0" label counts, and a new ball is
 * served. The first to 7 wins ("win" or "lose", then YOU WIN! or AI
 * WINS) and the game ends. Start during play also ends it.
 *
 * C twin of the Example Explorer program devi-pong-4
 * (py/devi-pong-4.py, the practise version of this solution).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the module's globals are one pong_t at file scope (too big for
 *     the stack), set again at the start of main;
 *   - floats are bento_real_t, each board's MicroPython float, and
 *     max()/min() are two helpers that keep Python's choice of argument;
 *   - each sum of products is one statement per step, so no board
 *     fuses a multiply and an add MicroPython rounds apart;
 *   - the score text is formatted into a buffer sized for any int32_t;
 *   - on_each_frame() is the update bento_game_run calls each frame;
 *     `return False` sets *out_stop, and it returns a status where
 *     MicroPython would raise;
 *   - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end
 *     main with 0;
 *   - joystick.init() inside title() prints its line on the PSoC Edge
 *     kits only, so the program prints it when bento_game says it ran.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

enum { /* sizes and places, px */
    PADDLE_W = 14,
    PADDLE_H = 90,
    BALL_SIZE = 14,
    PADDLE_X = 20,
    SCORE_LEFT_OF_MIDDLE = 24,
    SCORE_Y = 12,
    RESULT_LEFT_OF_MIDDLE = 70,
    AI_DEAD_ZONE = 6
};
enum { /* the game */
    FPS = 60,
    WIN_SCORE = 7,
    SCORE_CHARS = 32 /* "%d : %d", any two int32_t, and the NUL */
};

static const bento_real_t ACCEL = (bento_real_t)1.5;
static const bento_real_t MAX_SPEED = (bento_real_t)12.0;
static const bento_real_t FRICTION = (bento_real_t)0.78;
static const bento_real_t SPEEDUP = (bento_real_t)0.35;
static const bento_real_t BALL_CAP = (bento_real_t)14.0;
static const bento_real_t SPIN = (bento_real_t)0.28;
static const bento_real_t SERVE_SPEED = (bento_real_t)6.2;
static const bento_real_t SERVE_VY = (bento_real_t)3.4;
static const bento_real_t AI_SPEED = (bento_real_t)4.2;
/* the contact angle: ball_vy per unit of hit_offset */
static const bento_real_t CONTACT_ANGLE = (bento_real_t)2.0;

typedef struct {
    bento_game_t game;
    bento_game_box_t player_paddle;
    bento_game_box_t ai_paddle;
    bento_game_box_t ball;
    bento_game_text_t score_text;
    int32_t paddle_start_y;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
    bento_real_t player_y;
    bento_real_t player_speed;
    bento_real_t ai_y;
    int32_t player_score;
    int32_t ai_score;
    bool game_over;
} pong_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern pong_t g_pong;
pong_t g_pong;

/* Python's min(a, b): a, unless b is smaller */
static bento_real_t real_min(bento_real_t first, bento_real_t second)
{
    return (second < first) ? second : first;
}

/* Python's max(a, b): a, unless b is larger */
static bento_real_t real_max(bento_real_t first, bento_real_t second)
{
    return (second > first) ? second : first;
}

/* held = speed up, released = slide to a stop, kept on the court */
static bento_status_t move_player(pong_t *pong,
                                  const bento_game_keys_t *keys)
{
    const bento_real_t lowest_y =
        (bento_real_t)(pong->game.height - PADDLE_H);

    if (keys->up && !keys->down) {
        pong->player_speed -= ACCEL;
    } else if (keys->down && !keys->up) {
        pong->player_speed += ACCEL;
    } else {
        pong->player_speed *= FRICTION;
    }
    pong->player_speed =
        real_max(-MAX_SPEED, real_min(MAX_SPEED, pong->player_speed));
    pong->player_y = real_max(
        (bento_real_t)0,
        real_min(lowest_y, pong->player_y + pong->player_speed));
    return bento_game_box_move_to(&pong->game, &pong->player_paddle,
                                  (bento_real_t)PADDLE_X,
                                  pong->player_y);
}

/* the ball flies and bounces off the top and the bottom */
static bento_status_t move_ball(pong_t *pong)
{
    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if ((pong->ball_y <= (bento_real_t)0) ||
        (pong->ball_y >=
         (bento_real_t)(pong->game.height - BALL_SIZE))) {
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
}

/* -1 at a paddle's top end .. 1 at its bottom end */
static bento_real_t hit_offset(const pong_t *pong,
                               bento_real_t paddle_y)
{
    const bento_real_t half_ball =
        (bento_real_t)BALL_SIZE / (bento_real_t)2;
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;

    return ((pong->ball_y + half_ball) - (paddle_y + half_paddle)) /
           half_paddle;
}

/* the AI paddle follows the ball: its middle aims at the ball */
static bento_status_t move_ai(pong_t *pong)
{
    const bento_real_t lowest_y =
        (bento_real_t)(pong->game.height - PADDLE_H);
    const bento_real_t target_y =
        pong->ball_y - ((bento_real_t)PADDLE_H / (bento_real_t)2);

    if (pong->ai_y < (target_y - (bento_real_t)AI_DEAD_ZONE)) {
        pong->ai_y += AI_SPEED;
    } else if (pong->ai_y > (target_y + (bento_real_t)AI_DEAD_ZONE)) {
        pong->ai_y -= AI_SPEED;
    } else {
        /* inside the dead zone: the paddle rests */
    }
    pong->ai_y =
        real_max((bento_real_t)0, real_min(lowest_y, pong->ai_y));
    return bento_game_box_move_to(&pong->game, &pong->ai_paddle,
                                  pong->ai_paddle.x, pong->ai_y);
}

/* our paddle adds the contact angle and its spin; the AI's mirror
 * adds the angle only */
static void hit_paddles(pong_t *pong)
{
    if ((pong->ball_vx < (bento_real_t)0) &&
        bento_game_hit(&pong->ball, &pong->player_paddle)) {
        const bento_real_t angle =
            hit_offset(pong, pong->player_y) * CONTACT_ANGLE;
        const bento_real_t spin = pong->player_speed * SPIN;
        const bento_real_t push = angle + spin;

        pong->ball_vx = real_min(-pong->ball_vx + SPEEDUP, BALL_CAP);
        pong->ball_vy += push;
        bento_game_sfx(&pong->game, "paddle");
    } else if ((pong->ball_vx > (bento_real_t)0) &&
               bento_game_hit(&pong->ball, &pong->ai_paddle)) {
        const bento_real_t angle =
            hit_offset(pong, pong->ai_y) * CONTACT_ANGLE;

        pong->ball_vx = -real_min(pong->ball_vx + SPEEDUP, BALL_CAP);
        pong->ball_vy += angle;
        bento_game_sfx(&pong->game, "paddle");
    } else {
        /* neither paddle */
    }
}

/* a new ball from the middle, towards direction (1 right, -1 left) */
static void serve(pong_t *pong, int32_t direction)
{
    pong->ball_x = (bento_real_t)pong->game.width / (bento_real_t)2;
    pong->ball_y = (bento_real_t)pong->game.height / (bento_real_t)2;
    pong->ball_vx = SERVE_SPEED * (bento_real_t)direction;
    pong->ball_vy = SERVE_VY;
}

static bento_status_t show_score(pong_t *pong)
{
    char text[SCORE_CHARS];

    (void)snprintf(text, sizeof(text), "%" PRId32 " : %" PRId32,
                   pong->player_score, pong->ai_score);
    return bento_game_text_set(&pong->game, &pong->score_text, text);
}

/* a ball out on one side scores for the other, then a new serve */
static bento_status_t score_points(pong_t *pong)
{
    bento_status_t status = BENTO_OK;

    if (pong->ball_x < (bento_real_t)0) {
        pong->ai_score += 1;
        status = show_score(pong);
        serve(pong, 1);
    } else if (pong->ball_x > (bento_real_t)pong->game.width) {
        pong->player_score += 1;
        status = show_score(pong);
        serve(pong, -1);
    } else {
        /* still in play */
    }
    return status;
}

/* first to WIN_SCORE: the sound and the result, then the game ends */
static bento_status_t check_winner(pong_t *pong, bool *out_stop)
{
    bento_status_t status = BENTO_OK;

    if ((pong->player_score >= WIN_SCORE) ||
        (pong->ai_score >= WIN_SCORE)) {
        bento_game_t *game = &pong->game;
        const bool player_won = pong->player_score > pong->ai_score;
        bento_game_text_t result;

        bento_game_text_init(&result);
        bento_game_sfx(game, player_won ? "win" : "lose");
        status = bento_game_text_create(
            game, &result, player_won ? "YOU WIN!" : "AI WINS",
            (game->width / 2) - RESULT_LEFT_OF_MIDDLE, game->height / 2,
            player_won ? BENTO_GAME_GREEN : BENTO_GAME_RED);
        pong->game_over = true;
        *out_stop = true;
    }
    return status;
}

/* one frame; Start or the end of the match stops the game */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_pong (file scope, C9) */
    bento_game_keys_init(&keys);
    /* keys() never fails: a silent pad reads as no key held */
    (void)bento_game_keys(&pong->game, &keys);
    if (keys.start || pong->game_over) {
        *out_stop = true;
    } else {
        status = move_player(pong, &keys);
        if (status == BENTO_OK) {
            status = move_ai(pong);
        }
        if (status == BENTO_OK) {
            status = move_ball(pong);
        }
        if (status == BENTO_OK) {
            hit_paddles(pong);
            status = score_points(pong);
        }
        if (status == BENTO_OK) {
            status = check_winner(pong, out_stop);
        }
    }
    return status;
}

/* Is this the reTerminal? Asked at run time by the name the board
 * gives, so one Emulator build serves both of its board models. A
 * board that cannot say keeps the build target's answer. */
static bool on_reterminal(void)
{
    bento_gpio_board_info_t info;
#if defined(BENTO_TARGET_RETERMINAL)
    bool answer = true;
#else
    bool answer = false;
#endif

    bento_gpio_board_info_init(&info);
    if ((bento_gpio_board_info(&info) == BENTO_OK) &&
        (info.name != NULL)) {
        answer = (strcmp(info.name, "Seeed reTerminal") == 0);
    }
    return answer;
}

/* modjoystick.c's joystick.init() prints this line on the PSoC Edge
 * kits; the reTerminal opens its pad too, but prints nothing */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* both paddles, the ball and the score, then the starting numbers */
static bento_status_t build_court(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    const int32_t mid_x = game->width / 2;
    const int32_t mid_y = game->height / 2;
    const bento_real_t start_y = (bento_real_t)pong->paddle_start_y;
    bento_status_t status = bento_game_box_create(
        game, &pong->player_paddle, (bento_real_t)PADDLE_X, start_y,
        (bento_real_t)PADDLE_W, (bento_real_t)PADDLE_H,
        BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ai_paddle,
            (bento_real_t)(game->width - PADDLE_X - PADDLE_W), start_y,
            (bento_real_t)PADDLE_W, (bento_real_t)PADDLE_H,
            BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ball, (bento_real_t)mid_x, (bento_real_t)mid_y,
            (bento_real_t)BALL_SIZE, (bento_real_t)BALL_SIZE,
            BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &pong->score_text, "0 : 0",
            mid_x - SCORE_LEFT_OF_MIDDLE, SCORE_Y, BENTO_GAME_WHITE);
    }
    pong->ball_x = (bento_real_t)mid_x;
    pong->ball_y = (bento_real_t)mid_y;
    pong->ball_vx = -SERVE_SPEED;
    pong->ball_vy = SERVE_VY;
    pong->player_y = start_y;
    pong->player_speed = (bento_real_t)0.0;
    pong->ai_y = start_y;
    return status;
}

int main(void)
{
    pong_t *pong = &g_pong;
    bool quit = false;
    bool finished = false;
    bento_status_t status = BENTO_OK;

    (void)memset(pong, 0, sizeof(*pong));
    bento_game_init(&pong->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&pong->player_paddle);
    bento_game_box_init(&pong->ai_paddle);
    bento_game_box_init(&pong->ball);
    bento_game_text_init(&pong->score_text);
    /* the paddle starts in the middle of game.HEIGHT before start() */
    pong->paddle_start_y = (pong->game.height / 2) - (PADDLE_H / 2);

    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_court(pong);
    }
    if ((status == BENTO_OK) && !quit) {
        status = bento_game_run(&pong->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
