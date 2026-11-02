/*
 * s13_conveyor_sort.c -- คาบ 13 (คาบพิเศษ): สายพานคัดแยกพัสดุ = Shooter คาบ 12 ที่เปลี่ยนความหมาย
 *
 * โค้ดเดิมจาก shooter_step4.py ใช้ต่อได้เกือบทั้งหมด สิ่งที่เปลี่ยนคือ "ความหมาย"
 * กด Start แล้วกด A ค้าง = สายพานเร่ง (แถบบนสายพานวิ่งตาม) ปล่อย = ค่อย ๆ หยุด
 * จอยซ้าย = ประตูชี้ช่อง A (แดง) จอยขวา = ประตูชี้ช่อง B (น้ำเงิน) ช่องที่ชี้อยู่จะสว่าง
 * กล่องป้ายแดงต้องลงช่อง A กล่องป้ายน้ำเงินต้องลงช่อง B คัดผิด = ระเบิดที่ช่องนั้น
 * คัดผิดครบ 3 ครั้ง = สายพานหยุด (STOPPED) ผิดได้อีกครั้งเดียว = มีบรรทัดเตือนสีส้ม
 *
 * A parcel sorter told with the Shooter's code: four boxes ride a vertical belt that speeds
 * up while A is held and coasts to a stop when it is released. Left and right swing the gate
 * to chute A (red) or chute B (blue); a box that reaches the gate drops into the chute the
 * gate points at. A wrong chute explodes and costs one of three rejects; after the third the
 * belt stops, the explosion finishes and the program ends. The pure logic (belt_step, judge,
 * next_y_above) is checked once before the screen opens.
 *
 * Twin of s13_conveyor_sort.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_conveyor_sort.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the module globals and the shown dict are one conveyor_t, at file scope because it is large;
 *   - each label keeps the text it shows in a buffer, where Python keeps it in the shown dict;
 *   - the machine state string ("IDLE", "RUNNING", "STOPPED") is an enum;
 *   - a failed self-test case prints its number as MicroPython's str() of a float (bento_fmt),
 *     so next_y_above's -110 would print as -110.0 there;
 *   - on_frame's return False sets *out_stop; raise SystemExit after a failed self test ends
 *     main() with 0.
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

/* The belt (change these). */
/* The value cards, in the order they are made. */
enum {
    CARD_OK = 0,
    CARD_WRONG,
    CARD_LEFT,
    CARD_BELT
};

enum {
    BOXES = 4,   /* กล่องบนสายพาน (ชุดหมุนเวียน สร้างครั้งเดียว) */
    GAP = 78,    /* ระยะห่างระหว่างกล่อง (px) */
    REJECTS = 3, /* คัดผิดได้กี่ครั้งก่อนหยุดสายพาน */
    SIDES = 2,
    SLATS = 2,
    CARDS = 4,
    BOOM_PICTURES = 6,
    BOOM_STEP = 3, /* the explosion changes picture every 3 frames */
    FLASH_FRAMES = 6,
    OTHER_CASES =
        4, /* the self test's three judge cases and one next_y_above case */
    FPS = 30
};

