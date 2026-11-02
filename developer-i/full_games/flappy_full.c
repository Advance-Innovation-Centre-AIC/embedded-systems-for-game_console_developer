/*
 * flappy_full.c -- Flappy Bird ฉบับอ้างอิง (เกมเต็ม)
 *
 * Twin of flappy_full.py
 * (embedded-systems-for-game_console_developer/developer-i/full_games/flappy_full.py).
 *
 * Flappy Bird หน้าตาและจังหวะตามเกม C บนเครื่อง (page_game_flappy.c):
 * นกเงือก sprite ตัวจริง ท่อเขียวมีปากท่อ ท้องฟ้าไล่สี 8 แถบ เลือกความยาก
 * EASY/NORMAL/HARD คะแนน + Best ตายแล้วกด Start เล่นใหม่ หรือกด Y ระหว่าง
 * เล่นเพื่อเริ่มตาใหม่โหมดเดิมทันที กด A หรือขึ้นเพื่อกระพือ (กดหนึ่งครั้ง =
 * กระพือหนึ่งครั้ง) ความเร็วทุกตัวคิดต่อวินาทีแล้วคูณเวลาจริงของเฟรม (dt)
 * 4 วินาทีแรกหลังออกตัวนกตายไม่ได้
 *
 * The reference Flappy Bird, with the look and rhythm of the on-board
 * C game: the hornbill sprite, green pipes with caps, a banded sky,
 * EASY / NORMAL / HARD, Score and Best, Start to play again and Y to
 * restart the same mode at once. A or UP flaps, one press per flap.
 * Every speed is per second, times the real frame time (delta-time),
 * and the bird cannot die in the first 4 seconds after take-off.
 *
 * C twin of the Example Explorer program full-flappy
 * (py/full-flappy.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the game's state is one flappy_t at file scope (too big for the
 *     stack), set again at the start of main();
 *   - each pipe's dict is a pipe_t, its four boxes an array in the
 *     order top, bot, cap_t, cap_b; MODES is a table of structs;
 *   - the four boxes are hidden in MicroPython's dict order for this
 *     dict (bot, cap_t, top, cap_b), not the order written;
 *   - floats are bento_real_t, and no multiply-add is fused, as
 *     MicroPython rounds every operation;
 *   - update() is the function bento_game_run() calls; `return False`
 *     sets *out_stop, a bare `return` does not;
 *   - a call that raises in MicroPython returns its status, and main
 *     returns it; BACK at the title and Stop end with 0;
 *   - joystick.init() prints its line during title() in MicroPython;
 *     here main prints it when title() returns.
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

/* a*b+c is two roundings in MicroPython: no fused multiply-add (clang's
 * pragma; gcc ignores it and contracts by default) */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

enum { /* the bird, px: it stays at BIRD_X and the scene scrolls */
    BIRD_X = 120,
    BIRD_W = 44,
    BIRD_H = 29,
    BIRD_BOX = 36 /* the hit box, kinder than the sprite */
};
enum { /* the pipes and the ground, px */
    PIPES = 3,
    PIPE_PARTS = 4,
    PIPE_SPACING = 210,
    PIPE_W = 64,
    CAP_W = 82,
    CAP_H = 18,
    GROUND_H = 20,
    HIDDEN_X = -200,
    FIRST_GAP = 130,
    FIRST_GAP_Y = 189,
    GAP_JITTER = 12,
    GAP_MARGIN = 24,
    GAP_EDGE = 20,
    TINT_COUNT = 3
};
enum { /* the box of each part of a pipe */
    PART_TOP = 0,
    PART_BOTTOM = 1,
    PART_CAP_TOP = 2,
    PART_CAP_BOTTOM = 3
};
enum { /* the screen */
    SKY_BAND_COUNT = 8,
    TEXT_X = 16,
    HUD_Y = 8,
    STATUS_Y = 34,
    MODE_COUNT = 3,
    START_MODE = 1,      /* NORMAL, as the C game */
    HUD_TEXT_BYTES = 40, /* the text and two int32_t (11 each) */
    STATUS_TEXT_BYTES = 96
};
enum { /* time */
    FPS = 50,
    GRACE_MS = 4000,
    MS_PER_SECOND = 1000
};
enum {                   /* colours, 0xRRGGBB */
    CAP_COLOR = 0x7CE85C /* a little brighter than the pipe */
};

