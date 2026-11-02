/*
 * shooter_full.c -- Space Shooter ฉบับอ้างอิง (เกมเต็ม)
 *
 * Twin of shooter_full.py
 * (embedded-systems-for-game_console_developer/developer-i/full_games/shooter_full.py).
 *
 * เกมยิงอวกาศที่หน้าตาและจังหวะลอกมาจากเกม C บนเครื่อง
 * (page_game_shooter.c): ยานสไปรต์บินอิสระ 2 แกน กดค้างแล้วเร่ง
 * ปล่อยแล้วไถล ศัตรู 3 แบบ (invader / UFO / demon) สุ่มชนิดตอนเกิด
 * กระสุนติดโมเมนตัมของยาน ยิงโดนแล้วระเบิด 2 เฟรมสีตามชนิดศัตรู
 * พร้อมเสียงครบ และมีโหมด FREE SHOOTER / EASY / DIFFICULT
 * ปุ่ม: ลูกศร = ขับยาน, A = ยิง (กดทีละนัด), Y = เริ่มโหมดเดิมใหม่,
 * Start = พักเกม, Back = ออก
 *
 * The reference Space Shooter, with the look and rhythm of the
 * board's C game: the ship flies free on two axes (hold to speed up,
 * let go to glide), three enemy kinds come down at random, bullets
 * carry the ship's sideways momentum, a hit makes a two-frame
 * explosion coloured by the enemy kind, and there are three modes.
 * Keys: arrows fly, A fires one shot per press, Y restarts the same
 * mode, Start pauses, Back quits.
 *
 * C twin of the Example Explorer program full-shooter
 * (py/full-shooter.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the st dict, the bullet, enemy and explosion lists and hud_last
 *     are one shooter_t; it is over 512 bytes, so it lives at file
 *     scope as g_shooter and main() clears it first;
 *   - floats are bento_real_t, the width of each board's MicroPython
 *     float;
 *   - overlap() takes two rectangles instead of eight numbers;
 *   - the MODES, ENEMY_SPR and BOOM_FRAMES tuples are static const
 *     tables;
 *   - update() is the function bento_game_run calls each frame; it
 *     reads g_shooter, not its user pointer, and Python's
 *     `return False` sets *out_stop;
 *   - a call that raises in MicroPython returns its status, which
 *     ends the program; BACK at the title and Stop end it with 0;
 *   - start() prints the joystick line that MicroPython's
 *     joystick.init() prints on the PSoC Edge kits.
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

enum { /* the ship sprite and where it may fly, px */
    SHIP_W = 62,
    SHIP_H = 28,
    SHIP_Y_MIN = 24,
    SHIP_HOME_MARGIN = 32
};
enum { /* pools: a balance of few widgets and enough action */
    ENEMY_MAX = 5,
    BULLET_MAX = 4,
    BOOM_MAX = 3,
    STARS = 5,
    KIND_COUNT = 3,
    BOOM_TICKS = 6,
    NO_SLOT = -1
};
enum { /* colours, 0xRRGGBB */
    COLOR_SPACE = 0x10122C,
    COLOR_STAR = 0xB8C0E0,
    COLOR_LASER = 0x80F4FF,
    COLOR_LASER_EDGE = 0xFFFFFF
};
enum { /* the bullet box: parked off screen, size and corner, px */
    BULLET_PARK_X = -20,
    BULLET_PARK_Y = -50,
    BULLET_W = 5,
    BULLET_H = 12,
    BULLET_RADIUS = 2,
    BULLET_HIT_W = 4,
    BULLET_HIT_H = 10,
    BULLET_LIFT = 8,
    BULLET_NOSE = 2,
    BULLET_EDGE = -12
};
enum { /* enemies: spawn range, hit box and explosion offsets, px */
    ENEMY_LEFT = 20,
    ENEMY_RIGHT_MARGIN = 46,
    ENEMY_START_Y = -20,
    ENEMY_SPEED_MIN = 18,
    ENEMY_SPEED_MAX = 34,
    ENEMY_SPEED_TENTHS = 10,
    ENEMY_HIT_W = 26,
    ENEMY_HIT_H = 16,
    ENEMY_CENTER_X = 13,
    ENEMY_CENTER_Y = 8,
    BOOM_HALF_W = 15,
    BOOM_HALF_H = 12
};
enum { /* the round */
    HUD_X = 10,
    HUD_Y = 8,
    FIRST_SPAWN_WAIT = 8,
    FIRST_ENEMIES = 3,
    DEFAULT_MODE = 2,
    MODE_COUNT = 3,
    FPS = 50,
    /* "Score: ... Lives: ...", any three int32_t */
    HUD_CHARS = 72
};

