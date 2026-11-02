/*
 * s13_10_fsm_machine.c -- คาบ 13 ส่วนที่ 4: เครื่องสถานะของเครื่องซักผ้า
 *
 * เครื่องซักผ้าอยู่ในสถานะเดียวเสมอ IDLE -> FILL -> WASH -> SPIN ->
 * DONE -> IDLE ทุกสถานะมีตัวจับเวลาของตัวเอง N = t x fps นับถอยหลัง
 * ทีละเฟรม ถึง 0 แล้วย้ายไปสถานะถัดไป A = เริ่มซัก (กดครั้งเดียว)
 * B = หยุด กลับ IDLE ทันทีจากทุกสถานะ ทุกการย้ายมีเสียงไม่ซ้ำกัน
 * FILL น้ำขึ้นทีละแถบ · WASH ผ้าโยกไปมา · SPIN น้ำระบายแล้วผ้าหมุนเร็วขึ้น
 *
 * A washing machine is always in exactly one state: IDLE -> FILL ->
 * WASH -> SPIN -> DONE -> IDLE. Each state has its own timer, N = t x
 * fps, counted down one frame at a time; at 0 the machine moves to the
 * next state. A starts a wash (once per press), B stops at once from
 * any state, and every move has its own sound. FILL raises the water
 * band by band, WASH rocks the clothes, SPIN drains the water and
 * spins the clothes faster and faster.
 *
 * Twin of s13_10_fsm_machine.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_10_fsm_machine.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the state tuples are static const tables indexed by state_t;
 * - fsm_step() returns the new state and updates the frames left
 *   through a pointer, where Python returns a pair;
 * - math.sqrt / sin / cos are <math.h> in the board's float width,
 *   as py/modmath.c calls them (sqrtf / sinf / cosf on the PSoC Edge,
 *   sqrt / sin / cos on the reTerminal), with fp-contract off so each
 *   step rounds as MicroPython rounds it; math.pi is PI;
 * - the state lives in one struct at file scope (512 bytes or more);
 * - title()'s Back (SystemExit) and Stop end the program with 0.
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
#include "bento_runtime.h"

/* No fused multiply-add: each product and sum rounds on its own, as
 * MicroPython's float operations do, so the cloth pixels match. */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    FPS = 30,
    CX = 175, /* จุดกลางและรัศมีของประตูกลม */
    CY = 200,
    R = 80,
    LR = 46 /* รัศมีวงที่ผ้าวิ่ง (อยู่ในถังเสมอ) */
};
/* ---- end of settings ---- */

/* The machine and its drum. */
enum {
    BAND_COUNT = 4,  /* แถบน้ำ */
    CLOTH_COUNT = 3, /* ผ้า */
    PILE_RADIUS = 52,
    DRUM_EDGE_W = 6,
    WATER_INSET = 14,
    WATER_BAND_H = 18,
    WATER_RIM = 8,
    WATER_RADIUS = 6,
    CLOTH_PX = 16,
    CLOTH_RADIUS = 5,
    TEXT_CHARS = 64
};

/* The cabinet, the banner and the lamps. */
enum {
    BODY_X = 50,
    BODY_Y = 20,
    BODY_W = 250,
    BODY_H = 330,
    BODY_EDGE_W = 2,
    BODY_RADIUS = 14,
    SCREEN_X = 66,
    SCREEN_Y = 34,
    SCREEN_W = 218,
    SCREEN_H = 40,
    SCREEN_RADIUS = 8,
    TIME_X = 84,
    TIME_Y = 44,
    BANNER_X = 330,
    BANNER_Y = 20,
    BANNER_W = 440,
    BANNER_H = 64,
    BANNER_RADIUS = 10,
    STATE_TEXT_X = 352,
    STATE_TEXT_Y = 40,
    LAMP_X = 344,
    LAMP_Y = 112,
    LAMP_GAP = 88,
    LAMP_PX = 30,
    LAMP_RADIUS = 15,
    LAMP_LABEL_X = 338,
    LAMP_LABEL_Y = 150,
    RULE_X = 340,
    RULE_Y = 200,
    HINT_Y = 250
};

enum {
    COLOR_BODY = 0xD8DEE6,
    COLOR_EDGE = 0x8899AA,
    COLOR_SCREEN = 0x2A3440,
    COLOR_DRUM = 0x22303C,
    COLOR_WATER = 0x3D8BD9,
    COLOR_LAMP_OFF = 0x2A3440,
    COLOR_LAMP_EDGE = 0x556070
};

typedef enum {
    STATE_IDLE = 0,
    STATE_FILL,
    STATE_WASH,
    STATE_SPIN,
    STATE_DONE,
    STATE_COUNT
} state_t;

