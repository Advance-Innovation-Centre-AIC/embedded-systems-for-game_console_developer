/*
 * s13_tank_hmi.c -- คาบ 13 (คาบพิเศษ) โครงเว้นบางส่วน: หน้าจอควบคุมถังน้ำโรงเรือน = เทคนิคจาก Pong ในงานจริง
 *
 * โครงเว้นบางส่วน: เติม 2 ช่อง "เติมส่วนนี้เอง" ในส่วน 2) สมอง (next_level และ
 * valve_step) แล้วรัน ไฟล์ตรวจคำตอบเองก่อนเปิดจอ ผ่านครบ = Console ขึ้น "ผ่าน!"
 * แล้วเล่นได้เหมือนไฟล์เฉลย ยังไม่ถูก = Console บอกว่ากรณีไหนผิด แล้วหยุด
 * เล่น: กด Start · จอยขวา/ซ้าย = ดึงน้ำออกมากขึ้น/น้อยลง · A/B = เป้าหมาย
 * สูงขึ้น/ต่ำลง 5% ดูวาล์วไล่ระดับน้ำเข้าหาเส้นเหลือง น้ำต่ำ = เตือน LOW WATER
 *
 * The practise skeleton of the greenhouse tank screen. The student fills
 * in two functions of the "brain": next_level (level + inflow - outflow,
 * kept in 0..100) and valve_step (the three-step controller with a dead
 * band: +1 open, 0 hold, -1 close). The program checks the brain before
 * the screen opens: as shipped, the first case fails, the console says
 * which, and the program ends. Once the checks pass, it runs as the
 * solution does: the valve chases the yellow target, water runs in the
 * pipes, the cards show the values, and low water sounds the alarm.
 *
 * Twin of s13_tank_hmi.py (embedded-systems-for-game_console_developer/developer-i/practise_codes/s13_tank_hmi.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - next_level() and valve_step() are the shipped blanks (return level,
 *     return 0); their unused parameters are cast to void until filled;
 *   - the module globals and the shown dict are one tank_hmi_t at file
 *     scope (it is large); update() reads it there, not through void *;
 *   - each card keeps the text it shows in a buffer (Python's shown dict);
 *   - moving the target label (Python reaches into the Text's ui label) is
 *     bento_ui_set_pos on the label's id, its status checked;
 *   - C has one number type where Python's follows the argument: a failed
 *     next_level case prints a whole value as an int (the blank hands back
 *     its int 50) and any other as MicroPython's str() (bento_fmt);
 *   - raise SystemExit after a failed self test ends main() with 0.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The controller (change these; the real-valued ones are at the top of update()). */
/* The value cards, in the order they are made. */
enum {
    CARD_LEVEL = 0,
    CARD_TARGET,
    CARD_OUT,
    CARD_VALVE
};

enum {
    BAND = 3,          /* ช่องว่าง: ห่างเป้าไม่เกิน 3% = วาล์วหยุดนิ่ง */
    TARGET_START = 60, /* เป้าหมายระดับน้ำ (%) */
    TARGET_STEP = 5,
    TARGET_LOW = 10,
    TARGET_HIGH = 90,
    TARGET_MARGIN =
        2,          /* B keeps the target 2 % above the alarm's band */
    LOW_ALARM = 20, /* ต่ำกว่านี้ = เตือน LOW WATER */
    ALARM_EVERY = 20, /* เตือนดังทุก 20 เฟรม (ไม่ใช้ sleep ในวงวน) */
    FULL = 100,
    STEP_PERCENT = 10, /* one water segment = 10 % */
    PERCENT_DIGITS = 100,
    SLIDERS = 2,
    CARDS = 4,
    LEVEL_CASES = 3, /* the self test's next_level cases */
    VALVE_CASES = 4, /* its valve_step cases */
    BARS_CASES = 3,  /* its bars_on cases */
    FPS = 30
};