static const char BEST_NAME[] = "shooter";

/* BOOM_FRAMES: purple (invader), blue (UFO) and fire (demon) */
static const char *const BOOM_FRAMES[KIND_COUNT][2] = {
    {"boom1", "boom2"}, {"boom3", "boom4"}, {"boom5", "boom6"}};

/* one mode of the C game: enemy gap in frames, fall speed factor,
 * starting lives, and which losses cost a life */
typedef struct {
    int32_t spawn_gap;
    bento_real_t fall_scale;
    int32_t lives;
    bool pass_costs;
    bool collide_costs;
} mode_rules_t;

static const mode_rules_t MODES[MODE_COUNT] = {
    {16, (bento_real_t)0.85, 99, false, false},
    {26, (bento_real_t)0.70, 5, false, true},
    {12, (bento_real_t)1.00, 3, true, true}};

typedef struct {
    bento_real_t x;
    bento_real_t y;
    bento_real_t w;
    bento_real_t h;
} rect_t;

typedef struct {
    bool active;
    bento_real_t x;
    bento_real_t y;
    bento_real_t vx;
} bullet_t;

typedef struct {
    bool active;
    bento_real_t x;
    bento_real_t y;
    bento_real_t vy;
    int32_t kind;
} enemy_t;

typedef struct {
    int32_t ticks;
    int32_t kind;
} explosion_t;

typedef struct {
    int32_t score;
    int32_t best;
    int32_t lives;
} hud_row_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    const mode_rules_t *rules;
    int32_t ship_y_home;
    bento_game_sprite_t ship;
    bento_game_box_t bullet_boxes[BULLET_MAX];
    bento_game_sprite_t enemy_sprites[ENEMY_MAX];
    bento_game_sprite_t boom_sprites[BOOM_MAX];
    bento_game_text_t hud;
    bento_real_t ship_x;
    bento_real_t ship_y;
    bento_real_t ship_vx;
    bento_real_t ship_vy;
    int32_t score;
    int32_t lives;
    int32_t spawn_wait;
    const char *message;
    bool restart;
    bullet_t bullets[BULLET_MAX];
    enemy_t enemies[ENEMY_MAX];
    explosion_t explosions[BOOM_MAX];
    bool hud_shown;
    hud_row_t hud_last;
} shooter_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

/* the same formula as shooter_overlap of the C game: touching edges
 * count as a hit */
static bool overlap(const rect_t *first, const rect_t *second)
{
    return !(((first->x + first->w) < second->x) ||
             (first->x > (second->x + second->w)) ||
             ((first->y + first->h) < second->y) ||
             (first->y > (second->y + second->h)));
}

static bento_real_t clamp_speed(bento_real_t speed)
{
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t capped = (speed < max_speed) ? speed : max_speed;

    return (capped > -max_speed) ? capped : -max_speed;
}

