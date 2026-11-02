/*
 * s13_09_aabb.c -- คาบ 13: ชนแบบกล่อง เห็นสี่เงื่อนไขเป็น True/False
 *
 * ขับกล่องเหลือง (A หุ่นยนต์ส่งของ AGV) ด้วยจอยไปรอบ ๆ สามโซน
 * กล่อง A ชนกล่อง B เมื่อสี่ข้อนี้จริงพร้อมกัน:
 * ax < bx+bw และ ax+aw > bx และ ay < by+bh และ ay+ah > by
 * แผงขวาแสดงตัวเลขจริงของ A กับโซนที่ใกล้ที่สุด (B) และสี่ข้อเป็น
 * True / False พร้อมไฟ ครบสี่ดวงเมื่อไร โซนเป็นสีแดงและมีเสียงเตือน
 *
 * Drive the yellow box A (a delivery robot, AGV) with the joystick
 * around three zones. Box A hits box B when all four tests are true
 * at once: ax < bx+bw and ax+aw > bx and ay < by+bh and ay+ah > by.
 * The right panel shows the real numbers of A and of the nearest zone
 * B, and each test as True or False with a lamp. When all four are
 * true the zone turns red and a warning sound plays.
 *
 * Twin of s13_09_aabb.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_09_aabb.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the ZONES list of tuples is a static const table of zone_t;
 * - the four tests and their numbers fill arrays the caller owns;
 * - the hits tuple is a bit mask, one bit per zone;
 * - the shown dict is fields of the state, PART_NEVER and hits_known
 *   standing for Python's None;
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
#include "bento_runtime.h"

/* The field and box A (แก้ได้). */
enum {
    STEP_PX = 4, /* กดจอยค้าง กล่อง A ขยับเฟรมละ 4 px */
    A_W = 46,
    A_H = 30,
    A_START_X = 332,
    A_START_Y = 270,
    A_BORDER_W = 2,
    A_RADIUS = 3,
    FIELD_X = 12,
    FIELD_Y = 10,
    FIELD_W = 450,
    FIELD_H = 350,
    ZONE_COUNT = 3,
    ZONE_RADIUS = 4,
    ZONE_LABEL_DX = 8,
    ZONE_LABEL_DY = 6,
    FPS = 30
};

/* The panel on the right. */
enum {
    HUD_X = 478,
    HUD_A_Y = 12,
    HUD_NAME_Y = 40,
    HUD_B_Y = 66,
    LAMP_Y = 110,
    LINE_Y = 104,
    ROW_GAP = 32,
    LAMP_PX = 14,
    LAMP_RADIUS = 7,
    LINE_DX = 22,
    HIT_LAMP_Y = 242,
    HIT_LAMP_PX = 30,
    HIT_LAMP_RADIUS = 15,
    HUD_HIT_DX = 40,
    HUD_HIT_Y = 246,
    NOTE_Y = 290,
    KEYS_Y = 322,
    TEXT_CHARS = 64,
    PART_NEVER = -1
};

/* The four tests, in game.hit()'s order. */
enum {
    PART_A_LEFT = 0, /* ax < bx+bw */
    PART_A_RIGHT,    /* ax+aw > bx */
    PART_A_TOP,      /* ay < by+bh */
    PART_A_BOTTOM,   /* ay+ah > by */
    PART_COUNT
};

enum {
    COLOR_FIELD = 0x18202A,
    COLOR_FIELD_EDGE = 0x3A4A5A,
    COLOR_LAMP_OFF = 0x3A4450,
    COLOR_NOTE = 0x9AA8B8
};

typedef struct {
    const char *name;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint32_t color; /* สีปกติ */
} zone_t;

static const zone_t ZONES[ZONE_COUNT] = {
    {"ZONE 1", 40, 40, 140, 90, 0x2D5F8AU},
    {"ZONE 2", 290, 70, 150, 100, 0x2E7D4FU},
    {"ZONE 3", 130, 220, 170, 90, 0x8A3D6EU},
};
static const char *const PART_NAMES[PART_COUNT] = {
    "ax < bx+bw", "ax+aw > bx", "ay < by+bh", "ay+ah > by"};
