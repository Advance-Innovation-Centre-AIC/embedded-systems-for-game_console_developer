/*
 * s13_16_kit_characters.c -- คาบ 13 (คาบพิเศษ): ชุดตัวละครสำเร็จรูป ยกมาจากเกมเต็มของจริง
 *
 * ตัวละคร 1 ตัว = ภาพ (Sprite) + สถานะ + ฟังก์ชันก้าวทีละเฟรม สร้างครบทุกตัวครั้งเดียว โชว์ทีละตัว
 * กด Start แล้วใช้ซ้าย/ขวาเปลี่ยนตัวละคร A = ท่าประจำตัว
 * ยาน: บินซ้าย-ขวาเอง UP/DOWN ขับ A ยิง ดูกระสุนพุ่งเฉียงตามแรงยาน
 * ศัตรู: A = ระเบิดตัวล่างสุด ดูสีระเบิดตามชนิด · นก: A/UP กระพือ ดูช่วง 4 วินาทีที่ตกพื้นไม่ตาย
 * งู: A เลี้ยวตามเข็มนาฬิกา UP/DOWN เลี้ยวแบบเกมจริง ดูหัวหันตามทิศ · ไม้: UP/DOWN ขยับ A เสิร์ฟ
 *
 * The ready-made characters of the full games, shown one at a time: the ship with weight
 * (accelerate, glide, a speed ceiling) whose bullets carry its sideways speed, the three
 * enemies with their two-frame explosions coloured by kind, the bird with gravity, flaps and
 * a 4 second grace on the ground, the sprite snake whose head turns with its direction, and
 * the paddle that puts spin on the ball. Left and right change the character; A is its move.
 * Every widget is made once before the loop and only shown or hidden while playing.
 *
 * Twin of s13_16_kit_characters.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_16_kit_characters.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - each character's dict is a struct, and all five live in one kit_t at file scope (large);
 *   - the "all" and "base" widget lists are hide_piece() and show_base(), one case per piece;
 *   - PIECES' enter and play functions are chosen by a switch on the piece number;
 *   - a sprite keeps no w and h in bento_game, so the hit tests use bento_game_hit_rect;
 *   - the snake's want (None, "a", "up", "down") and snake_step()'s answer are enums;
 *   - ** 0.5 is py_pow(x, 0.5): powf on the kits, pow on the reTerminal, as MicroPython;
 *   - '%.1f' of a float is bento_fmt_real_precision(), the board's own float formatter.
 */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The pieces, in PIECES order. */
enum {
    PIECE_SHIP = 0,
    PIECE_ENEMY,
    PIECE_BIRD,
    PIECE_SNAKE,
    PIECE_PONG
};

/* The show. */
enum {
    FPS = 30,
    PIECES = 5,
    COLOR_SPACE = 0x10122C, /* สีอวกาศ (shooter_full.py:67) */
    COLOR_SKY = 0x88C1F5,   /* ฟ้าแถบกลางของ flappy_full.py:35 */
    COLOR_LAWN = 0x4CA838,  /* สนามหญ้า (snake_sprite_full.py:25) */
    COLOR_FELT = 0x1C7A34,  /* สักหลาดเขียว (pong_full.py:28) */
    CAPTION_X = 12,
    CAPTION_Y = 8,
    HELPER_Y = 32,
    INFO_Y = 56,
    SAY_CHARS =
        192, /* the longest line: Thai text and three numbers of any size */
    COUNT_CHARS = 12 /* any int32_t with its sign and NUL */
};

/* The ship (shooter_full.py). */
enum {
    SHIP_W = 62,
    SHIP_H = 28,
    SHIP_X0 = 365,
    SHIP_FLOOR = 32, /* the ship flies 32 px above the bottom */
    SHIP_TOP = 24,
    BULLETS = 3, /* โชว์ใช้ 3 นัดให้พองบ (เกมเต็มใช้ 4) */
    BULLET_W = 5,
    BULLET_H = 12,
    BULLET_RADIUS = 2,
    BULLET_PARK_X = -20,
    BULLET_PARK_Y = -50,
    BULLET_LOST =
        -12, /* a bullet this far past an edge goes back to the pool */
    BULLET_ABOVE = 8,
    PHASE_FRAMES =
        24, /* the show holds right, lets go, holds left, lets go: 24 frames each */
    PHASES = 4,
    COLOR_LASER = 0x80F4FF
};

/* The enemies and their explosions. */
enum {
    KINDS = 3,
    BOOMS = 3,
    BOOM_TICKS = 6, /* ค้างกี่เฟรม ครึ่งหลังสลับเป็นวงใหญ่ */
    BOOM_HALF_W = 15,
    BOOM_HALF_H = 12,
    ENEMY_HALF_W = 13,
    ENEMY_HALF_H = 8,
    ENEMY_X0 = 220,
    ENEMY_STEP_X = 160,
    ENEMY_Y0 = 100,
    ENEMY_STEP_Y = 60,
    ENEMY_TOP = -20,
    ENEMY_LEFT = 20,
    ENEMY_RIGHT_GAP = 46,
    FALL_MIN =
        18, /* a fall of 18/20 .. 34/20 px a frame: half the full game */
    FALL_MAX = 34,
    FALL_DIVISOR = 20
};

/* The bird (flappy_full.py). */
enum {
    BIRD_W = 44,
    BIRD_H = 29,
    BIRD_BOX = 36, /* the hit box is kinder than the picture */
    GRASS_H = 20,  /* พื้นหญ้าของนก = เส้นตาย */
    GRACE = 120,   /* 4 วินาทีแรก ที่ 30 fps */
    BIRD_HALF_W =
        BIRD_W / 2, /* floor division, as Python's floor divide */
    BIRD_HALF_H = BIRD_H / 2
};

/* The sprite snake (snake_sprite_full.py). */
enum {
    CELL = 16,
    COLS = BENTO_GAME_WIDTH / CELL,
    ROWS = BENTO_GAME_HEIGHT / CELL,
    SNAKE_POOL = 4, /* พูล 4 = ยาว 5 ท่อนพอดี กินแล้วไม่โต */
    SNAKE_LENGTH = 5,
    SNAKE_CAP =
        SNAKE_POOL +
        2, /* one more for the head added before the tail goes */
    SNAKE_COL0 = 10,
    SNAKE_ROW0 = 12,
    STEP_FRAMES = 4 /* ก้าวทุก 4 เฟรม = 133 ms */
};

/* The paddle and ball (pong_full.py). */
enum {
    PADDLE_W = 14,
    PADDLE_H = 90,
    PADDLE_X = 14,
    BALL_SIZE = 14,
    SERVE_SPREAD =
        22, /* a serve leaves at -2.2 .. 2.2 px a frame up or down */
    SERVE_DIVISOR = 10,
    MISS_PX = 25 /* a ball 25 px past the left edge is a miss */
};

typedef enum {
    WANT_NONE = 0,
    WANT_A,
    WANT_UP,
    WANT_DOWN
} want_t;

