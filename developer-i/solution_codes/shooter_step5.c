/*
 * shooter_step5.c -- Shooter #3 ขั้นต่อ (คาบ 12, ทีมที่เสร็จเร็ว):
 * ระดับความยาก + ปล่อยศัตรูเป็นจังหวะ
 *
 * Twin of shooter_step5.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/shooter_step5.py)
 *
 * ต่อจาก step 4 อีกสามอย่าง: ศัตรูเป็นชุดหมุนเวียนตัวจริง 8 ตัว
 * (ว่าง/ใช้อยู่ ยืม-คืนแบบกระสุน) ตัวนับ spawn_timer ปล่อยศัตรูทีละตัว
 * เป็นจังหวะ และตารางโหมด FREE / EASY / DIFFICULT ที่เก็บค่าความยาก
 * ทั้งหมดไว้ในตารางเดียว เลือกด้วย MODE (0/1/2) ชนกันใช้กล่องชนหด
 * และยานชนศัตรูแล้วเสียชีวิต (ยกเว้นโหมด FREE)
 *
 * Step 4 plus three things: the enemies become a real pool of eight
 * (free or in use, borrowed and returned like the bullets), a
 * spawn_timer releases one enemy at a time to a beat, and the mode
 * table FREE / EASY / DIFFICULT holds every difficulty value in one
 * place, chosen with MODE (0/1/2). Collisions use shrunk hitboxes, and
 * an enemy that rams the ship costs a life (not in FREE mode).
 *
 * C twin of the Example Explorer program devi-shooter-5
 * (py/devi-shooter-5.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - title()'s Back (SystemExit) and Stop end the program with 0;
 *   - joystick.init() prints its line on the kits only, so the
 *     program prints it there after title();
 *   - random is bento_random, the same generator and seed as the
 *     board's MicroPython, drawn in the same order;
 *   - MODES is a table of structs; boxes_overlap() takes two rects,
 *     as eight numbers pass the six-parameter limit;
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

/* The enemy pool: eight, all free and hidden at first. */
enum {
    MAX_ENEMIES = 8,
    ENEMY_W = 30,
    ENEMY_H = 24,
    SPAWN_Y = -20, /* just above the screen, not yet in sight */
    SPAWN_TIMER_START = 8,
    FIRST_WAVE = 3, /* as the full game, shooter_full.py:265-266 */
    ENEMY_COLOR_COUNT = 3
};

/* Shrunk hitboxes: a hit must reach into the body. Bullet 4 x 10,
 * enemy 26 x 16 (shooter_full.py:237); the ship keeps its 62 x 24. */
enum {
    BULLET_HIT_W = 4,
    BULLET_HIT_H = 10,
    ENEMY_HIT_W = 26,
    ENEMY_HIT_H = 16
};

/* The HUD at the top left, GAME OVER, and the game's numbers. */
enum {
    HUD_X = 10,
    HUD_Y = 8,
    HUD_BYTES = 64, /* a mode name, the labels, two int32_t, NUL */
    GAME_OVER_X = 320,
    GAME_OVER_Y = 180,
    FPS = 30
};

/* The mode to play: 0 FREE, 1 EASY, 2 DIFFICULT. The full game picks
 * it from a menu on the screen. */
enum {
    MODE = 2
};

static const uint32_t ENEMY_COLORS[ENEMY_COLOR_COUNT] = {
    BENTO_GAME_RED, BENTO_GAME_ORANGE, BENTO_GAME_PINK};

/* One row of the mode table (shooter_full.py:36-40). */
typedef struct {
    const char *name;
    int32_t spawn_delay;    /* frames between two releases */
    bento_real_t speed_mul; /* times every enemy's speed */
    int32_t start_lives;
    bool lose_when_pass; /* an enemy that lands costs a life */
    bool lose_when_hit;  /* an enemy that rams the ship costs one */
} difficulty_t;

static const difficulty_t MODES[] = {
    /* nothing punishes: practice */
    {"FREE SHOOTER", 16, (bento_real_t)0.85, 99, false, false},
    /* slow and sparse; landing is free, ramming the ship is not */
    {"EASY", 26, (bento_real_t)0.70, 5, false, true},
    /* the real thing: both cost a life */
    {"DIFFICULT", 12, (bento_real_t)1.00, 3, true, true}};

/* A box's place and size, for the shrunk hit test. */
typedef struct {
    bento_real_t x;
    bento_real_t y;
    bento_real_t width;
    bento_real_t height;
} rect_t;

/* What the game keeps between frames. */
typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_game_text_t game_over;
    bento_game_box_t bullets[MAX_BULLETS];
    bento_game_box_t enemies[MAX_ENEMIES];
    bool enemy_active[MAX_ENEMIES];
    bento_real_t enemy_speed[MAX_ENEMIES];
    const difficulty_t *mode;
    bento_random_t random;
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t fire_cooldown;
    int32_t spawn_timer;
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