static const char BEST_NAME[] = "flappy";

/* one difficulty: the numbers of the C game's s_flap_modes */
typedef struct {
    const char *name;
    int32_t gap;
    bento_real_t pipe_speed; /* px/s */
    bento_real_t gravity;    /* px/s^2 */
    bento_real_t flap_speed; /* px/s, upwards */
} difficulty_t;

static const difficulty_t MODES[MODE_COUNT] = {
    {"EASY", 175, (bento_real_t)110.0, (bento_real_t)850.0,
     (bento_real_t)-300.0},
    {"NORMAL", 130, (bento_real_t)150.0, (bento_real_t)1050.0,
     (bento_real_t)-340.0},
    {"HARD", 105, (bento_real_t)200.0, (bento_real_t)1200.0,
     (bento_real_t)-360.0},
};

typedef struct {
    bento_game_box_t part[PIPE_PARTS];
    bento_real_t x;
    int32_t width;
    int32_t gap;
    int32_t gap_y;
    bool passed;
    int32_t gap_top;
    int32_t gap_bottom;
    bool visible[PIPE_PARTS];
} pipe_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    pipe_t pipes[PIPES];
    bento_game_sprite_t bird;
    bento_game_text_t hud;
    bento_game_text_t status;
    int32_t play_height;
    int32_t mode;
    bool restart_same;
    bento_real_t bird_y;
    bento_real_t velocity_y;
    int32_t score;
    int32_t best_seen;
    bento_real_t scroll_rest; /* the part of a pixel not scrolled yet */
    const char *dead_message; /* NULL: no round ended yet */
    bool restart_now;
    bool started;
    uint32_t start_ms;
    uint32_t last_ms;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

/* the pipe that is furthest right decides where a reborn pipe goes */
static bento_real_t next_pipe_x(const flappy_t *flappy)
{
    bento_real_t furthest = flappy->pipes[0].x;

    for (int32_t i = 1; i < PIPES; i++) {
        if (flappy->pipes[i].x > furthest) {
            furthest = flappy->pipes[i].x;
        }
    }
    return furthest + (bento_real_t)PIPE_SPACING;
}

/* born again on the right with a random gap and tint; never resized,
 * since the pipe is a full-height box */
static bento_status_t reset_pipe(flappy_t *flappy, pipe_t *pipe,
                                 bento_real_t x)
{
    static const uint32_t pipe_tints[TINT_COUNT] = {
        0x6CD84CU, 0x4CB838U, 0x84E060U};
    int32_t jitter = 0;
    int32_t tint = 0;
    int32_t half = 0;
    bento_status_t status = bento_random_randint(
        &flappy->random, -GAP_JITTER, GAP_JITTER, &jitter);

    pipe->x = x;
    pipe->gap = MODES[flappy->mode].gap + jitter;
    half = pipe->gap / 2;
    if (status == BENTO_OK) {
        status = bento_random_randint(
            &flappy->random, half + GAP_MARGIN,
            flappy->play_height - half - GAP_MARGIN, &pipe->gap_y);
    }
    pipe->passed = false;
    pipe->gap_top = ((pipe->gap_y - half) > GAP_EDGE)
                        ? (pipe->gap_y - half)
                        : GAP_EDGE;
    pipe->gap_bottom =
        ((pipe->gap_y + half) < (flappy->play_height - GAP_EDGE))
            ? (pipe->gap_y + half)
            : (flappy->play_height - GAP_EDGE);
    if (status == BENTO_OK) {
        status = bento_random_randint(&flappy->random, 0,
                                      TINT_COUNT - 1, &tint);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &flappy->game, &pipe->part[PART_TOP], pipe_tints[tint]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &flappy->game, &pipe->part[PART_BOTTOM], pipe_tints[tint]);
    }
    return status;
}

