/*
 * s13_15_game_juice.c -- คาบ 13 (คาบพิเศษ): ความสนุกที่มาจากคณิตศาสตร์ที่เรียนแล้ว (game juice)
 *
 * ลูกเล่นทุกอย่างใช้แค่สมการที่เรียนแล้ว: ยิ่งเก่งยิ่งยาก speed = min(BASE + floor(score / 5), MAX)
 * โดนแล้วกะพริบขาว 6 เฟรม c = max(0, c - 1) ระเบิดเป็นภาพ 10 เฟรม ครึ่งแรก boom1 ครึ่งหลัง boom2
 * บอสมีแถบพลัง 5 ก้อน โดนหนึ่งครั้งซ่อนหนึ่งก้อน
 * กด Start แล้วใช้จอยซ้าย/ขวาขับยาน กด A ค้าง = ยิงต่อเนื่อง (มีคูลดาวน์) ยิงศัตรูที่ตกลงมา +1 แต้ม
 * ยิงบอสดูบอสกะพริบขาวและแถบพลังหายทีละก้อน บอสพลังหมด +5 แต้ม ดูบรรทัด SPEED ขึ้นทุก 5 แต้ม
 *
 * Game juice from maths already learnt: enemies fall faster every 5 points up to a ceiling,
 * the boss flashes white for 6 frames when hit, every hit shows a two-frame explosion, and
 * the boss's health bar is 5 boxes that hide one by one. Left/right drives the ship, A held
 * fires with a cooldown. An enemy shot is +1, a boss broken is +5 and a new one comes 2 s
 * later. The pure logic (ramp, countdown, boom_frame) is checked once before the screen opens.
 *
 * Twin of s13_15_game_juice.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_15_game_juice.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the st dict and the boom_t list are one juice_t, at file scope because it is large;
 *   - every position is a whole number in Python, so it is an int32_t here too;
 *   - a sprite keeps no w and h in bento_game, so each enemy keeps its own size for the hit test;
 *   - raise SystemExit after a failed self test ends main() with 0.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* The game (change these). */
enum {
    BASE_SPEED = 2,     /* ความเร็วศัตรูตอนเริ่ม (px ต่อเฟรม) */
    MAX_SPEED = 6,      /* เพดาน */
    SCORE_PER_STEP = 5, /* ทุก 5 แต้มเร็วขึ้น 1 */
    FLASH_FRAMES = 6,   /* บอสกะพริบขาวกี่เฟรม */
    BOOM_FRAMES = 10,   /* ระเบิดค้างกี่เฟรม */
    BOSS_HP = 5,        /* แถบพลังบอส 5 ก้อน */
    BOSS_RESPAWN = 60,  /* บอสตัวใหม่มาหลังตัวเก่าแตก 60 เฟรม (2 วินาที) */
    BOSS_BONUS = 5,
    ENEMIES = 3, /* ขนาดชุดหมุนเวียน: ของที่ขยับพร้อมกันน้อย = ลื่นบนบอร์ด */
    BULLETS = 3,
    BOOMS = 2,
    FIRE_COOLDOWN = 6,
    BULLET_SPEED = 10,
    SHIP_SPEED = 8,
    BOSS_SPEED = 3,
    KINDS = 3,
    FPS = 30
};

/* Where things are, in px, and the text buffers. */
enum {
    SHIP_X0 = 365,
    SHIP_Y = 330,
    SHIP_W = 62,
    MUZZLE_X = 28,
    MUZZLE_UP = 12,
    BOSS_X0 = 340,
    BOSS_Y = 60,
    BOSS_W = 110,
    BOSS_H = 34,
    BOSS_HALF_W = BOSS_W / 2,
    BOSS_HALF_H = BOSS_H / 2,
    BOSS_RADIUS = 8,
    BOSS_MARGIN = 20,
    FACE_HALF_W = 9,
    FACE_DOWN = 10,
    BULLET_PARK_X = -20,
    BULLET_PARK_Y = -50,
    BULLET_W = 5,
    BULLET_H = 12,
    BULLET_TOP = 30, /* a bullet above this line is spent */
    BULLET_TIP = 2, /* the bullet's middle, where it strikes the boss */
    DROP_X_MIN = 20,
    DROP_RIGHT = 40,
    DROP_Y_MIN = 20,
    DROP_Y_MAX = 220,
    BOOM_HALF_W = 14, /* the 29 x 24 explosion, centred on the hit */
    BOOM_HALF_H = 12,
    HUD_X = 12,
    HUD_Y = 8,
    RAMP_X = 230,
    BOSS_LABEL_X = 560,
    HP_X0 = 620,
    HP_STEP = 32,
    HP_Y = 10,
    HP_W = 26,
    HP_H = 16,
    HP_RADIUS = 3,
    TEXT_CHARS =
        80 /* "SPEED = min(..." with four int32_t of 11 characters and the NUL */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_SPACE = 0x10122C,
    COLOR_BOSS = 0x7B4FD6,
    COLOR_LASER = 0x80F4FF
};