static const char *const NAMES[STATE_COUNT] = {"IDLE", "FILL", "WASH",
                                               "SPIN", "DONE"};
/* อยู่ในแต่ละสถานะกี่วินาที (IDLE รอปุ่ม ไม่มีเวลา) */
static const int32_t SECONDS[STATE_COUNT] = {0, 3, 5, 4, 3};
/* สีป้ายใหญ่ (เข้มพอให้ตัวขาวอ่านง่าย) */
static const uint32_t BANNER[STATE_COUNT] = {
    0x3A4450U, 0x1F5FAFU, 0x0F7F8FU, 0xB0601AU, 0x2E8B57U};
static const bento_real_t PI = (bento_real_t)3.14159265358979323846;

typedef struct {
    bento_game_t game;
    bento_game_text_t hud_time;
    bento_game_box_t water[BAND_COUNT];
    bento_game_box_t clothes[CLOTH_COUNT];
    bento_game_box_t banner;
    bento_game_text_t hud_state;
    bento_game_box_t lamps[STATE_COUNT];
    bento_game_text_t hud_rule;
    state_t state;
    int32_t left;
    int32_t frame;
    bento_real_t spin;
    bento_real_t omega;
    int32_t level;
} washer_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern washer_t g_washer;
washer_t g_washer;

static int32_t frames_for(state_t state)
{
    return SECONDS[state] * FPS; /* N = t x fps */
}

/* กฎทั้งหมดของเครื่องอยู่ที่นี่ที่เดียว: คืนสถานะใหม่ ตั้งเฟรมที่เหลือ */
static state_t fsm_step(state_t state, int32_t *left, bool start,
                        bool stop)
{
    /* หมดเวลาแล้วไปสถานะไหน */
    static const state_t next_of[STATE_COUNT] = {
        STATE_IDLE, STATE_WASH, STATE_SPIN, STATE_DONE, STATE_IDLE};
    state_t next = state;

    if (stop && (state != STATE_IDLE)) {
        next = STATE_IDLE; /* หยุดไม่ถาม ไม่สนว่าอยู่สถานะไหน */
        *left = 0;
    } else if (state == STATE_IDLE) {
        /* กดหยุดค้าง = เริ่มไม่ได้ */
        next = (start && !stop) ? STATE_FILL : STATE_IDLE;
        *left = frames_for(next);
    } else {
        *left -= 1; /* นับเฟรมถอยหลัง (ไม่มี sleep) */
        if (*left <= 0) {
            next = next_of[state];
            *left = frames_for(next);
        }
    }
    return next;
}

/* จำนวนแถบน้ำ 0..4 : FILL ขึ้นทีละแถบ · WASH เต็ม · SPIN ระบายหมดในครึ่งแรก */
static int32_t water_level(state_t state, int32_t left)
{
    const int32_t total = frames_for(state);
    int32_t level = 0;

    if (state == STATE_FILL) {
        level = (((total - left) * BAND_COUNT) / total) + 1;
        level = (level < BAND_COUNT) ? level : BAND_COUNT;
    } else if (state == STATE_WASH) {
        level = BAND_COUNT;
    } else if (state == STATE_SPIN) {
        level =
            BAND_COUNT - (((total - left) * (2 * BAND_COUNT)) / total);
        level = (level > 0) ? level : 0;
    } else {
        /* IDLE and DONE: no water */
    }
    return level;
}

/* math.sqrt, math.sin and math.cos in the width of the board's
 * MicroPython float, the C functions py/modmath.c calls. */
static bento_real_t real_sqrt(bento_real_t value)
{
#if BENTO_FMT_REAL_DOUBLE
    return sqrt(value);
#else
    return sqrtf(value);
#endif
}

static bento_real_t real_sin(bento_real_t angle)
{
#if BENTO_FMT_REAL_DOUBLE
    return sin(angle);
#else
    return sinf(angle);
#endif
}

