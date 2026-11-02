/*
 * shooter_step3.c -- Shooter #2 (คาบ 11): เลเซอร์จาก "ชุดกระสุน
 * หมุนเวียน" 6 นัด
 *
 * Twin of shooter_step3.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/shooter_step3.py)
 *
 * เทคนิคของคาบ: Object Pool สร้างกระสุน 6 นัดครั้งเดียวก่อนเกมเริ่ม
 * จอดรอเหนือจอ แล้วยืม-คืนใช้ซ้ำ ไม่สร้างใหม่ทุกเฟรม (จอมีได้ไม่เกิน
 * 32 ชิ้น) กด A หรือขึ้นเพื่อยิง กระสุนบินขึ้น 9 px ต่อเฟรม พ้นเส้น
 * y = -20 แล้วถือว่าว่าง (คืน) ยานขับซ้ายขวาแบบมีน้ำหนักเหมือน step 2
 *
 * The lesson's technique is an object pool: six bullets are made once
 * before the game starts and parked above the screen, then borrowed
 * and returned again and again; nothing is made inside a frame (the
 * screen holds 32 widgets at most). Press A or up to fire: a bullet
 * flies up 9 px a frame and is free again (returned) once it passes
 * y = -20. The ship drives left and right with weight, as in step 2.
 *
 * C twin of the Example Explorer program devi-shooter-3
 * (py/devi-shooter-3.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - title()'s Back (SystemExit) and Stop end the program with 0;
 *   - joystick.init() prints its line on the kits only, so the
 *     program prints it there after title();
 *   - find_free_bullet() answers a pointer, NULL for None;
 *   - positions and speeds are bento_real_t, the width of each
 *     board's MicroPython float; move_to() truncates to whole pixels;
 *   - score and lives are kept as the original keeps them, for
 *     lesson 12, although nothing reads them yet.
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

/* The bullet pool: a bullet is free while it waits above y = -20. */
enum {
    MAX_BULLETS = 6, /* the full game uses 4 (shooter_full.py:30) */
    BULLET_W = 6,
    BULLET_H = 14,
    PARK_Y = -50,
    FREE_BELOW_Y = -20,
    MUZZLE_Y = 340,     /* a shot leaves the ship's nose */
    BULLET_STEP_PX = 9, /* up this much each frame */
    FIRE_COOLDOWN = 8   /* frames: 30 / 8, about 3.75 shots a second */
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
    bento_game_box_t bullets[MAX_BULLETS];
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t fire_cooldown;
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

/* The first bullet waiting above the screen, or NULL when all six
 * fly: this frame cannot fire. */
static bento_game_box_t *find_free_bullet(shooter_t *shooter)
{
    bento_game_box_t *free_bullet = NULL;
    int32_t i;

    for (i = 0; (i < MAX_BULLETS) && (free_bullet == NULL); i++) {
        if (shooter->bullets[i].y < (bento_real_t)FREE_BELOW_Y) {
            free_bullet = &shooter->bullets[i];
        }
    }
    return free_bullet;
}

/* Borrows a free bullet and sends it off from the ship's nose. */
static bento_status_t fire(shooter_t *shooter)
{
    /* floor division of ints: the ship's and the bullet's middles */
    const int32_t half_ship = SHIP_W / 2;
    const int32_t half_bullet = BULLET_W / 2;
    bento_game_box_t *bullet = find_free_bullet(shooter);
    bento_status_t status = BENTO_OK;

    if (bullet != NULL) {
        status = bento_game_box_show(&shooter->game, bullet);
        if (status == BENTO_OK) {
            const bento_real_t muzzle_x =
                (shooter->ship_x + (bento_real_t)half_ship) -
                (bento_real_t)half_bullet;

            status =
                bento_game_box_move_to(&shooter->game, bullet, muzzle_x,
                                       (bento_real_t)MUZZLE_Y);
        }
        if (status == BENTO_OK) {
            bento_game_sfx(&shooter->game, "fire");
            shooter->fire_cooldown = FIRE_COOLDOWN;
        }
    }
    return status;
}

/* Counts the cooldown down, then fires on A or up. */
static bento_status_t handle_fire(shooter_t *shooter,
                                  const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    shooter->fire_cooldown =
        (shooter->fire_cooldown > 1) ? (shooter->fire_cooldown - 1) : 0;
    if ((keys->a || keys->up) && (shooter->fire_cooldown == 0)) {
        status = fire(shooter);
    }
    return status;
}

/* A flying bullet goes up; past y = -20 it is free again, and hidden
 * from the player too. */
static bento_status_t fly_bullet(shooter_t *shooter,
                                 bento_game_box_t *bullet)
{
    bento_status_t status = bento_game_box_move_to(
        &shooter->game, bullet, bullet->x,
        bullet->y - (bento_real_t)BULLET_STEP_PX);

    if ((status == BENTO_OK) &&
        (bullet->y < (bento_real_t)FREE_BELOW_Y)) {
        status = bento_game_box_hide(&shooter->game, bullet);
    }
    return status;
}

static bento_status_t move_bullets(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MAX_BULLETS) && (status == BENTO_OK); i++) {
        if (shooter->bullets[i].y >= (bento_real_t)FREE_BELOW_Y) {
            status = fly_bullet(shooter, &shooter->bullets[i]);
        }
    }
    return status;
}

/* One frame; the engine handles Back (quit) and Start (pause). */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;
    *out_stop = false; /* this step never ends the game */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&shooter->game, &keys);
    if (status == BENTO_OK) {
        steer(shooter, &keys);
        status = move_ship(shooter);
    }
    if (status == BENTO_OK) {
        status = handle_fire(shooter, &keys);
    }
    if (status == BENTO_OK) {
        status = move_bullets(shooter);
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
    shooter->fire_cooldown = 0;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&shooter->game, &shooter->hud,
                                        "Score: 0   Lives: 3", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The pool, made once: six bullets parked at y = -50, then hidden
 * (the y, not the hiding, is what makes a bullet free). */
static bento_status_t build_bullets(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MAX_BULLETS) && (status == BENTO_OK); i++) {
        bento_game_box_init(&shooter->bullets[i]);
        status = bento_game_box_create(
            &shooter->game, &shooter->bullets[i], (bento_real_t)0,
            (bento_real_t)PARK_Y, (bento_real_t)BULLET_W,
            (bento_real_t)BULLET_H, BENTO_GAME_CYAN);
    }
    for (i = 0; (i < MAX_BULLETS) && (status == BENTO_OK); i++) {
        status =
            bento_game_box_hide(&shooter->game, &shooter->bullets[i]);
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
        status = build_bullets(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
