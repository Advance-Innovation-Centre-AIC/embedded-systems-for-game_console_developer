/*
 * shooter_step1.c -- Shooter #1 (คาบ 10) step 1: วางยาน + ป้าย HUD
 *
 * Twin of shooter_step1.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/shooter_step1.py)
 *
 * step แรกของเกมยิง: หน้าชื่อเกม SHOOTER (Start = เล่น, Back = ออก)
 * แล้ววางยานสีเขียวกว้าง 62 สูง 24 กลางจอใกล้ขอบล่าง กับป้าย HUD
 * คะแนนและชีวิตที่มุมซ้ายบน ยังเป็นภาพนิ่ง: ยังไม่อ่านปุ่ม ยังไม่ยิง
 * เอนจินจัดการ Back = ออก และ Start = พักเกม ให้เอง
 *
 * The shooter's first step: the SHOOTER title screen (Start plays,
 * Back quits), then the green ship, 62 wide and 24 high, in the middle
 * near the bottom, and the HUD with score and lives at the top left.
 * Still a picture: no keys read, no shots yet. The engine handles
 * Back (quit) and Start (pause) by itself.
 *
 * C twin of the Example Explorer program devi-shooter-1
 * (py/devi-shooter-1.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - title()'s Back (SystemExit) and Stop end the program with 0;
 *   - joystick.init() prints its line on the kits only, so the
 *     program prints it there after title();
 *   - ship_x, score and lives are kept as the original keeps them,
 *     for step 2, although nothing reads them yet.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The ship: centred, (792 - 62) / 2 = 365, near the bottom edge. */
enum {
    SHIP_X = 365,
    SHIP_Y = 352,
    SHIP_W = 62,
    SHIP_H = 24
};

/* The HUD at the top left, and the game's numbers. */
enum {
    HUD_X = 10,
    HUD_Y = 8,
    START_LIVES = 3,
    FPS = 30
};

/* What the game keeps between frames. */
typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_real_t ship_x; /* kept as a real, so step 2 can add a speed */
    int32_t score;
    int32_t lives;
} shooter_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

/* A still picture: nothing moves yet (step 2 makes the ship move). */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    (void)user;
    *out_stop = false; /* a still picture never ends the game */
    return BENTO_OK;
}

/* The reTerminal's joystick prints nothing when it opens; the kits
 * print a line. One Emulator build serves both, so ask the board. */
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

/* title() calls start(), whose joystick.init() prints this line on
 * the kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* The player's ship and the HUD. */
static bento_status_t build_field(shooter_t *shooter)
{
    bento_status_t status = bento_game_box_create(
        &shooter->game, &shooter->ship, (bento_real_t)SHIP_X,
        (bento_real_t)SHIP_Y, (bento_real_t)SHIP_W,
        (bento_real_t)SHIP_H, BENTO_GAME_GREEN);

    shooter->ship_x = (bento_real_t)SHIP_X;
    shooter->score = 0;
    shooter->lives = START_LIVES;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&shooter->game, &shooter->hud,
                                        "Score: 0   Lives: 3", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bento_status_t status;

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&shooter->ship);
    bento_game_text_init(&shooter->hud);
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    report_joystick(&shooter->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_field(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