static bento_real_t real_cos(bento_real_t angle)
{
#if BENTO_FMT_REAL_DOUBLE
    return cos(angle);
#else
    return cosf(angle);
#endif
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

static bento_status_t add_text(washer_t *washer,
                               bento_game_text_t *text,
                               const char *words, int32_t x, int32_t y,
                               uint32_t color)
{
    bento_game_text_init(text);
    return bento_game_text_create(&washer->game, text, words, x, y,
                                  color);
}

/* The cabinet, its small screen and the round door. */
static bento_status_t add_cabinet(washer_t *washer)
{
    bento_game_box_t body;
    bento_game_box_t screen;
    bento_game_box_t door;
    bento_status_t status = bento_game_box_create_styled(
        &washer->game, &body, (bento_real_t)BODY_X,
        (bento_real_t)BODY_Y, (bento_real_t)BODY_W,
        (bento_real_t)BODY_H, COLOR_BODY, COLOR_EDGE, BODY_RADIUS,
        BODY_EDGE_W);

    if (status == BENTO_OK) {
        /* จอเล็กของเครื่อง: เฟรมที่เหลือ */
        status = bento_game_box_create_styled(
            &washer->game, &screen, (bento_real_t)SCREEN_X,
            (bento_real_t)SCREEN_Y, (bento_real_t)SCREEN_W,
            (bento_real_t)SCREEN_H, COLOR_SCREEN,
            BENTO_GAME_BORDER_NONE, SCREEN_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = add_text(washer, &washer->hud_time, "", TIME_X, TIME_Y,
                          BENTO_GAME_GREEN);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            &washer->game, &door, (bento_real_t)CX - (bento_real_t)R,
            (bento_real_t)CY - (bento_real_t)R, (bento_real_t)(2 * R),
            (bento_real_t)(2 * R), COLOR_DRUM, COLOR_EDGE, R,
            DRUM_EDGE_W);
    }
    return status;
}

/* แถบล่างสุดคือแถบที่ 0 กว้างตามโค้งของถัง */
static bento_status_t add_water(washer_t *washer)
{
    const int32_t rim = (int32_t)R - WATER_RIM;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < BAND_COUNT); i++) {
        /* ระยะจากจุดกลางถึงกลางแถบ */
        const int32_t middle = (int32_t)R - WATER_INSET -
                               (i * WATER_BAND_H) - (WATER_BAND_H / 2);
        const int32_t inside = (rim * rim) - (middle * middle);
        const bento_real_t across =
            (bento_real_t)2 * real_sqrt((bento_real_t)inside);
        const int32_t width = (int32_t)across;
        const int32_t left_x = CX - (width / 2);
        const int32_t top_y = (CY + middle) - (WATER_BAND_H / 2);

        status = bento_game_box_create_styled(
            &washer->game, &washer->water[i], (bento_real_t)left_x,
            (bento_real_t)top_y, (bento_real_t)width,
            (bento_real_t)WATER_BAND_H, COLOR_WATER,
            BENTO_GAME_BORDER_NONE, WATER_RADIUS, 0);
        if (status == BENTO_OK) {
            status =
                bento_game_box_hide(&washer->game, &washer->water[i]);
        }
    }
    return status;
}

static bento_status_t add_clothes(washer_t *washer)
{
    static const uint32_t colors[CLOTH_COUNT] = {
        BENTO_GAME_RED, BENTO_GAME_YELLOW, BENTO_GAME_PINK};
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < CLOTH_COUNT); i++) {
        status = bento_game_box_create_styled(
            &washer->game, &washer->clothes[i], (bento_real_t)0,
            (bento_real_t)0, (bento_real_t)CLOTH_PX,
            (bento_real_t)CLOTH_PX, colors[i],
            (int32_t)BENTO_GAME_WHITE, CLOTH_RADIUS, 0);
    }
    return status;
}