static const char *const PART_OPERATORS[PART_COUNT] = {"<", ">", "<",
                                                       ">"};

typedef struct {
    bento_game_t game;
    bento_game_box_t zones[ZONE_COUNT];
    bento_game_box_t agv;
    bento_game_text_t hud_a;
    bento_game_text_t hud_name;
    bento_game_text_t hud_b;
    bento_game_box_t lamps[PART_COUNT];
    bento_game_text_t lines[PART_COUNT];
    bento_game_box_t hit_lamp;
    bento_game_text_t hud_hit;
    char shown_lines[PART_COUNT][TEXT_CHARS];
    int32_t shown_parts[PART_COUNT]; /* 0, 1, or PART_NEVER */
    int32_t shown_near;              /* PART_NEVER before the first */
    char shown_a[TEXT_CHARS];
    uint32_t shown_hits;
    bool hits_known;
} aabb_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern aabb_t g_aabb;
aabb_t g_aabb;

static bento_real_t clamp_real(bento_real_t value, bento_real_t low,
                               bento_real_t high)
{
    const bento_real_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

/* สี่ข้อของการชนแบบกล่อง เรียงเหมือน game.hit() ทุกตัวอักษร */
static void aabb_parts(const bento_game_box_t *a,
                       const bento_game_box_t *b,
                       bool parts[PART_COUNT])
{
    parts[PART_A_LEFT] = a->x < (b->x + b->w);
    parts[PART_A_RIGHT] = (a->x + a->w) > b->x;
    parts[PART_A_TOP] = a->y < (b->y + b->h);
    parts[PART_A_BOTTOM] = (a->y + a->h) > b->y;
}

static bool aabb_all(const bento_game_box_t *a,
                     const bento_game_box_t *b)
{
    bool parts[PART_COUNT];

    aabb_parts(a, b, parts);
    return parts[PART_A_LEFT] && parts[PART_A_RIGHT] &&
           parts[PART_A_TOP] && parts[PART_A_BOTTOM];
}

/* ตัวเลขซ้าย-ขวาของแต่ละข้อ เอาไว้โชว์ว่าเทียบอะไรกับอะไร */
static void part_numbers(const bento_game_box_t *a,
                         const bento_game_box_t *b,
                         int32_t numbers[PART_COUNT][2])
{
    numbers[PART_A_LEFT][0] = (int32_t)a->x;
    numbers[PART_A_LEFT][1] = (int32_t)(b->x + b->w);
    numbers[PART_A_RIGHT][0] = (int32_t)(a->x + a->w);
    numbers[PART_A_RIGHT][1] = (int32_t)b->x;
    numbers[PART_A_TOP][0] = (int32_t)a->y;
    numbers[PART_A_TOP][1] = (int32_t)(b->y + b->h);
    numbers[PART_A_BOTTOM][0] = (int32_t)(a->y + a->h);
    numbers[PART_A_BOTTOM][1] = (int32_t)b->y;
}

/* โซนที่จุดกลางอยู่ใกล้จุดกลางของ A ที่สุด (ระยะยกกำลังสอง ไม่ถอดราก) */
static int32_t nearest(const bento_game_box_t *a,
                       const bento_game_box_t boxes[ZONE_COUNT])
{
    bento_real_t a_x = (bento_real_t)0;
    bento_real_t a_y = (bento_real_t)0;
    bento_real_t best_distance = (bento_real_t)0;
    int32_t best = 0;
    int32_t i;

    bento_game_box_center(a, &a_x, &a_y);
    for (i = 0; i < ZONE_COUNT; i++) {
        bento_real_t b_x = (bento_real_t)0;
        bento_real_t b_y = (bento_real_t)0;
        bento_real_t distance;

        bento_game_box_center(&boxes[i], &b_x, &b_y);
        distance = (a_x - b_x) * (a_x - b_x);
        distance += (a_y - b_y) * (a_y - b_y);
        if ((i == 0) || (distance < best_distance)) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
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

static bento_status_t add_text(aabb_t *aabb, bento_game_text_t *text,
                               const char *words, int32_t x, int32_t y,
                               uint32_t color)
{
    bento_game_text_init(text);
    return bento_game_text_create(&aabb->game, text, words, x, y,
                                  color);
}

/* The field, each zone with its name, then box A. */
static bento_status_t add_field(aabb_t *aabb)
{
    bento_game_box_t field;
    int32_t i;
    bento_status_t status = bento_game_box_create_styled(
        &aabb->game, &field, (bento_real_t)FIELD_X,
        (bento_real_t)FIELD_Y, (bento_real_t)FIELD_W,
        (bento_real_t)FIELD_H, COLOR_FIELD, COLOR_FIELD_EDGE, 1, 0);

    for (i = 0; (status == BENTO_OK) && (i < ZONE_COUNT); i++) {
        const zone_t *zone = &ZONES[i];
        bento_game_text_t label;

        status = bento_game_box_create_styled(
            &aabb->game, &aabb->zones[i], (bento_real_t)zone->x,
            (bento_real_t)zone->y, (bento_real_t)zone->w,
            (bento_real_t)zone->h, zone->color,
            (int32_t)BENTO_GAME_WHITE, ZONE_RADIUS, 0);
        if (status == BENTO_OK) {
            status = add_text(
                aabb, &label, zone->name, zone->x + ZONE_LABEL_DX,
                zone->y + ZONE_LABEL_DY, BENTO_GAME_WHITE);
        }
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            &aabb->game, &aabb->agv, (bento_real_t)A_START_X,
            (bento_real_t)A_START_Y, (bento_real_t)A_W,
            (bento_real_t)A_H, BENTO_GAME_YELLOW,
            (int32_t)BENTO_GAME_BLACK, A_RADIUS, A_BORDER_W);
    }
    return status;
}

/* The four lamps and lines, one row per test. */
static bento_status_t add_part_rows(aabb_t *aabb)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < PART_COUNT); i++) {
        status = bento_game_box_create_styled(
            &aabb->game, &aabb->lamps[i], (bento_real_t)HUD_X,
            (bento_real_t)(LAMP_Y + (i * ROW_GAP)),
            (bento_real_t)LAMP_PX, (bento_real_t)LAMP_PX,
            COLOR_LAMP_OFF, BENTO_GAME_BORDER_NONE, LAMP_RADIUS, 0);
        if (status == BENTO_OK) {
            status =
                add_text(aabb, &aabb->lines[i], "", HUD_X + LINE_DX,
                         LINE_Y + (i * ROW_GAP), BENTO_GAME_WHITE);
        }
    }
    return status;
}

