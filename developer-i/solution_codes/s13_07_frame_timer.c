/*
 * s13_07_frame_timer.c -- คาบ 13 การ์ด 6: เวลาคือการนับเฟรม
 *
 * ตัวจับเวลาสามตัวบนจอเดียว ไม่มี sleep เลย: นาฬิกานับถอยหลัง 30
 * วินาที (N = t x fps = 900 เฟรม) คูลดาวน์การยิง 8 เฟรม
 * (c = max(0, c - 1) ทุกเฟรม) และปล่อยศัตรูทุก 45 เฟรม
 * จอยซ้าย/ขวา = ขยับยาน · กด A ค้าง = ยิงรัว · 5 วินาทีสุดท้าย
 * แถบเวลาเป็นสีแดงและมีเสียงนับ
 *
 * Three timers on one screen, and no sleep at all: a 30 second
 * countdown (N = t x fps = 900 frames), an 8 frame fire cooldown
 * (c = max(0, c - 1) every frame) and an enemy every 45 frames.
 * Joystick left and right moves the ship, holding A fires. In the
 * last 5 seconds the time bar turns red and counts with a tone.
 *
 * Twin of s13_07_frame_timer.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_07_frame_timer.py).
 * Style: CLEAN_C.md. What C does differently:
 * - find_free() answers an index, or -1 where Python answers None;
 * - a Sprite has no size in C: the enemies' 16 x 14 is a constant and
 *   game.hit on them is bento_game_hit_rect;
 * - the shots list is an array of FPS frame numbers with a length;
 * - the shown dict is fields of the struct, -1 for never shown;
 * - random is bento_random, its state in the program's struct;
 * - the state lives in one struct at file scope (512 bytes or more);
 * - title()'s Back (SystemExit) and Stop end the program with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    FPS = 30,
    ROUND_SEC = 30, /* นาฬิกานับถอยหลัง 30 วินาที */
    COOLDOWN = 8,   /* ยิงแล้วพัก 8 เฟรม */
    SPAWN = 45,     /* ปล่อยศัตรูทุก 45 เฟรม */
    BULLETS = 6,    /* ขนาดชุดหมุนเวียน (คาบ 11) สร้างครั้งเดียว */
    ENEMIES = 4,
    BULLET_SPEED = 12,
    ENEMY_SPEED = 3,
    SHIP_SPEED = 6,
    FIELD_W = 470, /* สนามยิงซ้ายมือ · ขวามือเป็นแผงตัวจับเวลา */
    SHIP_Y = 326,
    PANEL_X = 494,
    BAR_W = 280
};
/* ---- end of settings ---- */

enum {
    WARN_SEC = 5, /* 5 วินาทีสุดท้าย แถบแดงและมีเสียงนับ */
    PERCENT = 100,
    SHOT_SLOTS = FPS + 1 /* a shot a frame at most, for one second */
};

/* The field on the left and the timer panel on the right. */
enum {
    SHIP_X = 200,
    SHIP_W = 62,
    GUN_X = 29, /* the bullet leaves from the ship's nose */
    BAR_INSET = 2,
    BAR_H = 12,
    FRAME_Y = 42,
    FRAME_H = 16,
    DIVIDER_X = 478,
    DIVIDER_Y = 10,
    DIVIDER_H = 350,
    PARKED_Y = -20, /* above this a bullet or an enemy is free */
    BULLET_PARK_Y = -50,
    BULLET_W = 5,
    BULLET_H = 12,
    BULLET_RADIUS = 2,
    ENEMY_W = 16,
    ENEMY_H = 14,
    ENEMY_PARK = -100,
    ENEMY_MIN_X = 10,
    ENEMY_MARGIN = 30,
    ENEMY_FLOOR_Y = 360,
    TEXT_CHARS = 64
};

/* Where the texts sit. */
enum {
    CLOCK_Y = 12,
    TIME_Y = 66,
    FIRE_Y = 112,
    COOL_Y = 140,
    RATE_Y = 168,
    SPAWN_TEXT_Y = 214,
    NEXT_Y = 242,
    SCORE_Y = 296
};

