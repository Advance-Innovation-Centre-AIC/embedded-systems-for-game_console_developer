/*
 * s13_17_kit_scenes.c -- คาบ 13 (คาบพิเศษ): ชุดฉากสำเร็จรูป ยกมาจากเกมเต็มของจริง
 * พร้อมราคาวิดเจ็ตของแต่ละฉาก
 *
 * ฉาก = กล่องที่สร้างครั้งเดียวตอนจัดฉาก ไม่มีงานต่อเฟรม แต่กินงบวิดเจ็ตทุกชิ้น
 * กด Start แล้วใช้ซ้าย/ขวาเปลี่ยนฉาก (ลบฉากเก่าทิ้งก่อน สร้างฉากใหม่) A = ลูกเล่นของฉาก
 * อวกาศ: A สลับดาว 5 / 12 ดวง · โต๊ะปิงปอง: A เปิด/ปิดเน็ต 14 ขีด
 * ทุ่งหญ้า: A เปลี่ยน EASY/NORMAL/HARD ท่อเลื่อนอย่างเดียว ไม่ resize
 * Game Boy: A เปลี่ยนสีพื้นไล่ 4 โทน
 * ป้ายบนสุดบอกว่าฉากนี้ใช้วิดเจ็ตจริงกี่ชิ้น
 *
 * A gallery of the full games' scenery, one scene on the screen at a time: the space sky
 * with 5 or 12 stars, the ping-pong court with or without its 14-dash net, the meadow with
 * an 8-band sky, grass and three Flappy pipes that only slide (never resize), and the four
 * Game Boy tones with a box snake that walks by moving its tail box to the neck. Left and
 * right delete the old scene and build the next one; A changes the scene's option. The top
 * line counts the widgets the scene really made.
 *
 * Twin of s13_17_kit_scenes.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_17_kit_scenes.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the view dict and each scene's state are one kit_t, at file scope because it is large;
 *   - the made list is a fixed array of 24 tagged widgets (a Box or a Text), copied as made;
 *   - the SCENES table holds names, ink and hints; its build and play functions are a switch;
 *   - the pipe and the snake dicts are structs, the snake's lists are arrays shifted in place;
 *   - CAP_SPRITE is False as in Python, so a pipe cap is a Box (pipe_cap is also a sprite name
 *     in bento_game, for a game that makes its pipes once and never deletes them).
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

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The program and its scenes. */
enum {
    FPS = 30,
    SCENE_COUNT = 4,
    SCENE_SPACE = 0,
    SCENE_COURT = 1,
    SCENE_MEADOW = 2,
    SCENE_GB = 3,
    MADE_MAX =
        24, /* the most widgets one scene makes, with its two labels */
    PIECES_MAX = 14, /* the most boxes a starfield or a net makes */
    STARS_FEW = 5,
    STARS_MANY = 12,
    MODE_COUNT = 3,
    SKY_COUNT = 8,
    GB_COUNT = 4,
    PIPES = 3,
    SNAKE_LENGTH = 5,
    SAY_CHARS = 128, /* a SET_TEXT carries at most 126 bytes */
    COLOR_CHARS =
        12, /* "0x" and up to 8 hex digits of a uint32_t, and the NUL */
    LABEL_X = 12,
    CAPTION_Y = 8,
    HINT_Y = 32
};

/* Scenery sizes, in px. */
enum {
    CHALK_W = 3,
    NET_DASHES = 14,
    NET_DASH_H = 12,
    GROUND_H = 20,
    PIPE_SPACING = 290,
    PIPE_W = 64,
    CAP_W = 82,
    CAP_H = 18,
    PIPE_PARK = -200, /* a new pipe waits off screen */
    GAP_JITTER = 12,  /* each gap is up to 12 px wider or narrower */
    GAP_MARGIN =
        24, /* a gap's centre stays 24 px from the sky and the grass */
    GAP_EDGE = 20, /* a gap's edge stays 20 px from them */
    SWATCH_X = 60,
    SWATCH_STEP = 180,
    SWATCH_Y = 96,
    SWATCH_W = 140,
    SWATCH_H = 56,
    SWATCH_RADIUS = 6,
    SWATCH_NAME_Y = 160,
    SWATCH_HEX_Y = 184,
    CELL = 16,
    SNAKE_COL = 12,
    SNAKE_ROW = 16,
    SNAKE_EVERY = 3, /* the snake steps every 3 frames */
    TURN_RIGHT_COL =
        42, /* the snake walks a rectangle under the swatches */
    TURN_BOTTOM_ROW = 21,
    TURN_LEFT_COL = 6,
    TURN_TOP_ROW = 16
};

