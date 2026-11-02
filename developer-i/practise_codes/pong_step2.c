/*
 * pong_step2.c -- สร้าง Pong #2 (คาบ 12): ไม้ตีฝั่งเราขยับ (เร่ง +
 * เสียดทาน)
 *
 * โครงเว้นบางส่วน: หน้าเริ่ม "PONG" รอกด Start (Back = ออก) แล้ววาง
 * ไม้ตีฝั่งซ้าย ลูกบอล และข้อความหัวจอ จากนั้น game.run เรียก
 * on_each_frame() 60 ครั้งต่อวินาที ลูกบอลวิ่งและเด้งทั้งสี่ด้าน
 * (ชนบน/ล่างมีเสียง "wall") ช่องที่นักเรียนต้องเติม (ไม้ตีเร่งเมื่อกด
 * UP/DOWN ไถลหยุดเองเมื่อปล่อย และไม่หลุดสนาม) ยังเป็น pass ไม้จึง
 * ยังไม่ขยับ กด Back เพื่อออก
 *
 * A worksheet with parts left blank: the title screen "PONG" waits
 * for Start (Back quits), then the left paddle, the ball and a
 * caption, and the game loop calls on_each_frame() 60 times a second.
 * The ball moves and bounces off all four sides (the "wall" sound at
 * the top and the bottom). The student's blank (the paddle speeds up
 * while UP or DOWN is held, slides to a stop on release and stays on
 * the field) is still `pass`, so the paddle does not move yet. Back
 * ends the game.
 *
 * Twin of pong_step2.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/pong_step2.py),
 * from the library twin devi-pong-2 (py/devi-pong-2.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the module globals are one pong_t; it is 512 bytes or more, so it
 *     lives at file scope (C9) and the frame reads it there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - Python's float is bento_real_t, the board's MicroPython float,
 *     and a * b + c is never fused (FP_CONTRACT below);
 *   - ACCEL = 1.5, MAX_SPEED = 12.0 and FRICTION = 0.78 are read only
 *     by the blank, so C, which warns about an unread constant, leaves
 *     them out; player_y and player_speed are set and never read;
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
    /* วางไม้กลางจอตอนเริ่ม */
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
    bento_real_t player_y; /* ตำแหน่ง + ความเร็วไม้ของเรา */
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

/* ลูกบอล (ยังเด้งทั้ง 4 ด้าน: คาบหน้าทำให้ตีกลับด้วยไม้) */
static void step_ball(pong_t *pong)
{
    const bento_real_t bottom =
        (bento_real_t)(pong->game.height - BALL_SIZE);
    const bento_real_t right =
        (bento_real_t)(pong->game.width - BALL_SIZE);

    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if ((pong->ball_y <= (bento_real_t)0) || (pong->ball_y >= bottom)) {
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    if ((pong->ball_x <= (bento_real_t)0) || (pong->ball_x >= right)) {
        pong->ball_vx = -pong->ball_vx;
    }
}

/* One frame. The student's paddle (accelerate, friction, clamp) is
 * still blank, so only the ball moves. */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;

    /* never ends the game: Start/Back are handled by the run loop */
    *out_stop = false;
    (void)user; /* the state is g_pong: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&pong->game, &keys);
    /* Back = ออก, Start = พัก: game.run() จัดการให้ */
    step_ball(pong);
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
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
            game, &caption, "Move your paddle: UP / DOWN", CAPTION_X,
            CAPTION_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The field, the ball's and the paddle's state, then the game loop. */
static bento_status_t play(pong_t *pong)
{
    const bento_real_t serve_vx = (bento_real_t)6.2;
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