/* The enemy pictures' sizes in px (drop_enemy() names them). */
static const int32_t ENEMY_SIZE[KINDS][2] = {
    {16, 14}, {21, 10}, {18, 14}};

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_sprite_t enemies[ENEMIES];
    bento_game_box_t boss;
    bento_game_sprite_t boss_face;
    bento_game_box_t bullets[BULLETS];
    bento_game_sprite_t ship;
    bento_game_sprite_t booms[BOOMS];
    bento_game_text_t hud_score;
    bento_game_text_t hud_ramp;
    bento_game_box_t hp_boxes[BOSS_HP];
    int32_t enemy_x[ENEMIES];
    int32_t enemy_y[ENEMIES];
    int32_t enemy_kind[ENEMIES];
    int32_t bullet_x[BULLETS];
    int32_t bullet_y[BULLETS];
    int32_t boom_time[BOOMS];
    int32_t ship_x;
    int32_t boss_x;
    int32_t score;
    int32_t cool;
    int32_t flash;
    int32_t hp;
    int32_t away;
    int32_t boss_vx;
    int32_t shown; /* -1 = nothing shown yet (Python's None) */
} juice_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern juice_t g_juice;
juice_t g_juice;

/* ---- the brain: pure, no screen ---- */

/* Floor division: 0-4 points = +0, 5-9 points = +1, with a ceiling. */
static int32_t ramp(int32_t score)
{
    const int32_t speed = BASE_SPEED + (score / SCORE_PER_STEP);

    return (speed < MAX_SPEED) ? speed : MAX_SPEED;
}

/* Count down and stop at 0, never below. */
static int32_t countdown(int32_t count)
{
    return (count > 1) ? (count - 1) : 0;
}

/* The first half shows the small fireball, the second half the big ring. */
static const char *boom_frame(int32_t time)
{
    return (time > (BOOM_FRAMES / 2)) ? "boom1" : "boom2";
}

/* ---- check the brain before the screen opens ---- */

static bool check_number(int32_t got, int32_t want)
{
    if (got != want) {
        (void)printf("ยังไม่ผ่าน: ได้ %d ควรได้ %d\n", (int)got, (int)want);
    }
    return got == want;
}

static bool check_name(const char *got, const char *want)
{
    const bool same = strcmp(got, want) == 0;

    if (!same) {
        (void)printf("ยังไม่ผ่าน: ได้ %s ควรได้ %s\n", got, want);
    }
    return same;
}

static bool self_test(void)
{
    /* ramp(score) = speed, countdown(count) = count, boom_frame(time) = picture */
    static const int32_t ramp_cases[][2] = {
        {0, 2}, {4, 2}, {5, 3}, {19, 5}, {999, 6}};
    static const int32_t countdown_cases[][2] = {{3, 2}, {0, 0}};
    static const int32_t boom_times[] = {10, 5};
    static const char *const boom_wanted[] = {"boom1", "boom2"};
    const size_t ramp_count =
        sizeof(ramp_cases) / sizeof(ramp_cases[0]);
    const size_t countdown_count =
        sizeof(countdown_cases) / sizeof(countdown_cases[0]);
    const size_t boom_count =
        sizeof(boom_times) / sizeof(boom_times[0]);
    bool passed = true;
    int32_t i;

    for (i = 0; passed && (i < (int32_t)ramp_count); i++) {
        passed = check_number(ramp(ramp_cases[i][0]), ramp_cases[i][1]);
    }
    for (i = 0; passed && (i < (int32_t)countdown_count); i++) {
        passed = check_number(countdown(countdown_cases[i][0]),
                              countdown_cases[i][1]);
    }
    for (i = 0; passed && (i < (int32_t)boom_count); i++) {
        passed = check_name(boom_frame(boom_times[i]), boom_wanted[i]);
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของลูกเล่นถูกทุกกรณี\n");
    }
    return passed;
}

/* ---- the screen: made once (space first, the explosions and the HUD last, on top) ---- */

