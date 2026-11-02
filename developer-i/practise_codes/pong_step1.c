/*
 * pong_step1.c -- สร้าง Pong #1 (คาบ 12): ลูกบอลเด้งกำแพงบน/ล่าง
 *
 * โครงเว้น 30%: หน้าเริ่ม "PONG" รอกด Start (Back = ออก) แล้ววาง
 * ลูกบอล 14 x 14 กลางจอกับข้อความหัวจอ จากนั้น game.run เรียก
 * on_each_frame() 60 ครั้งต่อวินาที ช่องที่นักเรียนต้องเติม (ค่าเริ่ม
 * ของลูก การขยับ และการเด้งกำแพง) ยังเป็นค่า 0.0 กับ pass ทุกเฟรม
 * จึงวางลูกไว้ที่ (0, 0) มุมซ้ายบน กด Back เพื่อออก
 *
 * A worksheet with 30% left blank: the title screen "PONG" waits for
 * Start (Back quits), then a 14 x 14 ball in the middle of the screen
 * and a caption, and the game loop calls on_each_frame() 60 times a
 * second. The student's blanks (the ball's start, its step and the
 * wall bounce) are still 0.0 and `pass`, so every frame puts the ball
 * at (0, 0), the top left corner. Back ends the game.
 *
 * Twin of pong_step1.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/pong_step1.py),
 * from the library twin devi-pong-1 (py/devi-pong-1.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the module globals (the game, the ball box, its position and
 *     speed) are one pong_t; it is 512 bytes or more, so it lives at
 *     file scope (C9) and the frame reads it there, never through a
 *     cast of run()'s user pointer (MISRA 11.5);
 *   - Python's float is bento_real_t, the board's MicroPython float;
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

/* The ball, the caption and the game's pace. */
/* The middle of the screen: half of game.WIDTH and game.HEIGHT by
 * floor division; title()'s start() keeps the default 792 x 398. */
enum {
    CENTER_X = BENTO_GAME_WIDTH / 2,
    CENTER_Y = BENTO_GAME_HEIGHT / 2
};

enum {
    BALL_SIZE = 14, /* ลูกบอลกว้าง/สูงกี่พิกเซล */
    CAPTION_X = 16,
    CAPTION_Y = 12,
    FPS = 60
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ball;
    /* สถานะลูกบอล: ตำแหน่ง + ความเร็วต่อเฟรม (งานของคุณ) */
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
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

/* One frame. The student's step and bounce are still blank, so the
 * frame reads the pad and draws the ball where its state says. */
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
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
}

/* The ball and the caption, the ball's state, then the game loop. */
static bento_status_t play(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    bento_game_text_t caption;
    bool finished = false;
    bento_status_t status = bento_game_box_create(
        game, &pong->ball, (bento_real_t)CENTER_X,
        (bento_real_t)CENTER_Y, (bento_real_t)BALL_SIZE,
        (bento_real_t)BALL_SIZE, BENTO_GAME_GB_LIGHTEST);

    bento_game_text_init(&caption);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &caption, "Pong - ball bounces the walls", CAPTION_X,
            CAPTION_Y, BENTO_GAME_WHITE);
    }
    /* งานของคุณ: เริ่มกลางจอ และใส่ความเร็วต่อเฟรม (ยังเป็น 0.0) */
    pong->ball_x = (bento_real_t)0.0;
    pong->ball_y = (bento_real_t)0.0;
    pong->ball_vx = (bento_real_t)0.0;
    pong->ball_vy = (bento_real_t)0.0;
    if (status == BENTO_OK) {
        status =
            bento_game_run(game, on_each_frame, NULL, FPS, &finished);
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
    bento_game_box_init(&pong->ball);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = play(pong);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