/* show a part that comes on screen, hide one that leaves it */
static bento_status_t show_part(flappy_t *flappy, pipe_t *pipe,
                                int32_t part, bool on)
{
    bento_status_t status = BENTO_OK;

    if (on && !pipe->visible[part]) {
        status = bento_game_box_show(&flappy->game, &pipe->part[part]);
        pipe->visible[part] = true;
    } else if (!on && pipe->visible[part]) {
        status = bento_game_box_hide(&flappy->game, &pipe->part[part]);
        pipe->visible[part] = false;
    } else {
        /* already as it should be */
    }
    return status;
}

/* one frame of a pipe: move only, never resize; the display clips
 * what is past the edge */
static bento_status_t render_pipe(flappy_t *flappy, pipe_t *pipe)
{
    bento_game_t *game = &flappy->game;
    const int32_t x = (int32_t)pipe->x;
    const bool on = (-pipe->width < x) && (x < game->width);
    const int32_t cap_x = x + (pipe->width / 2) - (CAP_W / 2);
    bento_status_t status = BENTO_OK;

    for (int32_t part = 0; (part < PIPE_PARTS) && (status == BENTO_OK);
         part++) {
        status = show_part(flappy, pipe, part, on);
    }
    /* the top pipe ends at the gap's top, the bottom pipe starts at
     * the gap's bottom */
    if ((status == BENTO_OK) && on) {
        const int32_t top_y = pipe->gap_top - flappy->play_height;

        status = bento_game_box_move_to(game, &pipe->part[PART_TOP],
                                        (bento_real_t)x,
                                        (bento_real_t)top_y);
    }
    if ((status == BENTO_OK) && on) {
        status = bento_game_box_move_to(game, &pipe->part[PART_BOTTOM],
                                        (bento_real_t)x,
                                        (bento_real_t)pipe->gap_bottom);
    }
    if ((status == BENTO_OK) && on) {
        status = bento_game_box_move_to(
            game, &pipe->part[PART_CAP_TOP], (bento_real_t)cap_x,
            (bento_real_t)(pipe->gap_top - CAP_H));
    }
    if ((status == BENTO_OK) && on) {
        status = bento_game_box_move_to(
            game, &pipe->part[PART_CAP_BOTTOM], (bento_real_t)cap_x,
            (bento_real_t)pipe->gap_bottom);
    }
    return status;
}

/* the 36 px box around the bird's centre against the top and bottom
 * pipe (the C game's flappy_hit_pipe) */
static bool hit_pipe(const flappy_t *flappy, const pipe_t *pipe)
{
    const int32_t half_bird = BIRD_BOX / 2;
    const int32_t half = pipe->gap / 2;
    const int32_t gap_top = pipe->gap_y - half;
    const int32_t gap_bottom = pipe->gap_y + half;
    const bool beside =
        ((bento_real_t)(BIRD_X + half_bird) < pipe->x) ||
        ((bento_real_t)(BIRD_X - half_bird) >
         (pipe->x + (bento_real_t)pipe->width));
    bool hit = false;

    if (!beside) {
        hit = ((flappy->bird_y - (bento_real_t)half_bird) <
               (bento_real_t)gap_top) ||
              ((flappy->bird_y + (bento_real_t)half_bird) >
               (bento_real_t)gap_bottom);
    }
    return hit;
}

/* the real time since the last frame, in seconds, at most MAX_DT so
 * the bird never jumps through a pipe after a pause */
static bento_real_t frame_seconds(flappy_t *flappy, uint32_t now)
{
    const bento_real_t max_dt = (bento_real_t)0.05;
    bento_real_t dt =
        (bento_real_t)bento_time_ticks_diff(now, flappy->last_ms) /
        (bento_real_t)MS_PER_SECOND;

    flappy->last_ms = now;
    if (dt > max_dt) {
        dt = max_dt;
    }
    return dt;
}