/* Where things are, in px. */
enum {
    BELT_X = 336,
    BELT_W = 120,
    BELT_END = 300,
    BELT_TOP = 40,
    BELT_RAIL = 4,
    SLAT_Y0 = 60,
    SLAT_STEP = 130,
    SLAT_H = 8,
    SLAT_WRAP = 260,
    BOX_W = 54,
    BOX_H = 42,
    BOX_FIRST =
        70, /* the first box starts 70 px above the belt's end */
    BOX_RADIUS = 3,
    BOX_EDGE =
        8, /* a wide brown edge = the box, the middle = its colour tag */
    LANE_X = BELT_X + ((BELT_W - BOX_W) / 2),
    GATE_Y = 304,
    GATE_W = 160,
    GATE_H = 12,
    GATE_RADIUS = 4,
    CHUTE_Y = 322,
    CHUTE_W = 230,
    CHUTE_H = 46,
    CHUTE_RADIUS = 6,
    CHUTE_EDGE = 3,
    CHUTE_TEXT_X = 24,
    CHUTE_TEXT_Y = 334,
    BOOM_LEFT = 50,
    BOOM_Y = 333,
    BAR_EDGE_PX = -4,
    BAR_W = 800,
    BAR_H = 44,
    BAR_RADIUS = 4,
    TITLE_X = 14,
    TITLE_Y = 9,
    STATE_X = 600,
    WARN_X = 18,
    WARN_Y = 240,
    CARD_W = 230,
    CARD_H = 78,
    CARD_RADIUS = 8,
    CARD_TEXT_X = 16,
    CARD_TEXT_Y = 15,
    CARD_LEFT_X = 14,
    CARD_RIGHT_X = 548,
    CARD_TOP_Y = 52,
    CARD_LOW_Y = 142,
    TENTHS = 10,
    LABEL_CHARS = 128 /* a SET_TEXT carries at most 126 bytes */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_BELT = 0x202833,
    COLOR_RAIL = 0x8899AA,
    COLOR_SLAT = 0x46546A,
    COLOR_CARDBOARD = 0xC08A4E,
    COLOR_PANEL = 0x1B2633,
    COLOR_EDGE = 0x3D5266,
    COLOR_RUNNING = 0x1C5A34,
    COLOR_STOPPED = 0xB02020
};

typedef enum {
    MACHINE_IDLE = 0,
    MACHINE_RUNNING,
    MACHINE_STOPPED
} machine_state_t;

/* Colour tag 0 belongs in chute A, tag 1 in chute B. */
static const uint32_t COLORS[SIDES] = {BENTO_GAME_RED, BENTO_GAME_BLUE};
/* The gate swings towards chute A (left) or chute B (right). */
static const int32_t GATE_X[SIDES] = {BELT_X - 100, BELT_X + 60};
static const int32_t CHUTE_X[SIDES] = {146, 416};
static const uint32_t DIM[SIDES] = {
    0x2A1416U, 0x14202EU}; /* a chute the gate does not point at */
static const uint32_t LIT[SIDES] = {
    0x6A2226U, 0x1E4A78U}; /* the chute the gate points at */

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct {
    uint32_t color;
    int32_t border; /* a colour, or BENTO_GAME_BORDER_NONE */
    int32_t radius;
    int32_t border_w;
} box_style_t;

/* A label that remembers the text it shows, so it is written only when the text changes. */
typedef struct {
    bento_game_text_t text;
    char shown[LABEL_CHARS];
} label_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t slats[SLATS];
    bento_game_box_t boxes[BOXES];
    int32_t box_color[BOXES]; /* a box does not know its own colour */
    bento_game_box_t bar;
    bento_game_text_t hud_state;
    label_t warn;
    bento_game_box_t gate;
    bento_game_box_t chutes[SIDES];
    label_t chute_texts[SIDES];
    bento_game_sprite_t boom;
    label_t cards[CARDS];
    bento_real_t speed;
    int32_t gate_side;
    int32_t sorted_ok;
    int32_t wrong;
    int32_t rejects_left;
    int32_t count[SIDES]; /* boxes dropped into each chute */
    int32_t boom_frames;  /* frames left of the explosion */
    int32_t flash_frames; /* frames left of the chute's flash */
    machine_state_t shown_state;
    bool
        state_known; /* false = nothing shown yet (Python's missing key) */
} conveyor_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern conveyor_t g_conveyor;
conveyor_t g_conveyor;

/* ---- the brain: pure, no screen ---- */

/* Held = speed up (with a ceiling); released = friction; nearly still = stopped. */
static bento_real_t belt_step(bento_real_t speed, bool pressed,
                              bento_real_t accel, bento_real_t friction,
                              bento_real_t limit)
{
    const bento_real_t still_below = (bento_real_t)0.05;
    bento_real_t next;

    if (pressed) {
        const bento_real_t faster = speed + accel;

        next = (faster < limit) ? faster : limit;
    } else {
        next = speed * friction;
        if (next < still_below) {
            next = (bento_real_t)0;
        }
    }
    return next;
}

