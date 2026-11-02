/*
 * flappy_full.c -- Flappy Bird ฉบับอ้างอิง หน้าตาและจังหวะตามเกม C บนเครื่อง
 *
 * ตามเกม C (page_game_flappy.c): นกเงือก sprite ตัวจริง ท่อเขียวมีปากท่อ ท้องฟ้าไล่สี
 * เลือกความยาก EASY/NORMAL/HARD คะแนน + Best ตายแล้วกด Start เล่นใหม่ได้ทันที
 * หรือกด Y ระหว่างเล่นเพื่อเริ่มตาใหม่โหมดเดิมทันที กด 1 ครั้ง = กระพือ 1 ครั้ง
 * ความเร็วทุกตัวคิด "ต่อวินาที" คูณเวลาที่ผ่านจริง (delta-time) นกลอยรอจนกระพือครั้งแรก
 * และ 4 วินาทีแรกหลังออกตัวนกตายไม่ได้
 *
 * The reference Flappy Bird: a banded sky, grass, three recycled pipes with
 * caps (each a full-height box moved, never resized), the bird sprite, a mode
 * menu over the drawn scene, edge-triggered flaps, delta-time physics in
 * px/s, a 4 s grace after the first flap, Y to restart the same mode, and a
 * game-over panel that says GROUND! or PIPE!.
 *
 * Twin of flappy_full.py (embedded-systems-for-game_console_developer/
 * developer-ii/full_games/flappy_full.py). Style: CLEAN_C.md.
 * Each pipe's dict becomes a pipe_t; random is bento_random, MicroPython's
 * generator draw for draw.
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
#include "bento_time.h"

/* The bird, the pipes and the field (px, ยกชุดมาจากเกม C). */
enum {
    BIRD_X = 120, /* นกอยู่กับที่ ฉากเลื่อนเข้าหาแทน */
    BIRD_W = 44,  /* ขนาด sprite นกเงือกตัวจริง */
    BIRD_H = 29,
    BIRD_LEFT = BIRD_X - (BIRD_W / 2), /* floor division: whole px */
    BIRD_HALF_H = BIRD_H / 2,
    BIRD_BOX = 36, /* กล่องชนสี่เหลี่ยม (เกม C ใช้ 36) */
    PIPE_SPACING = 210,
    CAP_W = 82, /* ปากท่อ: กว้างเกินตัวท่อ สไตล์ท่อมาริโอ */
    CAP_H = 18,
    PIPE_W = 64,
    GROUND_H = 20, /* แถบพื้นหญ้าด้านล่าง = เส้นตาย */
    HIDDEN_X = -200,
    PIPE_COUNT = 3,
    GAP_JITTER = 12, /* ช่องว่างแต่ละต้น = GAP +- 12 */
    GAP_EDGE = 24,   /* ช่องไม่ชิดขอบสนามเกินนี้ */
    GAP_LIMIT = 20,  /* ขอบช่องไม่เลยขอบสนามเกินนี้ */
    TINT_COUNT = 3,
    SKY_BAND_COUNT = 8,
    HUD_X = 16,
    HUD_Y = 8,
    STATUS_Y = 34,
    LINE_CHARS = 64, /* the longest status line and its NUL */
    MODE_COUNT = 3,
    NORMAL_MODE = 1, /* เปิดมาชี้ NORMAL ก่อน เหมือนเกม C */
    GRACE_MS = 4000, /* 4 วิแรก: นกตายไม่ได้ */
    MS_PER_SECOND = 1000,
    FPS = 50
};

/* Colours, 0xRRGGBB. */
enum {
    CAP_COLOR = 0x7CE85C /* ปากท่อสว่างกว่าตัวท่อหน่อย ให้ดูนูน */
};

/* The four boxes of a pipe. */
enum {
    PART_TOP = 0,
    PART_BOTTOM,
    PART_CAP_TOP,
    PART_CAP_BOTTOM,
    PART_COUNT
};

/* (ชื่อ, ช่องว่างท่อ px, ความเร็วท่อ px/s, แรงโน้มถ่วง px/s^2, แรงกระพือ px/s) */
typedef struct {
    const char *name;
    int32_t gap;
    bento_real_t pipe_speed;
    bento_real_t gravity;
    bento_real_t flap_velocity;
} difficulty_t;