static bento_status_t add_box(juice_t *juice, bento_game_box_t *box,
                              const rect_t *place, uint32_t color,
                              int32_t border, int32_t radius)
{
    bento_game_box_init(box);
    return bento_game_box_create_styled(
        &juice->game, box, (bento_real_t)place->x,
        (bento_real_t)place->y, (bento_real_t)place->w,
        (bento_real_t)place->h, color, border, radius, 0);
}

static bento_status_t add_space_and_boss(juice_t *juice)
{
    bento_game_t *game = &juice->game;
    bento_game_box_t space;
    const rect_t space_place = {0, 0, game->width, game->height};
    const rect_t boss_place = {BOSS_X0, BOSS_Y, BOSS_W, BOSS_H};
    bento_status_t status =
        add_box(juice, &space, &space_place, COLOR_SPACE,
                BENTO_GAME_BORDER_NONE, 1);

    if (status == BENTO_OK) {
        status = bento_game_pool(game, juice->enemies,
                                 (uint32_t)ENEMIES, "enemy");
    }
    if (status == BENTO_OK) {
        juice->boss_x = BOSS_X0;
        bento_game_box_init(&juice->boss);
        status = bento_game_box_create_styled(
            game, &juice->boss, (bento_real_t)boss_place.x,
            (bento_real_t)boss_place.y, (bento_real_t)boss_place.w,
            (bento_real_t)boss_place.h, COLOR_BOSS,
            (int32_t)BENTO_GAME_WHITE, BOSS_RADIUS, 2);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(game, &juice->boss_face, "enemy3",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    return status;
}

static bento_status_t add_ship_and_shots(juice_t *juice)
{
    bento_game_t *game = &juice->game;
    const rect_t bullet_place = {BULLET_PARK_X, BULLET_PARK_Y, BULLET_W,
                                 BULLET_H};
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        juice->bullet_x[i] = BULLET_PARK_X;
        juice->bullet_y[i] = BULLET_PARK_Y;
        status = add_box(juice, &juice->bullets[i], &bullet_place,
                         COLOR_LASER, (int32_t)BENTO_GAME_WHITE, 2);
    }
    if (status == BENTO_OK) {
        juice->ship_x = SHIP_X0;
        status = bento_game_sprite_create(game, &juice->ship, "ship",
                                          (bento_real_t)SHIP_X0,
                                          (bento_real_t)SHIP_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_pool(game, juice->booms, (uint32_t)BOOMS,
                                 "boom1");
    }
    return status;
}

static bento_status_t add_hud(juice_t *juice)
{
    bento_game_t *game = &juice->game;
    bento_game_text_t boss_label;
    bento_status_t status;
    int32_t i;

    bento_game_text_init(&juice->hud_score);
    bento_game_text_init(&juice->hud_ramp);
    bento_game_text_init(&boss_label);
    status = bento_game_text_create(game, &juice->hud_score, "", HUD_X,
                                    HUD_Y, BENTO_GAME_WHITE);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &juice->hud_ramp, "", RAMP_X,
                                   HUD_Y, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &boss_label, "BOSS",
                                        BOSS_LABEL_X, HUD_Y,
                                        BENTO_GAME_WHITE);
    }
    for (i = 0; (status == BENTO_OK) && (i < BOSS_HP); i++) {
        const rect_t hp_place = {HP_X0 + (i * HP_STEP), HP_Y, HP_W,
                                 HP_H};

        status = add_box(juice, &juice->hp_boxes[i], &hp_place,
                         BENTO_GAME_RED, (int32_t)BENTO_GAME_WHITE,
                         HP_RADIUS);
    }
    return status;
}

static bento_status_t build_screen(juice_t *juice)
{
    bento_status_t status = add_space_and_boss(juice);

    if (status == BENTO_OK) {
        status = add_ship_and_shots(juice);
    }
    if (status == BENTO_OK) {
        status = add_hud(juice);
    }
    juice->hp = BOSS_HP;
    juice->boss_vx = BOSS_SPEED;
    juice->shown = -1;
    return status;
}