/* the HUD is redrawn only when a number changed, as the C game does */
static bento_status_t show_hud(shooter_t *shooter)
{
    const int32_t best = bento_game_best(&shooter->game, BEST_NAME);
    const hud_row_t row = {
        shooter->score, (best > shooter->score) ? best : shooter->score,
        shooter->lives};
    bento_status_t status = BENTO_OK;

    if (!shooter->hud_shown || (row.score != shooter->hud_last.score) ||
        (row.best != shooter->hud_last.best) ||
        (row.lives != shooter->hud_last.lives)) {
        char text[HUD_CHARS];

        shooter->hud_shown = true;
        shooter->hud_last = row;
        (void)snprintf(text, sizeof(text),
                       "Score: %" PRId32 "    Best: %" PRId32
                       "    Lives: %" PRId32,
                       row.score, row.best, row.lives);
        status =
            bento_game_text_set(&shooter->game, &shooter->hud, text);
    }
    return status;
}

/* a wall stops the speed on that axis, so the ship does not keep
 * pressing against it */
static bento_status_t set_ship(shooter_t *shooter, bento_real_t x,
                               bento_real_t y)
{
    const bento_real_t right =
        (bento_real_t)(shooter->game.width - SHIP_W);
    const bento_real_t bottom = (bento_real_t)shooter->ship_y_home;
    bento_real_t new_x = x;
    bento_real_t new_y = y;

    if (new_x < (bento_real_t)0) {
        new_x = (bento_real_t)0;
        shooter->ship_vx = (bento_real_t)0;
    }
    if (new_x > right) {
        new_x = right;
        shooter->ship_vx = (bento_real_t)0;
    }
    if (new_y < (bento_real_t)SHIP_Y_MIN) {
        new_y = (bento_real_t)SHIP_Y_MIN;
        shooter->ship_vy = (bento_real_t)0;
    }
    if (new_y > bottom) {
        new_y = bottom;
        shooter->ship_vy = (bento_real_t)0;
    }
    shooter->ship_x = new_x;
    shooter->ship_y = new_y;
    return bento_game_sprite_move_to(&shooter->game, &shooter->ship,
                                     new_x, new_y);
}

static bento_status_t reset_bullet(shooter_t *shooter, int32_t slot)
{
    shooter->bullets[slot].active = false;
    return bento_game_box_hide(&shooter->game,
                               &shooter->bullet_boxes[slot]);
}

static bento_status_t reset_enemy(shooter_t *shooter, int32_t slot)
{
    shooter->enemies[slot].active = false;
    return bento_game_sprite_hide(&shooter->game,
                                  &shooter->enemy_sprites[slot]);
}

static int32_t free_enemy_slot(const shooter_t *shooter)
{
    int32_t slot = NO_SLOT;
    int32_t i;

    for (i = 0; (i < ENEMY_MAX) && (slot == NO_SLOT); i++) {
        if (!shooter->enemies[i].active) {
            slot = i;
        }
    }
    return slot;
}

/* the random place, speed and kind of a new enemy, in the ranges of
 * the C game */
static bento_status_t roll_enemy(shooter_t *shooter, enemy_t *enemy)
{
    int32_t left = 0;
    int32_t tenths = 0;
    int32_t kind = 0;
    bento_status_t status = bento_random_randint(
        &shooter->random, ENEMY_LEFT,
        shooter->game.width - ENEMY_RIGHT_MARGIN, &left);

    if (status == BENTO_OK) {
        status = bento_random_randint(&shooter->random, ENEMY_SPEED_MIN,
                                      ENEMY_SPEED_MAX, &tenths);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(&shooter->random, 0,
                                      KIND_COUNT - 1, &kind);
    }
    if (status == BENTO_OK) {
        const bento_real_t speed =
            (bento_real_t)tenths / (bento_real_t)ENEMY_SPEED_TENTHS;

        enemy->x = (bento_real_t)left;
        enemy->y = (bento_real_t)ENEMY_START_Y;
        enemy->vy = speed * shooter->rules->fall_scale;
        enemy->kind = kind;
    }
    return status;
}

