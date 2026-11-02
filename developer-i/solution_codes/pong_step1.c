/*
 * pong_step1.c -- สร้าง Pong #1 (คาบ 5): ลูกบอลเด้งกำแพงบน/ล่าง (ยังไม่มีไม้ตี)
 *
 * ปล่อยลูกบอลหนึ่งลูกให้วิ่งในจอ ชนกำแพงบน/ล่างก็เด้งกลับพร้อมเสียง "wall"
 * เหมือนลูกแก้วเด้งในกล่อง ซึ่งเป็นหัวใจของฟิสิกส์เกม ยังไม่มีไม้ตี
 * ลูกจึงเด้งซ้าย/ขวาไว้ก่อนเพื่อให้อยู่ในจอ กด Start เพื่อเริ่ม
 * Back = ออก · Start = พักเกม
 *
 * Pong step 1: one ball runs across the screen at 60 frames per second.
 * Each frame adds the velocity to the position (integrate), and the ball
 * bounces off the top and bottom walls with the "wall" sound (reflect).
 * There are no paddles yet, so the ball also turns back at the left and
 * right edges to stay on the screen.
 *
 * Twin of pong_step1.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/pong_step1.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the ball's position and velocity are bento_real_t (float on the
 *     kits, double on the reTerminal: each board's MicroPython float);
 *   - on_each_frame() is the function bento_game_run calls each frame; it reads the
 *     file-scope state, not its void * argument;
 *   - a bentogame call that raises in MicroPython returns its status here;
 *     title()'s Back (SystemExit) and the IDE's Stop end the program with 0.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

enum {
    BALL_SIZE = 14, /* ลูกบอลกว้าง/สูงกี่พิกเซล */
    LABEL_X = 16,
    LABEL_Y = 12,
    FPS = 60
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ball;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx; /* บวก = ไปขวา, ลบ = ไปซ้าย */
    bento_real_t ball_vy; /* บวก = ลง, ลบ = ขึ้น */
} pong_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern pong_t g_pong;
pong_t g_pong;

/* The reTerminal's joystick prints nothing when it opens; the kits print a
 * line. */
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

/* title() calls start(), whose joystick.init() prints this line on the
 * kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* One frame: move the ball one step, then bounce it off the walls. */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    const bento_real_t bottom =
        (bento_real_t)(pong->game.height - BALL_SIZE);
    const bento_real_t right =
        (bento_real_t)(pong->game.width - BALL_SIZE);

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    *out_stop = false; /* the ball runs until Back */
    /* บวกความเร็วเข้าตำแหน่งคือการขยับ */
    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if (pong->ball_y <= (bento_real_t)0) { /* ชนขอบบน -> กลับทิศแนวตั้ง */
        pong->ball_y = (bento_real_t)0;
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    if (pong->ball_y >= bottom) { /* ชนขอบล่าง */
        pong->ball_y = bottom;
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    /* ยังไม่มีไม้ตี -> เด้งซ้าย/ขวาไว้ก่อน เพื่อให้ลูกอยู่ในจอ */
    if ((pong->ball_x <= (bento_real_t)0) || (pong->ball_x >= right)) {
        pong->ball_vx = -pong->ball_vx;
    }
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
}

/* The ball, the caption and the ball's starting state. */
static bento_status_t build_screen(pong_t *pong)
{
    const bento_real_t start_vx = (bento_real_t)6.2;
    const bento_real_t start_vy = (bento_real_t)3.4;
    const int32_t middle_x = pong->game.width / 2;
    const int32_t middle_y = pong->game.height / 2;
    bento_game_text_t caption;
    bento_status_t status;

    bento_game_text_init(&caption);
    status = bento_game_box_create(
        &pong->game, &pong->ball, (bento_real_t)middle_x,
        (bento_real_t)middle_y, (bento_real_t)BALL_SIZE,
        (bento_real_t)BALL_SIZE, BENTO_GAME_GB_LIGHTEST);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            &pong->game, &caption, "Pong - ball bounces the walls",
            LABEL_X, LABEL_Y, BENTO_GAME_WHITE);
    }
    /* ตั้งตำแหน่งเริ่มต้นกลางจอ + ความเร็ว x/y */
    pong->ball_x = (bento_real_t)middle_x;
    pong->ball_y = (bento_real_t)middle_y;
    pong->ball_vx = start_vx;
    pong->ball_vy = start_vy;
    return status;
}

int main(void)
{
    pong_t *pong = &g_pong;
    bool quit = false;
    bento_status_t status;

    (void)memset(pong, 0, sizeof(*pong));
    bento_game_init(&pong->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&pong->ball);
    /* หน้าเริ่ม (Start=เล่น Back=ออก) + ล้างจอ */
    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(pong);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        /* เรียก on_each_frame() 60 ครั้ง/วินาที */
        status = bento_game_run(&pong->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