/* Where things are, in px. */
enum {
    TANK_X = 160,
    TANK_Y = 52,
    TANK_W = 196,
    TANK_H = 294,
    SEG_H = 28, /* น้ำหนึ่งขั้น 10% สูง 28 px (10 ขั้น = 280 px = 100%) */
    BOTTOM = TANK_Y + TANK_H - 8,
    PIPE_Y = 72,
    OUT_Y = 324,
    CARD_X = 548,
    CARD_W = 232,
    CARD_Y0 = 52,
    CARD_STEP = 76,
    CARD_H = 66,
    CARD_RADIUS = 8,
    CARD_TEXT_X = 16,
    CARD_TEXT_Y = 9,
    BAR_EDGE_PX = -4,
    BAR_W = 800,
    BAR_H = 44,
    BAR_RADIUS = 4,
    TITLE_X = 14,
    TITLE_Y = 9,
    STATE_X = 560,
    PIPE_IN_X = 20,
    PIPE_H = 14,
    PIPE_OUT_W = 170,
    PIPE_IN_SHORT =
        16, /* the inflow pipe ends 16 px short of the tank */
    PIPE_OUT_TUCK =
        6, /* the outflow pipe starts 6 px inside the tank's wall */
    FLOW_END_GAP = 18, /* the inflow dots wrap 18 px before the tank */
    FLOW_X0 = 26,
    FLOW_STEP = 58,
    FLOW_W = 14,
    DOT_H = 6,
    DOT_INSET = 4,
    VALVE_X = 62,
    VALVE_SIZE = 38,
    VALVE_RAISE = 12,
    VALVE_RADIUS = 19,
    VALVE_EDGE = 3,
    VALVE_TAG_X = 54,
    VALVE_TAG_Y = 34,
    DROP_GAP = 10,
    DROP_STEP = 71,
    DROP_W = 10,
    DROP_RUN = 152,
    TANK_RADIUS = 6,
    TANK_EDGE = 3,
    WATER_INSET = 6,
    BASE_OVERHANG = 14,
    BASE_RADIUS = 4,
    LINE_OVERHANG = 6,
    LINE_H = 3,
    TAG_GAP = 14,
    TAG_RAISE = 11,
    FLOW_SPEED_MAX =
        5, /* the inflow dots run 1 .. 6 px a frame with the valve */
    DROP_SPEED =
        12, /* the outflow dots run 12 px a frame per 1 % of outflow */
    LABEL_CHARS = 128 /* a SET_TEXT carries at most 126 bytes */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_PANEL = 0x1B2633,
    COLOR_EDGE = 0x3D5266,
    COLOR_GRAY = 0x556070, /* วาล์วหยุด */
    COLOR_PIPE = 0x4A5868,
    COLOR_TANK = 0x101A24,
    COLOR_TANK_RIM = 0x8899AA,
    COLOR_TANK_LOW = 0x3A1216,
    COLOR_BASE = 0x2A3644,
    COLOR_DEEP = 0x1E5FA8,  /* เนื้อน้ำเข้ม */
    COLOR_LIGHT = 0x5DB4F0, /* ผิวน้ำอ่อน */
    COLOR_ALARM = 0xC62828,
    COLOR_ALARM_DIM = 0x7A1C1C
};

/* The valve's look by its move, -1 (close), 0 (hold) and +1 (open), at index move + 1. */
typedef struct {
    uint32_t color;
    const char *text;
} valve_look_t;

static const valve_look_t VALVE_LOOK[3] = {
    {BENTO_GAME_ORANGE, "CLOSE-"},
    {COLOR_GRAY, "HOLD"},
    {BENTO_GAME_GREEN, "OPEN+"}};

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
    bento_game_box_t bar;
    bento_game_text_t hud_state;
    bento_game_box_t flow_in[SLIDERS];
    bento_game_box_t valve_symbol;
    bento_game_text_t valve_tag;
    bento_game_box_t drops[SLIDERS];
    bento_game_box_t tank;
    bento_game_box_t water;
    bento_game_box_t surface;
    bento_game_box_t target_line;
    bento_game_text_t target_tag;
    label_t cards[CARDS];
    bento_real_t level;
    int32_t target;
    bento_real_t outflow;
    bento_real_t valve;
    int32_t shown_bars;
    int32_t shown_move;
    bool move_known; /* false = no move shown yet (Python's None) */
    bool shown_low;
    bool shown_open;
    int32_t alarm_count;
} tank_hmi_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern tank_hmi_t g_tank;
tank_hmi_t g_tank;

/* ---- the brain: pure, no screen ---- */