/* send a random enemy down from the top into a free pool slot */
static bento_status_t spawn_enemy(shooter_t *shooter)
{
    static const char *const enemy_sprites[KIND_COUNT] = {
        "enemy", "enemy2", "enemy3"};
    const int32_t slot = free_enemy_slot(shooter);
    bento_status_t status = BENTO_OK;

    if (slot != NO_SLOT) {
        enemy_t *enemy = &shooter->enemies[slot];
        bento_game_sprite_t *sprite = &shooter->enemy_sprites[slot];

        enemy->active = true;
        status = roll_enemy(shooter, enemy);
        if (status == BENTO_OK) {
            status = bento_game_sprite_frame(
                &shooter->game, sprite, enemy_sprites[enemy->kind]);
        }
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(&shooter->game, sprite,
                                               enemy->x, enemy->y);
        }
        if (status == BENTO_OK) {
            status = bento_game_sprite_show(&shooter->game, sprite);
        }
    }
    return status;
}

/* fire from the ship's nose, carrying part of its sideways speed */
static bento_status_t fire(shooter_t *shooter)
{
    const bento_real_t inherit = (bento_real_t)0.55;
    const bento_real_t half_ship =
        (bento_real_t)SHIP_W / (bento_real_t)2;
    int32_t slot = NO_SLOT;
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < BULLET_MAX) && (slot == NO_SLOT); i++) {
        if (!shooter->bullets[i].active) {
            slot = i;
        }
    }
    if (slot != NO_SLOT) {
        bullet_t *bullet = &shooter->bullets[slot];

        bullet->active = true;
        bento_game_sfx(&shooter->game, "fire");
        bullet->x =
            (shooter->ship_x + half_ship) - (bento_real_t)BULLET_NOSE;
        bullet->y = shooter->ship_y - (bento_real_t)BULLET_LIFT;
        bullet->vx = shooter->ship_vx * inherit;
        status = bento_game_box_move_to(&shooter->game,
                                        &shooter->bullet_boxes[slot],
                                        bullet->x, bullet->y);
        if (status == BENTO_OK) {
            status = bento_game_box_show(&shooter->game,
                                         &shooter->bullet_boxes[slot]);
        }
    }
    return status;
}

/* the C game plays a recorded blast; the nearest synth sound stands
 * in for it */
static bento_status_t boom(shooter_t *shooter, const enemy_t *enemy)
{
    const bento_real_t center_x =
        enemy->x + (bento_real_t)ENEMY_CENTER_X;
    const bento_real_t center_y =
        enemy->y + (bento_real_t)ENEMY_CENTER_Y;
    int32_t slot = NO_SLOT;
    bento_status_t status = BENTO_OK;
    int32_t i;

    bento_game_sfx(&shooter->game, "explode");
    for (i = 0; (i < BOOM_MAX) && (slot == NO_SLOT); i++) {
        if (shooter->explosions[i].ticks == 0) {
            slot = i;
        }
    }
    if (slot != NO_SLOT) {
        bento_game_sprite_t *sprite = &shooter->boom_sprites[slot];

        shooter->explosions[slot].ticks = BOOM_TICKS;
        shooter->explosions[slot].kind = enemy->kind;
        status = bento_game_sprite_frame(&shooter->game, sprite,
                                         BOOM_FRAMES[enemy->kind][0]);
        /* the 29 x 24 picture is centred on the enemy */
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(
                &shooter->game, sprite,
                center_x - (bento_real_t)BOOM_HALF_W,
                center_y - (bento_real_t)BOOM_HALF_H);
        }
        if (status == BENTO_OK) {
            status = bento_game_sprite_show(&shooter->game, sprite);
        }
    }
    return status;
}

static bento_status_t tick_boom(shooter_t *shooter, int32_t slot)
{
    explosion_t *explosion = &shooter->explosions[slot];
    bento_status_t status = BENTO_OK;

    explosion->ticks -= 1;
    if (explosion->ticks == 0) {
        status = bento_game_sprite_hide(&shooter->game,
                                        &shooter->boom_sprites[slot]);
    } else if (explosion->ticks == (BOOM_TICKS / 2)) {
        /* the second half shows the bigger blast */
        status = bento_game_sprite_frame(
            &shooter->game, &shooter->boom_sprites[slot],
            BOOM_FRAMES[explosion->kind][1]);
    } else {
        /* the frame stays */
    }
    return status;
}