/* The chute of the same colour = sorted right. */
static bool judge(int32_t color_index, int32_t gate_side)
{
    return color_index == gate_side;
}

/* A box goes back to the end of the line: one gap above the highest box. */
static bento_real_t next_y_above(const bento_real_t heights[],
                                 uint32_t count, bento_real_t gap)
{
    bento_real_t lowest = heights[0];
    uint32_t i;

    for (i = 1U; i < count; i++) {
        if (heights[i] < lowest) {
            lowest = heights[i];
        }
    }
    return lowest - gap;
}

/* ---- check the brain before the screen opens ---- */

static bool check_real(const char *name, bento_real_t got,
                       bento_real_t want)
{
    const bento_real_t tolerance = (bento_real_t)1e-6;
    const bento_real_t difference =
        (got > want) ? (got - want) : (want - got);
    char got_text[BENTO_FMT_BUF_SIZE];
    char want_text[BENTO_FMT_BUF_SIZE];
    uint32_t length = 0U;

    if (difference > tolerance) {
        /* BENTO_FMT_BUF_SIZE always holds the text, so neither call fails */
        (void)bento_fmt_real(got_text, (uint32_t)sizeof(got_text), got,
                             &length);
        (void)bento_fmt_real(want_text, (uint32_t)sizeof(want_text),
                             want, &length);
        (void)printf("ยังไม่ผ่าน: %s ได้ %s ควรได้ %s\n", name, got_text,
                     want_text);
    }
    return difference <= tolerance;
}

static bool check_bool(const char *name, bool got, bool want)
{
    if (got != want) {
        (void)printf("ยังไม่ผ่าน: %s ได้ %s ควรได้ %s\n", name,
                     got ? "True" : "False", want ? "True" : "False");
    }
    return got == want;
}

/* belt_step's cases, every one with accel 0.5, friction 0.9 and a ceiling of 6.0. */
static bool check_belt(uint32_t *count)
{
    typedef struct {
        bento_real_t speed;
        bool pressed;
        bento_real_t want;
    } belt_case_t;
    static const belt_case_t belt_tests[] = {
        {(bento_real_t)0.0, true, (bento_real_t)0.5},
        {(bento_real_t)5.8, true, (bento_real_t)6.0}, /* ชนเพดาน */
        {(bento_real_t)4.0, false,
         (bento_real_t)3.6}, /* ปล่อย = คูณแรงเสียดทาน */
        {(bento_real_t)0.04, false,
         (bento_real_t)0.0}}; /* ช้ามาก = หยุดสนิท */
    const bento_real_t test_accel = (bento_real_t)0.5;
    const bento_real_t test_friction = (bento_real_t)0.9;
    const bento_real_t test_limit = (bento_real_t)6.0;
    const uint32_t belt_cases =
        (uint32_t)(sizeof(belt_tests) / sizeof(belt_tests[0]));
    bool passed = true;
    uint32_t i;

    for (i = 0U; passed && (i < belt_cases); i++) {
        const belt_case_t *test = &belt_tests[i];

        passed =
            check_real("belt_step",
                       belt_step(test->speed, test->pressed, test_accel,
                                 test_friction, test_limit),
                       test->want);
    }
    *count = belt_cases;
    return passed;
}

/* judge's three cases and next_y_above's one. */
static bool check_judge_and_line(void)
{
    static const bento_real_t test_heights[] = {
        (bento_real_t)100, (bento_real_t)-20, (bento_real_t)300};
    const bento_real_t test_gap = (bento_real_t)90;
    const bento_real_t want_above = (bento_real_t)-110;
    const uint32_t heights_count =
        (uint32_t)(sizeof(test_heights) / sizeof(test_heights[0]));
    bool passed = check_bool("judge", judge(0, 0), true);

    if (passed) {
        passed = check_bool("judge", judge(1, 1), true);
    }
    if (passed) {
        passed = check_bool("judge", judge(1, 0), false);
    }
    if (passed) {
        passed = check_real(
            "next_y_above",
            next_y_above(test_heights, heights_count, test_gap),
            want_above);
    }
    return passed;
}

