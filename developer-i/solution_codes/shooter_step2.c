/*
 * shooter_step2.c -- Shooter #1 (คาบ 10) step 2: บังคับยานซ้าย/ขวา
 * (เร่ง + ไถล)
 *
 * Twin of shooter_step2.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/shooter_step2.py)
 *
 * ยานขับได้และมีน้ำหนัก เหมือนเข็นรถเข็นในห้าง: กดซ้ายหรือขวาค้างไว้
 * ยิ่งเร็วขึ้น (เร่ง) ปล่อยมือแล้วไถลต่อและช้าลงเอง (แรงเสียดทาน)
 * ความเร็วมีเพดาน และชนขอบจอแล้วหยุด ปุ่ม -> ความเร็ว -> ตำแหน่ง -> จอ
 *
 * The ship drives and has weight, like a shopping trolley: hold left
 * or right and it speeds up (acceleration); let go and it glides on
 * and slows down by itself (friction). The speed has a ceiling and the
 * ship stops at the screen's edges. Keys, then speed, then position,
 * then the screen.
 *
 * C twin of the Example Explorer program devi-shooter-2
 * (py/devi-shooter-2.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - title()'s Back (SystemExit) and Stop end the program with 0;
 *   - joystick.init() prints its line on the kits only, so the
 *     program prints it there after title();
 *   - ship_x and ship_speed are bento_real_t, the width of each
 *     board's MicroPython float; move_to() truncates to whole pixels;
 *   - score and lives are kept as the original keeps them, although
 *     nothing reads them yet.
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
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t score;
    int32_t lives;
} shooter_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

static bento_real_t clamp(bento_real_t value, bento_real_t low,
                          bento_real_t high)
{
    const bento_real_t below_high = (value < high) ? value : high;

    return (below_high > low) ? below_high : low;
}

/* A held key speeds the ship up; with none it glides and slows down.
 * The same numbers as the full game (shooter_full.py:18). */
static void steer(shooter_t *shooter, const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t max_speed = (bento_real_t)13.0;
    /* gliding keeps 80 % of the speed each frame */
    const bento_real_t friction = (bento_real_t)0.80;

    if (keys->left) {
        shooter->ship_speed -= accel;
    } else if (keys->right) {
        shooter->ship_speed += accel;
    } else {
        shooter->ship_speed *= friction;
    }
    shooter->ship_speed =
        clamp(shooter->ship_speed, -max_speed, max_speed);
}

/* Moves the ship by its speed, stopping at the screen's edges. */
static bento_status_t move_ship(shooter_t *shooter)
{
    const bento_real_t right_edge =
        (bento_real_t)shooter->game.width - shooter->ship.w;

    shooter->ship_x = clamp(shooter->ship_x + shooter->ship_speed,
                            (bento_real_t)0, right_edge);
    return bento_game_box_move_to(&shooter->game, &shooter->ship,
                                  shooter->ship_x,
                                  (bento_real_t)SHIP_Y);
}

/* One frame; the engine handles Back (quit) and Start (pause). */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;
    *out_stop = false; /* driving never ends the game */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&shooter->game, &keys);
    if (status == BENTO_OK) {
        steer(shooter, &keys);
        status = move_ship(shooter);
    }
    return status;
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
    shooter->ship_speed = (bento_real_t)0.0;
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