typedef enum {
    STEP_NONE = 0,
    STEP_EAT,
    STEP_WALL,
    STEP_SELF
} step_t;

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_sprite_t sprite;
    bento_game_box_t bullets[BULLETS];
    bool flying[BULLETS];
    bento_real_t bullet_x[BULLETS];
    bento_real_t bullet_y[BULLETS];
    bento_real_t bullet_vx[BULLETS];
    bento_real_t x;
    bento_real_t y;
    bento_real_t vx;
    bento_real_t vy;
    int32_t ticks;
} ship_t;

typedef struct {
    bento_game_sprite_t enemies[KINDS];
    bento_real_t fall[KINDS];
    bento_game_sprite_t booms[BOOMS];
    int32_t boom_ticks[BOOMS];
    int32_t boom_kind[BOOMS];
    int32_t bursts;
} enemies_t;

typedef struct {
    bento_game_box_t grass;
    bento_game_sprite_t sprite;
    int32_t x;
    int32_t play_h;
    bento_real_t y;
    bento_real_t vy;
    bool started;
    int32_t grace;
} bird_t;

typedef struct {
    cell_t body[SNAKE_CAP];
    int32_t length;
    int32_t dx;
    int32_t dy;
    int32_t shown;
    bento_game_sprite_t segments[SNAKE_POOL];
    bento_game_sprite_t head;
    bento_game_sprite_t food;
    cell_t food_cell;
    const char *head_name;
    int32_t ticks;
    want_t want;
    int32_t score;
} snake_t;

typedef struct {
    bento_game_sprite_t pad;
    bento_game_sprite_t ball;
    bento_real_t pad_y;
    bento_real_t pad_vy;
    bento_real_t spin;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
} pong_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t background;
    ship_t ship;
    enemies_t enemies;
    bird_t bird;
    snake_t snake;
    pong_t pong;
    bento_game_text_t caption;
    bento_game_text_t helper;
    bento_game_text_t info;
    int32_t piece;
    bool said;            /* false = nothing said yet (Python's None) */
    char last[SAY_CHARS]; /* the last text written to info */
} kit_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern kit_t g_kit;
kit_t g_kit;

/* The explosion pictures of each enemy kind: the small flash, then the big ring. */
static const char *const BOOM_NAMES[KINDS][2] = {
    {"boom1", "boom2"}, {"boom3", "boom4"}, {"boom5", "boom6"}};

/* ---- small helpers ---- */

static bento_real_t clamp_real(bento_real_t value, bento_real_t low,
                               bento_real_t high)
{
    const bento_real_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static bento_real_t absolute(bento_real_t value)
{
    return (value < (bento_real_t)0) ? -value : value;
}

/* -1, 0 or +1 for a pair of opposite keys. */
static int32_t key_axis(bool minus, bool plus)
{
    int32_t axis = 0;

    if (plus) {
        axis += 1;
    }
    if (minus) {
        axis -= 1;
    }
    return axis;
}

/* Write the info line only when its text changes. */
static bento_status_t say(kit_t *kit, const char *text)
{
    bento_status_t status = BENTO_OK;

    if (!kit->said || (strcmp(text, kit->last) != 0)) {
        status = bento_game_text_set(&kit->game, &kit->info, text);
        (void)snprintf(kit->last, sizeof(kit->last), "%s", text);
        kit->said = true;
    }
    return status;
}

/* ---- the ship with weight, and bullets that carry its speed ---- */

/* Held = speed up by ACCEL; released = glide with friction; then the speed ceiling. */
static bento_real_t axis_speed(bento_real_t speed, bool minus,
                               bool plus)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t friction = (bento_real_t)0.80;
    bento_real_t next = speed;

    if (minus && !plus) {
        next -= accel;
    } else if (plus && !minus) {
        next += accel;
    } else {
        next *= friction;
    }
    return clamp_real(next, -max_speed, max_speed);
}

/* Bullets fly up and sideways; one past an edge goes back to the pool. */
/* One bullet flies up and sideways; past an edge it goes back to the pool. */
static bento_status_t step_bullet(kit_t *kit, int32_t j)
{
    const bento_real_t bullet_vy = (bento_real_t)8.0;
    ship_t *ship = &kit->ship;
    bento_status_t status;

    ship->bullet_y[j] -= bullet_vy;
    ship->bullet_x[j] += ship->bullet_vx[j];
    if ((ship->bullet_y[j] < (bento_real_t)BULLET_LOST) ||
        (ship->bullet_x[j] < (bento_real_t)BULLET_LOST) ||
        (ship->bullet_x[j] > (bento_real_t)kit->game.width)) {
        ship->flying[j] = false;
        status = bento_game_box_hide(&kit->game, &ship->bullets[j]);
    } else {
        status = bento_game_box_move_to(&kit->game, &ship->bullets[j],
                                        ship->bullet_x[j],
                                        ship->bullet_y[j]);
    }
    return status;
}

static bento_status_t step_bullets(kit_t *kit)
{
    bento_status_t status = BENTO_OK;
    int32_t j;

    for (j = 0; (status == BENTO_OK) && (j < BULLETS); j++) {
        if (kit->ship.flying[j]) {
            status = step_bullet(kit, j);
        }
    }
    return status;
}

static bento_status_t step_ship(kit_t *kit, bool left, bool right,
                                bool up, bool down)
{
    ship_t *ship = &kit->ship;
    const bento_real_t x_max = (bento_real_t)(kit->game.width - SHIP_W);
    const bento_real_t y_max =
        (bento_real_t)(BENTO_GAME_HEIGHT - SHIP_H - SHIP_FLOOR);
    bento_real_t x;
    bento_real_t y;
    bento_status_t status;

    ship->vx = axis_speed(ship->vx, left, right);
    ship->vy = axis_speed(ship->vy, up, down);
    x = ship->x + ship->vx;
    y = ship->y + ship->vy;
    if ((x < (bento_real_t)0) || (x > x_max)) {
        /* an edge stops that axis, so the ship does not press against it */
        x = clamp_real(x, (bento_real_t)0, x_max);
        ship->vx = (bento_real_t)0;
    }
    if ((y < (bento_real_t)SHIP_TOP) || (y > y_max)) {
        y = clamp_real(y, (bento_real_t)SHIP_TOP, y_max);
        ship->vy = (bento_real_t)0;
    }
    ship->x = x;
    ship->y = y;
    status = bento_game_sprite_move_to(&kit->game, &ship->sprite, x, y);
    if (status == BENTO_OK) {
        status = step_bullets(kit);
    }
    return status;
}

/* A free bullet from the pool, if any; none free = no shot (no new bullet is made). */
/* Bullet j leaves the middle of the ship, carrying 55% of its sideways speed. */
static bento_status_t launch_bullet(kit_t *kit, int32_t j)
{
    const bento_real_t inherit = (bento_real_t)0.55;
    ship_t *ship = &kit->ship;
    const bento_real_t middle =
        ship->x + ((bento_real_t)SHIP_W / (bento_real_t)2);
    bento_status_t status;

    ship->flying[j] = true;
    bento_game_sfx(&kit->game, "fire");
    ship->bullet_x[j] = middle - (bento_real_t)2;
    ship->bullet_y[j] = ship->y - (bento_real_t)BULLET_ABOVE;
    ship->bullet_vx[j] = ship->vx * inherit;
    status =
        bento_game_box_move_to(&kit->game, &ship->bullets[j],
                               ship->bullet_x[j], ship->bullet_y[j]);
    if (status == BENTO_OK) {
        status = bento_game_box_show(&kit->game, &ship->bullets[j]);
    }
    return status;
}

