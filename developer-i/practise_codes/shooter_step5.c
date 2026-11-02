/*
 * shooter_step5.c -- Shooter #3 ขั้นต่อ: ระดับความยาก + ปล่อยศัตรูเป็นจังหวะ
 * (ฉบับฝึก เติมช่องว่าง)
 *
 * Twin of shooter_step5.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/shooter_step5.py).
 *
 * ต่อจาก step4: ยานมีน้ำหนัก (เร่ง ไถล เพดานความเร็ว) ยิงกระสุนจากชุดหมุนเวียน 6 นัด
 * ศัตรูเป็นชุดหมุนเวียน 8 ตัว ปล่อยตาม spawn_timer และตารางโหมด MODES เลือกด้วย
 * MODE ช่องว่าง 3 จุดยังเป็นแบบที่ส่งให้ฝึก: MODES มีแถว FREE แถวเดียว,
 * spawn_enemy() เป็น pass และยานชนศัตรูยังเป็น pass จึงยังไม่มีศัตรูออกมาเลย
 * บังคับยานด้วยซ้าย-ขวา ยิงด้วย A หรือขึ้น กด Back เพื่อออก
 *
 * Step 4 plus difficulty: a ship with weight (accelerate, slide, a
 * speed cap) fires from a pool of 6 bullets; the enemies are a pool of
 * 8, released by a spawn timer, and the MODES table holds the
 * difficulty. The three blanks are as the worksheet ships them: MODES
 * has only the FREE row, spawn_enemy() is `pass` and the ship/enemy
 * collision is `pass`, so no enemy is ever released. Steer with
 * left/right, fire with A or up; BACK quits.
 *
 * C twin of the Example Explorer program devi-shooter-5
 * (py/devi-shooter-5.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the module's globals (boxes, pools, mode row, score) are one
 *     file-scope g_shooter, which on_frame() reads (C9);
 *   - MODES is a static const table of shooter_mode_t (modes, inside
 *     reset_state, its one reader); MODE picks its row;
 *   - find_free_bullet() returns a pointer to the Box, NULL for None;
 *   - Python's floor division of ship.w by 2 is integer division of the int 62 (both operands
 *     positive, so it equals floor division);
 *   - on_frame()'s `return False` is *out_stop = true;
 *   - `import random` has nothing to map: only the blank would call it;
 *   - title()'s BACK (SystemExit) and Stop (KeyboardInterrupt) end
 *     main with 0; a call that raises in MicroPython returns its
 *     status;
 *   - main prints the joystick line that the kits' joystick.init()
 *     prints inside title() (the reTerminal prints nothing).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

enum { /* the pools */
    MAX_BULLETS = 6,
    MAX_ENEMIES = 8,
    ENEMY_COLOR_COUNT = 3
};
enum { /* the ship and the HUD */
    SHIP_X = 365,
    SHIP_Y = 352,
    SHIP_W = 62,
    SHIP_H = 24,
    HUD_X = 10,
    HUD_Y = 8,
    HUD_CHARS = 64 /* the mode name and two int32_t, NUL included */
};
enum { /* bullets and enemies */
    BULLET_W = 6,
    BULLET_H = 14,
    ENEMY_W = 30,
    ENEMY_H = 24,
    PARK_X = 0,
    PARK_Y = -50,
    FLYING_MIN_Y = -20, /* a bullet at or below this y is in flight */
    BULLET_STEP_PX = 9,
    MUZZLE_Y = 340,
    MUZZLE_HALF_W = 3,
    FIRE_COOLDOWN = 8
};
enum { /* the shrunk hitboxes: a hit must reach into the shape */
    BULLET_HIT_W = 4,
    BULLET_HIT_H = 10,
    ENEMY_HIT_W = 26,
    ENEMY_HIT_H = 16
};
enum { /* timing, the first wave and the end */
    FPS = 30,
    FIRST_SPAWN_TIMER = 8,
    FIRST_WAVE = 3,
    GAME_OVER_X = 320,
    GAME_OVER_Y = 180
};
enum { /* the modes: the worksheet ships the FREE row only */
    MODE_COUNT = 1,
    MODE = 0
};

