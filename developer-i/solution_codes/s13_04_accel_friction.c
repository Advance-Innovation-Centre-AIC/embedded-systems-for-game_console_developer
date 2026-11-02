/*
 * s13_04_accel_friction.c -- คาบ 13 การ์ด 3: เร่ง หน่วง และเพดาน
 *
 * กดจอยซ้าย/ขวาค้าง = เร่ง (v += a) ปล่อย = หน่วง (v *= f) และ v อยู่
 * ใน -VMAX ถึง +VMAX เสมอ กด A เปลี่ยน f: 0.95 ลื่นเหมือนน้ำแข็ง
 * 0.78 พอดีแบบ Pong 0.30 หนืดเหมือนโคลน (สีพื้นเปลี่ยนตาม) แถบความเร็ว
 * บอกขนาดของ v (ก้อนแดงปลายแถบ = ชนเพดาน) ธงเหลืองปักตรงจุดที่ปล่อย
 * จอย และบรรทัด "ไถล" บอกว่าไถลกี่เฟรม กี่พิกเซล
 *
 * Hold the joystick left or right to accelerate (v += a), let go to
 * slow down (v *= f); v always stays within -VMAX and +VMAX. A cycles
 * f between 0.95 (ice), 0.78 (just right, as Pong) and 0.30 (mud), and
 * the floor changes colour with it. The speed bar shows v (the red end
 * block means the ceiling), a yellow flag marks where the joystick was
 * released, and the slide line says how many frames and pixels the
 * ship slid.
 *
 * Twin of s13_04_accel_friction.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_04_accel_friction.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the bar dict {-1: [...], 1: [...]} is an array indexed 0 (left)
 *   and 1 (right);
 * - the slide list [frames, pixels, f] or None is a struct with a flag;
 * - "%.2f" and "%+.1f" are bento_fmt_real_precision (a '+' added);
 * - the state lives in one struct at file scope (512 bytes or more);
 * - title()'s Back (SystemExit) and Stop end the program with 0.
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

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
static const bento_real_t ACCEL =
    (bento_real_t)1.4; /* ค่าเดียวกับ shooter_step2.py */
static const bento_real_t VMAX = (bento_real_t)13.0;
/* ช้ากว่านี้ถือว่าหยุดแล้ว (v = 0) */
static const bento_real_t STOP = (bento_real_t)0.2;
enum {
    SEGS = 7, /* ก้อนแถบความเร็วต่อข้าง */
    SHIP_W = 62,
    SHIP_Y = 150,
    FLOOR_Y = 180,
    BAR_Y = 250,
    CENTER = BENTO_GAME_WIDTH / 2
};
/* ---- end of settings ---- */

/* The ship, the floor and the speed bar. */
enum {
    SIDES = 2,
    MODES = 3,
    FLOOR_H = 10,
    HALF_SHIP_W = SHIP_W / 2, /* floor division */
    SHIP_START_X = (int32_t)CENTER - HALF_SHIP_W,
    SEGMENT_GAP = 6,
    SEGMENT_STEP = 46,
    SEGMENT_W = 40,
    SEGMENT_H = 24,
    ZERO_LINE_W = 4,
    ZERO_LINE_RISE = 6,
    ZERO_LINE_H = 36,
    ZERO_LINE_X = (int32_t)CENTER - (ZERO_LINE_W / 2),
    LABEL_DROP = 34,
    MAX_LABEL_W = 30,
    SPEED_LABEL_SHIFT = 30,
    FLAG_PARK_X = -20,
    FLAG_W = 4,
    FLAG_H = 40,
    WALL_SOUND_SPEED = 2,
    FPS = 30,
    TEXT_CHARS = 128,
    SPEED_CHARS = 48
};

/* The texts on top. */
enum {
    TEXT_X = 16,
    RULE_Y = 4,
    FEEL_Y = 34,
    SLIDE_Y = 62
};

static const bento_real_t FRICTIONS[MODES] = {
    (bento_real_t)0.95, (bento_real_t)0.78, (bento_real_t)0.30};
/* สีพื้น: น้ำแข็ง · ปกติ · โคลน */
static const uint32_t FLOORS[MODES] = {0xA8E6FFU, 0x8899AAU, 0x8B5A2BU};