/* An enemy of a random kind starts again above the screen. */
static bento_status_t drop_enemy(juice_t *juice, int32_t slot)
{
    static const char *const enemy_names[KINDS] = {"enemy", "enemy2",
                                                   "enemy3"};
    bento_game_sprite_t *enemy = &juice->enemies[slot];
    int32_t height = 0;
    bento_status_t status = bento_random_randint(
        &juice->random, 0, KINDS - 1, &juice->enemy_kind[slot]);

    if (status == BENTO_OK) {
        status = bento_game_sprite_frame(
            &juice->game, enemy, enemy_names[juice->enemy_kind[slot]]);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(&juice->random, DROP_X_MIN,
                                      juice->game.width - DROP_RIGHT,
                                      &juice->enemy_x[slot]);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(&juice->random, DROP_Y_MIN,
                                      DROP_Y_MAX, &height);
    }
    if (status == BENTO_OK) {
        juice->enemy_y[slot] = -height;
        status = bento_game_sprite_move_to(
            &juice->game, enemy, (bento_real_t)juice->enemy_x[slot],
            (bento_real_t)juice->enemy_y[slot]);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_show(&juice->game, enemy);
    }
    return status;
}

/* The free explosion, or the one nearest its end. */
static bento_status_t boom_at(juice_t *juice, int32_t center_x,
                              int32_t center_y)
{
    int32_t chosen = 0;
    int32_t i;
    bento_status_t status;

    for (i = 1; i < BOOMS; i++) {
        if (juice->boom_time[i] < juice->boom_time[chosen]) {
            chosen = i;
        }
    }
    juice->boom_time[chosen] = BOOM_FRAMES;
    status = bento_game_sprite_frame(&juice->game,
                                     &juice->booms[chosen], "boom1");
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(
            &juice->game, &juice->booms[chosen],
            (bento_real_t)(center_x - BOOM_HALF_W),
            (bento_real_t)(center_y - BOOM_HALF_H));
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_show(&juice->game, &juice->booms[chosen]);
    }
    return status;
}

/* The boss breaks: it hides, explodes in its middle and comes back later. */
static bento_status_t break_boss(juice_t *juice)
{
    bento_status_t status =
        bento_game_box_hide(&juice->game, &juice->boss);

    if (status == BENTO_OK) {
        status =
            bento_game_sprite_hide(&juice->game, &juice->boss_face);
    }
    if (status == BENTO_OK) {
        status = boom_at(juice, juice->boss_x + BOSS_HALF_W,
                         BOSS_Y + BOSS_HALF_H);
    }
    juice->score += BOSS_BONUS;
    juice->away = BOSS_RESPAWN;
    bento_game_sfx(&juice->game, "explode");
    return status;
}

static bento_status_t hit_boss(juice_t *juice, int32_t x)
{
    /* a small explosion where the bullet struck under the boss */
    bento_status_t status = boom_at(juice, x, BOSS_Y + BOSS_H);

    juice->hp -= 1;
    if (status == BENTO_OK) {
        /* the bar loses one box; nothing is resized */
        status = bento_game_box_hide(&juice->game,
                                     &juice->hp_boxes[juice->hp]);
    }
    if (status == BENTO_OK) {
        /* flash: the colour changes now, then a countdown brings it back */
        status = bento_game_box_set_color(&juice->game, &juice->boss,
                                          BENTO_GAME_WHITE);
    }
    juice->flash = FLASH_FRAMES;
    bento_game_sfx(&juice->game, "hit");
    if ((status == BENTO_OK) && (juice->hp == 0)) {
        status = break_boss(juice);
    }
    return status;
}

/* ---- the main loop ---- */

static bento_status_t drive_and_fire(juice_t *juice,
                                     const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;
    int32_t free_bullet = -1;
    int32_t i;

    if (keys->left || keys->right) {
        const int32_t step = (keys->right ? SHIP_SPEED : 0) -
                             (keys->left ? SHIP_SPEED : 0);
        const int32_t x = juice->ship_x + step;
        const int32_t right_edge = juice->game.width - SHIP_W;

        juice->ship_x = (x < right_edge) ? x : right_edge;
        juice->ship_x = (juice->ship_x > 0) ? juice->ship_x : 0;
        status = bento_game_sprite_move_to(&juice->game, &juice->ship,
                                           (bento_real_t)juice->ship_x,
                                           (bento_real_t)SHIP_Y);
    }
    juice->cool = countdown(juice->cool);
    for (i = BULLETS - 1; i >= 0; i--) {
        if (juice->bullet_y[i] < 0) {
            free_bullet =
                i; /* counting down leaves the first free one, as Python's break */
        }
    }
    if ((status == BENTO_OK) && keys->a && (juice->cool == 0) &&
        (free_bullet >= 0)) {
        juice->bullet_x[free_bullet] = juice->ship_x + MUZZLE_X;
        juice->bullet_y[free_bullet] = SHIP_Y - MUZZLE_UP;
        status = bento_game_box_move_to(
            &juice->game, &juice->bullets[free_bullet],
            (bento_real_t)juice->bullet_x[free_bullet],
            (bento_real_t)juice->bullet_y[free_bullet]);
        juice->cool = FIRE_COOLDOWN;
        bento_game_sfx(&juice->game, "fire");
    }
    return status;
}