/* ตัวเลขชุดเดียวกับ s_flap_modes ของเกม C */
static const difficulty_t MODES[MODE_COUNT] = {
    {"EASY", 175, (bento_real_t)110.0, (bento_real_t)850.0,
     (bento_real_t)-300.0},
    {"NORMAL", 130, (bento_real_t)150.0, (bento_real_t)1050.0,
     (bento_real_t)-340.0},
    {"HARD", 105, (bento_real_t)200.0, (bento_real_t)1200.0,
     (bento_real_t)-360.0},
};
static const char *const MODE_NAMES[MODE_COUNT] = {"EASY", "NORMAL",
                                                   "HARD"};
static const char BEST_NAME[] = "flappy";

typedef struct {
    bento_real_t x;
    int32_t gap;
    int32_t gap_y;
    int32_t gap_top;
    int32_t gap_bottom;
    bool passed;
    bento_game_box_t parts[PART_COUNT];
    bool visible[PART_COUNT];
} pipe_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    int32_t mode;
    const char
        *dead_text; /* NULL = None: the heading is "SELECT MODE" */
    pipe_t pipes[PIPE_COUNT];
    bento_game_sprite_t bird;
    bento_game_text_t hud;
    bento_game_text_t status_label;
    int32_t play_h; /* ความสูงสนามเล่นจริง (เหนือพื้นหญ้า) */
    bento_real_t bird_y;
    bento_real_t velocity_y;
    bento_real_t scroll_acc; /* เศษ pixel สะสมของการเลื่อนท่อ */
    int32_t score;
    int32_t best_seen;
    bool restart_now;
    bool started; /* ยังไม่เริ่มบิน นกลอยรอจนกระพือครั้งแรก */
    uint32_t start_ms;
    uint32_t last_ms;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

/* เกม C ใช้ gradient จริง ฝั่งนี้ปูแถบสี 8 แถบแทน (เข้มบน -> จางล่าง) */
static bento_status_t draw_sky(flappy_t *flappy)
{
    static const uint32_t sky_bands[SKY_BAND_COUNT] = {
        0x4898F0U, 0x58A2F1U, 0x68ADF2U, 0x78B7F3U,
        0x88C1F5U, 0x98CBF6U, 0xA8D6F7U, 0xB8E0F8U};
    const int32_t band_h = flappy->play_h / SKY_BAND_COUNT;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < SKY_BAND_COUNT); i++) {
        const int32_t height = (i < (SKY_BAND_COUNT - 1))
                                   ? band_h
                                   : (flappy->play_h - (band_h * i));
        bento_game_box_t band;

        bento_game_box_init(&band);
        status =
            bento_game_box_create(&flappy->game, &band, (bento_real_t)0,
                                  (bento_real_t)(i * band_h),
                                  (bento_real_t)flappy->game.width,
                                  (bento_real_t)height, sky_bands[i]);
    }
    return status;
}

/* ต้นละ 4 ชิ้น สูงเต็มสนาม สร้างครั้งเดียวแบบซ่อนไว้ ไม่ resize อีกเลย */
static bento_status_t add_pipe(flappy_t *flappy, pipe_t *pipe)
{
    int32_t part;
    bento_status_t status = BENTO_OK;

    for (part = 0; (status == BENTO_OK) && (part < PART_COUNT);
         part++) {
        const bool is_cap = part >= PART_CAP_TOP;

        bento_game_box_init(&pipe->parts[part]);
        status = bento_game_box_create(
            &flappy->game, &pipe->parts[part], (bento_real_t)HIDDEN_X,
            (bento_real_t)0, (bento_real_t)(is_cap ? CAP_W : PIPE_W),
            (bento_real_t)(is_cap ? CAP_H : flappy->play_h),
            is_cap ? (uint32_t)CAP_COLOR : BENTO_GAME_GB_DARK);
        pipe->visible[part] = false;
    }
    for (part = 0; (status == BENTO_OK) && (part < PART_COUNT);
         part++) {
        status = bento_game_box_hide(&flappy->game, &pipe->parts[part]);
    }
    pipe->x = (bento_real_t)0;
    pipe->passed = false;
    return status;
}

