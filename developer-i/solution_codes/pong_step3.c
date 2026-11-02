/*
 * pong_step3.c -- สร้าง Pong #3 (คาบ 13): ตีลูกกลับ (สะท้อน + เร่ง + มุม + สปิน)
 *
 * Twin of pong_step3.py
 * (embedded-systems-for-game_console_developer/developer-i/solution_codes/pong_step3.py).
 *
 * หน้าเริ่ม PONG รอกด Start จากนั้นไม้ตีซ้ายของเราบังคับด้วย UP/DOWN แบบเร่งและไถล
 * (เหมือน step2) ลูกเสิร์ฟมาทางเรา ตีโดนไม้แล้วลูกเด้งกลับและเร็วขึ้นทีละนิด (มีเพดาน)
 * โดนปลายไม้ได้มุมชัน และถ้าไม้กำลังเคลื่อนตอนตีก็ใส่สปินให้ลูก (เสียง paddle) ฝั่งขวา
 * ยังเป็นกำแพง ลูกที่หลุดซ้ายจะวิ่งต่อไปเรื่อย ๆ กด Back เพื่อออก
 *
 * The PONG title waits for Start. Our left paddle moves with UP/DOWN,
 * speeding up and sliding as in step 2. The ball is served towards us;
 * a paddle hit sends it back a little faster (up to a cap), at a steeper
 * angle near the paddle's ends, with spin from a moving paddle, and the
 * "paddle" sound. The right side is still a wall; a ball missed on the
 * left keeps flying. Back quits.
 *
 * C twin of the Example Explorer program devi-pong-3
 * (py/devi-pong-3.py, the practise version of this solution).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the module's globals are one pong_t at file scope (too big for
 *     the stack), set again at the start of main;
 *   - floats are bento_real_t, each board's MicroPython float, and
 *     max()/min()/abs() are helpers that keep Python's results;
 *   - each `+=` of a product is two statements, so no board fuses the
 *     multiply and the add MicroPython rounds apart;
 *   - on_each_frame() is the update bento_game_run calls each frame,
 *     returning a status where MicroPython would raise;
 *   - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end
 *     main with 0;
 *   - joystick.init() inside title() prints its line on the PSoC Edge
 *     kits only, so the program prints it when bento_game says it ran.
 */
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
    CAPTION_X = 16,
    CAPTION_Y = 12
};
enum { /* frames per second */
    FPS = 60
};

static const bento_real_t ACCEL = (bento_real_t)1.5;
static const bento_real_t MAX_SPEED = (bento_real_t)12.0;
static const bento_real_t FRICTION = (bento_real_t)0.78;
static const bento_real_t START_VX = (bento_real_t)-6.2;
static const bento_real_t START_VY = (bento_real_t)3.4;
static const bento_real_t SPEEDUP = (bento_real_t)0.35;
static const bento_real_t BALL_CAP = (bento_real_t)14.0;
static const bento_real_t SPIN = (bento_real_t)0.28;

typedef struct {
    bento_game_t game;
    bento_game_box_t player_paddle;
    bento_game_box_t ball;
    int32_t paddle_start_y;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
    bento_real_t player_y;
    bento_real_t player_speed;
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

/* Python's abs() of a float */
static bento_real_t real_abs(bento_real_t value)
{
    return (value < (bento_real_t)0) ? -value : value;
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

/* a ball coming at our paddle goes back faster (capped), at the angle
 * of the contact point, with the paddle's spin */
static void hit_back(pong_t *pong)
{
    /* the contact angle: ball_vy per unit of hit_offset */
    const bento_real_t contact_angle = (bento_real_t)2.0;
    const bento_real_t half_ball =
        (bento_real_t)BALL_SIZE / (bento_real_t)2;
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;

    if ((pong->ball_vx < (bento_real_t)0) &&
        bento_game_hit(&pong->ball, &pong->player_paddle)) {
        /* -1 at the paddle's top end .. 1 at its bottom end */
        const bento_real_t hit_offset =
            ((pong->ball_y + half_ball) -
             (pong->player_y + half_paddle)) /
            half_paddle;
        const bento_real_t angle = hit_offset * contact_angle;
        const bento_real_t spin = pong->player_speed * SPIN;

        pong->ball_vx = real_min(-pong->ball_vx + SPEEDUP, BALL_CAP);
        pong->ball_vy += angle;
        pong->ball_vy += spin;
        bento_game_sfx(&pong->game, "paddle");
    }
}

/* one frame; only Back (handled by run) ends the game */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    *out_stop = false; /* the original never returns False */
    (void)user;        /* the state is g_pong (file scope, C9) */
    bento_game_keys_init(&keys);
    /* keys() never fails: a silent pad reads as no key held */
    (void)bento_game_keys(&pong->game, &keys);
    status = move_player(pong, &keys);
    if (status == BENTO_OK) {
        status = move_ball(pong);
    }
    if (status == BENTO_OK) {
        hit_back(pong);
        /* the right side is still a wall */
        if (pong->ball_x >=
            (bento_real_t)(pong->game.width - BALL_SIZE)) {
            pong->ball_vx = -real_abs(pong->ball_vx);
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

/* the paddle, the ball and the caption, then the starting numbers */
static bento_status_t build_court(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    const int32_t mid_x = game->width / 2;
    const int32_t mid_y = game->height / 2;
    bento_game_text_t caption;
    bento_status_t status = bento_game_box_create(
        game, &pong->player_paddle, (bento_real_t)PADDLE_X,
        (bento_real_t)pong->paddle_start_y, (bento_real_t)PADDLE_W,
        (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);

    bento_game_text_init(&caption);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ball, (bento_real_t)mid_x, (bento_real_t)mid_y,
            (bento_real_t)BALL_SIZE, (bento_real_t)BALL_SIZE,
            BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &caption, "Hit the ball back with your paddle!",
            CAPTION_X, CAPTION_Y, BENTO_GAME_WHITE);
    }
    pong->ball_x = (bento_real_t)mid_x;
    pong->ball_y = (bento_real_t)mid_y;
    pong->ball_vx = START_VX; /* served towards us */
    pong->ball_vy = START_VY;
    pong->player_y = (bento_real_t)pong->paddle_start_y;
    pong->player_speed = (bento_real_t)0.0;
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
    bento_game_box_init(&pong->ball);
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
