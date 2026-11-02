/*
 * s13_02_integrate.c -- คาบ 13 การ์ด 1: สะสมค่าทีละเฟรม ลูกบอลกับถังน้ำใช้สมการเดียวกัน
 *
 * ค่าใหม่ = ค่าเดิม + ที่เปลี่ยนต่อเฟรม: ลูกบอล x = x + v และถังน้ำ
 * ระดับ = ระดับ + น้ำเข้า - น้ำออก ถังมีขอบ 0 ถึง 100 ลูกวิ่งทะลุขวาแล้วโผล่ซ้าย
 * กด Start · จอยซ้าย/ขวา = ลด/เพิ่ม v ทีละ 1 · จอยขึ้น/ลง = เพิ่ม/ลดน้ำเข้าทีละ 0.10
 * A = เริ่มใหม่ · ขีดเหลืองใต้ลูก = ที่อยู่ของลูกทุก 6 เฟรม
 *
 * One equation, new = old + change per frame, drives two things: a ball
 * (x = x + v, wrapping from the right edge to the left) and a water tank
 * (level = level + in - out, kept between 0 and 100). Left and right
 * change v by 1, up and down change the inflow by 0.10, A starts again.
 * Two white lines show the sums being done every frame; a yellow tick
 * marks the ball every 6 frames, and ten blocks show the tank's level.
 *
 * Twin of s13_02_integrate.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s13_02_integrate.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - '%.1f' and '%.2f' are bento_fmt_real_precision, MicroPython's own
 *     float text on each board; round(v, 2) is nearbyint(v * 100) / 100 as
 *     modbuiltins.c does, written out for the non-negative inflow;
 *   - signed() writes into a buffer the caller gives (Python returns a str);
 *   - the shown dict is fields of one state struct, the texts in buffers;
 *   - on_frame() reads the file-scope state, not its void * argument;
 *   - frame is an int32_t: it wraps after 2^31 frames (about 2 years);
 *   - a bentogame call that raises in MicroPython returns its status here;
 *     title()'s Back (SystemExit) and the IDE's Stop end the program with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* ---- 1) ตั้งค่า (แก้ได้) ---- */
enum {
    V_START = 4, /* ความเร็วลูกตอนเริ่ม (พิกเซลต่อเฟรม) */
    V_MAX = 12,
    TRAIL_EVERY = 6, /* วางขีดเหลืองทุกกี่เฟรม */
    TICKS = 4,
    SEGMENTS = 10, /* ก้อนที่ 0 อยู่ก้นถัง */
    PERCENT_PER_SEGMENT = 10,
    FPS = 30
};

/* Where things are, in px. */
enum {
    LANE_X = 16,
    LANE_Y = 100,
    LANE_W = 370,
    LANE_H = 100,
    BALL = 14, /* ภาพ ball กว้าง/สูง 14 px */
    LANE_MARGIN = 4,
    X_LO = LANE_X + LANE_MARGIN,
    X_HI = LANE_X + LANE_W - BALL - LANE_MARGIN,
    BALL_DY = 24,
    TICK_X = -20,
    TICK_DY = 50,
    TICK_W = 3,
    TICK_H = 26,
    TICK_DX = 5,
    TANK_X = 470,
    TANK_Y = 100,
    TANK_W = 120,
    TANK_H = 262,
    TANK_BORDER_PX = 2,
    SEG_H = 24, /* แถบน้ำหนึ่งก้อน (10 ก้อน = 240 px = 100%) */
    SEG_INSET = 10,
    SEG_GAP = 4,
    TEXT_X = 16,
    TANK_TEXT_X = 420,
    TITLE_Y = 4,
    EQUATION_Y = 36,
    VALUE_Y = 64,
    HINT_Y = 210,
    KEYS_Y = 300,
    TEXT_BYTES =
        64, /* the longest text: three numbers and Thai words */
    NUMBER_BYTES = 40
};