/* จัดฉาก (สร้างครั้งเดียว ไม่มีงานต่อเฟรม): ฟ้า หญ้า ท่อ นก HUD */
static bento_status_t build_scene(flappy_t *flappy)
{
    bento_game_box_t grass;
    int32_t i;
    bento_status_t status = draw_sky(flappy);

    if (status == BENTO_OK) {
        status = bento_game_grass(&flappy->game, &grass, GROUND_H,
                                  BENTO_GAME_GB_DARK);
    }
    for (i = 0; (status == BENTO_OK) && (i < PIPE_COUNT); i++) {
        status = add_pipe(flappy, &flappy->pipes[i]);
    }
    if (status == BENTO_OK) {
        /* นกเงือก = sprite pixel-art ตัวเดียวกับเกม C */
        const int32_t bird_top_px = (flappy->play_h / 2) - BIRD_HALF_H;

        bento_game_sprite_init(&flappy->bird);
        status = bento_game_sprite_create(
            &flappy->game, &flappy->bird, "bird",
            (bento_real_t)BIRD_LEFT, (bento_real_t)bird_top_px);
    }
    return status;
}

static bento_status_t add_labels(flappy_t *flappy)
{
    char text[LINE_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "Score: 0    Best: %" PRId32,
                   bento_game_best(&flappy->game, BEST_NAME));
    bento_game_text_init(&flappy->hud);
    bento_game_text_init(&flappy->status_label);
    status = bento_game_text_create(&flappy->game, &flappy->hud, text,
                                    HUD_X, HUD_Y, BENTO_GAME_WHITE);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            &flappy->game, &flappy->status_label, "", HUD_X, STATUS_Y,
            BENTO_GAME_WHITE);
    }
    return status;
}

static bento_real_t rightmost_x(const flappy_t *flappy)
{
    bento_real_t right = flappy->pipes[0].x;
    int32_t i;

    for (i = 1; i < PIPE_COUNT; i++) {
        if (flappy->pipes[i].x > right) {
            right = flappy->pipes[i].x;
        }
    }
    return right;
}

/* The gap's size and place; its edges stay GAP_LIMIT inside the field. */
static bento_status_t draw_gap(flappy_t *flappy, pipe_t *pipe)
{
    int32_t jitter = 0;
    int32_t half;
    bento_status_t status = bento_random_randint(
        &flappy->random, -GAP_JITTER, GAP_JITTER, &jitter);

    pipe->gap = MODES[flappy->mode].gap + jitter;
    half = pipe->gap / 2;
    if (status == BENTO_OK) {
        status = bento_random_randint(&flappy->random, half + GAP_EDGE,
                                      flappy->play_h - half - GAP_EDGE,
                                      &pipe->gap_y);
    }
    pipe->gap_top = pipe->gap_y - half;
    if (pipe->gap_top < GAP_LIMIT) {
        pipe->gap_top = GAP_LIMIT;
    }
    pipe->gap_bottom = pipe->gap_y + half;
    if (pipe->gap_bottom > (flappy->play_h - GAP_LIMIT)) {
        pipe->gap_bottom = flappy->play_h - GAP_LIMIT;
    }
    return status;
}

/* เกิดใหม่ทางขวา: สุ่มช่องว่าง ตำแหน่งช่อง เฉดสี (ไม่ resize) */
static bento_status_t reset_pipe(flappy_t *flappy, pipe_t *pipe,
                                 const bento_real_t *first_x)
{
    static const uint32_t pipe_tints[TINT_COUNT] = {
        0x6CD84CU, 0x4CB838U, 0x84E060U};
    int32_t tint = 0;
    bento_status_t status;

    pipe->x = (first_x != NULL)
                  ? *first_x
                  : (rightmost_x(flappy) + (bento_real_t)PIPE_SPACING);
    status = draw_gap(flappy, pipe);
    pipe->passed = false;
    if (status == BENTO_OK) {
        status = bento_random_randint(&flappy->random, 0,
                                      TINT_COUNT - 1, &tint);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &flappy->game, &pipe->parts[PART_TOP], pipe_tints[tint]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &flappy->game, &pipe->parts[PART_BOTTOM], pipe_tints[tint]);
    }
    return status;
}