static bento_real_t clamp(bento_real_t value, bento_real_t low,
                          bento_real_t high)
{
    const bento_real_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static int32_t clamp_whole(int32_t value, int32_t low, int32_t high)
{
    const int32_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

/* ----- เติมส่วนนี้เอง (งานของน้อง) (1): การ์ด 1 + การ์ด 4 -----
 * TODO: ระดับใหม่ = ระดับเดิม + น้ำเข้า - น้ำออก แล้วจำกัดให้อยู่ใน
 *       0..100 ด้วย clamp()
 *   ใบ้: ball_x += ball_vx ใน pong_step1 และ max(0, min(...)) ใน
 *        pong_step2 */
static bento_real_t next_level(bento_real_t level, bento_real_t inflow,
                               bento_real_t outflow)
{
    (void)inflow; /* the blank's inputs, unused until it is filled */
    (void)outflow;
    return level; /* <- แก้บรรทัดนี้ (ตอนนี้ระดับน้ำไม่เปลี่ยนเลย) */
}

/* ----- เติมส่วนนี้เอง (งานของน้อง) (2): การ์ด 5 ตัวควบคุมสามระดับ -----
 * TODO: น้ำน้อยกว่า target - band -> คืน 1 (เปิดเพิ่ม)
 *       น้ำมากกว่า target + band -> คืน -1 (ปิดลง) · นอกนั้นคืน 0
 *   ใบ้: ไม้ AI ใน pong_step5 (ai_y < target_y - 6 ... ai_y > target_y + 6) */
static int32_t valve_step(bento_real_t level, int32_t target,
                          int32_t band)
{
    (void)level; /* the blank's inputs, unused until it is filled */
    (void)target;
    (void)band;
    return 0; /* <- แก้บรรทัดนี้ (ตอนนี้วาล์วไม่ขยับเลย) */
}

/* Ten segments of 10 % each (0 .. 9.9 % lights none). */
static int32_t bars_on(bento_real_t level)
{
    return (int32_t)level / STEP_PERCENT;
}

static int32_t level_to_y(bento_real_t level)
{
    const int32_t step_h = (int32_t)STEP_PERCENT * (int32_t)SEG_H;
    const bento_real_t height = level * (bento_real_t)step_h;
    const bento_real_t risen = height / (bento_real_t)FULL;
    const bento_real_t y = (bento_real_t)BOTTOM - risen;

    return (int32_t)y;
}

/* ---- check the brain before the screen opens ---- */

/* A number as Python prints it here: an int when whole (the blank hands
 * back its int argument; the wanted 100 and 0 are ints), else str(). */
static void number_text(bento_real_t value, char *text,
                        uint32_t capacity)
{
    const int32_t whole = (int32_t)value;
    uint32_t length = 0U;

    if ((bento_real_t)whole == value) {
        (void)snprintf(text, capacity, "%d", (int)whole);
    } else if (bento_fmt_real(text, capacity, value, &length) !=
               BENTO_OK) {
        /* BENTO_FMT_BUF_SIZE always holds it: never taken */
        text[0] = '\0';
    } else {
        /* the float's text is in place */
    }
}

static bool check_real(const char *name, bento_real_t got,
                       bento_real_t want)
{
    const bento_real_t tolerance = (bento_real_t)1e-6;
    const bento_real_t difference =
        (got > want) ? (got - want) : (want - got);
    char got_text[BENTO_FMT_BUF_SIZE];
    char want_text[BENTO_FMT_BUF_SIZE];

    if (difference > tolerance) {
        number_text(got, got_text, (uint32_t)sizeof(got_text));
        number_text(want, want_text, (uint32_t)sizeof(want_text));
        (void)printf("ยังไม่ผ่าน: %s ได้ %s ควรได้ %s\n", name, got_text,
                     want_text);
    }
    return difference <= tolerance;
}

static bool check_whole(const char *name, int32_t got, int32_t want)
{
    if (got != want) {
        (void)printf("ยังไม่ผ่าน: %s ได้ %d ควรได้ %d\n", name, (int)got,
                     (int)want);
    }
    return got == want;
}

/* next_level's three cases. */
static bool check_level_cases(void)
{
    typedef struct {
        bento_real_t level;
        bento_real_t inflow;
        bento_real_t outflow;
        bento_real_t want;
    } level_case_t;
    static const level_case_t level_tests[LEVEL_CASES] = {
        {(bento_real_t)50, (bento_real_t)1.0, (bento_real_t)0.5,
         (bento_real_t)50.5},
        /* ล้นไม่ได้ แล้วติดลบไม่ได้ */
        {(bento_real_t)99.8, (bento_real_t)1.0, (bento_real_t)0.0,
         (bento_real_t)100},
        {(bento_real_t)0.2, (bento_real_t)0.0, (bento_real_t)1.0,
         (bento_real_t)0}};
    bool passed = true;
    uint32_t i;

    for (i = 0U; passed && (i < (uint32_t)LEVEL_CASES); i++) {
        const level_case_t *test = &level_tests[i];

        passed = check_real(
            "next_level",
            next_level(test->level, test->inflow, test->outflow),
            test->want);
    }
    return passed;
}

/* valve_step's four cases: every one aims at 60 % with a band of 3; 63
 * sits on the band's edge: still hold. */
static bool check_valve_cases(void)
{
    typedef struct {
        int32_t level;
        int32_t want;
    } valve_case_t;
    static const valve_case_t valve_tests[VALVE_CASES] = {
        {40, 1}, {58, 0}, {63, 0}, {70, -1}};
    bool passed = true;
    uint32_t i;

    for (i = 0U; passed && (i < (uint32_t)VALVE_CASES); i++) {
        passed =
            check_whole("valve_step",
                        valve_step((bento_real_t)valve_tests[i].level,
                                   TARGET_START, BAND),
                        valve_tests[i].want);
    }
    return passed;
}

/* bars_on's three cases. */
static bool check_bars_cases(void)
{
    typedef struct {
        bento_real_t level;
        int32_t want;
    } bars_case_t;
    static const bars_case_t bars_tests[BARS_CASES] = {
        {(bento_real_t)9.9, 0},
        {(bento_real_t)55, 5},
        {(bento_real_t)FULL, STEP_PERCENT}};
    bool passed = true;
    uint32_t i;

    for (i = 0U; passed && (i < (uint32_t)BARS_CASES); i++) {
        passed = check_whole("bars_on", bars_on(bars_tests[i].level),
                             bars_tests[i].want);
    }
    return passed;
}

/* ---- 3) ตรวจสมองก่อนเปิดจอ: the first failing case is printed ---- */
static bool self_test(void)
{
    bool passed = check_level_cases();

    if (passed) {
        passed = check_valve_cases();
    }
    if (passed) {
        passed = check_bars_cases();
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของถังน้ำถูกทั้ง %d กรณี\n",
                     LEVEL_CASES + VALVE_CASES + BARS_CASES);
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

static bento_status_t add_text(bento_game_t *game,
                               bento_game_text_t *text,
                               const char *utf8, int32_t x, int32_t y,
                               uint32_t color)
{
    bento_game_text_init(text);
    return bento_game_text_create(game, text, utf8, x, y, color);
}

static bento_status_t add_status_bar(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    bento_game_text_t title;
    /* the bar reaches past the screen's edges */
    const rect_t place = {BAR_EDGE_PX, BAR_EDGE_PX, BAR_W, BAR_H};
    const box_style_t style = {COLOR_PANEL, COLOR_EDGE, BAR_RADIUS, 0};
    bento_status_t status = add_box(game, &tank->bar, place, style);

    if (status == BENTO_OK) {
        status = add_text(game, &title, "ถังน้ำโรงเรือน   TANK HMI",
                          TITLE_X, TITLE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(game, &tank->hud_state, "ปกติ  NORMAL",
                          STATE_X, TITLE_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t add_inflow(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    bento_game_box_t pipe;
    const rect_t pipe_place = {PIPE_IN_X, PIPE_Y,
                               TANK_X - PIPE_IN_SHORT, PIPE_H};
    const box_style_t pipe_style = {COLOR_PIPE, COLOR_EDGE, 1, 0};
    const rect_t valve_place = {VALVE_X, PIPE_Y - VALVE_RAISE,
                                VALVE_SIZE, VALVE_SIZE};
    const box_style_t valve_style = {COLOR_GRAY,
                                     (int32_t)BENTO_GAME_WHITE,
                                     VALVE_RADIUS, VALVE_EDGE};
    bento_status_t status =
        add_box(game, &pipe, pipe_place, pipe_style);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SLIDERS); i++) {
        const rect_t place = {FLOW_X0 + (i * FLOW_STEP),
                              PIPE_Y + DOT_INSET, FLOW_W, DOT_H};
        const box_style_t style = {COLOR_LIGHT, BENTO_GAME_BORDER_NONE,
                                   1, 0};

        status = add_box(game, &tank->flow_in[i], place, style);
    }
    if (status == BENTO_OK) {
        status = add_box(game, &tank->valve_symbol, valve_place,
                         valve_style);
    }
    if (status == BENTO_OK) {
        status = add_text(game, &tank->valve_tag, "HOLD", VALVE_TAG_X,
                          PIPE_Y + VALVE_TAG_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t add_outflow(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    bento_game_box_t pipe;
    const rect_t pipe_place = {(TANK_X + TANK_W) - PIPE_OUT_TUCK, OUT_Y,
                               PIPE_OUT_W, PIPE_H};
    const box_style_t pipe_style = {COLOR_PIPE, COLOR_EDGE, 1, 0};
    bento_status_t status =
        add_box(game, &pipe, pipe_place, pipe_style);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SLIDERS); i++) {
        const rect_t place = {TANK_X + TANK_W + DROP_GAP +
                                  (i * DROP_STEP),
                              OUT_Y + DOT_INSET, DROP_W, DOT_H};
        const box_style_t style = {COLOR_LIGHT, BENTO_GAME_BORDER_NONE,
                                   1, 0};

        status = add_box(game, &tank->drops[i], place, style);
    }
    return status;
}

static bento_status_t add_tank(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    bento_game_box_t base;
    const rect_t tank_place = {TANK_X, TANK_Y, TANK_W, TANK_H};
    const box_style_t tank_style = {COLOR_TANK, COLOR_TANK_RIM,
                                    TANK_RADIUS, TANK_EDGE};
    const rect_t water_place = {TANK_X + WATER_INSET, BOTTOM,
                                TANK_W - (2 * WATER_INSET), SEG_H};
    const box_style_t deep_style = {COLOR_DEEP, BENTO_GAME_BORDER_NONE,
                                    1, 0};
    const box_style_t light_style = {COLOR_LIGHT,
                                     BENTO_GAME_BORDER_NONE, 1, 0};
    /* the base hides any surface below the tank's floor */
    const rect_t base_place = {TANK_X - BASE_OVERHANG, BOTTOM,
                               TANK_W + (2 * BASE_OVERHANG), SEG_H + 2};
    const box_style_t base_style = {COLOR_BASE, COLOR_EDGE, BASE_RADIUS,
                                    0};
    bento_status_t status =
        add_box(game, &tank->tank, tank_place, tank_style);

    if (status == BENTO_OK) {
        status = add_box(game, &tank->water, water_place, deep_style);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_hide(game, &tank->water);
    }
    if (status == BENTO_OK) {
        status =
            add_box(game, &tank->surface, water_place, light_style);
    }
    if (status == BENTO_OK) {
        status = add_box(game, &base, base_place, base_style);
    }
    return status;
}

static bento_status_t add_target(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    const int32_t line_y = level_to_y((bento_real_t)TARGET_START);
    const rect_t place = {TANK_X - LINE_OVERHANG, line_y,
                          TANK_W + (2 * LINE_OVERHANG), LINE_H};
    const box_style_t style = {BENTO_GAME_YELLOW,
                               BENTO_GAME_BORDER_NONE, 1, 0};
    char text[LABEL_CHARS];
    bento_status_t status =
        add_box(game, &tank->target_line, place, style);

    (void)snprintf(text, sizeof(text), "เป้า %d%%", TARGET_START);
    if (status == BENTO_OK) {
        status = add_text(game, &tank->target_tag, text,
                          TANK_X + TANK_W + TAG_GAP, line_y - TAG_RAISE,
                          BENTO_GAME_YELLOW);
    }
    return status;
}

/* A value card: a dark panel with a coloured edge and a two-line label (name, then value). */
static bento_status_t add_cards(tank_hmi_t *tank)
{
    typedef struct {
        uint32_t accent;
        const char *title;
    } card_t;
    static const card_t card_list[CARDS] = {
        {COLOR_LIGHT, "ระดับน้ำ  LEVEL"},
        {BENTO_GAME_YELLOW, "เป้าหมาย  TARGET"},
        {BENTO_GAME_CYAN, "น้ำออก  OUT"},
        {BENTO_GAME_GREEN, "วาล์ว  VALVE"}};
    bento_status_t status = BENTO_OK;
    int32_t row;

    for (row = 0; (status == BENTO_OK) && (row < CARDS); row++) {
        const int32_t y = CARD_Y0 + (row * CARD_STEP);
        const rect_t place = {CARD_X, y, CARD_W, CARD_H};
        const box_style_t style = {COLOR_PANEL,
                                   (int32_t)card_list[row].accent,
                                   CARD_RADIUS, 2};
        bento_game_box_t panel;

        status = add_box(&tank->game, &panel, place, style);
        tank->cards[row].shown[0] = '\0';
        if (status == BENTO_OK) {
            status =
                add_text(&tank->game, &tank->cards[row].text,
                         card_list[row].title, CARD_X + CARD_TEXT_X,
                         y + CARD_TEXT_Y, BENTO_GAME_WHITE);
        }
    }
    return status;
}

static bento_status_t build_screen(tank_hmi_t *tank)
{
    const bento_real_t level_start = (bento_real_t)50.0;
    /* น้ำออกตอนเริ่ม (% ต่อเฟรม) */
    const bento_real_t out_start = (bento_real_t)0.20;
    bento_status_t status = add_status_bar(tank);

    if (status == BENTO_OK) {
        status = add_inflow(tank);
    }
    if (status == BENTO_OK) {
        status = add_outflow(tank);
    }
    if (status == BENTO_OK) {
        status = add_tank(tank);
    }
    if (status == BENTO_OK) {
        status = add_target(tank);
    }
    if (status == BENTO_OK) {
        status = add_cards(tank);
    }
    tank->level = level_start;
    tank->target = TARGET_START;
    tank->outflow = out_start;
    tank->valve = (bento_real_t)0.0;
    tank->shown_bars = 0;
    tank->shown_open = true;
    return status;
}

/* ---- one frame ---- */

/* Write the card only when its text changes (unchanged = nothing sent to the screen). */
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

/* Small boxes slide right and wrap to the pipe's start; sent only when the pixel changes. */
static bento_status_t slide(bento_game_t *game,
                            bento_game_box_t items[SLIDERS],
                            bento_real_t step, int32_t start,
                            int32_t end)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < SLIDERS); i++) {
        bento_game_box_t *item = &items[i];
        bento_real_t x = item->x + step;

        if (x > (bento_real_t)end) {
            const int32_t span = end - start;

            x -= (bento_real_t)span;
        }
        if ((int32_t)x != (int32_t)item->x) {
            status = bento_game_box_move_to(game, item, x, item->y);
        } else {
            item->x = x;
        }
    }
    return status;
}

/* The water body changes only when the level crosses a 10 % step (no resize every frame). */
/* The water body: hidden at 0 segments, else moved, resized and shown. */
static bento_status_t show_body(tank_hmi_t *tank, int32_t bars)
{
    bento_game_t *game = &tank->game;
    bento_status_t status;

    if (bars == 0) {
        status = bento_game_box_hide(game, &tank->water);
    } else {
        status = bento_game_box_move_to(
            game, &tank->water, tank->water.x,
            (bento_real_t)(BOTTOM - (bars * SEG_H)));
        if (status == BENTO_OK) {
            status =
                bento_game_box_resize(game, &tank->water, tank->water.w,
                                      (bento_real_t)(bars * SEG_H));
        }
        if (status == BENTO_OK) {
            status = bento_game_box_show(game, &tank->water);
        }
    }
    return status;
}

static bento_status_t show_water(tank_hmi_t *tank)
{
    const int32_t bars = bars_on(tank->level);
    const int32_t surface_y = level_to_y(tank->level);
    bento_status_t status = BENTO_OK;

    if (bars != tank->shown_bars) {
        status = show_body(tank, bars);
        tank->shown_bars = bars;
    }
    /* the surface follows the real level, 1 px at a time */
    if ((status == BENTO_OK) &&
        ((bento_real_t)surface_y != tank->surface.y)) {
        status = bento_game_box_move_to(&tank->game, &tank->surface,
                                        tank->surface.x,
                                        (bento_real_t)surface_y);
    }
    return status;
}

/* The target line and its label move only when A or B changes the target. */
static bento_status_t show_target(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    const int32_t line_y = level_to_y((bento_real_t)tank->target);
    char text[LABEL_CHARS];
    bento_status_t status = BENTO_OK;

    if ((bento_real_t)line_y != tank->target_line.y) {
        status = bento_game_box_move_to(game, &tank->target_line,
                                        tank->target_line.x,
                                        (bento_real_t)line_y);
        (void)snprintf(text, sizeof(text), "เป้า %d%%",
                       (int)tank->target);
        if (status == BENTO_OK) {
            status = bento_game_text_set(game, &tank->target_tag, text);
        }
        /* game.Text has no move_to: move the label inside it directly */
        if (status == BENTO_OK) {
            const int32_t tag_x =
                (int32_t)TANK_X + (int32_t)TANK_W + (int32_t)TAG_GAP;

            status = bento_ui_set_pos(game->ui, tank->target_tag.id,
                                      (int16_t)tag_x,
                                      (int16_t)(line_y - TAG_RAISE));
        }
    }
    return status;
}

/* Water runs in the pipes: inflow only while the valve is open (faster the wider), outflow
 * as fast as OUT. */
static bento_status_t show_pipes(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    const bool opened = tank->valve > (bento_real_t)0;
    const bento_real_t drop_step =
        tank->outflow * (bento_real_t)DROP_SPEED;
    bento_status_t status = BENTO_OK;
    int32_t i;

    if (opened != tank->shown_open) {
        for (i = 0; (status == BENTO_OK) && (i < SLIDERS); i++) {
            status = opened
                         ? bento_game_box_show(game, &tank->flow_in[i])
                         : bento_game_box_hide(game, &tank->flow_in[i]);
        }
        tank->shown_open = opened;
    }
    if ((status == BENTO_OK) && opened) {
        const bento_real_t wide =
            tank->valve * (bento_real_t)FLOW_SPEED_MAX;

        status = slide(game, tank->flow_in, (bento_real_t)1 + wide,
                       FLOW_X0, TANK_X - FLOW_END_GAP);
    }
    if (status == BENTO_OK) {
        status = slide(game, tank->drops, drop_step,
                       TANK_X + TANK_W + DROP_GAP,
                       TANK_X + TANK_W + DROP_RUN);
    }
    return status;
}

static bento_status_t show_valve(tank_hmi_t *tank, int32_t move)
{
    const valve_look_t *look = &VALVE_LOOK[move + 1];
    bento_status_t status = BENTO_OK;

    if (!tank->move_known || (move != tank->shown_move)) {
        status = bento_game_box_set_color(
            &tank->game, &tank->valve_symbol, look->color);
        if (status == BENTO_OK) {
            status = bento_game_text_set(&tank->game, &tank->valve_tag,
                                         look->text);
        }
        tank->shown_move = move;
        tank->move_known = true;
    }
    return status;
}

static bento_status_t show_cards(tank_hmi_t *tank, int32_t move)
{
    bento_game_t *game = &tank->game;
    const bento_real_t hundredths =
        tank->outflow * (bento_real_t)PERCENT_DIGITS;
    const bento_real_t half = (bento_real_t)0.5;
    const bento_real_t rounded = hundredths + half;
    const bento_real_t valve_percent = tank->valve * (bento_real_t)FULL;
    char text[LABEL_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "ระดับน้ำ  LEVEL\n%d %%",
                   (int)(int32_t)tank->level);
    status = show_label(game, &tank->cards[CARD_LEVEL], text);
    (void)snprintf(text, sizeof(text), "เป้าหมาย  TARGET\n%d %%",
                   (int)tank->target);
    if (status == BENTO_OK) {
        status = show_label(game, &tank->cards[CARD_TARGET], text);
    }
    (void)snprintf(text, sizeof(text), "น้ำออก  OUT\n%d.%02d %% ต่อเฟรม",
                   (int)(int32_t)tank->outflow,
                   (int)((int32_t)rounded % PERCENT_DIGITS));
    if (status == BENTO_OK) {
        status = show_label(game, &tank->cards[CARD_OUT], text);
    }
    (void)snprintf(text, sizeof(text), "วาล์ว  VALVE\n%d %%   %s",
                   (int)(int32_t)valve_percent,
                   VALVE_LOOK[move + 1].text);
    if (status == BENTO_OK) {
        status = show_label(game, &tank->cards[CARD_VALVE], text);
    }
    return status;
}

/* While the water is low: red with a beep, then dim, every ALARM_EVERY. */
static bento_status_t blink_alarm(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    bento_status_t status = BENTO_OK;

    if (tank->alarm_count == 0) {
        status =
            bento_game_box_set_color(game, &tank->bar, COLOR_ALARM);
        bento_game_sfx(game, "deny");
    } else if (tank->alarm_count == (ALARM_EVERY / 2)) {
        status =
            bento_game_box_set_color(game, &tank->bar, COLOR_ALARM_DIM);
    } else {
        /* the bar keeps its colour this frame */
    }
    tank->alarm_count = (tank->alarm_count + 1) % ALARM_EVERY;
    return status;
}

/* Low water: the top bar becomes a red warning that blinks with a beep every ALARM_EVERY. */
static bento_status_t show_alarm(tank_hmi_t *tank)
{
    bento_game_t *game = &tank->game;
    const bool low = tank->level < (bento_real_t)LOW_ALARM;
    bento_status_t status = BENTO_OK;

    if (low != tank->shown_low) {
        status = bento_game_text_set(game, &tank->hud_state,
                                     low ? "น้ำต่ำ  LOW WATER"
                                         : "ปกติ  NORMAL");
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                game, &tank->tank, low ? COLOR_TANK_LOW : COLOR_TANK);
        }
        if ((status == BENTO_OK) && !low) {
            status =
                bento_game_box_set_color(game, &tank->bar, COLOR_PANEL);
        }
        tank->shown_low = low;
        tank->alarm_count = 0;
    }
    if ((status == BENTO_OK) && low) {
        status = blink_alarm(tank);
    }
    return status;
}

/* Read the keys: right and left change the outflow, A and B move the target. */
static void read_keys(tank_hmi_t *tank, const bento_game_keys_t *keys)
{
    /* ดึงน้ำออกได้สูงสุด มากกว่าที่วาล์วเติมได้ น้ำจึงลดได้แม้วาล์วเปิดสุด */
    const bento_real_t out_max = (bento_real_t)0.80;
    /* กดจอยค้าง 1 เฟรม น้ำออกเปลี่ยนเท่านี้ */
    const bento_real_t out_step = (bento_real_t)0.01;

    if (keys->right) {
        tank->outflow =
            clamp(tank->outflow + out_step, (bento_real_t)0, out_max);
    }
    if (keys->left) {
        tank->outflow =
            clamp(tank->outflow - out_step, (bento_real_t)0, out_max);
    }
    if (bento_game_pressed_once(&tank->game, BENTO_GAME_KEY_A, keys)) {
        tank->target = clamp_whole(tank->target + TARGET_STEP,
                                   TARGET_LOW, TARGET_HIGH);
    }
    if (bento_game_pressed_once(&tank->game, BENTO_GAME_KEY_B, keys)) {
        tank->target =
            clamp_whole(tank->target - TARGET_STEP,
                        LOW_ALARM + BAND + TARGET_MARGIN, TARGET_HIGH);
    }
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state is the file-scope g_tank: no void * conversion (MISRA 11.5) */
    tank_hmi_t *tank = &g_tank;
    /* วาล์วเปิดสุด เติมน้ำได้กี่ % ต่อเฟรม */
    const bento_real_t in_max = (bento_real_t)0.6;
    /* วาล์วมอเตอร์ขยับได้เฟรมละ 2% */
    const bento_real_t valve_rate = (bento_real_t)0.02;
    bento_game_keys_t keys;
    bento_real_t opening;
    bento_status_t status;
    int32_t move;

    (void)user;
    /* on_frame always returns True: run until Back */
    *out_stop = false;
    bento_game_keys_init(&keys);
    /* keys() never fails: a silent pad is all keys up */
    (void)bento_game_keys(&tank->game, &keys);
    read_keys(tank, &keys);
    move = valve_step(tank->level, tank->target, BAND);
    opening = (bento_real_t)move * valve_rate;
    tank->valve =
        clamp(tank->valve + opening, (bento_real_t)0, (bento_real_t)1);
    tank->level =
        next_level(tank->level, tank->valve * in_max, tank->outflow);
    status = show_water(tank);
    if (status == BENTO_OK) {
        status = show_target(tank);
    }
    if (status == BENTO_OK) {
        status = show_pipes(tank);
    }
    if (status == BENTO_OK) {
        status = show_valve(tank, move);
    }
    if (status == BENTO_OK) {
        status = show_cards(tank, move);
    }
    if (status == BENTO_OK) {
        status = show_alarm(tank);
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
    tank_hmi_t *tank = &g_tank;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(tank, 0, sizeof(*tank));
    if (self_test()) {
        bento_game_init(&tank->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status = bento_game_title(&tank->game, "TANK HMI", NULL, &quit);
        report_joystick(&tank->game);
        if ((status == BENTO_OK) && !quit) {
            status = build_screen(tank);
        }
        if ((status == BENTO_OK) && !quit) {
            bool finished = false;

            status = bento_game_run(&tank->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