/* A new boss with full health. */
static bento_status_t return_boss(juice_t *juice)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    juice->hp = BOSS_HP;
    for (i = 0; (status == BENTO_OK) && (i < BOSS_HP); i++) {
        status = bento_game_box_show(&juice->game, &juice->hp_boxes[i]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_show(&juice->game, &juice->boss);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_show(&juice->game, &juice->boss_face);
    }
    return status;
}

/* The boss runs left and right and bounces off the edges. */
static bento_status_t move_boss(juice_t *juice)
{
    bento_status_t status = BENTO_OK;

    if (juice->away == 0) {
        if ((juice->boss_x < BOSS_MARGIN) ||
            (juice->boss_x >
             (juice->game.width - BOSS_MARGIN - BOSS_W))) {
            juice->boss_vx = -juice->boss_vx;
        }
        juice->boss_x += juice->boss_vx;
        status = bento_game_box_move_to(&juice->game, &juice->boss,
                                        (bento_real_t)juice->boss_x,
                                        (bento_real_t)BOSS_Y);
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(
                &juice->game, &juice->boss_face,
                (bento_real_t)((juice->boss_x + BOSS_HALF_W) -
                               FACE_HALF_W),
                (bento_real_t)BOSS_Y + (bento_real_t)FACE_DOWN);
        }
    } else {
        juice->away = countdown(juice->away);
        if (juice->away == 0) {
            status = return_boss(juice);
        }
    }
    return status;
}

/* Enemies fall faster with the score (with a ceiling); past the bottom they start again. */
static bento_status_t move_enemies(juice_t *juice)
{
    const int32_t speed = ramp(juice->score);
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < ENEMIES); i++) {
        juice->enemy_y[i] += speed;
        status =
            bento_game_sprite_move_to(&juice->game, &juice->enemies[i],
                                      (bento_real_t)juice->enemy_x[i],
                                      (bento_real_t)juice->enemy_y[i]);
        if ((status == BENTO_OK) &&
            (juice->enemy_y[i] > juice->game.height)) {
            status = drop_enemy(juice, i);
        }
    }
    return status;
}

static bool bullet_hits_enemy(const juice_t *juice, int32_t bullet,
                              int32_t enemy)
{
    const int32_t kind = juice->enemy_kind[enemy];

    return bento_game_hit_rect(
        (bento_real_t)juice->bullet_x[bullet],
        (bento_real_t)juice->bullet_y[bullet], (bento_real_t)BULLET_W,
        (bento_real_t)BULLET_H, (bento_real_t)juice->enemy_x[enemy],
        (bento_real_t)juice->enemy_y[enemy],
        (bento_real_t)ENEMY_SIZE[kind][0],
        (bento_real_t)ENEMY_SIZE[kind][1]);
}

static bento_status_t shoot_enemy(juice_t *juice, int32_t enemy)
{
    const int32_t kind = juice->enemy_kind[enemy];
    const int32_t half_w = ENEMY_SIZE[kind][0] / 2;
    const int32_t half_h = ENEMY_SIZE[kind][1] / 2;
    bento_status_t status =
        boom_at(juice, juice->enemy_x[enemy] + half_w,
                juice->enemy_y[enemy] + half_h);

    juice->score += 1;
    bento_game_sfx(&juice->game, "hit");
    if (status == BENTO_OK) {
        status = drop_enemy(juice, enemy);
    }
    return status;
}

/* What this bullet hit: each enemy, then the boss. *out_used is true when it is spent. */
static bento_status_t strike(juice_t *juice, int32_t bullet,
                             bool *out_used)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < ENEMIES); i++) {
        if (!*out_used && bullet_hits_enemy(juice, bullet, i)) {
            status = shoot_enemy(juice, i);
            *out_used = true;
        }
    }
    if ((status == BENTO_OK) && !*out_used && (juice->away == 0) &&
        bento_game_hit(&juice->bullets[bullet], &juice->boss)) {
        status = hit_boss(juice, juice->bullet_x[bullet] + BULLET_TIP);
        *out_used = true;
    }
    return status;
}