/* one press, one flap: both keys are read every frame so that
 * pressed_once remembers each of them */
static bento_status_t flap(flappy_t *flappy,
                           const bento_game_keys_t *keys, bool *flapped)
{
    bento_game_t *game = &flappy->game;
    const bool flap_a =
        bento_game_pressed_once(game, BENTO_GAME_KEY_A, keys);
    const bool flap_up =
        bento_game_pressed_once(game, BENTO_GAME_KEY_UP, keys);
    bento_status_t status = BENTO_OK;

    *flapped = flap_a || flap_up;
    if (*flapped) {
        flappy->velocity_y = MODES[flappy->mode].flap_speed;
        bento_game_sfx(game, "flap");
        status = bento_game_text_set(game, &flappy->status, "Flap!");
    }
    return status;
}

/* gravity and the flap move the bird; the ceiling stops it, the
 * ground ends the round unless the grace time still runs */
static bento_status_t move_bird(flappy_t *flappy, bento_real_t dt,
                                bool grace, bool *out_stop)
{
    const bento_real_t half_box =
        (bento_real_t)BIRD_BOX / (bento_real_t)2;
    const bento_real_t floor_y =
        (bento_real_t)flappy->play_height - half_box;
    const bento_real_t speed_gain = MODES[flappy->mode].gravity * dt;
    bento_real_t distance = (bento_real_t)0.0;
    bento_status_t status = BENTO_OK;

    flappy->velocity_y += speed_gain;
    distance = flappy->velocity_y * dt;
    flappy->bird_y += distance;
    if (flappy->bird_y < half_box) {
        flappy->bird_y = half_box;
        flappy->velocity_y = (bento_real_t)0.0;
    }
    if ((flappy->bird_y > floor_y) && grace) {
        flappy->bird_y = floor_y;
        flappy->velocity_y = (bento_real_t)0.0;
    } else if (flappy->bird_y > floor_y) {
        bento_game_sfx(&flappy->game, "fall");
        flappy->dead_message = "GROUND!";
        *out_stop = true;
    } else {
        /* still in the air */
    }
    if (!*out_stop) {
        const int32_t bird_left = BIRD_X - (BIRD_W / 2);
        const int32_t bird_top = (int32_t)flappy->bird_y - (BIRD_H / 2);

        status = bento_game_sprite_move_to(&flappy->game, &flappy->bird,
                                           (bento_real_t)bird_left,
                                           (bento_real_t)bird_top);
    }
    return status;
}

/* the bird is past the whole pipe: one point, and Best follows at
 * once as in the C game */
static bento_status_t score_point(flappy_t *flappy, pipe_t *pipe)
{
    char text[HUD_TEXT_BYTES];

    pipe->passed = true;
    flappy->score += 1;
    bento_game_sfx(&flappy->game, "point");
    if (flappy->score > flappy->best_seen) {
        flappy->best_seen = flappy->score;
    }
    (void)snprintf(text, sizeof(text),
                   "Score: %" PRId32 "    Best: %" PRId32,
                   flappy->score, flappy->best_seen);
    return bento_game_text_set(&flappy->game, &flappy->hud, text);
}

/* one pipe of one frame: scroll, score, reborn on the right when it
 * leaves on the left, draw, and the hit test */
static bento_status_t move_pipe(flappy_t *flappy, pipe_t *pipe,
                                int32_t step, bool grace,
                                bool *out_stop)
{
    bento_status_t status = BENTO_OK;

    pipe->x -= (bento_real_t)step;
    if (!pipe->passed && ((pipe->x + (bento_real_t)pipe->width) <
                          (bento_real_t)BIRD_X)) {
        status = score_point(flappy, pipe);
    }
    if ((status == BENTO_OK) &&
        ((pipe->x + (bento_real_t)pipe->width) < (bento_real_t)0)) {
        status = reset_pipe(flappy, pipe, next_pipe_x(flappy));
    }
    if (status == BENTO_OK) {
        status = render_pipe(flappy, pipe);
    }
    /* during the grace time a pipe does not kill */
    if ((status == BENTO_OK) && hit_pipe(flappy, pipe) && !grace) {
        bento_game_sfx(&flappy->game, "fall");
        flappy->dead_message = "PIPE!";
        *out_stop = true;
    }
    return status;
}