static bento_status_t step_booms(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < BOOM_MAX) && (status == BENTO_OK); i++) {
        if (shooter->explosions[i].ticks != 0) {
            status = tick_boom(shooter, i);
        }
    }
    return status;
}

/* held = speed up to the limit, released = glide to a stop */
static bento_real_t accelerate(bool minus_held, bool plus_held,
                               bento_real_t speed)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t friction = (bento_real_t)0.80;
    bento_real_t next = speed;

    if (minus_held && !plus_held) {
        next -= accel;
    } else if (plus_held && !minus_held) {
        next += accel;
    } else {
        next *= friction;
    }
    return next;
}

static bento_status_t steer_ship(shooter_t *shooter,
                                 const bento_game_keys_t *keys)
{
    shooter->ship_vx =
        accelerate(keys->left, keys->right, shooter->ship_vx);
    shooter->ship_vy =
        accelerate(keys->up, keys->down, shooter->ship_vy);
    shooter->ship_vx = clamp_speed(shooter->ship_vx);
    shooter->ship_vy = clamp_speed(shooter->ship_vy);
    return set_ship(shooter, shooter->ship_x + shooter->ship_vx,
                    shooter->ship_y + shooter->ship_vy);
}

/* the enemy clock: the harder the mode, the shorter the gap */
static bento_status_t tick_spawn(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;

    if (shooter->spawn_wait > 0) {
        shooter->spawn_wait -= 1;
    } else {
        status = spawn_enemy(shooter);
        shooter->spawn_wait = shooter->rules->spawn_gap;
    }
    return status;
}

/* bullets climb and drift; one that leaves the field goes back to the
 * pool */
static bento_status_t move_bullet(shooter_t *shooter, int32_t slot)
{
    const bento_real_t speed = (bento_real_t)8.0;
    const bento_real_t edge = (bento_real_t)BULLET_EDGE;
    bullet_t *bullet = &shooter->bullets[slot];
    bento_status_t status = BENTO_OK;

    bullet->y -= speed;
    bullet->x += bullet->vx;
    if ((bullet->y < edge) || (bullet->x < edge) ||
        (bullet->x > (bento_real_t)shooter->game.width)) {
        status = reset_bullet(shooter, slot);
    } else {
        status = bento_game_box_move_to(&shooter->game,
                                        &shooter->bullet_boxes[slot],
                                        bullet->x, bullet->y);
    }
    return status;
}

static bento_status_t move_bullets(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < BULLET_MAX) && (status == BENTO_OK); i++) {
        if (shooter->bullets[i].active) {
            status = move_bullet(shooter, i);
        }
    }
    return status;
}

/* one life less; at none the round ends with the given reason */
static void lose_life(shooter_t *shooter, const char *reason,
                      bool *out_stop)
{
    shooter->lives -= 1;
    if (shooter->lives <= 0) {
        bento_game_sfx(&shooter->game, "gameover");
        shooter->message = reason;
        *out_stop = true;
    }
}

/* past the bottom edge: only the DIFFICULT mode charges a life */
static bento_status_t pass_bottom(shooter_t *shooter, int32_t slot,
                                  bool *out_stop)
{
    const bento_status_t status = reset_enemy(shooter, slot);

    if ((status == BENTO_OK) && shooter->rules->pass_costs) {
        lose_life(shooter, "Base destroyed", out_stop);
    }
    return status;
}