/* Show or hide each part once, when the pipe comes on or goes off screen. */
static bento_status_t show_pipe(flappy_t *flappy, pipe_t *pipe, bool on)
{
    int32_t part;
    bento_status_t status = BENTO_OK;

    for (part = 0; (status == BENTO_OK) && (part < PART_COUNT);
         part++) {
        if (on && !pipe->visible[part]) {
            status =
                bento_game_box_show(&flappy->game, &pipe->parts[part]);
            pipe->visible[part] = true;
        } else if (!on && pipe->visible[part]) {
            status =
                bento_game_box_hide(&flappy->game, &pipe->parts[part]);
            pipe->visible[part] = false;
        } else {
            /* already as it should be */
        }
    }
    return status;
}

static bento_status_t move_part(flappy_t *flappy, pipe_t *pipe,
                                int32_t part, int32_t x_px,
                                int32_t y_px)
{
    return bento_game_box_move_to(&flappy->game, &pipe->parts[part],
                                  (bento_real_t)x_px,
                                  (bento_real_t)y_px);
}

/* ต่อเฟรม: "เลื่อนอย่างเดียว" ไม่ resize ส่วนพ้นขอบให้ฮาร์ดแวร์ตัด */
static bento_status_t render_pipe(flappy_t *flappy, pipe_t *pipe)
{
    const int32_t x_px = (int32_t)pipe->x;
    const bool on = (-PIPE_W < x_px) && (x_px < flappy->game.width);
    const int32_t cap_x = (x_px + (PIPE_W / 2)) - (CAP_W / 2);
    bento_status_t status = show_pipe(flappy, pipe, on);

    if ((status == BENTO_OK) && on) {
        /* ขอบล่างของท่อบน = ขอบช่องบน, ขอบบนของท่อล่าง = ขอบช่องล่าง */
        status = move_part(flappy, pipe, PART_TOP, x_px,
                           pipe->gap_top - flappy->play_h);
    }
    if ((status == BENTO_OK) && on) {
        status = move_part(flappy, pipe, PART_BOTTOM, x_px,
                           pipe->gap_bottom);
    }
    if ((status == BENTO_OK) && on) {
        status = move_part(flappy, pipe, PART_CAP_TOP, cap_x,
                           pipe->gap_top - CAP_H);
    }
    if ((status == BENTO_OK) && on) {
        status = move_part(flappy, pipe, PART_CAP_BOTTOM, cap_x,
                           pipe->gap_bottom);
    }
    return status;
}

/* กล่องชน 36px รอบใจกลางนก เทียบกับท่อบน/ล่าง (สูตรเดียวกับ flappy_hit_pipe) */
static bool hit_pipe(const flappy_t *flappy, const pipe_t *pipe)
{
    const int32_t half_bird = BIRD_BOX / 2;
    const int32_t half = pipe->gap / 2;
    bool hit = false;

    if (!((bento_real_t)(BIRD_X + half_bird) < pipe->x) &&
        !((bento_real_t)(BIRD_X - half_bird) >
          (pipe->x + (bento_real_t)PIPE_W))) {
        hit = ((flappy->bird_y - (bento_real_t)half_bird) <
               (bento_real_t)(pipe->gap_y - half)) ||
              ((flappy->bird_y + (bento_real_t)half_bird) >
               (bento_real_t)(pipe->gap_y + half));
    }
    return hit;
}

static bento_status_t score_point(flappy_t *flappy)
{
    char text[LINE_CHARS];

    flappy->score++;
    bento_game_sfx(&flappy->game, "point");
    if (flappy->score > flappy->best_seen) {
        /* Best ขยับตามทันทีเหมือนเกม C */
        flappy->best_seen = flappy->score;
    }
    (void)snprintf(text, sizeof(text),
                   "Score: %" PRId32 "    Best: %" PRId32,
                   flappy->score, flappy->best_seen);
    return bento_game_text_set(&flappy->game, &flappy->hud, text);
}

