/*
 * pong_step3.c -- Pong #3 (คาบ 13): ตีลูกกลับ (สะท้อน + เร่ง + มุม +
 * สปิน)
 *
 * โครงเริ่มต้นที่มีช่องให้เติม: หน้าเริ่ม "PONG" รอกด Start (Back = ออก)
 * แล้ววางไม้ตีฝั่งซ้าย ลูกบอล และข้อความหัวจอ จากนั้น game.run เรียก
 * on_each_frame() 60 ครั้งต่อวินาที ไม้ตีขยับด้วย UP/DOWN แบบเร่งและ
 * ไถลหยุดเอง ลูกบอลเด้งบน/ล่าง (เสียง "wall") และเด้งกำแพงฝั่งขวา
 * ช่องที่นักเรียนต้องเติม (ตีลูกกลับเมื่อชนไม้ พร้อมเร่ง มุม และสปิน)
 * ยังเป็น pass ลูกจึงทะลุไม้ออกซ้ายจอไป กด Back เพื่อออก
 *
 * A starting skeleton with a blank inside: the title screen "PONG"
 * waits for Start (Back quits), then the left paddle, the ball and a
 * caption, and the game loop calls on_each_frame() 60 times a second.
 * UP and DOWN speed the paddle up, and it slides to a stop on
 * release. The ball bounces off the top and the bottom (the "wall"
 * sound) and off the right side. The student's blank (hit the ball
 * back off the paddle, with speed-up, angle and spin) is still
 * `pass`, so the ball passes the paddle and leaves on the left. Back
 * ends the game.
 *
 * Twin of pong_step3.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/pong_step3.py),
 * from the library twin devi-pong-3 (py/devi-pong-3.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the module globals are one pong_t; it is 512 bytes or more, so it
 *     lives at file scope (C9) and the frame reads it there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - Python's float is bento_real_t, the board's MicroPython float,
 *     and a * b + c is never fused (FP_CONTRACT below); min() and max()
 *     are smaller_of() and larger_of(), which keep Python's choice on
 *     a tie;
 *   - SPEEDUP = 0.35, BALL_CAP = 14.0 and SPIN = 0.28 are read only by
 *     the blank, so C, which warns about an unread constant, leaves
 *     them out;
 *   - PADDLE_START_Y is read from game.HEIGHT before title() starts the
 *     game, which is BENTO_GAME_HEIGHT; the middle of the screen is
 *     CENTER_X and CENTER_Y, integer enums, as the floor division
 *     there is;
 *   - title()'s Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
 */
#include <stdbool.h>
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

/* The paddle, the ball, the caption and the game's pace. */
/* The middle of the screen: half of game.WIDTH and game.HEIGHT by
 * floor division; title()'s start() keeps the default 792 x 398. */
enum {
    CENTER_X = BENTO_GAME_WIDTH / 2,
    CENTER_Y = BENTO_GAME_HEIGHT / 2
};

enum {
    PADDLE_X = 20,
    PADDLE_W = 14,
    PADDLE_H = 90,
    BALL_SIZE = 14,
    PADDLE_START_Y = (BENTO_GAME_HEIGHT / 2) - (PADDLE_H / 2),
    CAPTION_X = 16,
    CAPTION_Y = 12,
    FPS = 60
};

typedef struct {
    bento_game_t game;
    bento_game_box_t player_paddle;
    bento_game_box_t ball;
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

/* ไม้ผู้เล่น: เร่งเมื่อกดค้าง ไถลหยุดเองเมื่อปล่อย และไม่หลุดสนาม */
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

/* One frame: the paddle, the ball, then the right wall. The student's
 * hit off the paddle is still blank. */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    const bento_real_t right =
        (bento_real_t)(pong->game.width - BALL_SIZE);
    bento_game_keys_t keys;
    bento_status_t status;

    /* never ends the game: Start/Back are handled by the run loop */
    *out_stop = false;
    (void)user; /* the state is g_pong: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&pong->game, &keys);
    status = move_player(pong, &keys);
    if (status == BENTO_OK) {
        status = move_ball(pong);
    }
    /* ฝั่งขวายังเป็นกำแพง (คาบหน้าทำคู่ต่อสู้ + คะแนน) */
    if ((status == BENTO_OK) && (pong->ball_x >= right)) {
        const bento_real_t speed = (pong->ball_vx < (bento_real_t)0)
                                       ? -pong->ball_vx
                                       : pong->ball_vx;

        pong->ball_vx = -speed;
    }
    return status;
}

/* The paddle, the ball and the caption. */
static bento_status_t add_field(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    bento_game_text_t caption;
    bento_status_t status = bento_game_box_create(
        game, &pong->player_paddle, (bento_real_t)PADDLE_X,
        (bento_real_t)PADDLE_START_Y, (bento_real_t)PADDLE_W,
        (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ball, (bento_real_t)CENTER_X,
            (bento_real_t)CENTER_Y, (bento_real_t)BALL_SIZE,
            (bento_real_t)BALL_SIZE, BENTO_GAME_GB_LIGHTEST);
    }
    bento_game_text_init(&caption);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &caption, "Hit the ball back with your paddle!",
            CAPTION_X, CAPTION_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The field, the ball's and the paddle's state, then the game loop. */
static bento_status_t play(pong_t *pong)
{
    /* เสิร์ฟมาทางเรา (vx ลบ = วิ่งมาทางซ้าย) */
    const bento_real_t serve_vx = (bento_real_t)-6.2;
    const bento_real_t serve_vy = (bento_real_t)3.4;
    bool finished = false;
    bento_status_t status = add_field(pong);

    pong->ball_x = (bento_real_t)CENTER_X;
    pong->ball_y = (bento_real_t)CENTER_Y;
    pong->ball_vx = serve_vx;
    pong->ball_vy = serve_vy;
    pong->player_y = (bento_real_t)PADDLE_START_Y;
    pong->player_speed = (bento_real_t)0.0;
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
    bento_game_box_init(&pong->ball);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = play(pong);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