static int32_t find_hit_bullet(const shooter_t *shooter,
                               const rect_t *target)
{
    int32_t hit = NO_SLOT;
    int32_t i;

    for (i = 0; (i < BULLET_MAX) && (hit == NO_SLOT); i++) {
        const bullet_t *bullet = &shooter->bullets[i];
        const rect_t box = {bullet->x, bullet->y,
                            (bento_real_t)BULLET_HIT_W,
                            (bento_real_t)BULLET_HIT_H};

        if (bullet->active && overlap(&box, target)) {
            hit = i;
        }
    }
    return hit;
}

/* a bullet hit: blast in the enemy's colour, a point, and a new enemy
 * at once */
static bento_status_t shoot_enemy(shooter_t *shooter, int32_t slot,
                                  const rect_t *target, bool *out_shot)
{
    const int32_t bullet = find_hit_bullet(shooter, target);
    bento_status_t status = BENTO_OK;

    if (bullet != NO_SLOT) {
        status = boom(shooter, &shooter->enemies[slot]);
        if (status == BENTO_OK) {
            status = reset_bullet(shooter, bullet);
        }
        if (status == BENTO_OK) {
            status = reset_enemy(shooter, slot);
        }
        if (status == BENTO_OK) {
            /* the hit sound plays over the blast */
            bento_game_sfx(&shooter->game, "hit");
            shooter->score += 1;
            status = spawn_enemy(shooter);
        }
        *out_shot = true;
    }
    return status;
}

/* an enemy rams the ship: a blast too; in FREE mode the ship holds */
static bento_status_t ram_ship(shooter_t *shooter, int32_t slot,
                               const rect_t *target, bool *out_stop)
{
    const rect_t ship = {shooter->ship_x, shooter->ship_y,
                         (bento_real_t)SHIP_W, (bento_real_t)SHIP_H};
    const bool rammed = overlap(&ship, target);
    bento_status_t status = BENTO_OK;

    if (rammed) {
        status = boom(shooter, &shooter->enemies[slot]);
    }
    if (rammed && (status == BENTO_OK)) {
        status = reset_enemy(shooter, slot);
    }
    if (rammed && (status == BENTO_OK) &&
        shooter->rules->collide_costs) {
        lose_life(shooter, "Ship destroyed", out_stop);
        if (!*out_stop) {
            /* hurt but alive: the warning sound */
            bento_game_sfx(&shooter->game, "lose_life");
        }
    }
    return status;
}

static bento_status_t step_enemy(shooter_t *shooter, int32_t slot,
                                 bool *out_stop)
{
    enemy_t *enemy = &shooter->enemies[slot];
    bento_status_t status = BENTO_OK;
    bool shot = false;

    enemy->y += enemy->vy;
    status = bento_game_sprite_move_to(&shooter->game,
                                       &shooter->enemy_sprites[slot],
                                       enemy->x, enemy->y);
    if (status != BENTO_OK) {
        /* the status ends the frame */
    } else if (enemy->y > (bento_real_t)shooter->game.height) {
        status = pass_bottom(shooter, slot, out_stop);
    } else {
        const rect_t target = {enemy->x, enemy->y,
                               (bento_real_t)ENEMY_HIT_W,
                               (bento_real_t)ENEMY_HIT_H};

        status = shoot_enemy(shooter, slot, &target, &shot);
        if ((status == BENTO_OK) && !shot) {
            status = ram_ship(shooter, slot, &target, out_stop);
        }
    }
    return status;
}

/* enemies fall, and every kind of hit is tested as in the C game */
static bento_status_t move_enemies(shooter_t *shooter, bool *out_stop)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < ENEMY_MAX) && (status == BENTO_OK) && !*out_stop;
         i++) {
        if (shooter->enemies[i].active) {
            status = step_enemy(shooter, i, out_stop);
        }
    }
    return status;
}