static bool self_test(void)
{
    uint32_t belt_cases = 0U;
    bool passed = check_belt(&belt_cases);

    if (passed) {
        passed = check_judge_and_line();
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของสายพานถูกทั้ง %d กรณี\n",
                     (int)belt_cases + OTHER_CASES);
    }
    return passed;
}

/* ---- the screen: made once (made first = below, made later = on top) ---- */

static bento_status_t add_box(bento_game_t *game, bento_game_box_t *box,
                              rect_t place, box_style_t style)
{
    bento_game_box_init(box);
    return bento_game_box_create_styled(
        game, box, (bento_real_t)place.x, (bento_real_t)place.y,
        (bento_real_t)place.w, (bento_real_t)place.h, style.color,
        style.border, style.radius, style.border_w);
}

static bento_status_t add_label(bento_game_t *game, label_t *label,
                                const char *text, int32_t x, int32_t y,
                                uint32_t color)
{
    label->shown[0] = '\0';
    bento_game_text_init(&label->text);
    return bento_game_text_create(game, &label->text, text, x, y,
                                  color);
}

static bento_status_t add_belt(conveyor_t *conveyor)
{
    bento_game_t *game = &conveyor->game;
    bento_game_box_t belt;
    const rect_t belt_place = {BELT_X, BELT_TOP, BELT_W,
                               BELT_END - BELT_TOP};
    const box_style_t belt_style = {COLOR_BELT, COLOR_RAIL, 1,
                                    BELT_RAIL};
    bento_status_t status =
        add_box(game, &belt, belt_place, belt_style);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SLATS); i++) {
        const rect_t place = {BELT_X + BELT_RAIL,
                              SLAT_Y0 + (i * SLAT_STEP),
                              BELT_W - (2 * BELT_RAIL), SLAT_H};
        const box_style_t style = {COLOR_SLAT, BENTO_GAME_BORDER_NONE,
                                   1, 0};

        status = add_box(game, &conveyor->slats[i], place, style);
    }
    /* every colour is drawn before the first box is made */
    for (i = 0; (status == BENTO_OK) && (i < BOXES); i++) {
        status = bento_random_randint(&conveyor->random, 0, 1,
                                      &conveyor->box_color[i]);
    }
    for (i = 0; (status == BENTO_OK) && (i < BOXES); i++) {
        const rect_t place = {LANE_X, BELT_END - BOX_FIRST - (i * GAP),
                              BOX_W, BOX_H};
        const box_style_t style = {COLORS[conveyor->box_color[i]],
                                   COLOR_CARDBOARD, BOX_RADIUS,
                                   BOX_EDGE};

        status = add_box(game, &conveyor->boxes[i], place, style);
    }
    return status;
}

static bento_status_t add_status_bar(conveyor_t *conveyor)
{
    bento_game_t *game = &conveyor->game;
    bento_game_text_t title;
    /* the bar reaches past the screen's edges and hides boxes coming in from the top */
    const rect_t place = {BAR_EDGE_PX, BAR_EDGE_PX, BAR_W, BAR_H};
    const box_style_t style = {COLOR_PANEL, COLOR_EDGE, BAR_RADIUS, 0};
    bento_status_t status = add_box(game, &conveyor->bar, place, style);

    bento_game_text_init(&title);
    bento_game_text_init(&conveyor->hud_state);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &title, "สายพานคัดแยกพัสดุ   SORTER", TITLE_X, TITLE_Y,
            BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &conveyor->hud_state,
                                        "IDLE  หยุดรอ", STATE_X, TITLE_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_label(game, &conveyor->warn, " ", WARN_X, WARN_Y,
                           BENTO_GAME_ORANGE);
    }
    return status;
}