/* Colours, 0xRRGGBB. */
enum {
    SPACE_BG = 0x10122C,
    STAR_DIM = 0xB8C0E0,
    FELT =
        0x1C7A34, /* green felt and white chalk, the C game's colours */
    CHALK = 0xF0F8F0,
    CAP_COLOR = 0x7CE85C
};

typedef struct {
    const char *name;
    uint32_t ink;
    const char *hint;
    bool plays; /* the scene has work every frame */
} scene_t;

typedef struct {
    const char *name;
    int32_t gap_px;
    bento_real_t speed_px_s;
} pipe_mode_t;

typedef struct {
    const char *name;
    uint32_t color;
} tone_t;

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

/* The scenes, read by open_scene() and update(). */
static const scene_t SCENES[SCENE_COUNT] = {
    {"SPACE  shooter_full", BENTO_GAME_YELLOW,
     "A = ดาว 5/12 ดวง   ซ้าย/ขวา = ฉากอื่น", false},
    {"COURT  pong_full", BENTO_GAME_WHITE,
     "A = เปิด/ปิดเน็ต   ซ้าย/ขวา = ฉากอื่น", false},
    {"MEADOW + PIPES  flappy_full", BENTO_GAME_WHITE, "", true},
    {"GAME BOY  snake_full", BENTO_GAME_GB_DARKEST, "", true}};

/* (name, gap px, pipe speed px/s) of each meadow mode. */
static const pipe_mode_t MODES[MODE_COUNT] = {
    {"EASY", 175, (bento_real_t)110.0},
    {"NORMAL", 130, (bento_real_t)150.0},
    {"HARD", 105, (bento_real_t)200.0}};

/* The Game Boy palette, darkest to lightest (the C game's values). */
static const tone_t GB[GB_COUNT] = {
    {"GB_DARKEST", BENTO_GAME_GB_DARKEST},
    {"GB_DARK", BENTO_GAME_GB_DARK},
    {"GB_LIGHT", BENTO_GAME_GB_LIGHT},
    {"GB_LIGHTEST", BENTO_GAME_GB_LIGHTEST}};

typedef enum {
    WIDGET_BOX = 0,
    WIDGET_TEXT
} widget_kind_t;

typedef struct {
    widget_kind_t kind;
    bento_game_box_t box;
    bento_game_text_t text;
} made_t;

typedef struct {
    bento_game_box_t top;
    bento_game_box_t bottom;
    bento_game_box_t cap_top;
    bento_game_box_t cap_bottom;
    bool visible;
    bento_real_t x;
    int32_t gap;
    int32_t gap_y;
    int32_t gap_top;
    int32_t gap_bottom;
} pipe_t;

typedef struct {
    cell_t body[SNAKE_LENGTH];
    bento_game_box_t head;
    bento_game_box_t boxes[SNAKE_LENGTH - 1];
} box_snake_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    made_t made[MADE_MAX];
    int32_t made_count;
    bento_game_box_t pieces[PIECES_MAX];
    pipe_t pipes[PIPES];
    bento_real_t carry_px; /* the part of a pixel the pipes still owe */
    box_snake_t snake;
    cell_t heading;
    int32_t snake_tick;
    int32_t scene;
    int32_t option[SCENE_COUNT];
    bento_game_text_t hint;
    char last[SAY_CHARS];
} kit_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern kit_t g_kit;
kit_t g_kit;

/* ---- the made list: every widget of the scene on the screen ---- */