/* One pipe per frame: move, score, recycle, draw, then the hit test. */
static bento_status_t advance_pipe(flappy_t *flappy, pipe_t *pipe,
                                   int32_t step, bool grace, bool *over)
{
    bento_status_t status = BENTO_OK;

    pipe->x -= (bento_real_t)step;
    if (!pipe->passed &&
        ((pipe->x + (bento_real_t)PIPE_W) < (bento_real_t)BIRD_X)) {
        pipe->passed = true; /* นกพ้นท่อทั้งต้น = ได้ 1 คะแนน */
        status = score_point(flappy);
    }
    if ((status == BENTO_OK) &&
        ((pipe->x + (bento_real_t)PIPE_W) < (bento_real_t)0)) {
        /* หลุดจอซ้าย: เกิดใหม่ทางขวา */
        status = reset_pipe(flappy, pipe, NULL);
    }
    if (status == BENTO_OK) {
        status = render_pipe(flappy, pipe);
    }
    if ((status == BENTO_OK) && hit_pipe(flappy, pipe) && !grace) {
        bento_game_sfx(&flappy->game, "fall");
        flappy->dead_text = "PIPE!";
        *over = true;
    }
    return status;
}

/* ท่อเลื่อนซ้ายเป็นจำนวนเต็ม pixel ส่วนเศษสะสมไว้ก่อน ครบ 1 pixel ค่อยขยับ */
static bento_status_t scroll_pipes(flappy_t *flappy, bento_real_t dt,
                                   bool grace, bool *over)
{
    const bento_real_t distance = MODES[flappy->mode].pipe_speed * dt;
    int32_t step;
    int32_t i;
    bento_status_t status = BENTO_OK;

    flappy->scroll_acc += distance;
    step = (int32_t)flappy->scroll_acc;
    flappy->scroll_acc -= (bento_real_t)step;
    for (i = 0; (status == BENTO_OK) && !*over && (i < PIPE_COUNT);
         i++) {
        status =
            advance_pipe(flappy, &flappy->pipes[i], step, grace, over);
    }
    return status;
}

/* Gravity and the ceiling; the floor ends the round unless in grace. */
static void fall(flappy_t *flappy, bento_real_t dt, bool grace,
                 bool *over)
{
    const bento_real_t half_box =
        (bento_real_t)BIRD_BOX / (bento_real_t)2;
    const bento_real_t floor_y =
        (bento_real_t)flappy->play_h - half_box;
    bento_real_t change = MODES[flappy->mode].gravity * dt;

    flappy->velocity_y += change;
    change = flappy->velocity_y * dt;
    flappy->bird_y += change;
    if (flappy->bird_y < half_box) {
        flappy->bird_y = half_box; /* ชนเพดาน: หยุดค้างไว้ ไม่ตาย */
        flappy->velocity_y = (bento_real_t)0;
    }
    if ((flappy->bird_y > floor_y) && grace) {
        flappy->bird_y = floor_y; /* ช่วง grace: ไม่ตาย แค่หยุดที่พื้น */
        flappy->velocity_y = (bento_real_t)0;
    } else if (flappy->bird_y > floor_y) {
        bento_game_sfx(&flappy->game, "fall");
        flappy->dead_text = "GROUND!";
        *over = true;
    } else {
        /* still in the air */
    }
}

/* The flight after the first flap: fall, draw the bird, scroll pipes. */
static bento_status_t fly(flappy_t *flappy, bento_real_t dt,
                          uint32_t now, bool *over)
{
    const bool grace =
        bento_time_ticks_diff(now, flappy->start_ms) < GRACE_MS;
    bento_status_t status = BENTO_OK;

    fall(flappy, dt, grace, over);
    if (!*over) {
        const int32_t bird_top_px =
            (int32_t)flappy->bird_y - BIRD_HALF_H;

        status = bento_game_sprite_move_to(&flappy->game, &flappy->bird,
                                           (bento_real_t)BIRD_LEFT,
                                           (bento_real_t)bird_top_px);
    }
    if ((status == BENTO_OK) && !*over) {
        status = scroll_pipes(flappy, dt, grace, over);
    }
    return status;
}