/* A free bullet from the pool, if any; none free = no shot (no new bullet is made). */
static bento_status_t fire_ship(kit_t *kit)
{
    bento_status_t status = BENTO_OK;
    bool fired = false;
    int32_t j;

    for (j = 0; (j < BULLETS) && !fired; j++) {
        if (!kit->ship.flying[j]) {
            status = launch_bullet(kit, j);
            fired = true;
        }
    }
    return status;
}

static bento_status_t make_ship(kit_t *kit)
{
    ship_t *ship = &kit->ship;
    bento_status_t status =
        bento_game_sprite_create(&kit->game, &ship->sprite, "ship",
                                 (bento_real_t)0, (bento_real_t)0);
    int32_t j;

    /* light-blue bars with a white edge, made once and hidden */
    for (j = 0; (status == BENTO_OK) && (j < BULLETS); j++) {
        bento_game_box_init(&ship->bullets[j]);
        status = bento_game_box_create_styled(
            &kit->game, &ship->bullets[j], (bento_real_t)BULLET_PARK_X,
            (bento_real_t)BULLET_PARK_Y, (bento_real_t)BULLET_W,
            (bento_real_t)BULLET_H, COLOR_LASER,
            (int32_t)BENTO_GAME_WHITE, BULLET_RADIUS, 0);
        if (status == BENTO_OK) {
            status = bento_game_box_hide(&kit->game, &ship->bullets[j]);
        }
    }
    return status;
}

/* ---- three enemies and their two-frame explosions ---- */

static bento_status_t make_enemies(kit_t *kit)
{
    static const char *const enemy_names[KINDS] = {"enemy", "enemy2",
                                                   "enemy3"};
    enemies_t *enemies = &kit->enemies;
    bento_status_t status = BENTO_OK;
    int32_t i;

    /* the full game makes the pool as "enemy" and changes the look when one appears */
    for (i = 0; (status == BENTO_OK) && (i < KINDS); i++) {
        status = bento_game_sprite_create(
            &kit->game, &enemies->enemies[i], "enemy", (bento_real_t)0,
            (bento_real_t)0);
        if (status == BENTO_OK) {
            status = bento_game_sprite_frame(
                &kit->game, &enemies->enemies[i], enemy_names[i]);
        }
    }
    if (status == BENTO_OK) {
        status = bento_game_pool(&kit->game, enemies->booms,
                                 (uint32_t)BOOMS, "boom1");
    }
    return status;
}

/* Explosion b shows the first frame of this kind, centred on (center_x, center_y). */
static bento_status_t place_boom(kit_t *kit, int32_t b,
                                 bento_real_t center_x,
                                 bento_real_t center_y, int32_t kind)
{
    enemies_t *enemies = &kit->enemies;
    bento_game_sprite_t *sprite = &enemies->booms[b];
    bento_status_t status;

    enemies->boom_ticks[b] = BOOM_TICKS;
    enemies->boom_kind[b] = kind;
    status = bento_game_sprite_frame(&kit->game, sprite,
                                     BOOM_NAMES[kind][0]);
    if (status == BENTO_OK) {
        /* centre the 29 x 24 explosion */
        status = bento_game_sprite_move_to(
            &kit->game, sprite, center_x - (bento_real_t)BOOM_HALF_W,
            center_y - (bento_real_t)BOOM_HALF_H);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_show(&kit->game, sprite);
    }
    return status;
}

static bento_status_t boom(kit_t *kit, bento_real_t center_x,
                           bento_real_t center_y, int32_t kind)
{
    bento_status_t status = BENTO_OK;
    bool placed = false;
    int32_t b;

    bento_game_sfx(&kit->game, "explode");
    for (b = 0; (b < BOOMS) && !placed; b++) {
        if (kit->enemies.boom_ticks[b] == 0) {
            status = place_boom(kit, b, center_x, center_y, kind);
            placed = true;
        }
    }
    return status;
}

/* One running explosion: the big ring at half time, hidden at the end. */
static bento_status_t step_boom(kit_t *kit, int32_t b)
{
    enemies_t *enemies = &kit->enemies;
    bento_status_t status = BENTO_OK;

    enemies->boom_ticks[b] -= 1;
    if (enemies->boom_ticks[b] == 0) {
        status = bento_game_sprite_hide(&kit->game, &enemies->booms[b]);
    } else if (enemies->boom_ticks[b] == (BOOM_TICKS / 2)) {
        /* the second half shows the big ring */
        status = bento_game_sprite_frame(
            &kit->game, &enemies->booms[b],
            BOOM_NAMES[enemies->boom_kind[b]][1]);
    } else {
        /* the flash goes on */
    }
    return status;
}

static bento_status_t step_booms(kit_t *kit)
{
    bento_status_t status = BENTO_OK;
    int32_t b;

    for (b = 0; (status == BENTO_OK) && (b < BOOMS); b++) {
        if (kit->enemies.boom_ticks[b] != 0) {
            status = step_boom(kit, b);
        }
    }
    return status;
}

/* ---- the bird: gravity, flaps, and a grace on the ground at the start ---- */

/* The bird floats in the middle until its first flap. */
static bento_status_t reset_bird(kit_t *kit)
{
    bird_t *bird = &kit->bird;
    const int32_t middle =
        bird->play_h / 2; /* floor division, as Python's floor divide */

    bird->y = (bento_real_t)middle;
    bird->vy = (bento_real_t)0;
    bird->started = false;
    bird->grace = 0;
    return bento_game_sprite_move_to(
        &kit->game, &bird->sprite,
        (bento_real_t)(bird->x - BIRD_HALF_W),
        (bento_real_t)(middle - BIRD_HALF_H));
}

/* Falls and flies one frame; *out_alive false when it reached the ground after the grace. */
static bento_status_t step_bird(kit_t *kit, bool flap,
                                bento_real_t frame_s, bool *out_alive)
{
    const bento_real_t gravity = (bento_real_t)1050.0;
    const bento_real_t flap_speed = (bento_real_t)-340.0;
    bird_t *bird = &kit->bird;
    const bento_real_t half = (bento_real_t)BIRD_BOX / (bento_real_t)2;
    bento_real_t change;
    bento_status_t status = BENTO_OK;

    *out_alive = true;
    if (flap) {
        bird->vy = flap_speed;
        bento_game_sfx(&kit->game, "flap");
    }
    if (flap && !bird->started) {
        /* the first flap starts the flight and the grace */
        bird->started = true;
        bird->grace = GRACE;
    }
    if (bird->started) {
        bird->grace = (bird->grace > 0) ? (bird->grace - 1) : 0;
        change = gravity * frame_s;
        bird->vy += change;
        change = bird->vy * frame_s;
        bird->y += change;
        if (bird->y < half) {
            /* the ceiling holds it; no harm */
            bird->y = half;
            bird->vy = (bento_real_t)0;
        }
        if (bird->y > ((bento_real_t)bird->play_h - half)) {
            *out_alive = (bird->grace != 0);
            /* during the grace it just stops on the ground */
            bird->y = (bento_real_t)bird->play_h - half;
            bird->vy = (bento_real_t)0;
        }
        if (*out_alive) {
            status = bento_game_sprite_move_to(
                &kit->game, &bird->sprite,
                (bento_real_t)(bird->x - BIRD_HALF_W),
                (bento_real_t)((int32_t)bird->y - BIRD_HALF_H));
        } else {
            bento_game_sfx(&kit->game, "fall");
        }
    }
    return status;
}