/* screen positions are whole pixels but a frame's distance is not:
 * the rest is kept until it makes a whole pixel */
static bento_status_t move_pipes(flappy_t *flappy, bento_real_t dt,
                                 bool grace, bool *out_stop)
{
    const bento_real_t distance = MODES[flappy->mode].pipe_speed * dt;
    int32_t step = 0;
    bento_status_t status = BENTO_OK;

    flappy->scroll_rest += distance;
    step = (int32_t)flappy->scroll_rest;
    flappy->scroll_rest -= (bento_real_t)step;
    for (int32_t i = 0;
         (i < PIPES) && (status == BENTO_OK) && !*out_stop; i++) {
        status =
            move_pipe(flappy, &flappy->pipes[i], step, grace, out_stop);
    }
    return status;
}

/* one frame after dt is known: Y restarts, the first flap takes off,
 * then the bird and the pipes move */
static bento_status_t play_frame(flappy_t *flappy, uint32_t now,
                                 bento_real_t dt, bool *out_stop)
{
    bento_game_keys_t keys;
    bool flapped = false;
    bento_status_t status = BENTO_OK;

    bento_game_keys_init(&keys);
    /* keys() never fails for valid pointers */
    (void)bento_game_keys(&flappy->game, &keys);
    if (bento_game_pressed_once(&flappy->game, BENTO_GAME_KEY_Y,
                                &keys)) {
        flappy->restart_now = true;
        *out_stop = true;
    } else {
        status = flap(flappy, &keys, &flapped);
    }
    /* before take-off the bird floats and the pipes wait */
    if ((status == BENTO_OK) && !*out_stop && !flappy->started &&
        flapped) {
        flappy->started = true;
        flappy->start_ms = now;
    }
    if ((status == BENTO_OK) && !*out_stop && flappy->started) {
        const bool grace =
            bento_time_ticks_diff(now, flappy->start_ms) < GRACE_MS;

        status = move_bird(flappy, dt, grace, out_stop);
        if ((status == BENTO_OK) && !*out_stop) {
            status = move_pipes(flappy, dt, grace, out_stop);
        }
    }
    return status;
}

/* the frame bento_game_run() calls FPS times a second */
static bento_status_t update(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    const uint32_t now = bento_time_ticks_ms();
    const bento_real_t dt = frame_seconds(flappy, now);
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_flappy (file scope, C9) */
    if (dt > (bento_real_t)0) {
        status = play_frame(flappy, now, dt, out_stop);
    }
    return status;
}

/* the C game draws a real gradient; eight bands give the same mood */
static bento_status_t add_sky(flappy_t *flappy)
{
    static const uint32_t sky_bands[SKY_BAND_COUNT] = {
        0x4898F0U, 0x58A2F1U, 0x68ADF2U, 0x78B7F3U,
        0x88C1F5U, 0x98CBF6U, 0xA8D6F7U, 0xB8E0F8U};
    bento_game_t *game = &flappy->game;
    const int32_t band_h = flappy->play_height / SKY_BAND_COUNT;
    bento_game_box_t band;
    bento_status_t status = BENTO_OK;

    bento_game_box_init(&band);
    for (int32_t i = 0; (i < SKY_BAND_COUNT) && (status == BENTO_OK);
         i++) {
        const int32_t height =
            (i < (SKY_BAND_COUNT - 1))
                ? band_h
                : (flappy->play_height - (band_h * i));
        const int32_t band_y = i * band_h;

        status = bento_game_box_create(
            game, &band, (bento_real_t)0, (bento_real_t)band_y,
            (bento_real_t)game->width, (bento_real_t)height,
            sky_bands[i]);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_grass(game, &band, GROUND_H, BENTO_GAME_GB_DARK);
    }
    return status;
}