static bento_status_t add_made_box(kit_t *kit,
                                   const bento_game_box_t *box)
{
    bento_status_t status = BENTO_E_NO_RESOURCE;

    if (kit->made_count < MADE_MAX) {
        made_t *slot = &kit->made[kit->made_count];

        slot->kind = WIDGET_BOX;
        slot->box = *box;
        bento_game_text_init(&slot->text);
        kit->made_count += 1;
        status = BENTO_OK;
    }
    return status;
}

static bento_status_t add_made_text(kit_t *kit,
                                    const bento_game_text_t *text)
{
    bento_status_t status = BENTO_E_NO_RESOURCE;

    if (kit->made_count < MADE_MAX) {
        made_t *slot = &kit->made[kit->made_count];

        slot->kind = WIDGET_TEXT;
        bento_game_box_init(&slot->box);
        slot->text = *text;
        kit->made_count += 1;
        status = BENTO_OK;
    }
    return status;
}

static bento_status_t add_plain_box(kit_t *kit, int32_t x, int32_t y,
                                    int32_t width_px, int32_t height_px,
                                    uint32_t color)
{
    bento_game_box_t box;
    bento_status_t status;

    bento_game_box_init(&box);
    status = bento_game_box_create(
        &kit->game, &box, (bento_real_t)x, (bento_real_t)y,
        (bento_real_t)width_px, (bento_real_t)height_px, color);
    if (status == BENTO_OK) {
        status = add_made_box(kit, &box);
    }
    return status;
}

static bento_status_t add_background(kit_t *kit, uint32_t color)
{
    bento_game_box_t box;
    bento_status_t status;

    bento_game_box_init(&box);
    status = bento_game_background(&kit->game, &box, color);
    if (status == BENTO_OK) {
        status = add_made_box(kit, &box);
    }
    return status;
}

/* The boxes a starfield or a net left in kit->pieces join the made list. */
static bento_status_t add_pieces(kit_t *kit, int32_t count)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < count); i++) {
        status = add_made_box(kit, &kit->pieces[i]);
    }
    return status;
}

static void delete_made(kit_t *kit)
{
    int32_t i;

    for (i = 0; i < kit->made_count; i++) {
        if (kit->made[i].kind == WIDGET_TEXT) {
            bento_game_text_delete(&kit->game, &kit->made[i].text);
        } else {
            bento_game_box_delete(&kit->game, &kit->made[i].box);
        }
    }
    kit->made_count = 0;
}

/* ---- the space scene (shooter_full uses 5 stars; its sky stands still) ---- */

static bento_status_t make_space(kit_t *kit, int32_t stars)
{
    bento_status_t status = add_background(kit, SPACE_BG);

    if (status == BENTO_OK) {
        status = bento_game_starfield(&kit->game, kit->pieces,
                                      (uint32_t)stars, STAR_DIM);
    }
    if (status == BENTO_OK) {
        status = add_pieces(kit, stars);
    }
    return status;
}

/* ---- the ping-pong court and its net ---- */

static bento_status_t make_court(kit_t *kit, bool with_net)
{
    const int32_t width = kit->game.width;
    const int32_t height = kit->game.height;
    bento_status_t status = add_background(kit, FELT);

    if (status == BENTO_OK) {
        status = add_plain_box(kit, 0, 0, width, CHALK_W, CHALK);
    }
    if (status == BENTO_OK) {
        status = add_plain_box(kit, 0, height - CHALK_W, width, CHALK_W,
                               CHALK);
    }
    if (status == BENTO_OK) {
        status = add_plain_box(kit, 0, 0, CHALK_W, height, CHALK);
    }
    if (status == BENTO_OK) {
        status = add_plain_box(kit, width - CHALK_W, 0, CHALK_W, height,
                               CHALK);
    }
    if ((status == BENTO_OK) && with_net) {
        /* 14 dashes = 14 widgets, the dearest part of this scene */
        status =
            bento_game_net(&kit->game, kit->pieces,
                           (uint32_t)NET_DASHES, CHALK, NET_DASH_H);
        if (status == BENTO_OK) {
            status = add_pieces(kit, NET_DASHES);
        }
    }
    return status;
}