/* ---- the sprite snake whose head turns with its direction ---- */

static const char *head_name(int32_t dx, int32_t dy)
{
    const char *name = "snake_head_d";

    if (dx > 0) {
        name = "snake_head_r";
    } else if (dx < 0) {
        name = "snake_head_l";
    } else if (dy < 0) {
        name = "snake_head_u";
    } else {
        /* (0, 1): down */
    }
    return name;
}

static bool on_snake(const snake_t *snake, cell_t cell)
{
    bool found = false;
    int32_t i;

    for (i = 0; (i < snake->length) && !found; i++) {
        found = (snake->body[i].col == cell.col) &&
                (snake->body[i].row == cell.row);
    }
    return found;
}

/* A turn of 90 degrees at a time, never straight back. */
static void turn_snake(snake_t *snake, bool up, bool down)
{
    if (up && (snake->dy == 0)) {
        snake->dx = 0;
        snake->dy = -1;
    } else if (down && (snake->dy == 0)) {
        snake->dx = 0;
        snake->dy = 1;
    } else {
        /* the show never steers left or right with these keys */
    }
}

/* Draw again until the food is on a cell the snake does not cover. */
static bento_status_t place_food(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    bento_status_t status = BENTO_OK;
    bool placed = false;

    while ((status == BENTO_OK) && !placed) {
        cell_t cell = {0, 0};

        status =
            bento_random_randint(&kit->random, 0, COLS - 1, &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&kit->random, 0, ROWS - 1,
                                          &cell.row);
        }
        if ((status == BENTO_OK) && !on_snake(snake, cell)) {
            snake->food_cell = cell;
            status = bento_game_sprite_move_to(
                &kit->game, &snake->food,
                (bento_real_t)(cell.col * CELL),
                (bento_real_t)(cell.row * CELL));
            placed = true;
        }
    }
    return status;
}

/* Segment i shows body cell i + 1 while i < count; past count it is hidden. Only a segment
 * that changes between shown and hidden is sent a show or hide. */
static bento_status_t draw_segment(kit_t *kit, int32_t i, int32_t count)
{
    snake_t *snake = &kit->snake;
    bento_game_sprite_t *segment = &snake->segments[i];
    bento_status_t status = BENTO_OK;

    if (i < count) {
        status = bento_game_sprite_move_to(
            &kit->game, segment,
            (bento_real_t)(snake->body[i + 1].col * CELL),
            (bento_real_t)(snake->body[i + 1].row * CELL));
        if ((status == BENTO_OK) && (i >= snake->shown)) {
            status = bento_game_sprite_show(&kit->game, segment);
        }
    } else if (i < snake->shown) {
        status = bento_game_sprite_hide(&kit->game, segment);
    } else {
        /* already hidden */
    }
    return status;
}

/* Body cells after the head go to the pool's sprites. */
static bento_status_t draw_segments(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    const int32_t count = ((snake->length - 1) < SNAKE_POOL)
                              ? (snake->length - 1)
                              : SNAKE_POOL;
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SNAKE_POOL); i++) {
        status = draw_segment(kit, i, count);
    }
    snake->shown = count;
    return status;
}

static bento_status_t draw_snake(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    const char *name = head_name(snake->dx, snake->dy);
    bento_status_t status = BENTO_OK;

    if (strcmp(name, snake->head_name) != 0) {
        /* change the head picture only when the direction changed */
        status =
            bento_game_sprite_frame(&kit->game, &snake->head, name);
        snake->head_name = name;
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(
            &kit->game, &snake->head,
            (bento_real_t)(snake->body[0].col * CELL),
            (bento_real_t)(snake->body[0].row * CELL));
    }
    if (status == BENTO_OK) {
        status = draw_segments(kit);
    }
    return status;
}

