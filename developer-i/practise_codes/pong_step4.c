/*
 * pong_step4.c -- สร้าง Pong #4 (คาบ 13): นับคะแนน + เสิร์ฟใหม่ +
 * first-to-N
 *
 * โครงเว้นให้เติม: หน้าเริ่ม "PONG" รอกด Start (Back = ออก) แล้ววาง
 * ไม้ตีสองฝั่ง ลูกบอล และป้ายคะแนน "0 : 0" จากนั้น game.run เรียก
 * on_each_frame() 60 ครั้งต่อวินาที ไม้ซ้ายขยับด้วย UP/DOWN แบบเร่งและ
 * ไถลหยุดเอง ลูกเด้งบน/ล่าง (เสียง "wall") และตีกลับได้จากไม้ทั้งสอง
 * ฝั่ง พร้อมเร่ง มุม และสปิน (เสียง "paddle") กด Start เพื่อจบลูป
 * ช่องที่นักเรียนต้องเติม (ไม้ AI ฝั่งขวาวิ่งตามลูก กับการนับคะแนน
 * เสิร์ฟใหม่ และจบเกม) ยังเป็น pass ไม้ขวาจึงไม่ขยับ ยังไม่มีใครได้
 * แต้ม และลูกที่หลุดก็วิ่งออกนอกจอไป
 *
 * A worksheet to fill in: the title screen "PONG" waits for Start
 * (Back quits), then both paddles, the ball and the score "0 : 0",
 * and the game loop calls on_each_frame() 60 times a second. UP and
 * DOWN speed the left paddle up, and it slides to a stop on release.
 * The ball bounces off the top and the bottom (the "wall" sound) and
 * off either paddle with speed-up, angle and spin (the "paddle"
 * sound). Start ends the loop. The student's blanks (the AI paddle on
 * the right that follows the ball, and the scoring with a new serve
 * and the end of the game) are still `pass`: the right paddle does
 * not move, nobody scores, and a missed ball leaves the screen.
 *
 * Twin of pong_step4.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/pong_step4.py),
 * from the library twin devi-pong-4 (py/devi-pong-4.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the module globals are one pong_t; it is 512 bytes or more, so it
 *     lives at file scope (C9) and the frame reads it there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - Python's float is bento_real_t, the board's MicroPython float,
 *     and a * b + c is never fused (FP_CONTRACT below); min() and max()
 *     are smaller_of() and larger_of(), which keep Python's choice on
 *     a tie;
 *   - serve(direction) is called only by the scoring blank, and
 *     AI_SPEED = 4.2 is read only by the AI blank: C warns about an
 *     unused function or constant, so both are left out;
 *   - PADDLE_START_Y is read from game.HEIGHT before title() starts the
 *     game, which is BENTO_GAME_HEIGHT; the middle of the screen is
 *     CENTER_X and CENTER_Y, integer enums, as the floor division
 *     there is;
 *   - `return False` from the frame is *out_stop = true;
 *   - title()'s Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* a * b + c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The paddles, the ball, the score and the game's pace. */
/* The middle of the screen: half of game.WIDTH and game.HEIGHT by
 * floor division; title()'s start() keeps the default 792 x 398. */
enum {
    CENTER_X = BENTO_GAME_WIDTH / 2,
    CENTER_Y = BENTO_GAME_HEIGHT / 2
};

enum {
    PADDLE_X = 20, /* the right paddle keeps the same gap to its edge */
    PADDLE_W = 14,
    PADDLE_H = 90,
    BALL_SIZE = 14,
    PADDLE_START_Y = (BENTO_GAME_HEIGHT / 2) - (PADDLE_H / 2),
    SCORE_HALF_W = 24, /* "0 : 0" sits about centred */
    SCORE_Y = 12,
    WIN_SCORE = 7, /* first-to-N, for the scoring blank */
    FPS = 60
};