/* Notes of the countdown and the colours. */
enum {
    NOTE_COUNT = 72,
    NOTE_LAST = 84,
    NOTE_MS = 80,
    VELOCITY = 100
};

enum {
    COLOR_DIM = 0x9AA8B8,
    COLOR_DIVIDER = 0x3A4A5A,
    COLOR_BAR_BACK = 0x1E2A36,
    COLOR_BAR_EDGE = 0x8899AA
};

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_sprite_t enemies[ENEMIES];
    bento_game_box_t bullets[BULLETS];
    bento_game_sprite_t ship;
    bento_game_box_t bar;
    bento_game_text_t hud_time;
    bento_game_text_t hud_cool;
    bento_game_text_t hud_rate;
    bento_game_text_t hud_spawn;
    bento_game_text_t hud_score;
    int32_t ship_x;
    int32_t frame;
    int32_t time_left;
    int32_t cool;
    int32_t spawn_left;
    int32_t score;
    int32_t best;
    int32_t shots[SHOT_SLOTS]; /* เลขเฟรมของนัดในหนึ่งวินาทีล่าสุด */
    int32_t shot_count;
    int32_t shown_sec;
    int32_t shown_cool;
    int32_t shown_rate;
    int32_t shown_score;
    int32_t shown_best;
} frame_timer_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern frame_timer_t g_timer;
frame_timer_t g_timer;

/* N = t x fps */
static int32_t frames_for(int32_t seconds)
{
    return seconds * FPS;
}

/* c = max(0, c - 1) ไม่ติดลบ */
static int32_t tick(int32_t counter)
{
    return (counter > 1) ? (counter - 1) : 0;
}

/* ปัดขึ้น: เหลือ 1 เฟรมก็ยังขึ้น 1 วินาที (frames is never negative) */
static int32_t seconds_left(int32_t frames)
{
    return (frames + FPS - 1) / FPS;
}

/* จอดรอเหนือจอ (y < -20) = ว่าง */
static int32_t find_free_bullet(const frame_timer_t *timer)
{
    int32_t found = -1;
    int32_t i;

    for (i = 0; (i < BULLETS) && (found < 0); i++) {
        if (timer->bullets[i].y < (bento_real_t)PARKED_Y) {
            found = i;
        }
    }
    return found;
}

static int32_t find_free_enemy(const frame_timer_t *timer)
{
    int32_t found = -1;
    int32_t i;

    for (i = 0; (i < ENEMIES) && (found < 0); i++) {
        if (timer->enemies[i].y < (bento_real_t)PARKED_Y) {
            found = i;
        }
    }
    return found;
}

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
 * the kits and nothing on the reTerminal. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* The enemy pool, the bullets (all made, then all hidden), the ship. */
static bento_status_t add_field(frame_timer_t *timer)
{
    bento_game_t *game = &timer->game;
    int32_t i;
    bento_status_t status =
        bento_game_pool(game, timer->enemies, ENEMIES, "enemy");

    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        status = bento_game_box_create_styled(
            game, &timer->bullets[i], (bento_real_t)0,
            (bento_real_t)BULLET_PARK_Y, (bento_real_t)BULLET_W,
            (bento_real_t)BULLET_H, BENTO_GAME_CYAN,
            (int32_t)BENTO_GAME_WHITE, BULLET_RADIUS, 0);
    }
    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        status = bento_game_box_hide(game, &timer->bullets[i]);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(game, &timer->ship, "ship",
                                          (bento_real_t)SHIP_X,
                                          (bento_real_t)SHIP_Y);
    }
    if (status == BENTO_OK) {
        bento_game_box_t divider;

        bento_game_box_init(&divider);
        status = bento_game_box_create(
            game, &divider, (bento_real_t)DIVIDER_X,
            (bento_real_t)DIVIDER_Y, (bento_real_t)2,
            (bento_real_t)DIVIDER_H, COLOR_DIVIDER);
    }
    return status;
}