/* Colours, 0xRRGGBB. */
enum {
    LANE_COLOR = 0x16222E,
    LANE_EDGE = 0x334455,
    TANK_COLOR = 0x1E2A36,
    TANK_EDGE = 0x8899AA
};

/* น้ำออกคงที่ (% ต่อเฟรม) */
static const bento_real_t OUT = (bento_real_t)0.30;
static const bento_real_t FULL = (bento_real_t)100.0;

typedef struct {
    bento_game_t game;
    bento_game_text_t ball_eq;
    bento_game_text_t tank_eq;
    bento_game_box_t ticks[TICKS];
    bento_game_sprite_t ball;
    bento_game_box_t segments[SEGMENTS];
    int32_t x;
    int32_t v;
    bento_real_t level;
    bento_real_t inflow;
    int32_t frame;
    int32_t next_tick;
    int32_t shown_bars;
    bool shown_limit;
    char shown_ball[TEXT_BYTES];
    char shown_tank[TEXT_BYTES];
} integrate_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern integrate_t g_integrate;
integrate_t g_integrate;

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

/* title() calls start(), whose joystick.init() prints this line on the
 * kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* ---- 2) สมอง: คิดล้วน ๆ ไม่แตะจอ ---- */
/* การ์ด 1: หัวใจของไฟล์นี้ — ลูกบอลและถังน้ำเรียกสมการนี้ตัวเดียวกัน */
static int32_t integrate_px(int32_t value, int32_t change)
{
    return value + change;
}

static bento_real_t integrate_level(bento_real_t value,
                                    bento_real_t change)
{
    return value + change;
}

/* ลูกทะลุขวา -> โผล่ซ้าย และกลับกัน (ไม่ใช่สะท้อน สะท้อนอยู่การ์ด 2) */
static int32_t wrap(int32_t x)
{
    const int32_t span = X_HI - X_LO;
    int32_t wrapped = x;

    if (x > X_HI) {
        wrapped = x - span;
    } else if (x < X_LO) {
        wrapped = x + span;
    } else {
        /* inside the lane */
    }
    return wrapped;
}

/* round(value, 2) for value >= 0: nearbyint (ties to even) of value * 100 */
static bento_real_t round_hundredths(bento_real_t value)
{
    const bento_real_t hundred = (bento_real_t)100;
    const bento_real_t half = (bento_real_t)0.5;
    const bento_real_t scaled = value * hundred;
    int32_t whole = (int32_t)scaled;
    const bento_real_t fraction = scaled - (bento_real_t)whole;

    if ((fraction > half) ||
        ((fraction == half) && ((whole % 2) != 0))) {
        whole += 1;
    }
    return (bento_real_t)whole / hundred;
}

/* (4) -> "+ 4" ให้สมการบนจอเหมือนที่เขียนบนกระดาน */
static void signed_int(int32_t value, char *text, uint32_t capacity)
{
    (void)snprintf(text, capacity, "%s%" PRId32,
                   (value < 0) ? "- " : "+ ",
                   (value < 0) ? -value : value);
}

/* (-0.3) -> "- 0.30" */
static void signed_real(bento_real_t value, char *text,
                        uint32_t capacity)
{
    char digits[BENTO_FMT_BUF_SIZE];
    uint32_t length = 0U;
    const bento_real_t size =
        (value < (bento_real_t)0) ? -value : value;

    if (bento_fmt_real_precision(digits, (uint32_t)sizeof(digits), size,
                                 2, &length) != BENTO_OK) {
        digits[0] = '\0'; /* the buffer always fits: never taken */
    }
    (void)snprintf(text, capacity, "%s%s",
                   (value < (bento_real_t)0) ? "- " : "+ ", digits);
}

/* '%.1f' % value */
static void tenths(bento_real_t value, char *text, uint32_t capacity)
{
    uint32_t length = 0U;

    if (bento_fmt_real_precision(text, capacity, value, 1, &length) !=
        BENTO_OK) {
        text[0] = '\0'; /* the buffer always fits: never taken */
    }
}