/* one pipe of three reused ones (32 widgets is the display's budget):
 * full-height top and bottom boxes and two caps, made hidden */
static bento_status_t add_pipe(flappy_t *flappy, pipe_t *pipe)
{
    /* MicroPython's dict order for these four keys */
    static const int32_t hide_order[PIPE_PARTS] = {
        PART_BOTTOM, PART_CAP_TOP, PART_TOP, PART_CAP_BOTTOM};
    bento_game_t *game = &flappy->game;
    bento_status_t status = BENTO_OK;

    for (int32_t part = 0; (part < PIPE_PARTS) && (status == BENTO_OK);
         part++) {
        const bool is_cap = part >= PART_CAP_TOP;

        bento_game_box_init(&pipe->part[part]);
        status = bento_game_box_create(
            game, &pipe->part[part], (bento_real_t)HIDDEN_X,
            (bento_real_t)0, (bento_real_t)(is_cap ? CAP_W : PIPE_W),
            (bento_real_t)(is_cap ? CAP_H : flappy->play_height),
            is_cap ? (uint32_t)CAP_COLOR : BENTO_GAME_GB_DARK);
    }
    for (int32_t i = 0; (i < PIPE_PARTS) && (status == BENTO_OK); i++) {
        status = bento_game_box_hide(game, &pipe->part[hide_order[i]]);
    }
    pipe->x = (bento_real_t)0.0;
    pipe->width = PIPE_W;
    pipe->gap = FIRST_GAP;
    pipe->gap_y = FIRST_GAP_Y;
    pipe->passed = false;
    pipe->gap_top = 0;
    pipe->gap_bottom = flappy->play_height;
    for (int32_t part = 0; part < PIPE_PARTS; part++) {
        pipe->visible[part] = false;
    }
    return status;
}

/* the whole scene once, before the menu: while the player chooses,
 * the display has time to draw it all */
