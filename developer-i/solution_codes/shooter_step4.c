/*
 * shooter_step4.c -- Shooter #3 (คาบ 12): ศัตรู + ชนแบบกล่อง + คะแนน
 * + ชีวิต = เกมที่เล่นจบรอบได้
 *
 * Twin of shooter_step4.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/shooter_step4.py)
 *
 * เกมเล่นจบได้จริง: ศัตรู 6 ตัวตกจากด้านบนด้วยความเร็วสุ่ม ยิงโดน =
 * ได้คะแนน หลุดถึงพื้น = เสียชีวิต ชีวิตหมด = GAME OVER ศัตรูวนกลับ
 * ขึ้นบนทุกครั้งที่หลุดพื้นหรือโดนยิง กระสุนยังเป็นชุดหมุนเวียน 6 นัด
 * ชนแบบกล่องด้วย game.hit และป้าย HUD เปลี่ยนทุกครั้งที่ตัวเลขเปลี่ยน
 *
 * A game you can finish: six enemies fall from the top at random
 * speeds. A hit scores a point, an enemy that reaches the ground costs
 * a life, and with no lives left it is GAME OVER. An enemy goes back
 * to the top each time it lands or is shot. The bullets are still the
 * six-shot pool; collisions are box tests (game.hit), and the HUD
 * changes whenever a number does.
 *
 * C twin of the Example Explorer program devi-shooter-4
 * (py/devi-shooter-4.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - title()'s Back (SystemExit) and Stop end the program with 0;
 *   - joystick.init() prints its line on the kits only, so the
 *     program prints it there after title();
 *   - random is bento_random, the same generator and seed as the
 *     board's MicroPython, drawn in the same order;
 *   - find_free_bullet() answers a pointer, NULL for None;
 *   - positions and speeds are bento_real_t, the width of each
 *     board's MicroPython float; move_to() truncates to whole pixels;
 *   - on_frame's `return False` sets *out_stop.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
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

/* The enemies: six that always fall, each back at the top when it
 * lands or is shot. */
enum {
    MAX_ENEMIES = 6,
    ENEMY_W = 30,
    ENEMY_H = 24,
    START_HIGH_MIN = 60, /* how far above the screen they start */
    START_HIGH_MAX = 700,
    RESPAWN_HIGH_MIN = 100,
    RESPAWN_HIGH_MAX = 500,
    ENEMY_COLOR_COUNT = 3
};

/* The HUD at the top left, GAME OVER, and the game's numbers. */
enum {
    HUD_X = 10,
    HUD_Y = 8,
    HUD_BYTES = 48, /* "Score:   Lives: " and two int32_t, NUL */
    GAME_OVER_X = 320,
    GAME_OVER_Y = 180,
    START_LIVES = 3,
    FPS = 30
};

static const uint32_t ENEMY_COLORS[ENEMY_COLOR_COUNT] = {
    BENTO_GAME_RED, BENTO_GAME_ORANGE, BENTO_GAME_PINK};

/* What the game keeps between frames. */
typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_game_text_t game_over;
    bento_game_box_t bullets[MAX_BULLETS];
    bento_game_box_t enemies[MAX_ENEMIES];
    bento_real_t enemy_speed[MAX_ENEMIES];
    bento_random_t random;
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

/* One of the three enemy colours, at random. */
static bento_status_t random_color(shooter_t *shooter, uint32_t *color)
{
    uint32_t index = 0U;
    const bento_status_t status = bento_random_choice(
        &shooter->random, ENEMY_COLOR_COUNT, &index);

    *color = ENEMY_COLORS[index];
    return status;
}

/* A random place above the screen: x inside it, y `low` to `high`
 * pixels above its top. */
static bento_status_t random_start(shooter_t *shooter, int32_t low,
                                   int32_t high, int32_t *x, int32_t *y)
{
    int32_t height = 0;
    bento_status_t status = bento_random_randint(
        &shooter->random, 0, shooter->game.width - ENEMY_W, x);

    if (status == BENTO_OK) {
        status =
            bento_random_randint(&shooter->random, low, high, &height);
    }
    *y = -height;
    return status;
}

/* One enemy, made once, somewhere above the screen. */
static bento_status_t add_enemy(shooter_t *shooter, int32_t index)
{
    int32_t x = 0;
    int32_t y = 0;
    uint32_t color = 0U;
    bento_status_t status =
        random_start(shooter, START_HIGH_MIN, START_HIGH_MAX, &x, &y);

    if (status == BENTO_OK) {
        status = random_color(shooter, &color);
    }
    if (status == BENTO_OK) {
        bento_game_box_init(&shooter->enemies[index]);
        status = bento_game_box_create(
            &shooter->game, &shooter->enemies[index], (bento_real_t)x,
            (bento_real_t)y, (bento_real_t)ENEMY_W,
            (bento_real_t)ENEMY_H, color);
    }
    return status;
}