/* ป้ายเปลี่ยนเมื่อข้อความเปลี่ยนเท่านั้น */
static bento_status_t show(const bento_game_text_t *widget, char *shown,
                           const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(text, shown) != 0) {
        status = bento_game_text_set(&g_integrate.game, widget, text);
        (void)snprintf(shown, TEXT_BYTES, "%s", text);
    }
    return status;
}

/* ---- 4) หน้าจอ: สร้างครั้งเดียว ---- */
static bento_status_t add_text(const char *text, int32_t x, int32_t y,
                               uint32_t color)
{
    bento_game_text_t label;

    bento_game_text_init(&label);
    return bento_game_text_create(&g_integrate.game, &label, text, x, y,
                                  color);
}

/* The headline and the two equations with their live lines. */
static bento_status_t add_equations(integrate_t *state)
{
    bento_status_t status =
        add_text("สมการเดียว: ใหม่ = เดิม + ที่เปลี่ยน", TEXT_X, TITLE_Y,
                 BENTO_GAME_YELLOW);

    if (status == BENTO_OK) {
        status = add_text("ลูกบอล    x = x + v", TEXT_X, EQUATION_Y,
                          BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(&state->game, &state->ball_eq, "",
                                   TEXT_X, VALUE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text("ถังน้ำ    น้ำ = น้ำ + เข้า - ออก", TANK_TEXT_X,
                          EQUATION_Y, BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(&state->game, &state->tank_eq,
                                        "", TANK_TEXT_X, VALUE_Y,
                                        BENTO_GAME_WHITE);
    }
    return status;
}

/* The lane, its four ticks, the ball and the tick caption. */
static bento_status_t add_lane(integrate_t *state)
{
    char text[TEXT_BYTES];
    const int32_t tick_y = LANE_Y + TICK_DY;
    const int32_t ball_y = LANE_Y + BALL_DY;
    bento_game_box_t lane;
    bento_status_t status;
    uint32_t i;

    bento_game_box_init(&lane);
    status = bento_game_box_create_styled(
        &state->game, &lane, (bento_real_t)LANE_X, (bento_real_t)LANE_Y,
        (bento_real_t)LANE_W, (bento_real_t)LANE_H, LANE_COLOR,
        LANE_EDGE, 1, 1);
    for (i = 0U; (status == BENTO_OK) && (i < (uint32_t)TICKS); i++) {
        status = bento_game_box_create(
            &state->game, &state->ticks[i], (bento_real_t)TICK_X,
            (bento_real_t)tick_y, (bento_real_t)TICK_W,
            (bento_real_t)TICK_H, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(&state->game, &state->ball,
                                          "ball", (bento_real_t)X_LO,
                                          (bento_real_t)ball_y);
    }
    (void)snprintf(text, sizeof(text), "ขีดเหลือง = ลูกอยู่ตรงนี้ทุก %d เฟรม",
                   TRAIL_EVERY);
    if (status == BENTO_OK) {
        status = add_text(text, TEXT_X, HINT_Y, BENTO_GAME_YELLOW);
    }
    return status;
}

/* The tank, its ten hidden blocks and the key caption. */
static bento_status_t add_tank(integrate_t *state)
{
    const int32_t seg_x = TANK_X + SEG_INSET;
    const int32_t seg_h = SEG_H - SEG_GAP;
    bento_game_box_t tank;
    bento_status_t status;
    int32_t i;

    bento_game_box_init(&tank);
    status = bento_game_box_create_styled(
        &state->game, &tank, (bento_real_t)TANK_X, (bento_real_t)TANK_Y,
        (bento_real_t)TANK_W, (bento_real_t)TANK_H, TANK_COLOR,
        TANK_EDGE, 1, TANK_BORDER_PX);
    for (i = 0; (status == BENTO_OK) && (i < SEGMENTS); i++) {
        const int32_t top = TANK_Y + TANK_H - SEG_INSET -
                            ((i + 1) * SEG_H) + (SEG_GAP / 2);

        status = bento_game_box_create(
            &state->game, &state->segments[i], (bento_real_t)seg_x,
            (bento_real_t)top, (bento_real_t)(TANK_W - (2 * SEG_INSET)),
            (bento_real_t)seg_h, BENTO_GAME_BLUE);
        if (status == BENTO_OK) {
            status =
                bento_game_box_hide(&state->game, &state->segments[i]);
        }
    }
    if (status == BENTO_OK) {
        status = add_text("ซ้ายขวา = v  ขึ้นลง = น้ำเข้า  A = ใหม่", TEXT_X,
                          KEYS_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* x, v, level and inflow as they start (and as A starts them again). */
static void restart(integrate_t *state)
{
    /* น้ำเข้าตอนเริ่ม (% ต่อเฟรม) · ระดับน้ำตอนเริ่ม (%) */
    static const bento_real_t in_start = (bento_real_t)0.40;
    static const bento_real_t level_start = (bento_real_t)30.0;

    state->x = X_LO;
    state->v = V_START;
    state->level = level_start;
    state->inflow = in_start;
}

/* เข้า: ปุ่มเปลี่ยน "ที่เปลี่ยนต่อเฟรม" ไม่ได้ย้ายลูกหรือเติมน้ำตรง ๆ */
static void read_keys(integrate_t *state, const bento_game_keys_t *keys)
{
    static const bento_real_t in_max =
        (bento_real_t)1.00; /* น้ำเข้าสูงสุด */
    const bento_real_t step = (bento_real_t)0.10;
    bento_game_t *game = &state->game;

    if (bento_game_pressed_once(game, BENTO_GAME_KEY_RIGHT, keys)) {
        state->v = ((state->v + 1) < V_MAX) ? (state->v + 1) : V_MAX;
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_LEFT, keys)) {
        state->v = ((state->v - 1) > -V_MAX) ? (state->v - 1) : -V_MAX;
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_UP, keys)) {
        const bento_real_t more = state->inflow + step;

        state->inflow =
            round_hundredths((more < in_max) ? more : in_max);
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_DOWN, keys)) {
        const bento_real_t less = state->inflow - step;

        state->inflow = round_hundredths(
            (less > (bento_real_t)0) ? less : (bento_real_t)0);
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_A, keys)) {
        restart(state);
        bento_game_sfx(game, "select");
    }
}

/* ออก: the ball, and every TRAIL_EVERY frames the oldest tick moved to it. */
static bento_status_t draw_ball(integrate_t *state)
{
    bento_status_t status = bento_game_sprite_move_to(
        &state->game, &state->ball, (bento_real_t)state->x,
        state->ball.y);

    if ((status == BENTO_OK) && ((state->frame % TRAIL_EVERY) == 0)) {
        bento_game_box_t *tick = &state->ticks[state->next_tick];

        /* ยืมขีดเก่าสุดมาวางใหม่ (วนรอบ 4 ขีด) */
        status = bento_game_box_move_to(
            &state->game, tick, (bento_real_t)(state->x + TICK_DX),
            tick->y);
        state->next_tick = (state->next_tick + 1) % TICKS;
    }
    return status;
}

/* ออก: the two live equations. */
static bento_status_t show_equations(integrate_t *state, int32_t old_x,
                                     bento_real_t old_level)
{
    char change[NUMBER_BYTES];
    char out_text[NUMBER_BYTES];
    char before[NUMBER_BYTES];
    char after[NUMBER_BYTES];
    char text[TEXT_BYTES];
    const char *note = "";
    bento_status_t status;

    signed_int(state->v, change, (uint32_t)sizeof(change));
    (void)snprintf(text, sizeof(text), "%" PRId32 " %s = %" PRId32,
                   old_x, change, old_x + state->v);
    status = show(&state->ball_eq, state->shown_ball, text);
    if (state->level >= FULL) {
        note = "  เต็ม";
    } else if (state->level <= (bento_real_t)0) {
        note = "  แห้ง";
    } else {
        /* neither full nor dry */
    }
    tenths(old_level, before, (uint32_t)sizeof(before));
    tenths(state->level, after, (uint32_t)sizeof(after));
    signed_real(state->inflow, change, (uint32_t)sizeof(change));
    signed_real(-OUT, out_text, (uint32_t)sizeof(out_text));
    (void)snprintf(text, sizeof(text), "%s %s %s = %s%s", before,
                   change, out_text, after, note);
    if (status == BENTO_OK) {
        status = show(&state->tank_eq, state->shown_tank, text);
    }
    return status;
}

/* ออก: ถังมี 10 ก้อน · แตะเฉพาะก้อนที่เปลี่ยน */
static bento_status_t show_bars(integrate_t *state)
{
    int32_t bars = (int32_t)state->level / PERCENT_PER_SEGMENT;
    bento_status_t status = BENTO_OK;

    bars = (bars < SEGMENTS) ? bars : SEGMENTS;
    bars = (bars > 0) ? bars : 0;
    while ((status == BENTO_OK) && (state->shown_bars < bars)) {
        status = bento_game_box_show(
            &state->game, &state->segments[state->shown_bars]);
        state->shown_bars += 1;
    }
    while ((status == BENTO_OK) && (state->shown_bars > bars)) {
        state->shown_bars -= 1;
        status = bento_game_box_hide(
            &state->game, &state->segments[state->shown_bars]);
    }
    return status;
}

/* ---- 5) วงวนหลัก ---- */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    integrate_t *state = &g_integrate;
    bento_game_keys_t keys;
    int32_t old_x;
    bento_real_t old_level;
    bool at_limit;
    bento_status_t status;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    *out_stop = false; /* the program runs until Back */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&state->game, &keys);
    read_keys(state, &keys);
    /* คิด: สมการเดียวกันสองที่ */
    old_x = state->x;
    old_level = state->level;
    state->x = wrap(integrate_px(state->x, state->v)); /* ลูกบอล */
    state->level = integrate_level(state->level, state->inflow - OUT);
    /* การ์ด 4: ถังมีขอบเสมอ */
    state->level = (state->level < FULL) ? state->level : FULL;
    state->level = (state->level > (bento_real_t)0) ? state->level
                                                    : (bento_real_t)0;
    at_limit =
        (state->level >= FULL) || (state->level <= (bento_real_t)0);
    state->frame += 1;
    /* ออก: วาด + ป้าย เฉพาะที่เปลี่ยน */
    if (status == BENTO_OK) {
        status = draw_ball(state);
    }
    if (status == BENTO_OK) {
        status = show_equations(state, old_x, old_level);
    }
    if (status == BENTO_OK) {
        status = show_bars(state);
    }
    if ((status == BENTO_OK) && at_limit && !state->shown_limit) {
        /* ถังเต็มหรือแห้ง เตือนครั้งเดียว */
        bento_game_sfx(&state->game, "deny");
    }
    state->shown_limit = at_limit;
    return status;
}

/* The objects the program fills in: none is made yet. */
static void init_objects(integrate_t *state)
{
    uint32_t i;

    bento_game_text_init(&state->ball_eq);
    bento_game_text_init(&state->tank_eq);
    bento_game_sprite_init(&state->ball);
    for (i = 0U; i < (uint32_t)TICKS; i++) {
        bento_game_box_init(&state->ticks[i]);
    }
    for (i = 0U; i < (uint32_t)SEGMENTS; i++) {
        bento_game_box_init(&state->segments[i]);
    }
}

int main(void)
{
    integrate_t *state = &g_integrate;
    bool quit = false;
    bento_status_t status;

    (void)memset(state, 0, sizeof(*state));
    bento_game_init(&state->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    init_objects(state);
    status = bento_game_title(&state->game, "INTEGRATE", NULL, &quit);
    report_joystick(&state->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_equations(state);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_lane(state);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_tank(state);
    }
    restart(state);
    state->frame = 0;
    state->next_tick = 0;
    state->shown_bars = 0;
    state->shown_limit = false;
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&state->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