static void add_head(snake_t *snake, cell_t head)
{
    int32_t i;

    for (i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = head;
    snake->length += 1;
}

/* One cell forward; *out_step says wall, self, eat or nothing. */
static bento_status_t step_snake(kit_t *kit, step_t *out_step)
{
    snake_t *snake = &kit->snake;
    const cell_t head = {snake->body[0].col + snake->dx,
                         snake->body[0].row + snake->dy};
    const bool inside = (head.col >= 0) && (head.col < COLS) &&
                        (head.row >= 0) && (head.row < ROWS);
    bento_status_t status = BENTO_OK;
    bool ate;

    *out_step = STEP_NONE;
    if (!inside) {
        bento_game_sfx(&kit->game, "die");
        *out_step = STEP_WALL;
    } else if (on_snake(snake, head)) {
        bento_game_sfx(&kit->game,
                       "lose"); /* a bite sounds unlike the wall */
        *out_step = STEP_SELF;
    } else {
        add_head(snake, head);
        ate = (head.col == snake->food_cell.col) &&
              (head.row == snake->food_cell.row);
        if (ate) {
            bento_game_sfx(&kit->game, "eat");
            status = place_food(kit);
            *out_step = STEP_EAT;
        }
        if (!ate || (snake->length > (SNAKE_POOL + 1))) {
            snake->length -=
                1; /* no food, or the pool is full: no growth */
        }
        if (status == BENTO_OK) {
            status = draw_snake(kit);
        }
    }
    return status;
}

static void reset_snake_body(snake_t *snake)
{
    int32_t i;

    for (i = 0; i < SNAKE_LENGTH; i++) {
        snake->body[i].col = SNAKE_COL0 - i;
        snake->body[i].row = SNAKE_ROW0;
    }
    snake->length = SNAKE_LENGTH;
    snake->dx = 1;
    snake->dy = 0;
    snake->shown = 0;
}

static bento_status_t make_snake(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    bento_status_t status =
        bento_game_pool(&kit->game, snake->segments,
                        (uint32_t)SNAKE_POOL, "snake_body");

    reset_snake_body(snake);
    snake->head_name = "snake_head_r";
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(
            &kit->game, &snake->head, "snake_head_r", (bento_real_t)0,
            (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(&kit->game, &snake->food,
                                          "snake_food", (bento_real_t)0,
                                          (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = place_food(kit);
    }
    if (status == BENTO_OK) {
        status = draw_snake(kit);
    }
    return status;
}

/* ---- the paddle with weight that puts spin on the ball ---- */

/* The ball in the middle of the table, served at a small random angle. */
static bento_status_t serve_ball(kit_t *kit, bool to_right)
{
    const bento_real_t first_speed = (bento_real_t)6.2;
    const bento_real_t half_ball =
        (bento_real_t)BALL_SIZE / (bento_real_t)2;
    pong_t *pong = &kit->pong;
    int32_t spread = 0;
    bento_status_t status;

    pong->ball_x =
        ((bento_real_t)kit->game.width / (bento_real_t)2) - half_ball;
    pong->ball_y =
        ((bento_real_t)kit->game.height / (bento_real_t)2) - half_ball;
    pong->ball_vx = to_right ? first_speed : -first_speed;
    status = bento_random_randint(&kit->random, -SERVE_SPREAD,
                                  SERVE_SPREAD, &spread);
    pong->ball_vy = (bento_real_t)spread / (bento_real_t)SERVE_DIVISOR;
    return status;
}

static bento_status_t step_paddle(kit_t *kit, bool up, bool down)
{
    const bento_real_t accel = (bento_real_t)1.5;
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t friction = (bento_real_t)0.78;
    const bento_real_t lowest =
        (bento_real_t)(kit->game.height - PADDLE_H);
    pong_t *pong = &kit->pong;

    if (up && !down) {
        pong->pad_vy -= accel;
    } else if (down && !up) {
        pong->pad_vy += accel;
    } else {
        pong->pad_vy *= friction;
    }
    pong->pad_vy = clamp_real(pong->pad_vy, -max_speed, max_speed);
    pong->pad_y += pong->pad_vy;
    if ((pong->pad_y <= (bento_real_t)0) || (pong->pad_y >= lowest)) {
        /* an edge stops the paddle dead: no bounce */
        pong->pad_y = clamp_real(pong->pad_y, (bento_real_t)0, lowest);
        pong->pad_vy = (bento_real_t)0;
    }
    return bento_game_sprite_move_to(
        &kit->game, &pong->pad, (bento_real_t)PADDLE_X, pong->pad_y);
}

/* x ** y and math.pow in the board's MicroPython float: powf on the
 * kits, pow on the reTerminal (MicroPython's float is double there) */
static bento_real_t py_pow(bento_real_t base, bento_real_t exponent)
{
#if BENTO_FMT_REAL_DOUBLE
    return pow(base, exponent);
#else
    return powf(base, exponent);
#endif
}

/* Reflect, speed up, aim by where it hit, add the running paddle's spin, then cap the speed. */
static void bounce_off_paddle(pong_t *pong)
{
    const bento_real_t speed_up = (bento_real_t)0.35;
    const bento_real_t spin_share = (bento_real_t)0.28;
    const bento_real_t max_speed = (bento_real_t)14.0;
    const bento_real_t half_pad =
        (bento_real_t)PADDLE_H / (bento_real_t)2;
    const bento_real_t ball_middle =
        pong->ball_y + ((bento_real_t)BALL_SIZE / (bento_real_t)2);
    const bento_real_t norm =
        (ball_middle - (pong->pad_y + half_pad)) / half_pad;
    const bento_real_t aim_share = (bento_real_t)2.0;
    bento_real_t aim = norm * aim_share;
    const bento_real_t root_power = (bento_real_t)0.5; /* x ** 0.5 */
    bento_real_t squares;
    bento_real_t speed;

    pong->ball_x = (bento_real_t)PADDLE_X + (bento_real_t)PADDLE_W;
    pong->ball_vx = absolute(pong->ball_vx) + speed_up;
    pong->spin = pong->pad_vy * spin_share;
    aim += pong->spin;
    pong->ball_vy += aim;
    squares = pong->ball_vx * pong->ball_vx;
    speed = pong->ball_vy * pong->ball_vy;
    squares += speed;
    speed = py_pow(squares, root_power);
    if (speed > max_speed) {
        /* cap the total speed, not each axis */
        pong->ball_vx = (pong->ball_vx * max_speed) / speed;
        pong->ball_vy = (pong->ball_vy * max_speed) / speed;
    }
}

/* One frame of the ball; *out_hit true when it hit the paddle this frame. */
static bento_status_t step_ball(kit_t *kit, bool *out_hit)
{
    pong_t *pong = &kit->pong;
    const bento_real_t lowest =
        (bento_real_t)(kit->game.height - BALL_SIZE);

    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if ((pong->ball_y <= (bento_real_t)0) || (pong->ball_y >= lowest)) {
        pong->ball_y =
            clamp_real(pong->ball_y, (bento_real_t)0, lowest);
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&kit->game, "wall");
    }
    /* the hit test sees this frame's ball before it is drawn */
    *out_hit = (pong->ball_vx < (bento_real_t)0) &&
               bento_game_hit_rect(
                   pong->ball_x, pong->ball_y, (bento_real_t)BALL_SIZE,
                   (bento_real_t)BALL_SIZE, pong->pad.x, pong->pad.y,
                   (bento_real_t)PADDLE_W, (bento_real_t)PADDLE_H);
    if (*out_hit) {
        bounce_off_paddle(pong);
        bento_game_sfx(&kit->game, "paddle");
    }
    /* drawn after the hit is settled, so the ball never sinks into the paddle */
    return bento_game_sprite_move_to(&kit->game, &pong->ball,
                                     pong->ball_x, pong->ball_y);
}

static bento_status_t make_pong(kit_t *kit)
{
    pong_t *pong = &kit->pong;
    const int32_t top = (kit->game.height / 2) - (PADDLE_H / 2);
    bento_status_t status = bento_game_sprite_create(
        &kit->game, &pong->pad, "paddle", (bento_real_t)PADDLE_X,
        (bento_real_t)top);

    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(&kit->game, &pong->ball, "ball",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    pong->pad_y = (bento_real_t)top;
    pong->pad_vy = (bento_real_t)0;
    pong->spin = (bento_real_t)0;
    if (status == BENTO_OK) {
        status = serve_ball(kit, false);
    }
    return status;
}

/* ---- the five pieces: enter (start again) and play (one frame) ---- */

static bento_status_t enter_ship(kit_t *kit)
{
    ship_t *ship = &kit->ship;
    bento_status_t status = bento_game_box_set_color(
        &kit->game, &kit->background, COLOR_SPACE);
    int32_t j;

    ship->x = (bento_real_t)SHIP_X0;
    ship->y = (bento_real_t)(kit->game.height - SHIP_H - SHIP_FLOOR);
    ship->vx = (bento_real_t)0;
    ship->vy = (bento_real_t)0;
    ship->ticks = 0;
    for (j = 0; j < BULLETS; j++) {
        ship->flying[j] = false;
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&kit->game, &ship->sprite,
                                           ship->x, ship->y);
    }
    return status;
}

static bento_status_t
play_ship(kit_t *kit, const bento_game_keys_t *keys, bool fire)
{
    ship_t *ship = &kit->ship;
    char text[SAY_CHARS];
    int32_t phase;
    bento_status_t status;

    ship->ticks += 1;
    phase = (ship->ticks / PHASE_FRAMES) %
            PHASES; /* right, let go, left, let go */
    status =
        step_ship(kit, phase == 2, phase == 0, keys->up, keys->down);
    if ((status == BENTO_OK) && fire) {
        status = fire_ship(kit);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "vx %d  vy %d  (เพดาน 13)   กระสุนพก vx ไป 55%%",
                       (int)(int32_t)ship->vx, (int)(int32_t)ship->vy);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t enter_enemy(kit_t *kit)
{
    enemies_t *enemies = &kit->enemies;
    bento_status_t status = bento_game_box_set_color(
        &kit->game, &kit->background, COLOR_SPACE);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < KINDS); i++) {
        status = bento_game_sprite_move_to(
            &kit->game, &enemies->enemies[i],
            (bento_real_t)(ENEMY_X0 + (i * ENEMY_STEP_X)),
            (bento_real_t)(ENEMY_Y0 + (i * ENEMY_STEP_Y)));
    }
    for (i = 0; i < BOOMS; i++) {
        enemies->boom_ticks[i] = 0;
    }
    /* half as fast as the full game */
    for (i = 0; (status == BENTO_OK) && (i < KINDS); i++) {
        int32_t fall = 0;

        status = bento_random_randint(&kit->random, FALL_MIN, FALL_MAX,
                                      &fall);
        enemies->fall[i] =
            (bento_real_t)fall / (bento_real_t)FALL_DIVISOR;
    }
    enemies->bursts = 0;
    return status;
}

/* A shot hits the lowest enemy: it explodes and comes back at the top. */
static bento_status_t burst_lowest(kit_t *kit)
{
    enemies_t *enemies = &kit->enemies;
    int32_t lowest = 0;
    int32_t x = 0;
    int32_t i;
    bento_status_t status;

    for (i = 1; i < KINDS; i++) {
        if (enemies->enemies[i].y > enemies->enemies[lowest].y) {
            lowest = i;
        }
    }
    status = boom(
        kit, enemies->enemies[lowest].x + (bento_real_t)ENEMY_HALF_W,
        enemies->enemies[lowest].y + (bento_real_t)ENEMY_HALF_H,
        lowest);
    bento_game_sfx(&kit->game, "hit");
    if (status == BENTO_OK) {
        status =
            bento_random_randint(&kit->random, ENEMY_LEFT,
                                 kit->game.width - ENEMY_RIGHT_GAP, &x);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(
            &kit->game, &enemies->enemies[lowest], (bento_real_t)x,
            (bento_real_t)ENEMY_TOP);
    }
    enemies->bursts += 1;
    return status;
}

static bento_status_t play_enemy(kit_t *kit, bool fire)
{
    enemies_t *enemies = &kit->enemies;
    char text[SAY_CHARS];
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < KINDS); i++) {
        bento_game_sprite_t *enemy = &enemies->enemies[i];
        /* past the bottom = born again at the top */
        const bento_real_t y =
            (enemy->y < (bento_real_t)kit->game.height)
                ? (enemy->y + enemies->fall[i])
                : (bento_real_t)ENEMY_TOP;

        status =
            bento_game_sprite_move_to(&kit->game, enemy, enemy->x, y);
    }
    if ((status == BENTO_OK) && fire) {
        status = burst_lowest(kit);
    }
    if (status == BENTO_OK) {
        status = step_booms(kit);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "invader / UFO / demon   ระเบิดไป %d ตัว",
                       (int)enemies->bursts);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t enter_bird(kit_t *kit)
{
    bento_status_t status = bento_game_box_set_color(
        &kit->game, &kit->background, COLOR_SKY);

    if (status == BENTO_OK) {
        status = reset_bird(kit);
    }
    return status;
}

static bento_status_t
play_bird(kit_t *kit, const bento_game_keys_t *keys, bool flap)
{
    const bento_real_t frame_s = (bento_real_t)1.0 / (bento_real_t)FPS;
    bird_t *bird = &kit->bird;
    /* read every key before the or: pressed_once must see every frame */
    const bool up =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_UP, keys);
    char text[SAY_CHARS];
    bool alive = true;
    bento_status_t status = step_bird(kit, flap || up, frame_s, &alive);

    if ((status == BENTO_OK) && !alive) {
        status = reset_bird(kit);
    }
    if (status != BENTO_OK) {
        /* the status ends the frame */
    } else if (!bird->started) {
        status = say(kit, "ลอยรอ  กด A หรือ UP เพื่อออกตัว");
    } else if (bird->grace > 0) {
        (void)snprintf(text, sizeof(text),
                       "grace อีก %" PRId32 " วินาที: ตกพื้นยังไม่ตาย",
                       (bird->grace + FPS - 1) / FPS);
        status = say(kit, text);
    } else {
        status = say(kit, "บินจริงแล้ว: ตกถึงพื้น = ตาย");
    }
    return status;
}