static bento_status_t add_gate_and_chutes(conveyor_t *conveyor)
{
    static const char *const chute_texts[SIDES] = {"ช่อง A   0",
                                                   "ช่อง B   0"};
    bento_game_t *game = &conveyor->game;
    const rect_t gate_place = {GATE_X[0], GATE_Y, GATE_W, GATE_H};
    const box_style_t gate_style = {
        COLORS[0], (int32_t)BENTO_GAME_WHITE, GATE_RADIUS, 2};
    bento_status_t status =
        add_box(game, &conveyor->gate, gate_place, gate_style);
    int32_t side;

    for (side = 0; (status == BENTO_OK) && (side < SIDES); side++) {
        const rect_t place = {CHUTE_X[side], CHUTE_Y, CHUTE_W, CHUTE_H};
        const box_style_t style = {(side == 0) ? LIT[side] : DIM[side],
                                   (int32_t)COLORS[side], CHUTE_RADIUS,
                                   CHUTE_EDGE};

        status = add_box(game, &conveyor->chutes[side], place, style);
        if (status == BENTO_OK) {
            status = add_label(game, &conveyor->chute_texts[side],
                               chute_texts[side],
                               CHUTE_X[side] + CHUTE_TEXT_X,
                               CHUTE_TEXT_Y, BENTO_GAME_WHITE);
        }
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(game, &conveyor->boom, "boom1",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_hide(game, &conveyor->boom);
    }
    return status;
}

/* A value card: a dark panel with a coloured edge and a two-line label (name, then value). */
static bento_status_t add_cards(conveyor_t *conveyor)
{
    typedef struct {
        int32_t x;
        int32_t y;
        uint32_t accent;
        const char *title;
    } card_t;
    static const card_t card_list[CARDS] = {
        {CARD_LEFT_X, CARD_TOP_Y, BENTO_GAME_GREEN, "คัดถูก  OK"},
        {CARD_LEFT_X, CARD_LOW_Y, BENTO_GAME_RED, "คัดผิด  WRONG"},
        {CARD_RIGHT_X, CARD_TOP_Y, BENTO_GAME_ORANGE, "ผิดได้อีก  LEFT"},
        {CARD_RIGHT_X, CARD_LOW_Y, BENTO_GAME_CYAN, "สายพาน  BELT"}};
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < CARDS); i++) {
        const card_t *card = &card_list[i];
        const rect_t place = {card->x, card->y, CARD_W, CARD_H};
        const box_style_t style = {COLOR_PANEL, (int32_t)card->accent,
                                   CARD_RADIUS, 2};
        bento_game_box_t panel;

        status = add_box(&conveyor->game, &panel, place, style);
        if (status == BENTO_OK) {
            status = add_label(&conveyor->game, &conveyor->cards[i],
                               card->title, card->x + CARD_TEXT_X,
                               card->y + CARD_TEXT_Y, BENTO_GAME_WHITE);
        }
    }
    return status;
}

static bento_status_t build_screen(conveyor_t *conveyor)
{
    bento_status_t status = add_belt(conveyor);

    if (status == BENTO_OK) {
        status = add_status_bar(conveyor);
    }
    if (status == BENTO_OK) {
        status = add_gate_and_chutes(conveyor);
    }
    if (status == BENTO_OK) {
        status = add_cards(conveyor);
    }
    conveyor->speed = (bento_real_t)0;
    conveyor->gate_side = 0;
    conveyor->rejects_left = REJECTS;
    return status;
}

/* ---- one frame ---- */

/* Write the label only when its text changes. */
static bento_status_t show_label(bento_game_t *game, label_t *label,
                                 const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(label->shown, text) != 0) {
        status = bento_game_text_set(game, &label->text, text);
        (void)snprintf(label->shown, sizeof(label->shown), "%s", text);
    }
    return status;
}