typedef struct {
    const char *name;
    int32_t spawn_delay;
    bento_real_t speed_mul;
    int32_t start_lives;
    bool lose_when_pass;
    bool lose_when_hit;
} shooter_mode_t;

typedef struct {
    bento_real_t x;
    bento_real_t y;
    bento_real_t width;
    bento_real_t height;
} rect_t;

typedef struct {
    bento_game_t game;
    shooter_mode_t mode;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_game_box_t bullets[MAX_BULLETS];
    bento_game_box_t enemies[MAX_ENEMIES];
    bool enemy_active[MAX_ENEMIES];
    bento_real_t enemy_speed[MAX_ENEMIES];
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t score;
    int32_t lives;
    int32_t fire_cooldown;
    int32_t spawn_timer;
} shooter_t;

/* The game's state: the frame callback reads it, and it is too big for
 * the 8 KB program stack. File scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

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

/* AABB with sizes of our own, for the shrunk hitboxes: touching edges
 * do not count */
static bool boxes_overlap(const rect_t *first, const rect_t *second)
{
    return (first->x < (second->x + second->width)) &&
           ((first->x + first->width) > second->x) &&
           (first->y < (second->y + second->height)) &&
           ((first->y + first->height) > second->y);
}

static bento_status_t show_hud(shooter_t *shooter)
{
    char text[HUD_CHARS];

    (void)snprintf(text, sizeof(text), "%s   Score: %d   Lives: %d",
                   shooter->mode.name, (int)shooter->score,
                   (int)shooter->lives);
    return bento_game_text_set(&shooter->game, &shooter->hud, text);
}

static bento_status_t add_ship_and_hud(shooter_t *shooter)
{
    char text[HUD_CHARS];
    bento_status_t status = bento_game_box_create(
        &shooter->game, &shooter->ship, (bento_real_t)SHIP_X,
        (bento_real_t)SHIP_Y, (bento_real_t)SHIP_W,
        (bento_real_t)SHIP_H, BENTO_GAME_GREEN);

    shooter->ship_x = (bento_real_t)SHIP_X;
    shooter->ship_speed = (bento_real_t)0.0;
    shooter->score = 0;
    shooter->lives = shooter->mode.start_lives;
    shooter->fire_cooldown = 0;
    shooter->spawn_timer = FIRST_SPAWN_TIMER;
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "%s   Score: 0   Lives: %d",
                       shooter->mode.name, (int)shooter->lives);
        status =
            bento_game_text_create(&shooter->game, &shooter->hud, text,
                                   HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* make a pool of hidden boxes once, parked above the screen */
static bento_status_t add_pool(bento_game_t *game,
                               bento_game_box_t *pool, uint32_t count,
                               const rect_t *size, uint32_t color)
{
    bento_status_t status = BENTO_OK;

    for (uint32_t i = 0U; (i < count) && (status == BENTO_OK); i++) {
        status = bento_game_box_create(
            game, &pool[i], (bento_real_t)PARK_X, (bento_real_t)PARK_Y,
            size->width, size->height, color);
    }
    for (uint32_t i = 0U; (i < count) && (status == BENTO_OK); i++) {
        status = bento_game_box_hide(game, &pool[i]);
    }
    return status;
}

static bento_status_t build_play_screen(shooter_t *shooter)
{
    static const uint32_t enemy_colors[ENEMY_COLOR_COUNT] = {
        BENTO_GAME_RED, BENTO_GAME_ORANGE, BENTO_GAME_PINK};
    const rect_t bullet_size = {(bento_real_t)0, (bento_real_t)0,
                                (bento_real_t)BULLET_W,
                                (bento_real_t)BULLET_H};
    const rect_t enemy_size = {(bento_real_t)0, (bento_real_t)0,
                               (bento_real_t)ENEMY_W,
                               (bento_real_t)ENEMY_H};
    bento_status_t status = add_ship_and_hud(shooter);

    if (status == BENTO_OK) {
        status = add_pool(&shooter->game, shooter->bullets,
                          (uint32_t)MAX_BULLETS, &bullet_size,
                          BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status = add_pool(&shooter->game, shooter->enemies,
                          (uint32_t)MAX_ENEMIES, &enemy_size,
                          enemy_colors[0]);
    }
    for (uint32_t i = 0U; i < (uint32_t)MAX_ENEMIES; i++) {
        shooter->enemy_active[i] = false;
        shooter->enemy_speed[i] = (bento_real_t)0.0;
    }
    return status;
}

static bento_game_box_t *find_free_bullet(shooter_t *shooter)
{
    bento_game_box_t *free_bullet = NULL;

    for (uint32_t i = 0U;
         (i < (uint32_t)MAX_BULLETS) && (free_bullet == NULL); i++) {
        if (shooter->bullets[i].y < (bento_real_t)FLYING_MIN_Y) {
            free_bullet = &shooter->bullets[i];
        }
    }
    return free_bullet;
}

/* blank 2: find a free enemy, borrow it, colour it from ENEMY_COLORS
 * (enemy_colors in build_play_screen; it goes back to file scope once
 * this blank reads it too), place it above the screen, show it and give it a speed times the
 * mode's speed_mul. Still `pass`, so it releases nothing. */
static void spawn_enemy(void)
{
}

/* give an enemy back to the pool */
static bento_status_t park_enemy(shooter_t *shooter, uint32_t index)
{
    bento_status_t status = BENTO_OK;

    shooter->enemy_active[index] = false;
    status =
        bento_game_box_hide(&shooter->game, &shooter->enemies[index]);
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &shooter->game, &shooter->enemies[index],
            (bento_real_t)PARK_X, (bento_real_t)PARK_Y);
    }
    return status;
}