/* กระพือปีก: อ่านทั้งสองปุ่มก่อน ค่อย or ให้ pressed_once จำสถานะถูก */
static bento_status_t flap(flappy_t *flappy,
                           const bento_game_keys_t *keys, bool *flapped)
{
    const bool flap_a =
        bento_game_pressed_once(&flappy->game, BENTO_GAME_KEY_A, keys);
    const bool flap_up =
        bento_game_pressed_once(&flappy->game, BENTO_GAME_KEY_UP, keys);
    bento_status_t status = BENTO_OK;

    *flapped = flap_a || flap_up;
    if (*flapped) {
        flappy->velocity_y = MODES[flappy->mode].flap_velocity;
        bento_game_sfx(&flappy->game, "flap");
        status = bento_game_text_set(&flappy->game,
                                     &flappy->status_label, "Flap!");
    }
    return status;
}

/* delta-time: เวลาที่ผ่านจริงตั้งแต่เฟรมก่อน; 0 or less skips the frame. */
static bento_real_t frame_seconds(flappy_t *flappy, uint32_t now)
{
    const bento_real_t longest = (bento_real_t)0.05;
    bento_real_t dt =
        (bento_real_t)bento_time_ticks_diff(now, flappy->last_ms) /
        (bento_real_t)MS_PER_SECOND;

    flappy->last_ms = now;
    if (dt > longest) {
        dt = longest; /* เพิ่งกลับจาก pause หรือเฟรมค้างนาน: กันนกวาร์ปทะลุท่อ */
    }
    return dt;
}

/* Keys after dt: Y restarts, a flap may start the flight, then fly. */
static bento_status_t play_frame(flappy_t *flappy, bento_real_t dt,
                                 uint32_t now, bool *out_stop)
{
    bento_game_keys_t keys;
    bool flapped = false;
    bento_status_t status;

    bento_game_keys_init(&keys);
    status = bento_game_keys(&flappy->game, &keys);
    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&flappy->game, BENTO_GAME_KEY_Y,
                                &keys)) {
        flappy->restart_now = true; /* Y = เริ่มตาใหม่โหมดเดิมทันที */
        *out_stop = true;
    } else if (status == BENTO_OK) {
        status = flap(flappy, &keys, &flapped);
        if (!flappy->started && flapped) {
            flappy->started = true; /* กระพือครั้งแรก = ออกบิน */
            flappy->start_ms = now;
        }
        if ((status == BENTO_OK) && flappy->started) {
            status = fly(flappy, dt, now, out_stop);
        }
    } else {
        /* the status ends the game loop */
    }
    return status;
}

static bento_status_t update(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    const uint32_t now = bento_time_ticks_ms();
    const bento_real_t dt = frame_seconds(flappy, now);
    bento_status_t status = BENTO_OK;

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    if (dt > (bento_real_t)0) {
        status = play_frame(flappy, dt, now, out_stop);
    }
    return status;
}

/* ตั้งค่าเริ่มรอบ: ท่อเริ่มนอกจอขวา นกกลางสนาม */
static bento_status_t start_round(flappy_t *flappy)
{
    const int32_t middle_px = flappy->play_h / 2;
    char text[LINE_CHARS];
    int32_t i;
    bento_status_t status;

    (void)snprintf(text, sizeof(text),
                   "%s  -  A/UP = flap  (กระพือเพื่อออกตัว)",
                   MODES[flappy->mode].name);
    status =
        bento_game_text_set(&flappy->game, &flappy->status_label, text);
    for (i = 0; (status == BENTO_OK) && (i < PIPE_COUNT); i++) {
        const bento_real_t first_x =
            (bento_real_t)(flappy->game.width + (i * PIPE_SPACING));

        status = reset_pipe(flappy, &flappy->pipes[i], &first_x);
        if (status == BENTO_OK) {
            status = render_pipe(flappy, &flappy->pipes[i]);
        }
    }
    flappy->bird_y = (bento_real_t)middle_px;
    flappy->velocity_y = (bento_real_t)0;
    flappy->score = 0;
    flappy->best_seen = bento_game_best(&flappy->game, BEST_NAME);
    flappy->scroll_acc = (bento_real_t)0;
    flappy->dead_text = "GAME OVER";
    flappy->restart_now = false;
    flappy->started = false;
    flappy->start_ms = 0U;
    flappy->last_ms = bento_time_ticks_ms();
    return status;
}