static bento_status_t add_panel(aabb_t *aabb)
{
    bento_game_text_t note;
    bento_game_text_t keys_hint;
    bento_status_t status = add_text(aabb, &aabb->hud_a, "", HUD_X,
                                     HUD_A_Y, BENTO_GAME_YELLOW);

    if (status == BENTO_OK) {
        status = add_text(aabb, &aabb->hud_name, "", HUD_X, HUD_NAME_Y,
                          BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(aabb, &aabb->hud_b, "", HUD_X, HUD_B_Y,
                          BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_part_rows(aabb);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            &aabb->game, &aabb->hit_lamp, (bento_real_t)HUD_X,
            (bento_real_t)HIT_LAMP_Y, (bento_real_t)HIT_LAMP_PX,
            (bento_real_t)HIT_LAMP_PX, COLOR_LAMP_OFF,
            BENTO_GAME_BORDER_NONE, HIT_LAMP_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = add_text(aabb, &aabb->hud_hit, "", HUD_X + HUD_HIT_DX,
                          HUD_HIT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(aabb, &note, "ผิดข้อเดียว = ไม่ชน", HUD_X, NOTE_Y,
                          COLOR_NOTE);
    }
    if (status == BENTO_OK) {
        status = add_text(aabb, &keys_hint, "จอย = ขับกล่อง A", HUD_X,
                          KEYS_Y, BENTO_GAME_GB_LIGHT);
    }
    return status;
}

/* รับปุ่ม (วัด): ขับกล่อง A อยู่ในสนาม */
static bento_status_t drive_agv(aabb_t *aabb,
                                const bento_game_keys_t *keys)
{
    const int32_t step_x =
        (keys->right ? STEP_PX : 0) - (keys->left ? STEP_PX : 0);
    const int32_t step_y =
        (keys->down ? STEP_PX : 0) - (keys->up ? STEP_PX : 0);
    bento_status_t status = BENTO_OK;

    if ((step_x != 0) || (step_y != 0)) {
        const bento_real_t x = clamp_real(
            aabb->agv.x + (bento_real_t)step_x, (bento_real_t)FIELD_X,
            (bento_real_t)((FIELD_X + FIELD_W) - A_W));
        const bento_real_t y = clamp_real(
            aabb->agv.y + (bento_real_t)step_y, (bento_real_t)FIELD_Y,
            (bento_real_t)((FIELD_Y + FIELD_H) - A_H));

        status = bento_game_box_move_to(&aabb->game, &aabb->agv, x, y);
    }
    return status;
}

/* A label changes only when its text changes. */
static bento_status_t set_if_changed(aabb_t *aabb,
                                     const bento_game_text_t *label,
                                     char shown[TEXT_CHARS],
                                     const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(text, shown) != 0) {
        status = bento_game_text_set(&aabb->game, label, text);
        (void)snprintf(shown, TEXT_CHARS, "%s", text);
    }
    return status;
}

static bento_status_t show_nearest(aabb_t *aabb, int32_t near)
{
    const bento_game_box_t *b = &aabb->zones[near];
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (near != aabb->shown_near) {
        (void)snprintf(text, sizeof(text), "B = %s (ใกล้สุด)",
                       ZONES[near].name);
        status =
            bento_game_text_set(&aabb->game, &aabb->hud_name, text);
        (void)snprintf(text, sizeof(text), "B x=%d y=%d w=%d h=%d",
                       (int)b->x, (int)b->y, (int)b->w, (int)b->h);
        if (status == BENTO_OK) {
            status =
                bento_game_text_set(&aabb->game, &aabb->hud_b, text);
        }
        aabb->shown_near = near;
    }
    return status;
}

static bento_status_t show_parts(aabb_t *aabb,
                                 const bool parts[PART_COUNT],
                                 const int32_t numbers[PART_COUNT][2])
{
    char text[TEXT_CHARS];
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < PART_COUNT); i++) {
        const int32_t part = parts[i] ? 1 : 0;

        (void)snprintf(text, sizeof(text),
                       "%s  %" PRId32 "%s%" PRId32 "  %s",
                       PART_NAMES[i], numbers[i][0], PART_OPERATORS[i],
                       numbers[i][1], parts[i] ? "True" : "False");
        status = set_if_changed(aabb, &aabb->lines[i],
                                aabb->shown_lines[i], text);
        if ((status == BENTO_OK) && (part != aabb->shown_parts[i])) {
            status = bento_game_box_set_color(
                &aabb->game, &aabb->lamps[i],
                parts[i] ? BENTO_GAME_GREEN : COLOR_LAMP_OFF);
            aabb->shown_parts[i] = part;
        }
    }
    return status;
}