/* the first three enemies at once, as the full game does */
static void release_first_wave(void)
{
    for (uint32_t i = 0U; i < (uint32_t)FIRST_WAVE; i++) {
        spawn_enemy();
    }
}

/* count down one frame; at 0 release one enemy and start again */
static void count_spawn(shooter_t *shooter)
{
    if (shooter->spawn_timer > 0) {
        shooter->spawn_timer -= 1;
    } else {
        spawn_enemy();
        shooter->spawn_timer = shooter->mode.spawn_delay;
    }
}

/* accelerate, slide, cap the speed, keep the ship on the screen */
static bento_status_t steer_ship(shooter_t *shooter,
                                 const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t friction = (bento_real_t)0.80;
    const bento_real_t right_edge =
        (bento_real_t)shooter->game.width - shooter->ship.w;
    bento_real_t next_x = (bento_real_t)0;

    if (keys->left) {
        shooter->ship_speed -= accel;
    } else if (keys->right) {
        shooter->ship_speed += accel;
    } else {
        shooter->ship_speed *= friction;
    }
    if (shooter->ship_speed > max_speed) {
        shooter->ship_speed = max_speed;
    }
    if (shooter->ship_speed < -max_speed) {
        shooter->ship_speed = -max_speed;
    }
    next_x = shooter->ship_x + shooter->ship_speed;
    if (next_x > right_edge) {
        next_x = right_edge;
    }
    if (next_x < (bento_real_t)0) {
        next_x = (bento_real_t)0;
    }
    shooter->ship_x = next_x;
    return bento_game_box_move_to(&shooter->game, &shooter->ship,
                                  shooter->ship_x,
                                  (bento_real_t)SHIP_Y);
}