/* [เฟรม, พิกเซล, f] ระหว่างวัดระยะไถล */
typedef struct {
    bool active;
    int32_t frames;
    bento_real_t pixels;
    bento_real_t friction;
} slide_t;

typedef struct {
    bento_game_t game;
    bento_game_text_t feel_text;
    bento_game_text_t slide_text;
    bento_game_text_t speed_text;
    bento_game_box_t floor;
    bento_game_box_t flag;
    bento_game_sprite_t ship;
    bento_game_box_t bar[SIDES][SEGS];
    bento_real_t ship_x;
    bento_real_t speed;
    int32_t mode;
    int32_t prev_push;
    slide_t slide;
    int32_t shown_level;
    char shown_speed[SPEED_CHARS];
} slide_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern slide_game_t g_slide;
slide_game_t g_slide;

static bento_real_t absolute(bento_real_t value)
{
    return (value < (bento_real_t)0) ? -value : value;
}

static int32_t absolute_int(int32_t value)
{
    return (value < 0) ? -value : value;
}

/* การ์ด 3 ทั้งใบ: push = -1 ซ้าย · +1 ขวา · 0 ปล่อย */
static bento_real_t next_speed(bento_real_t speed, int32_t push,
                               bento_real_t friction)
{
    bento_real_t result = speed;

    if (push != 0) {
        result += (bento_real_t)push * ACCEL; /* กดค้าง -> เร่ง */
    } else {
        result *= friction; /* ปล่อย -> หน่วง */
    }
    if (result > VMAX) {
        result = VMAX;
    }
    if (result < -VMAX) {
        result = -VMAX;
    }
    return result;
}

/* v -> จำนวนก้อนที่ติด มีเครื่องหมาย (-7 ถึง +7) */
static int32_t bar_level(bento_real_t speed)
{
    const bento_real_t half = (bento_real_t)0.5;
    const bento_real_t share = absolute(speed) / VMAX;
    const bento_real_t blocks = share * (bento_real_t)SEGS;
    const bento_real_t rounded = blocks + half;
    const int32_t count = (int32_t)rounded;

    return (speed < (bento_real_t)0) ? -count : count;
}

/* ก้อนที่ i (นับจากกลาง) ของข้าง side (-1 ซ้าย / +1 ขวา) ควรติดไหม */
static bool box_on(int32_t side, int32_t i, int32_t level)
{
    return ((level * side) > 0) && (i < absolute_int(level));
}