/* Hit when the boxes really overlap; edges that only touch do not. */
static bool boxes_overlap(const rect_t *first, const rect_t *second)
{
    return (first->x < (second->x + second->width)) &&
           ((first->x + first->width) > second->x) &&
           (first->y < (second->y + second->height)) &&
           ((first->y + first->height) > second->y);
}

/* The mode's speed range times its speed factor. */
static bento_status_t random_speed(shooter_t *shooter,
                                   bento_real_t *speed)
{
    const bento_random_real_t slowest = (bento_random_real_t)1.8;
    const bento_random_real_t fastest = (bento_random_real_t)3.4;
    bento_random_real_t drawn = (bento_random_real_t)0;
    const bento_status_t status = bento_random_uniform(
        &shooter->random, slowest, fastest, &drawn);

    *speed = (bento_real_t)drawn * shooter->mode->speed_mul;
    return status;
}

/* Borrows a free enemy: a random colour, a random x just above the
 * screen, shown, with its own speed. */
static bento_status_t release_enemy(shooter_t *shooter, int32_t index)
{
    bento_game_box_t *enemy = &shooter->enemies[index];
    uint32_t color = 0U;
    int32_t x = 0;
    bento_status_t status = random_color(shooter, &color);

    shooter->enemy_active[index] = true;
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(&shooter->game, enemy, color);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(
            &shooter->random, 0, shooter->game.width - ENEMY_W, &x);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&shooter->game, enemy,
                                        (bento_real_t)x,
                                        (bento_real_t)SPAWN_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_show(&shooter->game, enemy);
    }
    if (status == BENTO_OK) {
        status = random_speed(shooter, &shooter->enemy_speed[index]);
    }
    return status;
}

/* Finds the first free enemy and borrows it; with none free, nothing
 * happens. */
static bento_status_t spawn_enemy(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    bool released = false;
    int32_t index;

    for (index = 0; (index < MAX_ENEMIES) && !released; index++) {
        if (!shooter->enemy_active[index]) {
            status = release_enemy(shooter, index);
            released = true;
        }
    }
    return status;
}

/* Returns an enemy to the pool: free, hidden, parked above. */
static bento_status_t park_enemy(shooter_t *shooter, int32_t index)
{
    bento_status_t status;

    shooter->enemy_active[index] = false;
    status =
        bento_game_box_hide(&shooter->game, &shooter->enemies[index]);
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &shooter->game, &shooter->enemies[index], (bento_real_t)0,
            (bento_real_t)PARK_Y);
    }
    return status;
}

/* The pool, made once: every enemy free, hidden above the screen. */
static bento_status_t build_enemies(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MAX_ENEMIES) && (status == BENTO_OK); i++) {
        bento_game_box_init(&shooter->enemies[i]);
        status = bento_game_box_create(
            &shooter->game, &shooter->enemies[i], (bento_real_t)0,
            (bento_real_t)PARK_Y, (bento_real_t)ENEMY_W,
            (bento_real_t)ENEMY_H, ENEMY_COLORS[0]);
    }
    for (i = 0; (i < MAX_ENEMIES) && (status == BENTO_OK); i++) {
        status =
            bento_game_box_hide(&shooter->game, &shooter->enemies[i]);
    }
    for (i = 0; i < MAX_ENEMIES; i++) {
        shooter->enemy_active[i] = false;
        shooter->enemy_speed[i] = (bento_real_t)0.0;
    }
    return status;
}

/* The HUD line: the mode, the score and the lives. */
static void format_hud(const shooter_t *shooter, char *text,
                       size_t size)
{
    (void)snprintf(text, size,
                   "%s   Score: %" PRId32 "   Lives: %" PRId32,
                   shooter->mode->name, shooter->score, shooter->lives);
}

/* The HUD changes whenever the score or the lives do. */
static bento_status_t show_hud(shooter_t *shooter)
{
    char text[HUD_BYTES];

    format_hud(shooter, text, sizeof(text));
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

/* One life less on the HUD; at none the game ends. */
static bento_status_t lose_life(shooter_t *shooter, bool *out_stop)
{
    bento_status_t status;

    shooter->lives -= 1;
    status = show_hud(shooter);
    if ((status == BENTO_OK) && (shooter->lives <= 0)) {
        status = end_game(shooter, out_stop);
    }
    return status;
}

/* A flying bullet hit the enemy: a point, the bullet parks again
 * (free) and the enemy returns to the pool. */
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
        status = park_enemy(shooter, index);
    }
    return status;
}

/* The first flying bullet whose shrunk box overlaps the enemy's ends
 * it; *was_hit says so. */