/* ---- the meadow: a sky in 8 bands over the grass ---- */

static int32_t play_height(const kit_t *kit)
{
    return kit->game.height - GROUND_H;
}

static bento_status_t make_meadow(kit_t *kit)
{
    static const uint32_t sky_bands[SKY_COUNT] = {
        0x4898F0U, 0x58A2F1U, 0x68ADF2U, 0x78B7F3U,
        0x88C1F5U, 0x98CBF6U, 0xA8D6F7U, 0xB8E0F8U};
    const int32_t band_h = play_height(kit) / SKY_COUNT;
    bento_game_box_t grass;
    bento_status_t status = BENTO_OK;
    int32_t i;

    /* dark at the top, light below, in place of the C game's gradient */
    for (i = 0; (status == BENTO_OK) && (i < SKY_COUNT); i++) {
        const int32_t height = (i < (SKY_COUNT - 1))
                                   ? band_h
                                   : (play_height(kit) - (band_h * i));

        status = add_plain_box(kit, 0, i * band_h, kit->game.width,
                               height, sky_bands[i]);
    }
    bento_game_box_init(&grass);
    if (status == BENTO_OK) {
        status = bento_game_grass(&kit->game, &grass, GROUND_H,
                                  BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        status = add_made_box(kit, &grass);
    }
    return status;
}

/* ---- Flappy pipes: a fixed size, they only slide ---- */

/* A pipe is born again on the right: a random gap, gap place and tint; no resize. */
static bento_status_t place_pipe(kit_t *kit, pipe_t *pipe, int32_t gap,
                                 bento_real_t x)
{
    static const uint32_t pipe_tints[MODE_COUNT] = {
        0x6CD84CU, 0x4CB838U, 0x84E060U};
    int32_t jitter = 0;
    int32_t tint = 0;
    int32_t half;
    bento_status_t status = bento_random_randint(
        &kit->random, -GAP_JITTER, GAP_JITTER, &jitter);

    pipe->x = x;
    pipe->gap = gap + jitter;
    half = pipe->gap / 2;
    if (status == BENTO_OK) {
        status = bento_random_randint(
            &kit->random, half + GAP_MARGIN,
            play_height(kit) - half - GAP_MARGIN, &pipe->gap_y);
    }
    if (status == BENTO_OK) {
        const int32_t top = pipe->gap_y - half;
        const int32_t bottom = pipe->gap_y + half;

        pipe->gap_top = (top > GAP_EDGE) ? top : GAP_EDGE;
        pipe->gap_bottom = (bottom < (play_height(kit) - GAP_EDGE))
                               ? bottom
                               : (play_height(kit) - GAP_EDGE);
        status = bento_random_randint(&kit->random, 0, MODE_COUNT - 1,
                                      &tint);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(&kit->game, &pipe->top,
                                          pipe_tints[tint]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(&kit->game, &pipe->bottom,
                                          pipe_tints[tint]);
    }
    return status;
}

static bento_status_t show_pipe(kit_t *kit, const pipe_t *pipe, bool on)
{
    const bento_game_box_t *const parts[] = {
        &pipe->top, &pipe->bottom, &pipe->cap_top, &pipe->cap_bottom};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    for (i = 0U; (status == BENTO_OK) &&
                 (i < (sizeof(parts) / sizeof(parts[0])));
         i++) {
        status = on ? bento_game_box_show(&kit->game, parts[i])
                    : bento_game_box_hide(&kit->game, parts[i]);
    }
    return status;
}

/* The part beyond the screen edge is cut by the display itself. */
static bento_status_t move_pipe(kit_t *kit, pipe_t *pipe, int32_t x)
{
    const int32_t cap_x = (x + (PIPE_W / 2)) - (CAP_W / 2);
    bento_game_t *game = &kit->game;
    bento_status_t status;

    /* the top pipe's lower edge is the gap's upper edge */
    status = bento_game_box_move_to(
        game, &pipe->top, (bento_real_t)x,
        (bento_real_t)(pipe->gap_top - play_height(kit)));
    if (status == BENTO_OK) {
        status =
            bento_game_box_move_to(game, &pipe->bottom, (bento_real_t)x,
                                   (bento_real_t)pipe->gap_bottom);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            game, &pipe->cap_top, (bento_real_t)cap_x,
            (bento_real_t)(pipe->gap_top - CAP_H));
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(game, &pipe->cap_bottom,
                                        (bento_real_t)cap_x,
                                        (bento_real_t)pipe->gap_bottom);
    }
    return status;
}

static bento_status_t render_pipe(kit_t *kit, pipe_t *pipe)
{
    const int32_t x = (int32_t)pipe->x;
    const bool on = (-PIPE_W < x) && (x < kit->game.width);
    bento_status_t status = BENTO_OK;

    if (on != pipe->visible) {
        /* show or hide only when it changes */
        status = show_pipe(kit, pipe, on);
        pipe->visible = on;
    }
    if ((status == BENTO_OK) && on) {
        status = move_pipe(kit, pipe, x);
    }
    return status;
}

static bento_status_t create_pipe(kit_t *kit, pipe_t *pipe)
{
    bento_game_box_t *const parts[] = {
        &pipe->top, &pipe->bottom, &pipe->cap_top, &pipe->cap_bottom};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    /* four widgets a pipe: the top and bottom pipe as tall as the field, and two caps */
    for (i = 0U; (status == BENTO_OK) &&
                 (i < (sizeof(parts) / sizeof(parts[0])));
         i++) {
        const bool is_pipe = (i < 2U);

        bento_game_box_init(parts[i]);
        status = bento_game_box_create(
            &kit->game, parts[i], (bento_real_t)PIPE_PARK,
            (bento_real_t)0, (bento_real_t)(is_pipe ? PIPE_W : CAP_W),
            (bento_real_t)(is_pipe ? play_height(kit) : CAP_H),
            is_pipe ? BENTO_GAME_GB_DARK : (uint32_t)CAP_COLOR);
    }
    pipe->visible = true;
    return status;
}

static bento_status_t make_pipes(kit_t *kit, int32_t gap)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < PIPES); i++) {
        status = create_pipe(kit, &kit->pipes[i]);
    }
    /* the first pipe just past the right edge, the next ones PIPE_SPACING apart */
    for (i = 0; (status == BENTO_OK) && (i < PIPES); i++) {
        const int32_t first_x = kit->game.width + (i * PIPE_SPACING);

        status =
            place_pipe(kit, &kit->pipes[i], gap, (bento_real_t)first_x);
        if (status == BENTO_OK) {
            status = render_pipe(kit, &kit->pipes[i]);
        }
    }
    return status;
}