/* เมนูลอยทับฉากที่วาดครบแล้ว; BACK จากหน้าเมนู = เลิกเล่น */
static bento_status_t pick_mode(flappy_t *flappy, bool *playing)
{
    int32_t picked = -1;
    const char *heading =
        (flappy->dead_text != NULL) ? flappy->dead_text : "SELECT MODE";
    const bento_status_t status =
        bento_game_menu(&flappy->game, MODE_NAMES, MODE_COUNT, heading,
                        flappy->mode, &picked);

    *playing = picked >= 0;
    if ((status == BENTO_OK) && *playing) {
        flappy->mode = picked;
    }
    return status;
}

/* What the player chose when the game loop ended. */
static bento_status_t after_round(flappy_t *flappy, bool finished,
                                  bool *playing, bool *restart_same)
{
    bento_status_t status = BENTO_OK;

    if (!finished) {
        /* Back กลางเกม = เลิกเล่น; save_best answers the best, not a status */
        (void)bento_game_save_best(&flappy->game, &flappy->score,
                                   BEST_NAME);
        *playing = false;
    } else if (flappy->restart_now) {
        /* Y = วนไปเริ่มโหมดเดิมใหม่ทันที; save_best answers the best */
        (void)bento_game_save_best(&flappy->game, &flappy->score,
                                   BEST_NAME);
        status = bento_game_replay(&flappy->game);
        *restart_same = true;
    } else {
        /* Start = เล่นรอบใหม่ / Back = ออก */
        status = bento_game_over(&flappy->game, &flappy->score,
                                 BEST_NAME, flappy->dead_text, playing);
    }
    return status;
}

/* One round: a fresh scene, the menu unless Y restarted, then play. */
static bento_status_t play_round(flappy_t *flappy, bool *playing,
                                 bool *restart_same)
{
    bool finished = false;
    /* ล้างฉากของรอบก่อน ไม่งั้น widget สะสมข้ามรอบจนทะลุงบ */
    bento_status_t status = bento_game_clear(&flappy->game);

    flappy->play_h = flappy->game.height - GROUND_H;
    if (status == BENTO_OK) {
        status = build_scene(flappy);
    }
    if (status == BENTO_OK) {
        status = add_labels(flappy);
    }
    if ((status == BENTO_OK) && !*restart_same) {
        status = pick_mode(flappy, playing);
    }
    *restart_same = false;
    if ((status == BENTO_OK) && *playing) {
        status = start_round(flappy);
    }
    if ((status == BENTO_OK) && *playing) {
        status =
            bento_game_run(&flappy->game, update, NULL, FPS, &finished);
        if (status == BENTO_OK) {
            status =
                after_round(flappy, finished, playing, restart_same);
        }
    }
    return status;
}

/* The reTerminal's joystick prints nothing when it opens; the kits print a
 * line. */
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

int main(void)
{
    flappy_t *flappy = &g_flappy;
    bool quit = false;
    bool playing = true;
    bool restart_same = false;
    bento_status_t status;

    (void)memset(flappy, 0, sizeof(*flappy));
    status = bento_random_init(&flappy->random);
    if (status == BENTO_OK) {
        bento_game_init(&flappy->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status = bento_game_title_melody(&flappy->game, "FLAPPY", NULL,
                                         NULL, 0U, &quit);
        /* title() calls start(), whose joystick.init() prints this line */
        if ((flappy->game.joystick_init == BENTO_OK) &&
            !on_reterminal()) {
            (void)printf(
                "joystick: USB Host HID initialized on CM55\n");
        }
    }
    flappy->mode = NORMAL_MODE;
    flappy->dead_text = NULL;
    /* ตายแล้ววนกลับมาเลือกโหมดใหม่ได้เลย ไม่ต้องรีบูตเครื่อง */
    while ((status == BENTO_OK) && !quit && playing) {
        status = play_round(flappy, &playing, &restart_same);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