static bento_status_t enter_snake(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    bento_status_t status = bento_game_box_set_color(
        &kit->game, &kit->background, COLOR_LAWN);

    reset_snake_body(snake);
    snake->ticks = 0;
    snake->want = WANT_NONE;
    snake->score = 0;
    if (status == BENTO_OK) {
        status = draw_snake(kit);
    }
    return status;
}

/* Remember the key; the turn happens at the step (one step at a time, as the full game). */
static void remember_turn(kit_t *kit, const bento_game_keys_t *keys,
                          bool turn)
{
    const bool up =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_UP, keys);
    const bool down =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_DOWN, keys);

    if (turn) {
        kit->snake.want = WANT_A;
    } else if (up) {
        kit->snake.want = WANT_UP;
    } else if (down) {
        kit->snake.want = WANT_DOWN;
    } else {
        /* keep what was wanted */
    }
}

/* One step of the snake, turned as wanted; a dead snake starts again. */
static bento_status_t move_snake(kit_t *kit)
{
    snake_t *snake = &kit->snake;
    step_t got = STEP_NONE;
    bento_status_t status;

    if (snake->want == WANT_A) {
        /* the show's short cut: turn clockwise, right, down, left, up */
        const int32_t old_dx = snake->dx;

        snake->dx = -snake->dy;
        snake->dy = old_dx;
    } else {
        turn_snake(snake, snake->want == WANT_UP,
                   snake->want == WANT_DOWN);
    }
    snake->want = WANT_NONE;
    status = step_snake(kit, &got);
    if (got == STEP_EAT) {
        snake->score += 1;
    }
    if ((status == BENTO_OK) &&
        ((got == STEP_WALL) || (got == STEP_SELF))) {
        /* dead: a new snake starts (the full game shows GAME OVER) */
        const int32_t score = snake->score;

        status = enter_snake(kit);
        snake->score = score;
    }
    return status;
}

static bento_status_t
play_snake(kit_t *kit, const bento_game_keys_t *keys, bool turn)
{
    snake_t *snake = &kit->snake;
    char text[SAY_CHARS];
    bento_status_t status = BENTO_OK;

    remember_turn(kit, keys, turn);
    snake->ticks += 1;
    /* a step every 4 frames = 133 ms (the full game's CLASSIC is 115 ms) */
    if ((snake->ticks % STEP_FRAMES) == 0) {
        status = move_snake(kit);
        if (status == BENTO_OK) {
            (void)snprintf(text, sizeof(text),
                           "หัว: %s   กินไป %d   ยาว %d",
                           snake->head_name, (int)snake->score,
                           (int)snake->length);
            status = say(kit, text);
        }
    }
    return status;
}