static bento_real_t rightmost_pipe_x(const kit_t *kit)
{
    bento_real_t most = kit->pipes[0].x;
    int32_t i;

    for (i = 1; i < PIPES; i++) {
        if (kit->pipes[i].x > most) {
            most = kit->pipes[i].x;
        }
    }
    return most;
}

/* Every frame: step_px is this frame's whole pixels. */
static bento_status_t step_pipes(kit_t *kit, int32_t step_px,
                                 int32_t gap)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < PIPES); i++) {
        pipe_t *pipe = &kit->pipes[i];

        pipe->x -= (bento_real_t)step_px;
        if ((pipe->x + (bento_real_t)PIPE_W) < (bento_real_t)0) {
            /* off the left edge: born again on the right */
            status = place_pipe(kit, pipe, gap,
                                rightmost_pipe_x(kit) +
                                    (bento_real_t)PIPE_SPACING);
        }
        if (status == BENTO_OK) {
            status = render_pipe(kit, pipe);
        }
    }
    return status;
}

/* ---- the Game Boy palette and the box snake that moves its tail ---- */

static bento_status_t make_box_snake(kit_t *kit, int32_t col,
                                     int32_t row)
{
    box_snake_t *snake = &kit->snake;
    bento_status_t status;
    int32_t i;

    for (i = 0; i < SNAKE_LENGTH; i++) {
        snake->body[i] = (cell_t){col - i, row};
    }
    /* the head darkest, the body dark, as snake_full */
    bento_game_box_init(&snake->head);
    status = bento_game_box_create(
        &kit->game, &snake->head,
        (bento_real_t)(snake->body[0].col * CELL),
        (bento_real_t)(snake->body[0].row * CELL),
        (bento_real_t)(CELL - 2), (bento_real_t)(CELL - 2),
        BENTO_GAME_GB_DARKEST);
    for (i = 1; (status == BENTO_OK) && (i < SNAKE_LENGTH); i++) {
        bento_game_box_t *box = &snake->boxes[i - 1];

        bento_game_box_init(box);
        status = bento_game_box_create(
            &kit->game, box, (bento_real_t)(snake->body[i].col * CELL),
            (bento_real_t)(snake->body[i].row * CELL),
            (bento_real_t)(CELL - 2), (bento_real_t)(CELL - 2),
            BENTO_GAME_GB_DARK);
    }
    return status;
}