/* Six enemies, then their speeds: slow enough to shoot in time. */
static bento_status_t build_enemies(shooter_t *shooter)
{
    const bento_random_real_t slowest = (bento_random_real_t)1.5;
    const bento_random_real_t fastest = (bento_random_real_t)2.5;
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MAX_ENEMIES) && (status == BENTO_OK); i++) {
        status = add_enemy(shooter, i);
    }
    for (i = 0; (i < MAX_ENEMIES) && (status == BENTO_OK); i++) {
        bento_random_real_t speed = (bento_random_real_t)0;

        status = bento_random_uniform(&shooter->random, slowest,
                                      fastest, &speed);
        shooter->enemy_speed[i] = (bento_real_t)speed;
    }
    return status;
}

/* Back to the top: a random x, above the screen, a random colour. */
static bento_status_t respawn_enemy(shooter_t *shooter, int32_t index)
{
    int32_t x = 0;
    int32_t y = 0;
    uint32_t color = 0U;
    bento_status_t status = random_start(shooter, RESPAWN_HIGH_MIN,
                                         RESPAWN_HIGH_MAX, &x, &y);

    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &shooter->game, &shooter->enemies[index], (bento_real_t)x,
            (bento_real_t)y);
    }
    if (status == BENTO_OK) {
        status = random_color(shooter, &color);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &shooter->game, &shooter->enemies[index], color);
    }
    return status;
}

/* The HUD changes whenever the score or the lives do. */
static bento_status_t show_hud(shooter_t *shooter)
{
    char text[HUD_BYTES];

    (void)snprintf(text, sizeof(text),
                   "Score: %" PRId32 "   Lives: %" PRId32,
                   shooter->score, shooter->lives);
    return bento_game_text_set(&shooter->game, &shooter->hud, text);
}

/* No lives left: the sound, GAME OVER in red, and the game ends. */
static bento_status_t end_game(shooter_t *shooter, bool *out_stop)
{
    bento_game_sfx(&shooter->game, "gameover");
    *out_stop = true;
    return bento_game_text_create(&shooter->game, &shooter->game_over,
                                  "GAME OVER", GAME_OVER_X, GAME_OVER_Y,
                                  BENTO_GAME_RED);
}

/* An enemy reached the ground: one life less, and it goes back up. */
static bento_status_t enemy_landed(shooter_t *shooter, int32_t index,
                                   bool *out_stop)
{
    bento_status_t status;

    shooter->lives -= 1;
    status = show_hud(shooter);
    if (status == BENTO_OK) {
        status = respawn_enemy(shooter, index);
    }
    if ((status == BENTO_OK) && (shooter->lives <= 0)) {
        status = end_game(shooter, out_stop);
    }
    return status;
}

/* A flying bullet hit the enemy: a point, the bullet parks again
 * (free) and the enemy goes back up. */
static bento_status_t score_hit(shooter_t *shooter, int32_t index,
                                bento_game_box_t *bullet)
{
    bento_status_t status;

    shooter->score += 1;
    bento_game_sfx(&shooter->game, "hit");
    status = show_hud(shooter);
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&shooter->game, bullet,
                                        (bento_real_t)0,
                                        (bento_real_t)PARK_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_hide(&shooter->game, bullet);
    }
    if (status == BENTO_OK) {
        status = respawn_enemy(shooter, index);
    }
    return status;
}

/* The first flying bullet that hits the enemy ends it; the others
 * fly on. */
static bento_status_t shoot_enemy(shooter_t *shooter, int32_t index)
{
    bool hit = false;
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MAX_BULLETS) && !hit; i++) {
        bento_game_box_t *bullet = &shooter->bullets[i];

        hit = (bullet->y >= (bento_real_t)FREE_BELOW_Y) &&
              bento_game_hit(bullet, &shooter->enemies[index]);
        if (hit) {
            status = score_hit(shooter, index, bullet);
        }
    }
    return status;
}

/* Every enemy falls one step, then lands or is shot. */
static bento_status_t move_enemies(shooter_t *shooter, bool *out_stop)
{
    bento_status_t status = BENTO_OK;
    int32_t index;

    for (index = 0;
         (index < MAX_ENEMIES) && (status == BENTO_OK) && !*out_stop;
         index++) {
        bento_game_box_t *enemy = &shooter->enemies[index];

        status = bento_game_box_move_to(
            &shooter->game, enemy, enemy->x,
            enemy->y + shooter->enemy_speed[index]);
        if ((status == BENTO_OK) &&
            (enemy->y > (bento_real_t)shooter->game.height)) {
            status = enemy_landed(shooter, index, out_stop);
        } else if (status == BENTO_OK) {
            status = shoot_enemy(shooter, index);
        } else {
            /* the status ends the loop */
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
    *out_stop = false;
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
    if (status == BENTO_OK) {
        status = move_enemies(shooter, out_stop);
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
    bento_game_text_init(&shooter->game_over);
    status = bento_random_init(&shooter->random);
    if (status == BENTO_OK) {
        status =
            bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
        report_joystick(&shooter->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_field(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_bullets(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_enemies(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