static bento_status_t enter_pong(kit_t *kit)
{
    pong_t *pong = &kit->pong;
    const int32_t top =
        (kit->game.height / 2) - (PADDLE_H / 2); /* floor division */
    bento_status_t status = bento_game_box_set_color(
        &kit->game, &kit->background, COLOR_FELT);

    pong->pad_y = (bento_real_t)top;
    pong->pad_vy = (bento_real_t)0;
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&kit->game, &pong->pad,
                                           (bento_real_t)PADDLE_X,
                                           pong->pad_y);
    }
    if (status == BENTO_OK) {
        status = serve_ball(kit, false);
    }
    if (status == BENTO_OK) {
        status = say(kit, "ตีตอนไม้กำลังวิ่ง = ใส่สปิน");
    }
    return status;
}

static bento_status_t say_spin(kit_t *kit)
{
    char spin[BENTO_FMT_BUF_SIZE];
    char ball[BENTO_FMT_BUF_SIZE];
    char text[SAY_CHARS];
    uint32_t length = 0U;
    bento_status_t status = bento_fmt_real_precision(
        spin, (uint32_t)sizeof(spin), kit->pong.spin, 1, &length);

    if (status == BENTO_OK) {
        status =
            bento_fmt_real_precision(ball, (uint32_t)sizeof(ball),
                                     kit->pong.ball_vy, 1, &length);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "ตีโดน! vy ไม้ %d -> สปิน %s -> vy ลูก %s",
                       (int)(int32_t)kit->pong.pad_vy, spin, ball);
        status = say(kit, text);
    }
    return status;
}

/* The show has a right wall where the full game has the AI paddle; a miss serves again. */
static bento_status_t keep_ball_on_table(kit_t *kit)
{
    pong_t *pong = &kit->pong;
    const bento_real_t right_wall =
        (bento_real_t)(kit->game.width - BALL_SIZE);
    bento_status_t status = BENTO_OK;

    if (pong->ball_x > right_wall) {
        pong->ball_x = right_wall;
        pong->ball_vx = -absolute(pong->ball_vx);
        status = bento_game_sprite_move_to(&kit->game, &pong->ball,
                                           pong->ball_x, pong->ball_y);
        bento_game_sfx(&kit->game, "wall");
    } else if (pong->ball_x < (bento_real_t)-MISS_PX) {
        bento_game_sfx(&kit->game, "pong_score");
        status = serve_ball(kit, false);
    } else {
        /* the rally goes on */
    }
    return status;
}

static bento_status_t
play_pong(kit_t *kit, const bento_game_keys_t *keys, bool serve)
{
    bool hit = false;
    bento_status_t status = serve ? serve_ball(kit, false) : BENTO_OK;

    if (status == BENTO_OK) {
        status = step_paddle(kit, keys->up, keys->down);
    }
    if (status == BENTO_OK) {
        status = step_ball(kit, &hit);
    }
    if ((status == BENTO_OK) && hit) {
        status = say_spin(kit);
    }
    if (status == BENTO_OK) {
        status = keep_ball_on_table(kit);
    }
    return status;
}

/* ---- showing one piece at a time ---- */

/* Hide or show a list of sprites, stopping at the first failure. */
static bento_status_t
set_sprites_shown(kit_t *kit,
                  const bento_game_sprite_t *const sprites[],
                  int32_t count, bool shown)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < count); i++) {
        status = shown ? bento_game_sprite_show(&kit->game, sprites[i])
                       : bento_game_sprite_hide(&kit->game, sprites[i]);
    }
    return status;
}

static bento_status_t hide_ship(kit_t *kit)
{
    bento_status_t status =
        bento_game_sprite_hide(&kit->game, &kit->ship.sprite);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        status = bento_game_box_hide(&kit->game, &kit->ship.bullets[i]);
    }
    return status;
}

static bento_status_t hide_enemies(kit_t *kit)
{
    const bento_game_sprite_t *sprites[KINDS + BOOMS];
    int32_t i;

    for (i = 0; i < KINDS; i++) {
        sprites[i] = &kit->enemies.enemies[i];
        sprites[i + KINDS] = &kit->enemies.booms[i];
    }
    return set_sprites_shown(kit, sprites, KINDS + BOOMS, false);
}

static bento_status_t hide_snake(kit_t *kit)
{
    const bento_game_sprite_t *sprites[SNAKE_POOL + 2];
    int32_t i;

    for (i = 0; i < SNAKE_POOL; i++) {
        sprites[i] = &kit->snake.segments[i];
    }
    sprites[SNAKE_POOL] = &kit->snake.head;
    sprites[SNAKE_POOL + 1] = &kit->snake.food;
    return set_sprites_shown(kit, sprites, SNAKE_POOL + 2, false);
}

/* The bird piece is its grass and the bird, in that order. */
static bento_status_t show_bird(kit_t *kit, bool shown)
{
    bento_status_t status =
        shown ? bento_game_box_show(&kit->game, &kit->bird.grass)
              : bento_game_box_hide(&kit->game, &kit->bird.grass);

    if (status == BENTO_OK) {
        status =
            shown
                ? bento_game_sprite_show(&kit->game, &kit->bird.sprite)
                : bento_game_sprite_hide(&kit->game, &kit->bird.sprite);
    }
    return status;
}

/* Two sprites, in order: the snake's head and food, or the paddle and ball. */
static bento_status_t show_pair(kit_t *kit,
                                const bento_game_sprite_t *first,
                                const bento_game_sprite_t *second,
                                bool shown)
{
    const bento_game_sprite_t *const sprites[2] = {first, second};

    return set_sprites_shown(kit, sprites, 2, shown);
}

/* Hide every widget of a piece (its "all" list). */
static bento_status_t hide_piece(kit_t *kit, int32_t piece)
{
    bento_status_t status;

    switch (piece) {
    case PIECE_SHIP:
        status = hide_ship(kit);
        break;
    case PIECE_ENEMY:
        status = hide_enemies(kit);
        break;
    case PIECE_BIRD:
        status = show_bird(kit, false);
        break;
    case PIECE_SNAKE:
        status = hide_snake(kit);
        break;
    default:
        status = show_pair(kit, &kit->pong.pad, &kit->pong.ball, false);
        break;
    }
    return status;
}

/* Show the widgets a piece needs when it opens (its "base" list); bullets, explosions and
 * the snake's body show themselves as the game needs them. */