/* The banner, the five lamps with their names, and the two rules. */
static bento_status_t add_panel(washer_t *washer)
{
    bento_game_text_t hint;
    int32_t i;
    bento_status_t status = bento_game_box_create_styled(
        &washer->game, &washer->banner, (bento_real_t)BANNER_X,
        (bento_real_t)BANNER_Y, (bento_real_t)BANNER_W,
        (bento_real_t)BANNER_H, BANNER[STATE_IDLE],
        BENTO_GAME_BORDER_NONE, BANNER_RADIUS, 0);

    if (status == BENTO_OK) {
        status = add_text(washer, &washer->hud_state, "", STATE_TEXT_X,
                          STATE_TEXT_Y, BENTO_GAME_WHITE);
    }
    for (i = 0; (status == BENTO_OK) && (i < (int32_t)STATE_COUNT);
         i++) {
        bento_game_text_t label;

        status = bento_game_box_create_styled(
            &washer->game, &washer->lamps[i],
            (bento_real_t)(LAMP_X + (i * LAMP_GAP)),
            (bento_real_t)LAMP_Y, (bento_real_t)LAMP_PX,
            (bento_real_t)LAMP_PX, COLOR_LAMP_OFF, COLOR_LAMP_EDGE,
            LAMP_RADIUS, 0);
        if (status == BENTO_OK) {
            status = add_text(washer, &label, NAMES[i],
                              LAMP_LABEL_X + (i * LAMP_GAP),
                              LAMP_LABEL_Y, BENTO_GAME_WHITE);
        }
    }
    if (status == BENTO_OK) {
        status = add_text(washer, &washer->hud_rule, "", RULE_X, RULE_Y,
                          BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(washer, &hint,
                          "A = START (กดครั้งเดียว)    B = STOP ทันที",
                          RULE_X, HINT_Y, BENTO_GAME_GB_LIGHT);
    }
    return status;
}

/* วางผ้าบนวงกลมรอบจุดกลางถัง ที่มุม angle (เรเดียน) */
static bento_status_t place(washer_t *washer, bento_game_box_t *cloth,
                            bento_real_t angle, int32_t radius)
{
    const bento_real_t across = (bento_real_t)radius * real_cos(angle);
    const bento_real_t down = (bento_real_t)radius * real_sin(angle);
    const int32_t x =
        (int32_t)((bento_real_t)CX + across) - (CLOTH_PX / 2);
    const int32_t y =
        (int32_t)((bento_real_t)CY + down) - (CLOTH_PX / 2);

    return bento_game_box_move_to(&washer->game, cloth, (bento_real_t)x,
                                  (bento_real_t)y);
}

/* ผ้ากองนิ่งที่ก้นถัง */
static bento_status_t pile_clothes(washer_t *washer)
{
    const bento_real_t apart = (bento_real_t)0.5;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < CLOTH_COUNT); i++) {
        const int32_t step = i - 1;
        const bento_real_t offset = (bento_real_t)step * apart;

        status = place(washer, &washer->clothes[i],
                       (PI / (bento_real_t)2) + offset, PILE_RADIUS);
    }
    return status;
}

/* IDLE shows READY or STOP; another state shows its timer rule. */
static bento_status_t show_rule(washer_t *washer, state_t state,
                                bool by_stop)
{
    char text[TEXT_CHARS];
    bento_status_t status;

    if (state == STATE_IDLE) {
        status = bento_game_text_set(&washer->game, &washer->hud_time,
                                     by_stop ? "STOP" : "READY");
        if (status == BENTO_OK) {
            status = bento_game_text_set(
                &washer->game, &washer->hud_rule,
                by_stop ? "หยุดแล้ว (B)  กด A เริ่มใหม่" : "กด A เพื่อเริ่มซัก");
        }
    } else {
        /* ตัวจับเวลาของสถานะนี้: N = t x fps */
        (void)snprintf(text, sizeof(text),
                       "%s = %" PRId32 " s x %d = %" PRId32 " f",
                       NAMES[state], SECONDS[state], FPS,
                       frames_for(state));
        status =
            bento_game_text_set(&washer->game, &washer->hud_rule, text);
    }
    return status;
}

/* ทำครั้งเดียวตอนย้ายสถานะ: ป้าย ไฟ เสียง (อัปเดตเฉพาะที่เปลี่ยน) */
static bento_status_t enter(washer_t *washer, state_t next, state_t old,
                            bool by_stop)
{
    static const char *const thai[STATE_COUNT] = {
        "รอเริ่ม", "เติมน้ำ", "ซัก", "ปั่นหมาด", "เสร็จแล้ว"};
    /* เสียงตอนเข้าแต่ละสถานะ (หยุดด้วย B = "deny") */
    static const char *const sounds[STATE_COUNT] = {
        "back", "start", "select", "fire", "win"};
    static const uint32_t lamp[STATE_COUNT] = {
        0x8899AAU, BENTO_GAME_BLUE, BENTO_GAME_CYAN, BENTO_GAME_ORANGE,
        BENTO_GAME_GREEN};
    char text[TEXT_CHARS];
    bento_status_t status = bento_game_box_set_color(
        &washer->game, &washer->banner, BANNER[next]);

    (void)snprintf(text, sizeof(text), "%s   %s", NAMES[next],
                   thai[next]);
    if (status == BENTO_OK) {
        status = bento_game_text_set(&washer->game, &washer->hud_state,
                                     text);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &washer->game, &washer->lamps[old], COLOR_LAMP_OFF);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &washer->game, &washer->lamps[next], lamp[next]);
    }
    if ((status == BENTO_OK) && (next != old)) {
        /* ตอนเปิดเครื่องครั้งแรกไม่มีเสียง */
        bento_game_sfx(&washer->game, by_stop ? "deny" : sounds[next]);
    }
    if ((status == BENTO_OK) &&
        ((next == STATE_IDLE) || (next == STATE_FILL) ||
         (next == STATE_DONE))) {
        status = pile_clothes(washer);
    }
    if (status == BENTO_OK) {
        status = show_rule(washer, next, by_stop);
    }
    return status;
}