/* The whole snake seems to move, but only the head and the tail box change. */
static bento_status_t step_box_snake(kit_t *kit, int32_t dx, int32_t dy)
{
    box_snake_t *snake = &kit->snake;
    const cell_t old = snake->body[0];
    const cell_t fresh = {old.col + dx, old.row + dy};
    bento_game_box_t tail = snake->boxes[SNAKE_LENGTH - 2];
    bento_status_t status;
    int32_t i;

    for (i = SNAKE_LENGTH - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = fresh;
    status = bento_game_box_move_to(&kit->game, &snake->head,
                                    (bento_real_t)(fresh.col * CELL),
                                    (bento_real_t)(fresh.row * CELL));
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&kit->game, &tail,
                                        (bento_real_t)(old.col * CELL),
                                        (bento_real_t)(old.row * CELL));
    }
    for (i = SNAKE_LENGTH - 2; i > 0; i--) {
        snake->boxes[i] = snake->boxes[i - 1];
    }
    snake->boxes[0] = tail; /* the tail box becomes the neck */
    return status;
}

static uint32_t ground_tone(int32_t option)
{
    return GB[(GB_COUNT - 1) - (option % GB_COUNT)].color;
}

static bento_status_t add_swatch(kit_t *kit, int32_t tone)
{
    const int32_t x = SWATCH_X + (tone * SWATCH_STEP);
    char hex[COLOR_CHARS];
    bento_game_box_t box;
    bento_game_text_t name;
    bento_game_text_t value;
    bento_status_t status;

    bento_game_box_init(&box);
    bento_game_text_init(&name);
    bento_game_text_init(&value);
    status = bento_game_box_create_styled(
        &kit->game, &box, (bento_real_t)x, (bento_real_t)SWATCH_Y,
        (bento_real_t)SWATCH_W, (bento_real_t)SWATCH_H, GB[tone].color,
        (int32_t)BENTO_GAME_GB_DARKEST, SWATCH_RADIUS, 2);
    if (status == BENTO_OK) {
        status = add_made_box(kit, &box);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(&kit->game, &name,
                                        GB[tone].name, x, SWATCH_NAME_Y,
                                        BENTO_GAME_GB_DARKEST);
    }
    if (status == BENTO_OK) {
        status = add_made_text(kit, &name);
    }
    if (status == BENTO_OK) {
        (void)snprintf(hex, sizeof(hex), "0x%06X",
                       (unsigned int)GB[tone].color);
        status =
            bento_game_text_create(&kit->game, &value, hex, x,
                                   SWATCH_HEX_Y, BENTO_GAME_GB_DARKEST);
    }
    if (status == BENTO_OK) {
        status = add_made_text(kit, &value);
    }
    return status;
}