static bento_status_t add_label(frame_timer_t *timer, const char *text,
                                int32_t y)
{
    bento_game_text_t label;

    bento_game_text_init(&label);
    return bento_game_text_create(&timer->game, &label, text, PANEL_X,
                                  y, COLOR_DIM);
}

static bento_status_t add_clock(frame_timer_t *timer)
{
    bento_game_t *game = &timer->game;
    bento_game_box_t frame_box;
    char text[TEXT_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "CLOCK  %d x %d = %" PRId32 " f",
                   ROUND_SEC, FPS, frames_for(ROUND_SEC));
    status = add_label(timer, text, CLOCK_Y);
    bento_game_box_init(&frame_box);
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &frame_box, (bento_real_t)PANEL_X,
            (bento_real_t)FRAME_Y, (bento_real_t)BAR_W,
            (bento_real_t)FRAME_H, COLOR_BAR_BACK, COLOR_BAR_EDGE, 1,
            0);
    }
    if (status == BENTO_OK) {
        const int32_t bar_x = (int32_t)PANEL_X + BAR_INSET;
        const int32_t bar_y = FRAME_Y + BAR_INSET;

        status = bento_game_box_create(
            game, &timer->bar, (bento_real_t)bar_x, (bento_real_t)bar_y,
            (bento_real_t)(BAR_W - (2 * BAR_INSET)),
            (bento_real_t)BAR_H, BENTO_GAME_GREEN);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &timer->hud_time, "", PANEL_X,
                                   TIME_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t add_panel(frame_timer_t *timer)
{
    bento_game_t *game = &timer->game;
    char text[TEXT_CHARS];
    bento_status_t status = add_clock(timer);

    if (status == BENTO_OK) {
        status = add_label(timer, "FIRE  c = max(0, c-1)", FIRE_Y);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &timer->hud_cool, "", PANEL_X,
                                   COOL_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &timer->hud_rate, "", PANEL_X,
                                   RATE_Y, BENTO_GAME_WHITE);
    }
    (void)snprintf(text, sizeof(text), "SPAWN  every %d f", SPAWN);
    if (status == BENTO_OK) {
        status = add_label(timer, text, SPAWN_TEXT_Y);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &timer->hud_spawn, "", PANEL_X,
                                   NEXT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &timer->hud_score, "", PANEL_X,
                                   SCORE_Y, BENTO_GAME_YELLOW);
    }
    return status;
}

/* ตัวที่ 1: นาฬิกานับถอยหลัง หมดแล้วเริ่มรอบใหม่ */
static void run_clock(frame_timer_t *timer)
{
    timer->time_left = tick(timer->time_left);
    if (timer->time_left == 0) {
        timer->best =
            (timer->score > timer->best) ? timer->score : timer->best;
        timer->score = 0;
        timer->time_left = frames_for(ROUND_SEC);
        bento_game_sfx(&timer->game, "win");
    }
}

static bento_status_t move_ship(frame_timer_t *timer,
                                const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    if (keys->left || keys->right) {
        const int32_t want =
            timer->ship_x + (keys->right ? SHIP_SPEED : -SHIP_SPEED);
        const int32_t below = (want < ((int32_t)FIELD_W - SHIP_W))
                                  ? want
                                  : ((int32_t)FIELD_W - SHIP_W);

        timer->ship_x = (below > 0) ? below : 0;
        status = bento_game_sprite_move_to(&timer->game, &timer->ship,
                                           (bento_real_t)timer->ship_x,
                                           (bento_real_t)SHIP_Y);
    }
    return status;
}

/* shots = เลขเฟรมของนัดที่ยิงในหนึ่งวินาทีล่าสุด */
static void record_shot(frame_timer_t *timer)
{
    if (timer->shot_count < SHOT_SLOTS) {
        timer->shots[timer->shot_count] = timer->frame;
        timer->shot_count += 1;
    }
}