/* one frame; bento_game_run hands back the state main() gave it */
/* a frame after the Y test: fly, fire, spawn, move and hit */
static bento_status_t play_frame(shooter_t *shooter,
                                 const bento_game_keys_t *keys,
                                 bool *out_stop)
{
    bento_status_t status = steer_ship(shooter, keys);

    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&shooter->game, BENTO_GAME_KEY_A,
                                keys)) {
        status = fire(shooter);
    }
    if (status == BENTO_OK) {
        status = tick_spawn(shooter);
    }
    if (status == BENTO_OK) {
        status = move_bullets(shooter);
    }
    if (status == BENTO_OK) {
        status = move_enemies(shooter, out_stop);
    }
    if ((status == BENTO_OK) && !*out_stop) {
        status = step_booms(shooter);
    }
    if ((status == BENTO_OK) && !*out_stop) {
        status = show_hud(shooter);
    }
    return status;
}

/* one frame; Y restarts the same mode at once */
static bento_status_t update(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_shooter (file scope, C9) */
    bento_game_keys_init(&keys);
    /* keys() never fails: a silent pad reads as no key */
    (void)bento_game_keys(&shooter->game, &keys);
    if (bento_game_pressed_once(&shooter->game, BENTO_GAME_KEY_Y,
                                &keys)) {
        shooter->restart = true;
        *out_stop = true;
    } else {
        status = play_frame(shooter, &keys, out_stop);
    }
    return status;
}

/* the scene is made anew each round; widgets are never made inside
 * update() */