static bento_status_t build_gb(kit_t *kit, int32_t option)
{
    bento_status_t status = add_background(kit, ground_tone(option));
    int32_t i;

    /* the four tones, each with its name and value */
    for (i = 0; (status == BENTO_OK) && (i < GB_COUNT); i++) {
        status = add_swatch(kit, i);
    }
    if (status == BENTO_OK) {
        status = make_box_snake(kit, SNAKE_COL, SNAKE_ROW);
    }
    if (status == BENTO_OK) {
        status = add_made_box(kit, &kit->snake.head);
    }
    for (i = 0; (status == BENTO_OK) && (i < (SNAKE_LENGTH - 1)); i++) {
        status = add_made_box(kit, &kit->snake.boxes[i]);
    }
    kit->heading = (cell_t){1, 0};
    kit->snake_tick = 0;
    return status;
}

static bento_status_t build_meadow(kit_t *kit, int32_t option)
{
    bento_status_t status = make_meadow(kit);
    int32_t i;

    if (status == BENTO_OK) {
        status = make_pipes(kit, MODES[option % MODE_COUNT].gap_px);
    }
    for (i = 0; (status == BENTO_OK) && (i < PIPES); i++) {
        status = add_made_box(kit, &kit->pipes[i].top);
        if (status == BENTO_OK) {
            status = add_made_box(kit, &kit->pipes[i].bottom);
        }
        if (status == BENTO_OK) {
            status = add_made_box(kit, &kit->pipes[i].cap_top);
        }
        if (status == BENTO_OK) {
            status = add_made_box(kit, &kit->pipes[i].cap_bottom);
        }
    }
    kit->carry_px = (bento_real_t)0;
    return status;
}

static bento_status_t build_scene(kit_t *kit, int32_t scene)
{
    const int32_t option = kit->option[scene];
    bento_status_t status;

    switch (scene) {
    case SCENE_SPACE:
        status = make_space(kit, ((option % 2) != 0) ? STARS_MANY
                                                     : STARS_FEW);
        break;
    case SCENE_COURT:
        status = make_court(kit, (option % 2) == 0);
        break;
    case SCENE_MEADOW:
        status = build_meadow(kit, option);
        break;
    default:
        status = build_gb(kit, option);
        break;
    }
    return status;
}

/* ---- one scene at a time ---- */

/* The label is written only when its text changes. */
static bento_status_t say(kit_t *kit, const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(text, kit->last) != 0) {
        status = bento_game_text_set(&kit->game, &kit->hint, text);
        (void)snprintf(kit->last, sizeof(kit->last), "%s", text);
    }
    return status;
}

/* Delete every widget of the old scene, build the new one, then its labels on top. */
static bento_status_t open_scene(kit_t *kit, int32_t scene)
{
    const scene_t *view = &SCENES[scene];
    char caption_text[SAY_CHARS];
    bento_game_text_t caption;
    bento_status_t status;

    delete_made(kit);
    status = build_scene(kit, scene);
    bento_game_text_init(&caption);
    bento_game_text_init(&kit->hint);
    if (status == BENTO_OK) {
        (void)snprintf(caption_text, sizeof(caption_text),
                       "%d/%d  %s   ใช้ %d ชิ้น", (int)(scene + 1),
                       SCENE_COUNT, view->name, (int)kit->made_count);
        status =
            bento_game_text_create(&kit->game, &caption, caption_text,
                                   LABEL_X, CAPTION_Y, view->ink);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(&kit->game, &kit->hint, view->hint,
                                   LABEL_X, HINT_Y, view->ink);
    }
    (void)snprintf(kit->last, sizeof(kit->last), "%s", view->hint);
    kit->scene = scene;
    if (status == BENTO_OK) {
        status = add_made_text(kit, &caption);
    }
    if (status == BENTO_OK) {
        status = add_made_text(kit, &kit->hint);
    }
    return status;
}