static bento_status_t build_scene(flappy_t *flappy)
{
    bento_game_t *game = &flappy->game;
    char text[HUD_TEXT_BYTES];
    /* about 20 widgets a round: without a clear they pile up past the
     * display's 32 */
    bento_status_t status = bento_game_clear(game);

    flappy->play_height = game->height - GROUND_H;
    bento_game_sprite_init(&flappy->bird);
    bento_game_text_init(&flappy->hud);
    bento_game_text_init(&flappy->status);
    if (status == BENTO_OK) {
        status = add_sky(flappy);
    }
    for (int32_t i = 0; (i < PIPES) && (status == BENTO_OK); i++) {
        status = add_pipe(flappy, &flappy->pipes[i]);
    }
    if (status == BENTO_OK) {
        const int32_t bird_left = BIRD_X - (BIRD_W / 2);
        const int32_t bird_top =
            (flappy->play_height / 2) - (BIRD_H / 2);

        status = bento_game_sprite_create(game, &flappy->bird, "bird",
                                          (bento_real_t)bird_left,
                                          (bento_real_t)bird_top);
    }
    (void)snprintf(text, sizeof(text), "Score: 0    Best: %" PRId32,
                   bento_game_best(game, BEST_NAME));
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &flappy->hud, text, TEXT_X, HUD_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &flappy->status, "", TEXT_X,
                                   STATUS_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* the menu over the drawn scene, headed by why the last round ended;
 * a Y restart skips it. BACK there quits */
static bento_status_t choose_mode(flappy_t *flappy, bool *playing)
{
    static const char *const options[MODE_COUNT] = {"EASY", "NORMAL",
                                                    "HARD"};
    int32_t picked = 0;
    bento_status_t status = BENTO_OK;

    if (!flappy->restart_same) {
        status = bento_game_menu(&flappy->game, options, MODE_COUNT,
                                 (flappy->dead_message != NULL)
                                     ? flappy->dead_message
                                     : "SELECT MODE",
                                 flappy->mode, &picked);
        if ((status == BENTO_OK) && (picked < 0)) {
            *playing = false;
        } else if (status == BENTO_OK) {
            flappy->mode = picked;
        } else {
            /* the status ends the program */
        }
    }
    if (*playing) {
        flappy->restart_same = false;
    }
    return status;
}

/* the round's start: the mode on the status line, the pipes on the
 * right, the bird waiting in the middle */
static bento_status_t start_round(flappy_t *flappy)
{
    bento_game_t *game = &flappy->game;
    char text[STATUS_TEXT_BYTES];
    int32_t middle = 0;
    bento_status_t status = BENTO_OK;

    (void)snprintf(text, sizeof(text),
                   "%s  -  A/UP = flap  (กระพือเพื่อออกตัว)",
                   MODES[flappy->mode].name);
    status = bento_game_text_set(game, &flappy->status, text);
    for (int32_t i = 0; (i < PIPES) && (status == BENTO_OK); i++) {
        const int32_t start_x = game->width + (i * PIPE_SPACING);

        status = reset_pipe(flappy, &flappy->pipes[i],
                            (bento_real_t)start_x);
        if (status == BENTO_OK) {
            status = render_pipe(flappy, &flappy->pipes[i]);
        }
    }
    middle = flappy->play_height / 2;
    flappy->bird_y = (bento_real_t)middle;
    flappy->velocity_y = (bento_real_t)0.0;
    flappy->score = 0;
    flappy->best_seen = bento_game_best(game, BEST_NAME);
    flappy->scroll_rest = (bento_real_t)0.0;
    flappy->dead_message = "GAME OVER";
    flappy->restart_now = false;
    flappy->started = false;
    flappy->start_ms = 0U;
    flappy->last_ms = bento_time_ticks_ms();
    return status;
}

/* after run(): BACK quits keeping the score, Y restarts the same mode,
 * a death shows game over (Start plays again, Back quits) */
static bento_status_t finish_round(flappy_t *flappy, bool finished,
                                   bool *playing)
{
    bento_status_t status = BENTO_OK;

    if (!finished || flappy->restart_now) {
        /* answers the best score, not a status */
        (void)bento_game_save_best(&flappy->game, &flappy->score,
                                   BEST_NAME);
    }
    if (!finished) {
        *playing = false;
    } else if (flappy->restart_now) {
        status = bento_game_replay(&flappy->game);
        flappy->restart_same = true;
    } else {
        status =
            bento_game_over(&flappy->game, &flappy->score, BEST_NAME,
                            flappy->dead_message, playing);
    }
    return status;
}

/* rounds until the player quits: a dead bird goes back to the menu,
 * no reboot needed */
static bento_status_t play_rounds(flappy_t *flappy)
{
    bool playing = true;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && playing) {
        bool finished = false;

        status = build_scene(flappy);
        if (status == BENTO_OK) {
            status = choose_mode(flappy, &playing);
        }
        if ((status == BENTO_OK) && playing) {
            status = start_round(flappy);
        }
        if ((status == BENTO_OK) && playing) {
            status = bento_game_run(&flappy->game, update, NULL, FPS,
                                    &finished);
        }
        if ((status == BENTO_OK) && playing) {
            status = finish_round(flappy, finished, &playing);
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

int main(void)
{
    flappy_t *flappy = &g_flappy;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(flappy, 0, sizeof(*flappy));
    bento_game_init(&flappy->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    flappy->mode = START_MODE;
    status = bento_random_init(&flappy->random);
    if (status == BENTO_OK) {
        status = bento_game_title_melody(&flappy->game, "FLAPPY", NULL,
                                         NULL, 0U, &quit);
    }
    /* the kits' joystick.init() prints this line; the reTerminal's
     * joystick.py prints nothing */
    if ((flappy->game.joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
    if ((status == BENTO_OK) && !quit) {
        status = play_rounds(flappy);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