static bento_status_t build_scene(shooter_t *shooter)
{
    bento_game_t *game = &shooter->game;
    bento_game_box_t background;
    bento_game_box_t stars[STARS];
    bento_status_t status =
        bento_game_background(game, &background, (uint32_t)COLOR_SPACE);
    int32_t i;

    /* stars first, so they stay behind the ship and enemies */
    if (status == BENTO_OK) {
        status = bento_game_starfield(game, stars, (uint32_t)STARS,
                                      (uint32_t)COLOR_STAR);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(game, &shooter->ship, "ship",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    for (i = 0; (i < BULLET_MAX) && (status == BENTO_OK); i++) {
        status = bento_game_box_create_styled(
            game, &shooter->bullet_boxes[i],
            (bento_real_t)BULLET_PARK_X, (bento_real_t)BULLET_PARK_Y,
            (bento_real_t)BULLET_W, (bento_real_t)BULLET_H,
            (uint32_t)COLOR_LASER, COLOR_LASER_EDGE, BULLET_RADIUS, 0);
    }
    for (i = 0; (i < BULLET_MAX) && (status == BENTO_OK); i++) {
        status = bento_game_box_hide(game, &shooter->bullet_boxes[i]);
    }
    if (status == BENTO_OK) {
        status = bento_game_pool(game, shooter->enemy_sprites,
                                 (uint32_t)ENEMY_MAX, "enemy");
    }
    if (status == BENTO_OK) {
        status = bento_game_pool(game, shooter->boom_sprites,
                                 (uint32_t)BOOM_MAX, "boom1");
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &shooter->hud, "", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* the start values of a round: the st dict and the pools' lists */
static void reset_round(shooter_t *shooter)
{
    shooter->ship_x =
        (bento_real_t)(shooter->game.width - SHIP_W) / (bento_real_t)2;
    shooter->ship_y = (bento_real_t)shooter->ship_y_home;
    shooter->ship_vx = (bento_real_t)0;
    shooter->ship_vy = (bento_real_t)0;
    shooter->score = 0;
    shooter->lives = shooter->rules->lives;
    shooter->spawn_wait = FIRST_SPAWN_WAIT;
    shooter->message = "GAME OVER";
    shooter->restart = false;
    (void)memset(shooter->bullets, 0, sizeof(shooter->bullets));
    (void)memset(shooter->enemies, 0, sizeof(shooter->enemies));
    (void)memset(shooter->explosions, 0, sizeof(shooter->explosions));
    shooter->hud_shown = false;
}

/* the scene, the ship at home, and the C game's first three enemies */
static bento_status_t start_round(shooter_t *shooter, int32_t mode)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    shooter->rules = &MODES[mode];
    status = build_scene(shooter);
    if (status == BENTO_OK) {
        reset_round(shooter);
        status = set_ship(shooter, shooter->ship_x, shooter->ship_y);
    }
    for (i = 0; (i < FIRST_ENEMIES) && (status == BENTO_OK); i++) {
        status = spawn_enemy(shooter);
    }
    if (status == BENTO_OK) {
        status = show_hud(shooter);
    }
    return status;
}

/* after a round: Back quits, Y replays the same mode, a lost round
 * shows game over (Start plays again from the menu) */
static bento_status_t end_round(shooter_t *shooter, bool finished,
                                bool *out_same, bool *out_playing)
{
    bento_status_t status = BENTO_OK;

    if (!finished) {
        /* save_best answers the best score, not a status */
        (void)bento_game_save_best(&shooter->game, &shooter->score,
                                   BEST_NAME);
        *out_playing = false;
    } else if (shooter->restart) {
        /* the Best must not be lost with a round thrown away by Y */
        (void)bento_game_save_best(&shooter->game, &shooter->score,
                                   BEST_NAME);
        status = bento_game_replay(&shooter->game);
        *out_same = true;
    } else {
        status =
            bento_game_over(&shooter->game, &shooter->score, BEST_NAME,
                            shooter->message, out_playing);
    }
    return status;
}

/* Back at the menu quits, as in the C game */
static bento_status_t pick_mode(shooter_t *shooter, int32_t *mode,
                                bool *out_playing)
{
    static const char *const options[MODE_COUNT] = {
        "FREE SHOOTER", "EASY", "DIFFICULT"};
    int32_t picked = NO_SLOT;
    const bento_status_t status =
        bento_game_menu(&shooter->game, options, (uint32_t)MODE_COUNT,
                        NULL, *mode, &picked);

    if ((status == BENTO_OK) && (picked < 0)) {
        *out_playing = false;
    } else if (status == BENTO_OK) {
        *mode = picked;
    } else {
        /* the status ends the game */
    }
    return status;
}

/* the C game opens on DIFFICULT; Y replays a mode without the menu */
/* one round of a mode, from the scene to what comes after it */
static bento_status_t play_round(shooter_t *shooter, int32_t mode,
                                 bool *out_same, bool *out_playing)
{
    bool finished = false;
    bento_status_t status = start_round(shooter, mode);

    if (status == BENTO_OK) {
        status = bento_game_run(&shooter->game, update, NULL, FPS,
                                &finished);
    }
    if (status == BENTO_OK) {
        status = end_round(shooter, finished, out_same, out_playing);
    }
    return status;
}

/* the C game opens on DIFFICULT; Y replays a mode without the menu */
static bento_status_t play(shooter_t *shooter)
{
    int32_t mode = DEFAULT_MODE;
    bool same_mode = false;
    bool playing = true;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && playing) {
        if (!same_mode) {
            status = pick_mode(shooter, &mode, &playing);
        }
        same_mode = false;
        if ((status == BENTO_OK) && playing) {
            status = play_round(shooter, mode, &same_mode, &playing);
        }
    }
    return status;
}

/* Is this the reTerminal? Asked at run time, by the name the board
 * gives, so one Emulator build serves both of its board models. A
 * board that cannot say keeps the build target's answer. */
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

/* MicroPython's joystick.init() prints this line on the PSoC Edge
 * kits; the reTerminal opens its pad silently */
static void print_joystick_line(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_sprite_init(&shooter->ship);
    bento_game_text_init(&shooter->hud);
    status = bento_random_init(&shooter->random);
    /* game.HEIGHT before start(), as Python reads it at import */
    shooter->ship_y_home =
        BENTO_GAME_HEIGHT - SHIP_H - SHIP_HOME_MARGIN;
    if (status == BENTO_OK) {
        status = bento_game_title_melody(&shooter->game, "SHOOTER",
                                         NULL, NULL, 0U, &quit);
        print_joystick_line(&shooter->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = play(shooter);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