static bento_status_t launch_bullet(frame_timer_t *timer,
                                    bento_game_box_t *bullet)
{
    const int32_t bullet_y = (int32_t)SHIP_Y - BULLET_H;
    bento_status_t status = bento_game_box_move_to(
        &timer->game, bullet, (bento_real_t)(timer->ship_x + GUN_X),
        (bento_real_t)bullet_y);

    if (status == BENTO_OK) {
        status = bento_game_box_show(&timer->game, bullet);
    }
    if (status == BENTO_OK) {
        bento_game_sfx(&timer->game, "fire");
        timer->cool = COOLDOWN;
        record_shot(timer);
    }
    return status;
}

/* ตัวที่ 2: คูลดาวน์ ยิงได้เมื่อถึง 0 และมีกระสุนว่าง */
static bento_status_t fire(frame_timer_t *timer,
                           const bento_game_keys_t *keys)
{
    const int32_t free_bullet =
        (keys->a && (timer->cool == 0)) ? find_free_bullet(timer) : -1;
    bento_status_t status = BENTO_OK;

    if (free_bullet >= 0) {
        status = launch_bullet(timer, &timer->bullets[free_bullet]);
    }
    return status;
}

/* ทิ้งนัดที่เก่ากว่า 1 วินาที */
static void forget_old_shots(frame_timer_t *timer)
{
    while ((timer->shot_count > 0) &&
           ((timer->frame - timer->shots[0]) >= FPS)) {
        int32_t i;

        for (i = 1; i < timer->shot_count; i++) {
            timer->shots[i - 1] = timer->shots[i];
        }
        timer->shot_count -= 1;
    }
}

static bento_status_t drop_enemy(frame_timer_t *timer,
                                 bento_game_sprite_t *enemy)
{
    int32_t x = 0;
    bento_status_t status =
        bento_random_randint(&timer->random, ENEMY_MIN_X,
                             (int32_t)FIELD_W - ENEMY_MARGIN, &x);

    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&timer->game, enemy,
                                           (bento_real_t)x,
                                           (bento_real_t)-ENEMY_H);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_show(&timer->game, enemy);
    }
    return status;
}

/* ตัวที่ 3: ตัวจับเวลาปล่อยศัตรู (ชุดเต็ม = รอบนี้ข้ามไป) */
static bento_status_t spawn_enemy(frame_timer_t *timer)
{
    bento_status_t status = BENTO_OK;

    timer->spawn_left = tick(timer->spawn_left);
    if (timer->spawn_left == 0) {
        const int32_t free_enemy = find_free_enemy(timer);

        timer->spawn_left = SPAWN;
        if (free_enemy >= 0) {
            status = drop_enemy(timer, &timer->enemies[free_enemy]);
        }
    }
    return status;
}

/* พ้นจอ = คืนเข้าชุด */
static bento_status_t fly_bullet(frame_timer_t *timer,
                                 bento_game_box_t *bullet)
{
    bento_status_t status =
        bento_game_box_move_to(&timer->game, bullet, bullet->x,
                               bullet->y - (bento_real_t)BULLET_SPEED);

    if ((status == BENTO_OK) && (bullet->y < (bento_real_t)PARKED_Y)) {
        status = bento_game_box_hide(&timer->game, bullet);
    }
    return status;
}

static bento_status_t move_bullets(frame_timer_t *timer)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < BULLETS); i++) {
        if (timer->bullets[i].y >= (bento_real_t)PARKED_Y) {
            status = fly_bullet(timer, &timer->bullets[i]);
        }
    }
    return status;
}

static bool bullet_hits(const bento_game_box_t *bullet,
                        const bento_game_sprite_t *enemy)
{
    return (bullet->y >= (bento_real_t)PARKED_Y) &&
           bento_game_hit_rect(
               bullet->x, bullet->y, bullet->w, bullet->h, enemy->x,
               enemy->y, (bento_real_t)ENEMY_W, (bento_real_t)ENEMY_H);
}