static bento_status_t show_base(kit_t *kit, int32_t piece)
{
    const bento_game_sprite_t *enemies[KINDS];
    bento_status_t status;
    int32_t i;

    switch (piece) {
    case PIECE_SHIP:
        status = bento_game_sprite_show(&kit->game, &kit->ship.sprite);
        break;
    case PIECE_ENEMY:
        for (i = 0; i < KINDS; i++) {
            enemies[i] = &kit->enemies.enemies[i];
        }
        status = set_sprites_shown(kit, enemies, KINDS, true);
        break;
    case PIECE_BIRD:
        status = show_bird(kit, true);
        break;
    case PIECE_SNAKE:
        status =
            show_pair(kit, &kit->snake.head, &kit->snake.food, true);
        break;
    default:
        status = show_pair(kit, &kit->pong.pad, &kit->pong.ball, true);
        break;
    }
    return status;
}

static bento_status_t enter_piece(kit_t *kit, int32_t piece)
{
    bento_status_t status;

    switch (piece) {
    case PIECE_SHIP:
        status = enter_ship(kit);
        break;
    case PIECE_ENEMY:
        status = enter_enemy(kit);
        break;
    case PIECE_BIRD:
        status = enter_bird(kit);
        break;
    case PIECE_SNAKE:
        status = enter_snake(kit);
        break;
    default:
        status = enter_pong(kit);
        break;
    }
    return status;
}

static bento_status_t
play_piece(kit_t *kit, const bento_game_keys_t *keys, bool action)
{
    bento_status_t status;

    switch (kit->piece) {
    case PIECE_SHIP:
        status = play_ship(kit, keys, action);
        break;
    case PIECE_ENEMY:
        status = play_enemy(kit, action);
        break;
    case PIECE_BIRD:
        status = play_bird(kit, keys, action);
        break;
    case PIECE_SNAKE:
        status = play_snake(kit, keys, action);
        break;
    default:
        status = play_pong(kit, keys, action);
        break;
    }
    return status;
}

/* Hide the old piece whole, show the new one, then start its state again. */
static bento_status_t open_piece(kit_t *kit, int32_t piece)
{
    /* Each piece's name and its key hint. */
    static const char *const piece_names[PIECES] = {
        "SHIP  shooter_full", "ENEMY + BOOM  shooter_full",
        "BIRD  flappy_full", "SNAKE HEAD  snake_sprite_full",
        "PADDLE + SPIN  pong_full"};
    static const char *const piece_help[PIECES] = {
        "UP/DOWN = ขับ   A = ยิง   (ซ้าย-ขวาบินเอง)",
        "A = ยิงตัวล่างสุดให้ระเบิด", "A หรือ UP = กระพือ",
        "A = เลี้ยวตามเข็ม   UP/DOWN = เลี้ยว",
        "UP/DOWN = ขยับไม้   A = เสิร์ฟ"};
    char text[SAY_CHARS];
    bento_status_t status = hide_piece(kit, kit->piece);

    kit->piece = piece;
    if (status == BENTO_OK) {
        status = say(kit, ""); /* clear the last piece's info line */
    }
    if (status == BENTO_OK) {
        status = show_base(kit, piece);
    }
    if (status == BENTO_OK) {
        status = enter_piece(kit, piece);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "%d/%d  %s   (ซ้าย/ขวา = ตัวอื่น)", (int)(piece + 1),
                       PIECES, piece_names[piece]);
        status = bento_game_text_set(&kit->game, &kit->caption, text);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_set(&kit->game, &kit->helper,
                                     piece_help[piece]);
    }
    return status;
}

/* ---- the screen: every character made once, then shown one at a time ---- */

static bento_status_t make_bird(kit_t *kit)
{
    bird_t *bird = &kit->bird;
    bento_status_t status;

    bento_game_box_init(&bird->grass);
    /* the bird's grass is the line of death (flappy_full.py:60) */
    status = bento_game_grass(&kit->game, &bird->grass, GRASS_H,
                              BENTO_GAME_GB_DARK);
    bird->x = kit->game.width / 2;
    bird->play_h = kit->game.height - GRASS_H;
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(&kit->game, &bird->sprite, "bird",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = reset_bird(kit);
    }
    return status;
}

static bento_status_t make_texts(kit_t *kit)
{
    bento_status_t status;

    /* the labels come last, so they sit on top */
    bento_game_text_init(&kit->caption);
    bento_game_text_init(&kit->helper);
    bento_game_text_init(&kit->info);
    status =
        bento_game_text_create(&kit->game, &kit->caption, "", CAPTION_X,
                               CAPTION_Y, BENTO_GAME_YELLOW);
    if (status == BENTO_OK) {
        status = bento_game_text_create(&kit->game, &kit->helper, "",
                                        CAPTION_X, HELPER_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(&kit->game, &kit->info, "",
                                   CAPTION_X, INFO_Y, BENTO_GAME_CYAN);
    }
    return status;
}

static bento_status_t build_screen(kit_t *kit)
{
    bento_status_t status;
    int32_t piece;

    /* one background serves every piece; only its colour changes */
    bento_game_box_init(&kit->background);
    status = bento_game_background(&kit->game, &kit->background,
                                   COLOR_SPACE);
    if (status == BENTO_OK) {
        status = make_ship(kit);
    }
    if (status == BENTO_OK) {
        status = make_bird(kit);
    }
    if (status == BENTO_OK) {
        status = make_snake(kit);
    }
    if (status == BENTO_OK) {
        status = make_pong(kit);
    }
    if (status == BENTO_OK) {
        status = make_enemies(
            kit); /* the explosions come after: they sit on top */
    }
    if (status == BENTO_OK) {
        status = make_texts(kit);
    }
    kit->piece = 0;
    kit->said = false;
    /* start with every piece hidden */
    for (piece = 0; (status == BENTO_OK) && (piece < PIECES); piece++) {
        status = hide_piece(kit, piece);
    }
    if (status == BENTO_OK) {
        status = open_piece(kit, 0);
    }
    return status;
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope, so the user pointer is not needed */
    kit_t *kit = &g_kit;
    bento_game_keys_t keys;
    bool right;
    bool left;
    int32_t go;
    bento_status_t status;

    (void)user;
    *out_stop = false; /* on_frame always returns True: run() goes on */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &kit->game,
        &keys); /* never fails: a silent pad is all keys up */
    right = bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_RIGHT,
                                    &keys);
    left =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_LEFT, &keys);
    go = key_axis(left, right);
    if (go != 0) {
        /* change the piece only on the frame the key went down */
        status = open_piece(kit, (kit->piece + go + PIECES) % PIECES);
        bento_game_sfx(&kit->game, "move");
    } else {
        const bool action = bento_game_pressed_once(
            &kit->game, BENTO_GAME_KEY_A, &keys);

        status = play_piece(kit, &keys, action);
    }
    return status;
}

/* The reTerminal's joystick prints nothing when it opens; the kits print a line. */
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

/* title() calls start(), whose joystick.init() prints this line on the kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

int main(void)
{
    kit_t *kit = &g_kit;
    bool quit = false;
    bento_status_t status;

    (void)memset(kit, 0, sizeof(*kit));
    status = bento_random_init(&kit->random);
    if (status == BENTO_OK) {
        bento_game_init(&kit->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status =
            bento_game_title(&kit->game, "KIT CHARACTERS", NULL, &quit);
        report_joystick(&kit->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(kit);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&kit->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