static bento_status_t steer_gate(conveyor_t *conveyor,
                                 const bento_game_keys_t *keys,
                                 bool stopped)
{
    bento_game_t *game = &conveyor->game;
    int32_t side = conveyor->gate_side;
    bento_status_t status = BENTO_OK;

    if (keys->left) {
        side = 0;
    } else if (keys->right) {
        side = 1;
    } else {
        /* the gate stays where it points */
    }
    if ((side != conveyor->gate_side) && !stopped) {
        conveyor->gate_side = side;
        status = bento_game_box_move_to(game, &conveyor->gate,
                                        (bento_real_t)GATE_X[side],
                                        (bento_real_t)GATE_Y);
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(game, &conveyor->gate,
                                              COLORS[side]);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                game, &conveyor->chutes[side], LIT[side]);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                game, &conveyor->chutes[1 - side], DIM[1 - side]);
        }
        bento_game_sfx(game, "move");
    }
    return status;
}

/* Wrong chute: it flashes orange and the explosion plays there. */
static bento_status_t reject_box(conveyor_t *conveyor, int32_t side)
{
    bento_game_t *game = &conveyor->game;
    bento_status_t status;

    conveyor->wrong += 1;
    conveyor->rejects_left -= 1;
    status = bento_game_box_set_color(game, &conveyor->chutes[side],
                                      BENTO_GAME_ORANGE);
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(
            game, &conveyor->boom,
            (bento_real_t)((CHUTE_X[side] + CHUTE_W) - BOOM_LEFT),
            (bento_real_t)BOOM_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_show(game, &conveyor->boom);
    }
    conveyor->boom_frames = BOOM_STEP * BOOM_PICTURES;
    bento_game_sfx(game, "explode");
    return status;
}

/* The box reached the gate: into the chute, then back to the end of the line, new colour. */
static bento_status_t sort_box(conveyor_t *conveyor, int32_t slot)
{
    bento_game_t *game = &conveyor->game;
    const int32_t side = conveyor->gate_side;
    bento_game_box_t *box = &conveyor->boxes[slot];
    bento_real_t heights[BOXES];
    bento_status_t status;
    int32_t i;

    conveyor->count[side] += 1;
    if (judge(conveyor->box_color[slot], side)) {
        conveyor->sorted_ok += 1;
        /* the chute flashes its own colour: the right box */
        status = bento_game_box_set_color(game, &conveyor->chutes[side],
                                          COLORS[side]);
        bento_game_sfx(game, "point");
    } else {
        status = reject_box(conveyor, side);
    }
    conveyor->flash_frames = FLASH_FRAMES;
    if (status == BENTO_OK) {
        status = bento_random_randint(&conveyor->random, 0, 1,
                                      &conveyor->box_color[slot]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            game, box, COLORS[conveyor->box_color[slot]]);
    }
    for (i = 0; i < BOXES; i++) {
        heights[i] = conveyor->boxes[i].y;
    }
    box->y = next_y_above(heights, (uint32_t)BOXES, (bento_real_t)GAP);
    return status;
}

/* The slats and every box ride the one belt speed; a box at the gate is sorted. */
static bento_status_t move_belt(conveyor_t *conveyor)
{
    bento_game_t *game = &conveyor->game;
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SLATS); i++) {
        bento_game_box_t *slat = &conveyor->slats[i];
        const bento_real_t y = slat->y + conveyor->speed;
        const bool past_end =
            y > ((bento_real_t)BELT_END - (bento_real_t)SLAT_H);

        status = bento_game_box_move_to(
            game, slat, slat->x,
            past_end ? (y - (bento_real_t)SLAT_WRAP) : y);
    }
    for (i = 0; (status == BENTO_OK) && (i < BOXES); i++) {
        bento_game_box_t *box = &conveyor->boxes[i];

        /* moved in the variable first, sent to the screen once at the end */
        box->y += conveyor->speed;
        if (bento_game_hit(box, &conveyor->gate)) {
            status = sort_box(conveyor, i);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_move_to(game, box, box->x, box->y);
        }
    }
    return status;
}