/* The bullet that hit goes back to the pool, and the score rises. */
static bento_status_t retire_bullet(frame_timer_t *timer,
                                    bento_game_box_t *bullet)
{
    bento_status_t status = bento_game_box_move_to(
        &timer->game, bullet, bullet->x, (bento_real_t)BULLET_PARK_Y);

    if (status == BENTO_OK) {
        status = bento_game_box_hide(&timer->game, bullet);
    }
    timer->score += 1;
    bento_game_sfx(&timer->game, "hit");
    return status;
}

/* The first flying bullet that hits the enemy goes back to the pool. */
static bento_status_t shoot_down(frame_timer_t *timer,
                                 const bento_game_sprite_t *enemy,
                                 bool *out_hit)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    *out_hit = false;
    for (i = 0; (i < BULLETS) && !*out_hit; i++) {
        if (bullet_hits(&timer->bullets[i], enemy)) {
            status = retire_bullet(timer, &timer->bullets[i]);
            *out_hit = true;
        }
    }
    return status;
}

static bento_status_t park_enemy(frame_timer_t *timer,
                                 bento_game_sprite_t *enemy)
{
    bento_status_t status = bento_game_sprite_hide(&timer->game, enemy);

    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&timer->game, enemy,
                                           (bento_real_t)ENEMY_PARK,
                                           (bento_real_t)ENEMY_PARK);
    }
    return status;
}

/* ชนหรือหล่นพ้นจอ -> คืนเข้าชุด */
static bento_status_t fall_enemy(frame_timer_t *timer,
                                 bento_game_sprite_t *enemy)
{
    bool shot = false;
    bento_status_t status =
        bento_game_sprite_move_to(&timer->game, enemy, enemy->x,
                                  enemy->y + (bento_real_t)ENEMY_SPEED);

    if (status == BENTO_OK) {
        status = shoot_down(timer, enemy, &shot);
    }
    if ((status == BENTO_OK) &&
        (shot || (enemy->y > (bento_real_t)ENEMY_FLOOR_Y))) {
        status = park_enemy(timer, enemy);
    }
    return status;
}

static bento_status_t move_enemies(frame_timer_t *timer)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < ENEMIES); i++) {
        if (timer->enemies[i].y >= (bento_real_t)PARKED_Y) {
            status = fall_enemy(timer, &timer->enemies[i]);
        }
    }
    return status;
}

/* แถบเวลาเปลี่ยนแค่วินาทีละครั้ง */
static bento_status_t show_bar(frame_timer_t *timer, int32_t sec)
{
    const int32_t inner_w = BAR_W - (2 * BAR_INSET);
    const int32_t width =
        (inner_w * timer->time_left) / frames_for(ROUND_SEC);
    bento_status_t status = bento_game_box_resize(
        &timer->game, &timer->bar,
        (bento_real_t)((width > 2) ? width : 2), (bento_real_t)BAR_H);

    if (sec <= WARN_SEC) {
        bento_game_tone(&timer->game,
                        (sec > 1) ? NOTE_COUNT : NOTE_LAST,
                        BENTO_GAME_WAVE_DEFAULT, VELOCITY, NOTE_MS);
    }
    if ((status == BENTO_OK) &&
        ((sec == WARN_SEC) || (sec == ROUND_SEC))) {
        status = bento_game_box_set_color(
            &timer->game, &timer->bar,
            (sec == WARN_SEC) ? BENTO_GAME_RED : BENTO_GAME_GREEN);
    }
    timer->shown_sec = sec;
    return status;
}

/* ตัวเลขเฟรมเปลี่ยนทุกเฟรม */
static bento_status_t show_clock(frame_timer_t *timer)
{
    const int32_t sec = seconds_left(timer->time_left);
    char text[TEXT_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "%" PRId32 " s   %" PRId32 " f",
                   sec, timer->time_left);
    status = bento_game_text_set(&timer->game, &timer->hud_time, text);
    if ((status == BENTO_OK) && (sec != timer->shown_sec)) {
        status = show_bar(timer, sec);
    }
    return status;
}