/* Recolour each zone whose hit changed. */
static bento_status_t recolor_zones(aabb_t *aabb, uint32_t hits)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < ZONE_COUNT); i++) {
        const uint32_t bit = 1UL << (uint32_t)i;
        const bool changed = (hits & bit) != (aabb->shown_hits & bit);

        if (!aabb->hits_known || changed) {
            status = bento_game_box_set_color(
                &aabb->game, &aabb->zones[i],
                ((hits & bit) != 0U) ? BENTO_GAME_RED : ZONES[i].color);
        }
    }
    return status;
}

/* The first zone hit, or PART_NEVER (Python's True not in hits). */
static int32_t first_hit(uint32_t hits)
{
    int32_t first = PART_NEVER;
    int32_t i;

    for (i = 0; (i < ZONE_COUNT) && (first == PART_NEVER); i++) {
        if ((hits & (1UL << (uint32_t)i)) != 0U) {
            first = i;
        }
    }
    return first;
}

/* The HIT line and lamp; the first hit names itself. */
static bento_status_t show_hit_line(aabb_t *aabb, int32_t first)
{
    char text[TEXT_CHARS];
    bento_status_t status;

    if (first != PART_NEVER) {
        (void)snprintf(text, sizeof(text), "HIT = True  %s",
                       ZONES[first].name);
        status = bento_game_text_set(&aabb->game, &aabb->hud_hit, text);
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                &aabb->game, &aabb->hit_lamp, BENTO_GAME_RED);
        }
        bento_game_sfx(&aabb->game, "deny");
    } else {
        status = bento_game_text_set(&aabb->game, &aabb->hud_hit,
                                     "HIT = False");
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                &aabb->game, &aabb->hit_lamp, COLOR_LAMP_OFF);
        }
    }
    return status;
}