static bento_status_t play_meadow(kit_t *kit, int32_t option)
{
    const pipe_mode_t *mode = &MODES[option % MODE_COUNT];
    const bento_real_t frame_s = (bento_real_t)1.0 / (bento_real_t)FPS;
    const bento_real_t moved = mode->speed_px_s * frame_s;
    char text[SAY_CHARS];
    int32_t step_px;
    bento_status_t status;

    /* gather the parts of a pixel; move once a whole pixel is due */
    kit->carry_px += moved;
    step_px = (int32_t)kit->carry_px;
    kit->carry_px -= (bento_real_t)step_px;
    status = step_pipes(kit, step_px, mode->gap_px);
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "A = %s  ช่อง %d  ท่อ %d px/s   ซ้าย/ขวา = ฉากอื่น",
                       mode->name, (int)mode->gap_px,
                       (int)mode->speed_px_s);
        status = say(kit, text);
    }
    return status;
}

/* The snake walks a rectangle under the swatches: right, down, left, up. */
static cell_t turn_at_corner(cell_t heading, cell_t head)
{
    cell_t next = heading;

    if (heading.col == 1) {
        next = (head.col >= TURN_RIGHT_COL) ? (cell_t){0, 1} : heading;
    } else if (heading.row == 1) {
        next =
            (head.row >= TURN_BOTTOM_ROW) ? (cell_t){-1, 0} : heading;
    } else if (heading.col == -1) {
        next = (head.col <= TURN_LEFT_COL) ? (cell_t){0, -1} : heading;
    } else {
        next = (head.row <= TURN_TOP_ROW) ? (cell_t){1, 0} : heading;
    }
    return next;
}

static bento_status_t play_gb(kit_t *kit, int32_t option)
{
    char text[SAY_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "A = สีพื้น: %s   ซ้าย/ขวา = ฉากอื่น",
                   GB[(GB_COUNT - 1) - (option % GB_COUNT)].name);
    status = say(kit, text);
    kit->snake_tick += 1;
    if ((status == BENTO_OK) &&
        ((kit->snake_tick % SNAKE_EVERY) == 0)) {
        kit->heading = turn_at_corner(kit->heading, kit->snake.body[0]);
        status =
            step_box_snake(kit, kit->heading.col, kit->heading.row);
    }
    return status;
}

/* A changes the scene's option: a still scene is built again, the Game Boy only recolours. */
static bento_status_t press_option(kit_t *kit, int32_t scene)
{
    bento_status_t status = BENTO_OK;

    kit->option[scene] += 1;
    bento_game_sfx(&kit->game, "select");
    if (!SCENES[scene].plays) {
        status = open_scene(kit, scene);
    } else if (scene == SCENE_GB) {
        status =
            bento_game_box_set_color(&kit->game, &kit->made[0].box,
                                     ground_tone(kit->option[scene]));
    } else {
        /* the meadow uses its new mode from the next pipe on */
    }
    return status;
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    kit_t *kit = &g_kit;
    const int32_t scene = kit->scene;
    bento_game_keys_t keys;
    bool right;
    bool left;
    bento_status_t status = BENTO_OK;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: the game runs until Back */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &kit->game,
        &keys); /* never fails: a silent pad is all keys up */
    right = bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_RIGHT,
                                    &keys);
    left =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_LEFT, &keys);
    if (bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_A, &keys)) {
        status = press_option(kit, scene);
    }
    if ((status == BENTO_OK) && (right != left)) {
        /* change the scene only on the frame the key went down */
        status =
            open_scene(kit, (scene + (right ? 1 : (SCENE_COUNT - 1))) %
                                SCENE_COUNT);
        bento_game_sfx(&kit->game, "move");
    } else if ((status == BENTO_OK) && (scene == SCENE_MEADOW)) {
        status = play_meadow(kit, kit->option[scene]);
    } else if ((status == BENTO_OK) && (scene == SCENE_GB)) {
        status = play_gb(kit, kit->option[scene]);
    } else {
        /* a still scene has no work this frame */
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
    kit->option[SCENE_MEADOW] = 1; /* the meadow opens on NORMAL */
    status = bento_random_init(&kit->random);
    if (status == BENTO_OK) {
        bento_game_init(&kit->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status =
            bento_game_title(&kit->game, "KIT SCENES", NULL, &quit);
        report_joystick(&kit->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = open_scene(kit, SCENE_SPACE);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&kit->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