/* One water band shows or hides. */
static bento_status_t show_band(washer_t *washer, int32_t i, bool wet)
{
    return wet ? bento_game_box_show(&washer->game, &washer->water[i])
               : bento_game_box_hide(&washer->game, &washer->water[i]);
}

/* น้ำเปลี่ยนเฉพาะตอนจำนวนแถบเปลี่ยน */
static bento_status_t show_water(washer_t *washer)
{
    const int32_t level_now = water_level(washer->state, washer->left);
    const bool changed = level_now != washer->level;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; changed && (status == BENTO_OK) && (i < BAND_COUNT);
         i++) {
        const bool wet = i < level_now;
        const bool was_wet = i < washer->level;

        if (wet != was_wet) {
            status = show_band(washer, i, wet);
        }
    }
    washer->level = level_now;
    return status;
}

/* ผ้าขยับเฉพาะตอน WASH (โยกไปมาใต้ถัง) กับ SPIN (หมุนรอบถัง) */
static bento_status_t move_clothes(washer_t *washer)
{
    const bento_real_t swing = (bento_real_t)0.9;
    const bento_real_t swing_rate = (bento_real_t)0.18;
    const bento_real_t omega_max = (bento_real_t)0.5;
    const bento_real_t omega_step = (bento_real_t)0.01;
    const bento_real_t third_turn = (bento_real_t)2.094; /* 120 องศา */
    int32_t i;
    bento_status_t status = BENTO_OK;

    if (washer->state == STATE_SPIN) {
        const bento_real_t faster = washer->omega + omega_step;

        washer->omega = (faster < omega_max) ? faster : omega_max;
        washer->spin += washer->omega;
    }
    for (i = 0; (status == BENTO_OK) && (i < CLOTH_COUNT); i++) {
        if (washer->state == STATE_WASH) {
            const bento_real_t phase =
                ((bento_real_t)washer->frame * swing_rate) +
                ((bento_real_t)i * swing);
            const bento_real_t rock = swing * real_sin(phase);

            status = place(washer, &washer->clothes[i],
                           (PI / (bento_real_t)2) + rock, LR);
        } else if (washer->state == STATE_SPIN) {
            const bento_real_t turn = (bento_real_t)i * third_turn;

            status = place(washer, &washer->clothes[i],
                           washer->spin + turn, LR);
        } else {
            /* the clothes lie still */
        }
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    washer_t *washer = &g_washer;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;        /* the state is g_washer */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&washer->game, &keys);
    washer->frame += 1;
    if (status == BENTO_OK) {
        /* A จับจังหวะกด · B ดูแค่ว่ากดอยู่ไหม (หยุดต้องไวที่สุด) */
        const bool start = bento_game_pressed_once(
            &washer->game, BENTO_GAME_KEY_A, &keys);
        const state_t next =
            fsm_step(washer->state, &washer->left, start, keys.b);

        if (next != washer->state) {
            status = enter(washer, next, washer->state,
                           keys.b && (washer->state != STATE_IDLE));
            washer->state = next;
            washer->spin = (bento_real_t)0;
            washer->omega = (bento_real_t)0;
        }
    }
    if (status == BENTO_OK) {
        status = show_water(washer);
    }
    if (status == BENTO_OK) {
        status = move_clothes(washer);
    }
    if ((status == BENTO_OK) && (washer->state != STATE_IDLE)) {
        char text[TEXT_CHARS];

        (void)snprintf(text, sizeof(text), "เหลือ %" PRId32 " f",
                       washer->left);
        status =
            bento_game_text_set(&washer->game, &washer->hud_time, text);
    }
    return status;
}

static bento_status_t build_screen(washer_t *washer)
{
    bento_status_t status = add_cabinet(washer);

    if (status == BENTO_OK) {
        status = add_water(washer);
    }
    if (status == BENTO_OK) {
        status = add_clothes(washer);
    }
    if (status == BENTO_OK) {
        status = add_panel(washer);
    }
    if (status == BENTO_OK) {
        status = enter(washer, STATE_IDLE, STATE_IDLE, false);
    }
    return status;
}

int main(void)
{
    washer_t *washer = &g_washer;
    bool quit = false;
    bento_status_t status;

    (void)memset(washer, 0, sizeof(*washer));
    bento_game_init(&washer->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    washer->state = STATE_IDLE;
    status = bento_game_title(&washer->game, "WASHER FSM", NULL, &quit);
    report_joystick(&washer->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(washer);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&washer->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