static bento_status_t show_hits(aabb_t *aabb, uint32_t hits)
{
    bento_status_t status = recolor_zones(aabb, hits);

    if (status == BENTO_OK) {
        status = show_hit_line(aabb, first_hit(hits));
    }
    aabb->shown_hits = hits;
    aabb->hits_known = true;
    return status;
}

/* คิด (ตัดสิน) แล้ว ออก (โชว์): อัปเดตเฉพาะที่เปลี่ยน */
static bento_status_t judge_and_show(aabb_t *aabb)
{
    const int32_t near = nearest(&aabb->agv, aabb->zones);
    bool parts[PART_COUNT];
    int32_t numbers[PART_COUNT][2];
    uint32_t hits = 0U;
    char text[TEXT_CHARS];
    int32_t i;
    bento_status_t status;

    aabb_parts(&aabb->agv, &aabb->zones[near], parts);
    part_numbers(&aabb->agv, &aabb->zones[near], numbers);
    for (i = 0; i < ZONE_COUNT; i++) {
        if (aabb_all(&aabb->agv, &aabb->zones[i])) {
            hits |= 1UL << (uint32_t)i;
        }
    }
    (void)snprintf(text, sizeof(text), "A x=%d y=%d w=%d h=%d",
                   (int)aabb->agv.x, (int)aabb->agv.y, A_W, A_H);
    status = set_if_changed(aabb, &aabb->hud_a, aabb->shown_a, text);
    if (status == BENTO_OK) {
        status = show_nearest(aabb, near);
    }
    if (status == BENTO_OK) {
        status = show_parts(aabb, parts, numbers);
    }
    if ((status == BENTO_OK) &&
        (!aabb->hits_known || (hits != aabb->shown_hits))) {
        status = show_hits(aabb, hits);
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    aabb_t *aabb = &g_aabb;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;        /* the state is g_aabb */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&aabb->game, &keys);
    if (status == BENTO_OK) {
        status = drive_agv(aabb, &keys);
    }
    if (status == BENTO_OK) {
        status = judge_and_show(aabb);
    }
    return status;
}

static void reset_state(aabb_t *aabb)
{
    int32_t i;

    (void)memset(aabb, 0, sizeof(*aabb));
    bento_game_init(&aabb->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    for (i = 0; i < ZONE_COUNT; i++) {
        bento_game_box_init(&aabb->zones[i]);
    }
    bento_game_box_init(&aabb->agv);
    for (i = 0; i < PART_COUNT; i++) {
        bento_game_box_init(&aabb->lamps[i]);
        aabb->shown_parts[i] = PART_NEVER;
    }
    bento_game_box_init(&aabb->hit_lamp);
    aabb->shown_near = PART_NEVER;
    aabb->hits_known = false;
}

int main(void)
{
    aabb_t *aabb = &g_aabb;
    bool quit = false;
    bento_status_t status;

    reset_state(aabb);
    status = bento_game_title(&aabb->game, "AABB", NULL, &quit);
    report_joystick(&aabb->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_field(aabb);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_panel(aabb);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&aabb->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