/* "%.<digits>f", with a '+' on a positive when signed_text asks. */
static void format_real(char *text, uint32_t size, bento_real_t value,
                        int32_t digits, bool signed_text)
{
    char digits_text[BENTO_FMT_BUF_SIZE];
    uint32_t length = 0U;
    const bool plus = signed_text && (value >= (bento_real_t)0);

    digits_text[0] = '\0';
    /* BENTO_FMT_BUF_SIZE holds any result, so it cannot fail */
    (void)bento_fmt_real_precision(digits_text,
                                   (uint32_t)sizeof(digits_text), value,
                                   digits, &length);
    (void)snprintf(text, size, "%s%s", plus ? "+" : "", digits_text);
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

static bento_status_t add_texts(slide_game_t *slide_game)
{
    bento_game_t *game = &slide_game->game;
    bento_game_text_t rule;
    char text[TEXT_CHARS];
    bento_status_t status;

    bento_game_text_init(&rule);
    (void)snprintf(text, sizeof(text),
                   "กดค้าง: v += a     ปล่อย: v *= f     เพดาน: -%" PRId32
                   " ถึง +%" PRId32,
                   (int32_t)VMAX, (int32_t)VMAX);
    status = bento_game_text_create(game, &rule, text, TEXT_X, RULE_Y,
                                    BENTO_GAME_YELLOW);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &slide_game->feel_text, "",
                                   TEXT_X, FEEL_Y, BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &slide_game->slide_text, "เร่งให้เต็มแถบ แล้วปล่อยจอย",
            TEXT_X, SLIDE_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t add_floor_and_ship(slide_game_t *slide_game)
{
    bento_game_t *game = &slide_game->game;
    const int32_t flag_y = (int32_t)FLOOR_Y - FLAG_H;
    bento_status_t status = bento_game_box_create(
        game, &slide_game->floor, (bento_real_t)0,
        (bento_real_t)FLOOR_Y, (bento_real_t)BENTO_GAME_WIDTH,
        (bento_real_t)FLOOR_H, FLOORS[0]);

    if (status == BENTO_OK) {
        /* ธงเหลือง = จุดที่เพิ่งปล่อยจอย */
        status = bento_game_box_create(
            game, &slide_game->flag, (bento_real_t)FLAG_PARK_X,
            (bento_real_t)flag_y, (bento_real_t)FLAG_W,
            (bento_real_t)FLAG_H, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(
            game, &slide_game->ship, "ship", (bento_real_t)SHIP_START_X,
            (bento_real_t)SHIP_Y);
    }
    return status;
}

/* One block of the bar, hidden; the outermost one is red (เพดาน). */
static bento_status_t add_segment(slide_game_t *slide_game,
                                  int32_t side, int32_t i)
{
    const int32_t left =
        (side > 0)
            ? ((int32_t)CENTER + SEGMENT_GAP + (i * SEGMENT_STEP))
            : ((int32_t)CENTER - SEGMENT_STEP - (i * SEGMENT_STEP));
    const uint32_t color =
        (i == (SEGS - 1)) ? BENTO_GAME_RED : BENTO_GAME_GREEN;
    bento_game_box_t *segment = &slide_game->bar[(side > 0) ? 1 : 0][i];
    bento_status_t status = bento_game_box_create(
        &slide_game->game, segment, (bento_real_t)left,
        (bento_real_t)BAR_Y, (bento_real_t)SEGMENT_W,
        (bento_real_t)SEGMENT_H, color);

    if (status == BENTO_OK) {
        status = bento_game_box_hide(&slide_game->game, segment);
    }
    return status;
}

static bento_status_t add_bar(slide_game_t *slide_game)
{
    int32_t side;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (side = -1; (status == BENTO_OK) && (side <= 1); side += 2) {
        for (i = 0; (status == BENTO_OK) && (i < SEGS); i++) {
            status = add_segment(slide_game, side, i);
        }
    }
    return status;
}

static bento_status_t add_bar_labels(slide_game_t *slide_game)
{
    bento_game_t *game = &slide_game->game;
    const int32_t label_y = (int32_t)BAR_Y + LABEL_DROP;
    const int32_t zero_line_y = (int32_t)BAR_Y - ZERO_LINE_RISE;
    bento_game_box_t zero_line;
    bento_game_text_t low_label;
    bento_game_text_t high_label;
    char text[SPEED_CHARS];
    bento_status_t status;

    bento_game_box_init(&zero_line);
    bento_game_text_init(&low_label);
    bento_game_text_init(&high_label);
    status = bento_game_box_create(
        game, &zero_line, (bento_real_t)ZERO_LINE_X,
        (bento_real_t)zero_line_y, (bento_real_t)ZERO_LINE_W,
        (bento_real_t)ZERO_LINE_H, BENTO_GAME_WHITE);
    (void)snprintf(text, sizeof(text), "-%" PRId32, (int32_t)VMAX);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &low_label, text,
            (int32_t)CENTER - (SEGMENT_STEP * (int32_t)SEGS), label_y,
            BENTO_GAME_WHITE);
    }
    (void)snprintf(text, sizeof(text), "+%" PRId32, (int32_t)VMAX);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &high_label, text,
            ((int32_t)CENTER + (SEGMENT_STEP * (int32_t)SEGS)) -
                MAX_LABEL_W,
            label_y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &slide_game->speed_text, "",
                                   (int32_t)CENTER - SPEED_LABEL_SHIFT,
                                   label_y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t set_mode(slide_game_t *slide_game, int32_t mode)
{
    static const char *const feels[MODES] = {
        "ลื่นเหมือนน้ำแข็ง", "พอดีแบบ Pong", "หนืดเหมือนโคลน"};
    char friction_text[BENTO_FMT_BUF_SIZE];
    char text[TEXT_CHARS];
    bento_status_t status;

    format_real(friction_text, (uint32_t)sizeof(friction_text),
                FRICTIONS[mode], 2, false);
    (void)snprintf(text, sizeof(text), "f = %s   %s   (A = เปลี่ยน)",
                   friction_text, feels[mode]);
    status = bento_game_text_set(&slide_game->game,
                                 &slide_game->feel_text, text);
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &slide_game->game, &slide_game->floor, FLOORS[mode]);
    }
    return status;
}