/* the bullet leaves from the ship's middle */
static bento_status_t launch_bullet(shooter_t *shooter,
                                    bento_game_box_t *bullet)
{
    const int32_t half_w = SHIP_W / 2;
    const bento_real_t muzzle_x =
        shooter->ship_x + (bento_real_t)half_w;
    bento_status_t status = bento_game_box_show(&shooter->game, bullet);

    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&shooter->game, bullet,
                                        muzzle_x -
                                            (bento_real_t)MUZZLE_HALF_W,
                                        (bento_real_t)MUZZLE_Y);
    }
    if (status == BENTO_OK) {
        bento_game_sfx(&shooter->game, "fire");
        shooter->fire_cooldown = FIRE_COOLDOWN;
    }
    return status;
}

/* A or up fires a free bullet once the cooldown is over */
static bento_status_t fire(shooter_t *shooter,
                           const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    shooter->fire_cooldown =
        (shooter->fire_cooldown > 1) ? (shooter->fire_cooldown - 1) : 0;
    if ((keys->a || keys->up) && (shooter->fire_cooldown == 0)) {
        bento_game_box_t *bullet = find_free_bullet(shooter);

        if (bullet != NULL) {
            status = launch_bullet(shooter, bullet);
        }
    }
    return status;
}

/* a flying bullet moves up; one that leaves the top is hidden */
static bento_status_t move_bullet(shooter_t *shooter,
                                  bento_game_box_t *bullet)
{
    bento_status_t status = BENTO_OK;

    if (bullet->y >= (bento_real_t)FLYING_MIN_Y) {
        status = bento_game_box_move_to(
            &shooter->game, bullet, bullet->x,
            bullet->y - (bento_real_t)BULLET_STEP_PX);
        if ((status == BENTO_OK) &&
            (bullet->y < (bento_real_t)FLYING_MIN_Y)) {
            status = bento_game_box_hide(&shooter->game, bullet);
        }
    }
    return status;
}

static bento_status_t move_bullets(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;

    for (uint32_t i = 0U;
         (i < (uint32_t)MAX_BULLETS) && (status == BENTO_OK); i++) {
        status = move_bullet(shooter, &shooter->bullets[i]);
    }
    return status;
}

/* the last life is gone: the sound, the words, and run() ends */
static bento_status_t show_game_over(shooter_t *shooter, bool *out_stop)
{
    bento_game_text_t over;
    bento_status_t status = BENTO_OK;

    bento_game_text_init(&over);
    bento_game_sfx(&shooter->game, "gameover");
    status = bento_game_text_create(&shooter->game, &over, "GAME OVER",
                                    GAME_OVER_X, GAME_OVER_Y,
                                    BENTO_GAME_RED);
    if (status == BENTO_OK) {
        *out_stop = true;
    }
    return status;
}

/* an enemy left the bottom: park it; only a mode that punishes a pass
 * takes a life */
static bento_status_t pass_bottom(shooter_t *shooter, uint32_t index,
                                  bool *out_stop)
{
    bento_status_t status = park_enemy(shooter, index);

    if ((status == BENTO_OK) && shooter->mode.lose_when_pass) {
        shooter->lives -= 1;
        status = show_hud(shooter);
        if ((status == BENTO_OK) && (shooter->lives <= 0)) {
            status = show_game_over(shooter, out_stop);
        }
    }
    return status;
}