static bento_status_t shoot_enemy(shooter_t *shooter, int32_t index,
                                  const rect_t *enemy_box,
                                  bool *was_hit)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    *was_hit = false;
    for (i = 0; (i < MAX_BULLETS) && !*was_hit; i++) {
        bento_game_box_t *bullet = &shooter->bullets[i];
        const rect_t bullet_box = {bullet->x, bullet->y,
                                   (bento_real_t)BULLET_HIT_W,
                                   (bento_real_t)BULLET_HIT_H};

        *was_hit = (bullet->y >= (bento_real_t)FREE_BELOW_Y) &&
                   boxes_overlap(&bullet_box, enemy_box);
        if (*was_hit) {
            status = score_hit(shooter, index, bullet);
        }
    }
    return status;
}

/* An enemy rammed the ship: it returns to the pool, and the modes
 * that punish it take a life, with an explosion while lives remain. */
static bento_status_t crash(shooter_t *shooter, int32_t index,
                            bool *out_stop)
{
    bento_status_t status = park_enemy(shooter, index);

    if ((status == BENTO_OK) && shooter->mode->lose_when_hit) {
        status = lose_life(shooter, out_stop);
        if ((status == BENTO_OK) && !*out_stop) {
            /* the full game plays "lose_life" here */
            bento_game_sfx(&shooter->game, "explode");
        }
    }
    return status;
}

/* The ship keeps its full 62 x 24 box. */
static bento_status_t ram_ship(shooter_t *shooter, int32_t index,
                               const rect_t *enemy_box, bool *out_stop)
{
    const rect_t ship_box = {shooter->ship_x, (bento_real_t)SHIP_Y,
                             shooter->ship.w, shooter->ship.h};
    bento_status_t status = BENTO_OK;

    if (boxes_overlap(&ship_box, enemy_box)) {
        status = crash(shooter, index, out_stop);
    }
    return status;
}

/* An enemy that landed returns to the pool; only DIFFICULT takes a
 * life for it. */
static bento_status_t enemy_landed(shooter_t *shooter, int32_t index,
                                   bool *out_stop)
{
    bento_status_t status = park_enemy(shooter, index);

    if ((status == BENTO_OK) && shooter->mode->lose_when_pass) {
        status = lose_life(shooter, out_stop);
    }
    return status;
}

/* One enemy in use falls a step, then lands, is shot, or rams. */
static bento_status_t update_enemy(shooter_t *shooter, int32_t index,
                                   bool *out_stop)
{
    bento_game_box_t *enemy = &shooter->enemies[index];
    bool was_hit = false;
    bento_status_t status =
        bento_game_box_move_to(&shooter->game, enemy, enemy->x,
                               enemy->y + shooter->enemy_speed[index]);

    if ((status == BENTO_OK) &&
        (enemy->y > (bento_real_t)shooter->game.height)) {
        status = enemy_landed(shooter, index, out_stop);
    } else if (status == BENTO_OK) {
        const rect_t enemy_box = {enemy->x, enemy->y,
                                  (bento_real_t)ENEMY_HIT_W,
                                  (bento_real_t)ENEMY_HIT_H};

        status = shoot_enemy(shooter, index, &enemy_box, &was_hit);
        if ((status == BENTO_OK) && !was_hit) {
            status = ram_ship(shooter, index, &enemy_box, out_stop);
        }
    } else {
        /* the status ends the frame */
    }
    return status;
}

static bento_status_t move_enemies(shooter_t *shooter, bool *out_stop)
{
    bento_status_t status = BENTO_OK;
    int32_t index;

    for (index = 0;
         (index < MAX_ENEMIES) && (status == BENTO_OK) && !*out_stop;
         index++) {
        if (shooter->enemy_active[index]) {
            status = update_enemy(shooter, index, out_stop);
        }
    }
    return status;
}

/* Counts down a frame at a time; at 0 one enemy is released and the
 * count starts again from the mode's delay. */
static bento_status_t tick_spawn_timer(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;

    if (shooter->spawn_timer > 0) {
        shooter->spawn_timer -= 1;
    } else {
        status = spawn_enemy(shooter);
        shooter->spawn_timer = shooter->mode->spawn_delay;
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
        status = tick_spawn_timer(shooter);
    }
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
    shooter->lives = shooter->mode->start_lives;
    shooter->fire_cooldown = 0;
    shooter->spawn_timer = SPAWN_TIMER_START;
    if (status == BENTO_OK) {
        char text[HUD_BYTES];

        format_hud(shooter, text, sizeof(text));
        status =
            bento_game_text_create(&shooter->game, &shooter->hud, text,
                                   HUD_X, HUD_Y, BENTO_GAME_WHITE);
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
    int32_t i;

    (void)memset(shooter, 0, sizeof(*shooter));
    shooter->mode = &MODES[MODE];
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
    for (i = 0; (i < FIRST_WAVE) && (status == BENTO_OK) && !quit;
         i++) {
        status = spawn_enemy(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