/* The explosion changes picture every 3 frames, then hides with its first picture ready. */
static bento_status_t step_boom(conveyor_t *conveyor)
{
    static const char *const boom_names[BOOM_PICTURES] = {
        "boom1", "boom2", "boom3", "boom4", "boom5", "boom6"};
    bento_game_t *game = &conveyor->game;
    bento_status_t status = BENTO_OK;

    conveyor->boom_frames -= 1;
    if (conveyor->boom_frames == 0) {
        status = bento_game_sprite_hide(game, &conveyor->boom);
        if (status == BENTO_OK) {
            /* the first picture, ready for the next time */
            status = bento_game_sprite_frame(game, &conveyor->boom,
                                             boom_names[0]);
        }
    } else if ((conveyor->boom_frames % BOOM_STEP) == 0) {
        status = bento_game_sprite_frame(
            game, &conveyor->boom,
            boom_names[BOOM_PICTURES -
                       (conveyor->boom_frames / BOOM_STEP)]);
    } else {
        /* the same picture this frame */
    }
    return status;
}

/* Frame counters, no sleep: the explosion, then a chute's 6-frame flash. */
static bento_status_t animate(conveyor_t *conveyor)
{
    bento_status_t status = BENTO_OK;

    if (conveyor->boom_frames > 0) {
        status = step_boom(conveyor);
    }
    if ((status == BENTO_OK) && (conveyor->flash_frames > 0)) {
        conveyor->flash_frames -= 1;
        if (conveyor->flash_frames == 0) {
            status = bento_game_box_set_color(
                &conveyor->game, &conveyor->chutes[conveyor->gate_side],
                LIT[conveyor->gate_side]);
        }
    }
    return status;
}

static bento_status_t show_cards(conveyor_t *conveyor)
{
    static const char *const side_names[SIDES] = {"A", "B"};
    bento_game_t *game = &conveyor->game;
    const bento_real_t tenths = conveyor->speed * (bento_real_t)TENTHS;
    const int32_t whole = (int32_t)conveyor->speed;
    const int32_t tenth = (int32_t)tenths % TENTHS;
    const int32_t rejects_shown =
        (conveyor->rejects_left > 0) ? conveyor->rejects_left : 0;
    char text[LABEL_CHARS];
    bento_status_t status;
    int32_t side;

    (void)snprintf(text, sizeof(text), "คัดถูก  OK\n%d กล่อง",
                   (int)conveyor->sorted_ok);
    status = show_label(game, &conveyor->cards[CARD_OK], text);
    (void)snprintf(text, sizeof(text), "คัดผิด  WRONG\n%d กล่อง",
                   (int)conveyor->wrong);
    if (status == BENTO_OK) {
        status = show_label(game, &conveyor->cards[CARD_WRONG], text);
    }
    (void)snprintf(text, sizeof(text), "ผิดได้อีก  LEFT\n%d ครั้ง",
                   (int)rejects_shown);
    if (status == BENTO_OK) {
        status = show_label(game, &conveyor->cards[CARD_LEFT], text);
    }
    (void)snprintf(text, sizeof(text), "สายพาน  BELT\n%d.%d px ต่อเฟรม",
                   (int)whole, (int)tenth);
    if (status == BENTO_OK) {
        status = show_label(game, &conveyor->cards[CARD_BELT], text);
    }
    for (side = 0; (status == BENTO_OK) && (side < SIDES); side++) {
        (void)snprintf(text, sizeof(text), "ช่อง %s   %d",
                       side_names[side], (int)conveyor->count[side]);
        status = show_label(game, &conveyor->chute_texts[side], text);
    }
    return status;
}