static bento_status_t show_fire(frame_timer_t *timer)
{
    const int32_t ideal = ((int32_t)FPS * PERCENT) / (int32_t)COOLDOWN;
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (timer->cool != timer->shown_cool) {
        (void)snprintf(text, sizeof(text), "c = %" PRId32 "   (hold A)",
                       timer->cool);
        status =
            bento_game_text_set(&timer->game, &timer->hud_cool, text);
        timer->shown_cool = timer->cool;
    }
    if ((status == BENTO_OK) &&
        (timer->shot_count != timer->shown_rate)) {
        (void)snprintf(
            text, sizeof(text),
            "%" PRId32 " shots/s  ideal %" PRId32 ".%02" PRId32,
            timer->shot_count, ideal / PERCENT, ideal % PERCENT);
        status =
            bento_game_text_set(&timer->game, &timer->hud_rate, text);
        timer->shown_rate = timer->shot_count;
    }
    return status;
}

static bento_status_t show_spawn_and_score(frame_timer_t *timer)
{
    char text[TEXT_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "next in %" PRId32 " f",
                   timer->spawn_left);
    status = bento_game_text_set(&timer->game, &timer->hud_spawn, text);
    if ((status == BENTO_OK) && ((timer->score != timer->shown_score) ||
                                 (timer->best != timer->shown_best))) {
        (void)snprintf(text, sizeof(text),
                       "SCORE %" PRId32 "   BEST %" PRId32,
                       timer->score, timer->best);
        status =
            bento_game_text_set(&timer->game, &timer->hud_score, text);
        timer->shown_score = timer->score;
        timer->shown_best = timer->best;
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    frame_timer_t *timer = &g_timer;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;        /* the state is g_timer */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&timer->game, &keys);
    timer->frame += 1;
    run_clock(timer);
    timer->cool = tick(timer->cool);
    if (status == BENTO_OK) {
        status = move_ship(timer, &keys);
    }
    if (status == BENTO_OK) {
        status = fire(timer, &keys);
    }
    forget_old_shots(timer);
    if (status == BENTO_OK) {
        status = spawn_enemy(timer);
    }
    if (status == BENTO_OK) {
        status = move_bullets(timer);
    }
    if (status == BENTO_OK) {
        status = move_enemies(timer);
    }
    if (status == BENTO_OK) {
        status = show_clock(timer);
    }
    if (status == BENTO_OK) {
        status = show_fire(timer);
    }
    if (status == BENTO_OK) {
        status = show_spawn_and_score(timer);
    }
    return status;
}

static void reset_state(frame_timer_t *timer)
{
    int32_t i;

    (void)memset(timer, 0, sizeof(*timer));
    bento_game_init(&timer->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    for (i = 0; i < ENEMIES; i++) {
        bento_game_sprite_init(&timer->enemies[i]);
    }
    for (i = 0; i < BULLETS; i++) {
        bento_game_box_init(&timer->bullets[i]);
    }
    bento_game_sprite_init(&timer->ship);
    bento_game_box_init(&timer->bar);
    bento_game_text_init(&timer->hud_time);
    bento_game_text_init(&timer->hud_cool);
    bento_game_text_init(&timer->hud_rate);
    bento_game_text_init(&timer->hud_spawn);
    bento_game_text_init(&timer->hud_score);
    timer->ship_x = SHIP_X;
    timer->time_left = frames_for(ROUND_SEC);
    timer->spawn_left = SPAWN;
    timer->shown_sec = -1;
    timer->shown_cool = -1;
    timer->shown_rate = -1;
    timer->shown_score = -1;
    timer->shown_best = -1;
}

int main(void)
{
    frame_timer_t *timer = &g_timer;
    bool quit = false;
    bento_status_t status;

    reset_state(timer);
    status = bento_random_init(&timer->random);
    if (status == BENTO_OK) {
        status =
            bento_game_title(&timer->game, "FRAME TIMER", NULL, &quit);
        report_joystick(&timer->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_field(timer);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_panel(timer);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&timer->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