static bento_status_t build_screen(slide_game_t *slide_game)
{
    bento_status_t status = add_texts(slide_game);

    if (status == BENTO_OK) {
        status = add_floor_and_ship(slide_game);
    }
    if (status == BENTO_OK) {
        status = add_bar(slide_game);
    }
    if (status == BENTO_OK) {
        status = add_bar_labels(slide_game);
    }
    if (status == BENTO_OK) {
        status = set_mode(slide_game, 0);
    }
    return status;
}

/* คิด: ปุ่มเปลี่ยน "ความเร็ว" แล้วความเร็วค่อยเปลี่ยนตำแหน่ง */
static bento_status_t steer(slide_game_t *slide_game, int32_t push)
{
    const bento_real_t friction = FRICTIONS[slide_game->mode];
    bento_status_t status = BENTO_OK;

    slide_game->speed = next_speed(slide_game->speed, push, friction);
    if ((push == 0) && (absolute(slide_game->speed) < STOP)) {
        slide_game->speed = (bento_real_t)0.0;
    }
    if ((push == 0) && (slide_game->prev_push != 0) &&
        (absolute(slide_game->speed) > (bento_real_t)1)) {
        /* เพิ่งปล่อย -> เริ่มวัดระยะไถล ปักธงตรงนี้ */
        slide_game->slide.active = true;
        slide_game->slide.frames = 0;
        slide_game->slide.pixels = (bento_real_t)0.0;
        slide_game->slide.friction = friction;
        status = bento_game_box_move_to(
            &slide_game->game, &slide_game->flag,
            (bento_real_t)((int32_t)slide_game->ship_x + HALF_SHIP_W),
            slide_game->flag.y);
    } else if (push != 0) {
        slide_game->slide.active = false;
    } else {
        /* still sliding, or standing still */
    }
    slide_game->prev_push = push;
    return status;
}

/* ชนขอบจอ = หยุด (เร็วพอถึงจะมีเสียง) */
static bool move_ship(slide_game_t *slide_game)
{
    const bento_real_t want = slide_game->ship_x + slide_game->speed;
    const bento_real_t rightmost =
        (bento_real_t)(BENTO_GAME_WIDTH - SHIP_W);
    bento_real_t ship_x = (want < rightmost) ? want : rightmost;
    bool hit_wall;

    if (ship_x < (bento_real_t)0.0) {
        ship_x = (bento_real_t)0.0;
    }
    slide_game->ship_x = ship_x;
    hit_wall = (ship_x != want);
    if (hit_wall && (absolute(slide_game->speed) >
                     (bento_real_t)WALL_SOUND_SPEED)) {
        bento_game_sfx(&slide_game->game, "wall");
    }
    if (hit_wall) {
        slide_game->speed = (bento_real_t)0.0;
    }
    return hit_wall;
}

static bento_status_t measure_slide(slide_game_t *slide_game,
                                    bool hit_wall)
{
    slide_t *slide = &slide_game->slide;
    bento_status_t status = BENTO_OK;

    if (slide->active) {
        slide->frames += 1;
        slide->pixels += absolute(slide_game->speed);
        if (slide_game->speed == (bento_real_t)0.0) {
            char friction_text[BENTO_FMT_BUF_SIZE];
            char text[TEXT_CHARS];

            format_real(friction_text, (uint32_t)sizeof(friction_text),
                        slide->friction, 2, false);
            (void)snprintf(
                text, sizeof(text),
                "f %s  ปล่อยแล้วไถล %" PRId32 " เฟรม  %" PRId32 " px%s",
                friction_text, slide->frames, (int32_t)slide->pixels,
                hit_wall ? "  (ชนขอบ)" : "");
            status = bento_game_text_set(&slide_game->game,
                                         &slide_game->slide_text, text);
            slide->active = false;
        }
    }
    return status;
}

static bento_status_t show_segment(slide_game_t *slide_game,
                                   int32_t side, int32_t i,
                                   int32_t level)
{
    const bool now = box_on(side, i, level);
    const bool before = box_on(side, i, slide_game->shown_level);
    const bento_game_box_t *segment =
        &slide_game->bar[(side > 0) ? 1 : 0][i];
    bento_status_t status = BENTO_OK;

    if (now && !before) {
        status = bento_game_box_show(&slide_game->game, segment);
    }
    if (before && !now) {
        status = bento_game_box_hide(&slide_game->game, segment);
    }
    return status;
}