/* the bullet scores: the sound, the HUD, both go back to their pools */
static bento_status_t score_hit(shooter_t *shooter, uint32_t index,
                                bento_game_box_t *bullet)
{
    bento_status_t status = BENTO_OK;

    shooter->score += 1;
    bento_game_sfx(&shooter->game, "hit");
    status = show_hud(shooter);
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&shooter->game, bullet,
                                        (bento_real_t)PARK_X,
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

/* the first flying bullet inside the enemy's shrunk box scores */
static bento_status_t hit_by_bullet(shooter_t *shooter, uint32_t index,
                                    bool *out_was_hit)
{
    const bento_game_box_t *enemy = &shooter->enemies[index];
    const rect_t enemy_box = {enemy->x, enemy->y,
                              (bento_real_t)ENEMY_HIT_W,
                              (bento_real_t)ENEMY_HIT_H};
    bento_status_t status = BENTO_OK;

    for (uint32_t i = 0U; (i < (uint32_t)MAX_BULLETS) && !*out_was_hit;
         i++) {
        bento_game_box_t *bullet = &shooter->bullets[i];
        const rect_t bullet_box = {bullet->x, bullet->y,
                                   (bento_real_t)BULLET_HIT_W,
                                   (bento_real_t)BULLET_HIT_H};

        if ((bullet->y >= (bento_real_t)FLYING_MIN_Y) &&
            boxes_overlap(&bullet_box, &enemy_box)) {
            status = score_hit(shooter, index, bullet);
            *out_was_hit = true;
        }
    }
    return status;
}

/* one enemy in use: fall, then the bottom, a bullet, or the ship */
static bento_status_t step_enemy(shooter_t *shooter, uint32_t index,
                                 bool *out_stop)
{
    bento_game_box_t *enemy = &shooter->enemies[index];
    bool was_hit = false;
    bento_status_t status =
        bento_game_box_move_to(&shooter->game, enemy, enemy->x,
                               enemy->y + shooter->enemy_speed[index]);

    if (status != BENTO_OK) {
        /* the status ends the frame */
    } else if (enemy->y > (bento_real_t)shooter->game.height) {
        status = pass_bottom(shooter, index, out_stop);
    } else {
        status = hit_by_bullet(shooter, index, &was_hit);
        /* blank 3: when not hit, the ship against enemy_box: park the
         * enemy, take a life when lose_when_hit, "gameover" with GAME
         * OVER and return False at 0 lives, else "explode". Still
         * `pass`. */
    }
    return status;
}

/* each enemy in use, until a status or the end of the game */
static bento_status_t step_enemies(shooter_t *shooter, bool *out_stop)
{
    bento_status_t status = BENTO_OK;

    for (uint32_t index = 0U; (index < (uint32_t)MAX_ENEMIES) &&
                              (status == BENTO_OK) && !*out_stop;
         index++) {
        if (shooter->enemy_active[index]) {
            status = step_enemy(shooter, index, out_stop);
        }
    }
    return status;
}

/* One frame: the spawn timer, the ship, firing, bullets, enemies */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_shooter (file scope, C9) */
    bento_game_keys_init(&keys);
    /* keys() never fails for valid pointers */
    (void)bento_game_keys(&shooter->game, &keys);
    count_spawn(shooter);
    status = steer_ship(shooter, &keys);
    if (status == BENTO_OK) {
        status = fire(shooter, &keys);
    }
    if (status == BENTO_OK) {
        status = move_bullets(shooter);
    }
    if (status == BENTO_OK) {
        status = step_enemies(shooter, out_stop);
    }
    return status;
}

static void reset_state(shooter_t *shooter)
{
    /* blank 1: the student adds the EASY and DIFFICULT rows */
    static const shooter_mode_t modes[MODE_COUNT] = {
        {"FREE SHOOTER", 16, (bento_real_t)0.85, 99, false, false}};

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    shooter->mode = modes[MODE];
    bento_game_box_init(&shooter->ship);
    bento_game_text_init(&shooter->hud);
    for (uint32_t i = 0U; i < (uint32_t)MAX_BULLETS; i++) {
        bento_game_box_init(&shooter->bullets[i]);
    }
    for (uint32_t i = 0U; i < (uint32_t)MAX_ENEMIES; i++) {
        bento_game_box_init(&shooter->enemies[i]);
    }
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bool finished = false;
    bento_status_t status = BENTO_OK;

    reset_state(shooter);
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    /* the kits' joystick.init() prints this line; the reTerminal's
     * joystick.py prints nothing */
    if ((shooter->game.joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_play_screen(shooter);
        if (status == BENTO_OK) {
            release_first_wave();
            status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