static bento_status_t move_bullet(juice_t *juice, int32_t bullet)
{
    bento_game_box_t *box = &juice->bullets[bullet];
    bool used;
    bento_status_t status;

    juice->bullet_y[bullet] -= BULLET_SPEED;
    status = bento_game_box_move_to(
        &juice->game, box, (bento_real_t)juice->bullet_x[bullet],
        (bento_real_t)juice->bullet_y[bullet]);
    used = juice->bullet_y[bullet] < BULLET_TOP;
    if (status == BENTO_OK) {
        status = strike(juice, bullet, &used);
    }
    if ((status == BENTO_OK) && used) {
        juice->bullet_x[bullet] = BULLET_PARK_X;
        juice->bullet_y[bullet] = BULLET_PARK_Y;
        status = bento_game_box_move_to(&juice->game, box,
                                        (bento_real_t)BULLET_PARK_X,
                                        (bento_real_t)BULLET_PARK_Y);
    }
    return status;
}

/* One explosion: the big ring in its second half, then hidden. */
static bento_status_t step_boom(juice_t *juice, int32_t boom)
{
    bento_status_t status = BENTO_OK;

    juice->boom_time[boom] -= 1;
    if (juice->boom_time[boom] == (BOOM_FRAMES / 2)) {
        status =
            bento_game_sprite_frame(&juice->game, &juice->booms[boom],
                                    boom_frame(juice->boom_time[boom]));
    }
    if ((status == BENTO_OK) && (juice->boom_time[boom] == 0)) {
        status =
            bento_game_sprite_hide(&juice->game, &juice->booms[boom]);
    }
    return status;
}

/* The flash counts down to the boss's own colour; the explosions play on. */
static bento_status_t animate(juice_t *juice)
{
    bento_status_t status = BENTO_OK;
    int32_t j;

    if (juice->flash > 0) {
        juice->flash = countdown(juice->flash);
        if (juice->flash == 0) {
            status = bento_game_box_set_color(&juice->game,
                                              &juice->boss, COLOR_BOSS);
        }
    }
    for (j = 0; (status == BENTO_OK) && (j < BOOMS); j++) {
        if (juice->boom_time[j] > 0) {
            status = step_boom(juice, j);
        }
    }
    return status;
}

/* The HUD is written only when the score changes. */
static bento_status_t show_hud(juice_t *juice)
{
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (juice->score != juice->shown) {
        (void)snprintf(text, sizeof(text), "SCORE %d",
                       (int)juice->score);
        status =
            bento_game_text_set(&juice->game, &juice->hud_score, text);
        if (status == BENTO_OK) {
            (void)snprintf(text, sizeof(text),
                           "SPEED = min(%d + %d // 5, %d) = %d",
                           BASE_SPEED, (int)juice->score, MAX_SPEED,
                           (int)ramp(juice->score));
            status = bento_game_text_set(&juice->game, &juice->hud_ramp,
                                         text);
        }
        juice->shown = juice->score;
    }
    return status;
}

/* One frame. on_frame always returns True. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    juice_t *juice = &g_juice;
    bento_game_keys_t keys;
    bento_status_t status;
    int32_t i;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: the game goes on */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &juice->game,
        &keys); /* never fails: a silent pad is all keys up */
    status = drive_and_fire(juice, &keys);
    if (status == BENTO_OK) {
        status = move_boss(juice);
    }
    if (status == BENTO_OK) {
        status = move_enemies(juice);
    }
    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        if (juice->bullet_y[i] >= 0) {
            status = move_bullet(juice, i);
        }
    }
    if (status == BENTO_OK) {
        status = animate(juice);
    }
    if (status == BENTO_OK) {
        status = show_hud(juice);
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
    juice_t *juice = &g_juice;
    bool quit = false;
    bento_status_t status = BENTO_OK;
    int32_t i;

    (void)memset(juice, 0, sizeof(*juice));
    status = bento_random_init(&juice->random);
    if ((status == BENTO_OK) && self_test()) {
        bento_game_init(&juice->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status =
            bento_game_title(&juice->game, "GAME JUICE", NULL, &quit);
        report_joystick(&juice->game);
        if ((status == BENTO_OK) && !quit) {
            status = build_screen(juice);
        }
        for (i = 0; (status == BENTO_OK) && !quit && (i < ENEMIES);
             i++) {
            status = drop_enemy(juice, i);
        }
        if ((status == BENTO_OK) && !quit) {
            bool finished = false;

            status = bento_game_run(&juice->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