/* The state machine: the top bar's colour tells the machine's state. */
static bento_status_t show_state(conveyor_t *conveyor)
{
    static const char *const state_texts[] = {
        "IDLE  หยุดรอ", "RUNNING  กำลังเดิน", "STOPPED  หยุดเครื่อง"};
    static const uint32_t state_colors[] = {COLOR_PANEL, COLOR_RUNNING,
                                            COLOR_STOPPED};
    machine_state_t state = MACHINE_IDLE;
    bento_status_t status = BENTO_OK;

    if (conveyor->rejects_left <= 0) {
        state = MACHINE_STOPPED;
    } else if (conveyor->speed > (bento_real_t)0) {
        state = MACHINE_RUNNING;
    } else {
        /* idle */
    }
    if (!conveyor->state_known || (state != conveyor->shown_state)) {
        status = bento_game_text_set(
            &conveyor->game, &conveyor->hud_state, state_texts[state]);
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                &conveyor->game, &conveyor->bar, state_colors[state]);
        }
        conveyor->shown_state = state;
        conveyor->state_known = true;
        if (state == MACHINE_STOPPED) {
            bento_game_sfx(&conveyor->game, "gameover");
        }
    }
    return status;
}

static bento_status_t show_warning(conveyor_t *conveyor)
{
    char text[LABEL_CHARS];

    if (conveyor->rejects_left <= 0) {
        (void)snprintf(text, sizeof(text), "หยุดสายพาน!\nคัดผิดครบ %d ครั้ง",
                       REJECTS);
    } else if (conveyor->rejects_left == 1) {
        (void)snprintf(text, sizeof(text), "%s",
                       "ระวัง! ผิดอีก 1 ครั้ง\nสายพานจะหยุด");
    } else {
        (void)snprintf(text, sizeof(text), "%s", " ");
    }
    return show_label(&conveyor->game, &conveyor->warn, text);
}

static bento_status_t update(void *user, bool *out_stop)
{
    /* the state is the file-scope g_conveyor: no void * conversion (MISRA 11.5) */
    conveyor_t *conveyor = &g_conveyor;
    const bento_real_t accel =
        (bento_real_t)0.20; /* กด A ค้าง 1 เฟรม สายพานเร็วขึ้นเท่านี้ */
    const bento_real_t friction =
        (bento_real_t)0.96; /* ปล่อย A แล้วความเร็วคูณเท่านี้ทุกเฟรม */
    const bento_real_t max_speed =
        (bento_real_t)6.0; /* เพดานความเร็วสายพาน (px ต่อเฟรม) */
    bento_game_keys_t keys;
    const bool stopped = conveyor->rejects_left <= 0;
    bento_status_t status;

    (void)user;
    bento_game_keys_init(&keys);
    /* never fails: a silent pad is all keys up */
    (void)bento_game_keys(&conveyor->game, &keys);
    conveyor->speed = stopped ? (bento_real_t)0
                              : belt_step(conveyor->speed, keys.a,
                                          accel, friction, max_speed);
    status = steer_gate(conveyor, &keys, stopped);
    if ((status == BENTO_OK) && (conveyor->speed > (bento_real_t)0)) {
        status = move_belt(conveyor);
    }
    if (status == BENTO_OK) {
        status = animate(conveyor);
    }
    if (status == BENTO_OK) {
        status = show_cards(conveyor);
    }
    if (status == BENTO_OK) {
        status = show_state(conveyor);
    }
    if (status == BENTO_OK) {
        status = show_warning(conveyor);
    }
    /* wait for the explosion to finish, then end the program */
    *out_stop = stopped && (conveyor->boom_frames == 0);
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
    conveyor_t *conveyor = &g_conveyor;
    bool quit = false;
    bento_status_t status;

    (void)memset(conveyor, 0, sizeof(*conveyor));
    status = bento_random_init(&conveyor->random);
    if ((status == BENTO_OK) && self_test()) {
        bento_game_init(&conveyor->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status = bento_game_title(&conveyor->game, "CONVEYOR SORT",
                                  NULL, &quit);
        report_joystick(&conveyor->game);
        if ((status == BENTO_OK) && !quit) {
            status = build_screen(conveyor);
        }
        if ((status == BENTO_OK) && !quit) {
            bool finished = false;

            status = bento_game_run(&conveyor->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