static bento_status_t show_side(slide_game_t *slide_game, int32_t side,
                                int32_t level)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < SEGS); i++) {
        status = show_segment(slide_game, side, i, level);
    }
    return status;
}

/* ออก: แถบ เฉพาะเมื่อจำนวนก้อนเปลี่ยน */
static bento_status_t show_bar(slide_game_t *slide_game)
{
    const int32_t level = bar_level(slide_game->speed);
    int32_t side;
    bento_status_t status = BENTO_OK;

    if (level != slide_game->shown_level) {
        for (side = -1; (status == BENTO_OK) && (side <= 1);
             side += 2) {
            status = show_side(slide_game, side, level);
        }
        slide_game->shown_level = level;
    }
    return status;
}

/* ออก: ป้าย v เฉพาะเมื่อข้อความเปลี่ยน */
static bento_status_t show_speed(slide_game_t *slide_game)
{
    char speed_text[BENTO_FMT_BUF_SIZE + 1U];
    char text[SPEED_CHARS];
    bento_status_t status = BENTO_OK;

    format_real(speed_text, (uint32_t)sizeof(speed_text),
                slide_game->speed, 1, true);
    (void)snprintf(text, sizeof(text), "v %s", speed_text);
    if (strcmp(text, slide_game->shown_speed) != 0) {
        status = bento_game_text_set(&slide_game->game,
                                     &slide_game->speed_text, text);
        (void)snprintf(slide_game->shown_speed,
                       sizeof(slide_game->shown_speed), "%s", text);
    }
    return status;
}

/* เข้า: A เปลี่ยน f */
static bento_status_t read_mode_key(slide_game_t *slide_game,
                                    const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    if (bento_game_pressed_once(&slide_game->game, BENTO_GAME_KEY_A,
                                keys)) {
        slide_game->mode = (slide_game->mode + 1) % MODES;
        status = set_mode(slide_game, slide_game->mode);
        bento_game_sfx(&slide_game->game, "select");
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    slide_game_t *slide_game = &g_slide;
    bento_game_keys_t keys;
    bool hit_wall = false;
    bento_status_t status;

    (void)user;        /* the state is g_slide */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&slide_game->game, &keys);
    if (status == BENTO_OK) {
        status = read_mode_key(slide_game, &keys);
    }
    if (status == BENTO_OK) {
        status = steer(slide_game,
                       (keys.right ? 1 : 0) - (keys.left ? 1 : 0));
    }
    if (status == BENTO_OK) {
        hit_wall = move_ship(slide_game);
        status = bento_game_sprite_move_to(
            &slide_game->game, &slide_game->ship, slide_game->ship_x,
            (bento_real_t)SHIP_Y);
    }
    if (status == BENTO_OK) {
        status = measure_slide(slide_game, hit_wall);
    }
    if (status == BENTO_OK) {
        status = show_bar(slide_game);
    }
    if (status == BENTO_OK) {
        status = show_speed(slide_game);
    }
    return status;
}

static void reset_state(slide_game_t *slide_game)
{
    int32_t side;
    int32_t i;

    (void)memset(slide_game, 0, sizeof(*slide_game));
    bento_game_init(&slide_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_text_init(&slide_game->feel_text);
    bento_game_text_init(&slide_game->slide_text);
    bento_game_text_init(&slide_game->speed_text);
    bento_game_box_init(&slide_game->floor);
    bento_game_box_init(&slide_game->flag);
    bento_game_sprite_init(&slide_game->ship);
    for (side = 0; side < SIDES; side++) {
        for (i = 0; i < SEGS; i++) {
            bento_game_box_init(&slide_game->bar[side][i]);
        }
    }
    slide_game->ship_x = (bento_real_t)SHIP_START_X;
}

int main(void)
{
    slide_game_t *slide_game = &g_slide;
    bool quit = false;
    bento_status_t status;

    reset_state(slide_game);
    status = bento_game_title(&slide_game->game, "SLIDE", NULL, &quit);
    report_joystick(&slide_game->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(slide_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&slide_game->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