typedef struct {
    bento_game_t game;
    bento_game_box_t player_paddle;
    bento_game_box_t ai_paddle;
    bento_game_box_t ball;
    bento_game_text_t score_text;
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

/* One Emulator build serves both board models, so ask the board for
 * its name; a board that cannot say keeps the build target's answer.
 */
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

/* The PSoC Edge kits' joystick.init() prints this line when start()
 * opens the pad; the reTerminal's prints nothing. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* Python's min(a, b): a unless b is smaller. */
static bento_real_t smaller_of(bento_real_t first, bento_real_t second)
{
    return (second < first) ? second : first;
}

/* Python's max(a, b): a unless b is larger. */
static bento_real_t larger_of(bento_real_t first, bento_real_t second)
{
    return (second > first) ? second : first;
}

/* ไม้ผู้เล่น (ซ้าย) UP/DOWN: เร่งเมื่อกดค้าง ไถลหยุดเองเมื่อปล่อย */
static bento_status_t move_player(pong_t *pong,
                                  const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.5;
    const bento_real_t max_speed = (bento_real_t)12.0;
    const bento_real_t friction = (bento_real_t)0.78;
    const bento_real_t lowest =
        (bento_real_t)(pong->game.height - PADDLE_H);
    bento_real_t moved_y;

    if (keys->up && !keys->down) {
        pong->player_speed -= accel;
    } else if (keys->down && !keys->up) {
        pong->player_speed += accel;
    } else {
        pong->player_speed *= friction;
    }
    pong->player_speed = larger_of(
        -max_speed, smaller_of(max_speed, pong->player_speed));
    moved_y = pong->player_y + pong->player_speed;
    pong->player_y =
        larger_of((bento_real_t)0, smaller_of(lowest, moved_y));
    return bento_game_box_move_to(&pong->game, &pong->player_paddle,
                                  (bento_real_t)PADDLE_X,
                                  pong->player_y);
}

/* ลูกบอล + เด้งบน/ล่าง */
static bento_status_t move_ball(pong_t *pong)
{
    const bento_real_t bottom =
        (bento_real_t)(pong->game.height - BALL_SIZE);

    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if ((pong->ball_y <= (bento_real_t)0) || (pong->ball_y >= bottom)) {
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
}

/* Where the ball met a paddle whose top is at paddle_y: about -1 at
 * the paddle's top end, 0 in its middle, 1 at its bottom end. */
static bento_real_t contact_offset(const pong_t *pong,
                                   bento_real_t paddle_y)
{
    const bento_real_t half_ball =
        (bento_real_t)BALL_SIZE / (bento_real_t)2;
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;

    return ((pong->ball_y + half_ball) - (paddle_y + half_paddle)) /
           half_paddle;
}

/* ตีไม้ซ้าย (เหมือน step3) หรือไม้ขวา (mirror ของไม้ซ้าย) */
static void hit_paddles(pong_t *pong)
{
    const bento_real_t speedup = (bento_real_t)0.35;
    const bento_real_t ball_cap = (bento_real_t)14.0;
    const bento_real_t spin = (bento_real_t)0.28;
    bento_real_t hit_offset;

    if ((pong->ball_vx < (bento_real_t)0) &&
        bento_game_hit(&pong->ball, &pong->player_paddle)) {
        hit_offset = contact_offset(pong, pong->player_y);
        pong->ball_vx = smaller_of(-pong->ball_vx + speedup, ball_cap);
        pong->ball_vy += (hit_offset * (bento_real_t)2) +
                         (pong->player_speed * spin);
        bento_game_sfx(&pong->game, "paddle");
    } else if ((pong->ball_vx > (bento_real_t)0) &&
               bento_game_hit(&pong->ball, &pong->ai_paddle)) {
        hit_offset = contact_offset(pong, pong->ai_y);
        pong->ball_vx = -smaller_of(pong->ball_vx + speedup, ball_cap);
        pong->ball_vy += hit_offset * (bento_real_t)2;
        bento_game_sfx(&pong->game, "paddle");
    } else {
        /* neither paddle met the ball */
    }
}

/* One frame: Start or the end of the game stops the loop; otherwise
 * the left paddle, the ball and the hits. The student's AI paddle and
 * scoring are still blank. */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_pong: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&pong->game, &keys);
    if (keys.start || pong->game_over) {
        *out_stop = true;
    } else {
        status = move_player(pong, &keys);
        if (status == BENTO_OK) {
            status = move_ball(pong);
        }
        if (status == BENTO_OK) {
            hit_paddles(pong);
        }
    }
    return status;
}

/* Both paddles, the ball and the score. */
static bento_status_t add_field(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    bento_status_t status = bento_game_box_create(
        game, &pong->player_paddle, (bento_real_t)PADDLE_X,
        (bento_real_t)PADDLE_START_Y, (bento_real_t)PADDLE_W,
        (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ai_paddle,
            (bento_real_t)(game->width - PADDLE_X - PADDLE_W),
            (bento_real_t)PADDLE_START_Y, (bento_real_t)PADDLE_W,
            (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ball, (bento_real_t)CENTER_X,
            (bento_real_t)CENTER_Y, (bento_real_t)BALL_SIZE,
            (bento_real_t)BALL_SIZE, BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &pong->score_text, "0 : 0",
            (int32_t)CENTER_X - (int32_t)SCORE_HALF_W, SCORE_Y,
            BENTO_GAME_WHITE);
    }
    return status;
}

/* The field, the match's state, then the game loop. */
static bento_status_t play(pong_t *pong)
{
    const bento_real_t serve_speed = (bento_real_t)6.2;
    const bento_real_t serve_vy = (bento_real_t)3.4;
    bool finished = false;
    bento_status_t status = add_field(pong);

    pong->ball_x = (bento_real_t)CENTER_X;
    pong->ball_y = (bento_real_t)CENTER_Y;
    pong->ball_vx = -serve_speed;
    pong->ball_vy = serve_vy;
    pong->player_y = (bento_real_t)PADDLE_START_Y;
    pong->player_speed = (bento_real_t)0.0;
    pong->ai_y = (bento_real_t)PADDLE_START_Y;
    pong->player_score = 0;
    pong->ai_score = 0;
    pong->game_over = false;
    if (status == BENTO_OK) {
        status = bento_game_run(&pong->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return status;
}

int main(void)
{
    pong_t *pong = &g_pong;
    bento_ui_t *ui = bento_runtime_get_ui();
    bool quit = false;
    bento_status_t status;

    (void)memset(pong, 0, sizeof(*pong));
    bento_game_init(&pong->game, ui, bento_runtime_is_running);
    bento_game_box_init(&pong->player_paddle);
    bento_game_box_init(&pong->ai_paddle);
    bento_game_box_init(&pong->ball);
    bento_game_text_init(&pong->score_text);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = play(pong);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
