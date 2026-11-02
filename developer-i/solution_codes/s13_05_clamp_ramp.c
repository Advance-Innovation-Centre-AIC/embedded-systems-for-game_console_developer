/*
 * s13_05_clamp_ramp.c -- คาบ 13 การ์ด 4: จำกัดขอบ และเร่งแบบมีเพดาน
 *
 * กล่องฟ้าวิ่งในกรอบสนามด้วยจอย กดค้างยิ่งนานยิ่งเร็ว
 * speed = min(BASE + hold, MAX) ปล่อยจอยแล้ว hold กลับเป็น 0
 * ทุกเฟรมจำกัดขอบทั้งสองแกน x = max(ซ้ายสุด, min(ขวาสุด, x))
 * กล่องจึงไม่มีวันหลุดกรอบ ชนขอบด้านไหน ขอบนั้นกับตัวกล่องเป็น
 * สีส้มพร้อมเสียง A = สลับเพดาน MAX ระหว่าง 30 กับ 10 แถบความเร็ว
 * 10 ก้อนไล่ตามค่าจริงเฟรมละไม่เกิน 3 ก้อน ขีดแดงบอกเพดาน
 *
 * A cyan box runs inside a framed field with the joystick: the longer
 * a direction is held, the faster it goes, speed = min(BASE + hold,
 * MAX), and hold returns to 0 on release. Every frame clamps both
 * axes, x = max(left, min(right, x)), so the box never leaves the
 * frame; touching an edge turns that edge and the box orange with a
 * sound. A swaps the ceiling MAX between 30 and 10. A 10-block speed
 * bar follows the speed by at most 3 blocks a frame; a red tick marks
 * the ceiling. The self test of ramp_speed and clamp runs first.
 *
 * Twin of s13_05_clamp_ramp.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_05_clamp_ramp.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the shown dict is fields of the state struct, texts in buffers;
 * - the touch tuple is an array of four bool, compared one by one;
 * - a failed self test (raise SystemExit) ends main with 0;
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

/* Speeds and the bar (แก้ได้). */
enum {
    BASE_SPEED = 6, /* เฟรมแรกที่แตะจอยได้ 6 + 1 = 7 */
    MAX_HIGH = 30,  /* A สลับเพดานความเร็วระหว่างสองค่านี้ */
    MAX_LOW = 10,
    MAX_CHOICES = 2,
    PER_BOX = 3,  /* แถบ 1 ก้อน = 3 พิกเซลต่อเฟรม */
    BAR_STEP = 3, /* แถบเปลี่ยนได้เฟรมละไม่เกิน 3 ก้อน */
    BAR_BOXES = 10,
    HERO_PX = 40,
    HERO_RADIUS = 8,
    FPS = 30
};

/* The field: a frame 6 px thick. */
enum {
    FIELD_X = 8,
    FIELD_Y = 94,
    FIELD_W = 776,
    FIELD_H = 272,
    EDGE_PX = 6,
    X_MIN = FIELD_X + EDGE_PX,
    X_MAX = FIELD_X + FIELD_W - EDGE_PX - HERO_PX,
    Y_MIN = FIELD_Y + EDGE_PX,
    Y_MAX = FIELD_Y + FIELD_H - EDGE_PX - HERO_PX,
    EDGE_COUNT = 4
};

/* The speed bar, its ceiling tick and the texts. */
enum {
    TRACK_X = 12,
    TRACK_Y = 60,
    TRACK_H = 26,
    BAR_X = 16,
    BAR_Y = 64,
    BAR_GAP_PX = 34,
    BAR_W = 30,
    BAR_H = 18,
    TRACK_PAD_PX = 4,
    CEILING_Y = 56,
    CEILING_W = 4,
    CEILING_H = 34,
    TEXT_X = 16,
    RULE_Y = 4,
    LIVE_Y = 32,
    MAX_TEXT_X = 372,
    INFO_Y = 62,
    PLACE_TEXT_X = 590,
    LIVE_CHARS = 96,
    PLACE_CHARS = 32,
    MAX_CHARS = 48
};

enum {
    COLOR_DIM = 0x334455,
    COLOR_TRACK = 0x16222E
};

typedef enum {
    EDGE_LEFT = 0,
    EDGE_RIGHT,
    EDGE_TOP,
    EDGE_BOTTOM
} edge_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} place_t;

typedef struct {
    bento_game_t game;
    bento_game_text_t live;
    bento_game_box_t bar[BAR_BOXES];
    bento_game_box_t ceiling;
    bento_game_text_t max_text;
    bento_game_text_t place_text;
    bento_game_box_t edges[EDGE_COUNT];
    bento_game_box_t hero;
    int32_t hero_x;
    int32_t hero_y;
    int32_t hold;
    int32_t max_choice;
    int32_t bar_on;
    char shown_live[LIVE_CHARS];
    char shown_place[PLACE_CHARS];
    bool shown_touch[EDGE_COUNT];
} clamp_ramp_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern clamp_ramp_t g_clamp;
clamp_ramp_t g_clamp;

/* การ์ด 4 ครึ่งแรก: ค่าที่เปลี่ยนได้ต้องมีขอบเสมอ */
static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    const int32_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

/* การ์ด 4 ครึ่งหลัง: กดค้างยิ่งนานยิ่งเร็ว แต่ไม่เกินเพดาน */
static int32_t ramp_speed(int32_t hold, int32_t max_speed)
{
    const int32_t speed = BASE_SPEED + hold;

    return (speed < max_speed) ? speed : max_speed;
}

/* แตะขอบไหนอยู่บ้าง: (ซ้าย, ขวา, บน, ล่าง) */
static void touching(int32_t x, int32_t y, bool touch[EDGE_COUNT])
{
    touch[EDGE_LEFT] = (x == X_MIN);
    touch[EDGE_RIGHT] = (x == X_MAX);
    touch[EDGE_TOP] = (y == Y_MIN);
    touch[EDGE_BOTTOM] = (y == Y_MAX);
}

static int32_t max_speed_of(int32_t choice)
{
    return (choice == 0) ? MAX_HIGH : MAX_LOW;
}

/* ตรวจสมองก่อนเปิดจอ */
static bool self_test(void)
{
    const int32_t long_hold = 14;
    const int32_t very_long_hold = 100;
    const int32_t ten = 10;
    const int32_t fifteen = 15;
    const int32_t minus_five = -5;
    const int32_t twenty = 20;
    const bool ok = (ramp_speed(0, MAX_HIGH) == BASE_SPEED) &&
                    (ramp_speed(long_hold, MAX_HIGH) == twenty) &&
                    (ramp_speed(very_long_hold, MAX_LOW) == MAX_LOW) &&
                    (clamp(minus_five, 0, ten) == 0) &&
                    (clamp(fifteen, 0, ten) == ten);

    (void)printf("%s\n", ok ? "ผ่าน! ramp_speed กับ clamp ถูกทุกกรณี"
                            : "ยังไม่ผ่าน: ตรวจ ramp_speed กับ clamp");
    return ok;
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

/* The track, then the ten bar blocks, hidden until the speed rises. */
static bento_status_t add_bar(clamp_ramp_t *clamp_ramp)
{
    bento_game_t *game = &clamp_ramp->game;
    bento_game_box_t track;
    int32_t i;
    bento_status_t status;

    bento_game_box_init(&track);
    status = bento_game_box_create_styled(
        game, &track, (bento_real_t)TRACK_X, (bento_real_t)TRACK_Y,
        (bento_real_t)((BAR_BOXES * BAR_GAP_PX) + TRACK_PAD_PX),
        (bento_real_t)TRACK_H, COLOR_TRACK, COLOR_DIM, 1, 0);
    for (i = 0; (status == BENTO_OK) && (i < BAR_BOXES); i++) {
        status = bento_game_box_create(
            game, &clamp_ramp->bar[i],
            (bento_real_t)(BAR_X + (i * BAR_GAP_PX)),
            (bento_real_t)BAR_Y, (bento_real_t)BAR_W,
            (bento_real_t)BAR_H, BENTO_GAME_GREEN);
    }
    for (i = 0; (status == BENTO_OK) && (i < BAR_BOXES); i++) {
        status = bento_game_box_hide(game, &clamp_ramp->bar[i]);
    }
    return status;
}

static bento_status_t add_edges(clamp_ramp_t *clamp_ramp)
{
    static const place_t places[EDGE_COUNT] = {
        {FIELD_X, FIELD_Y, EDGE_PX, FIELD_H},
        {FIELD_X + FIELD_W - EDGE_PX, FIELD_Y, EDGE_PX, FIELD_H},
        {FIELD_X, FIELD_Y, FIELD_W, EDGE_PX},
        {FIELD_X, FIELD_Y + FIELD_H - EDGE_PX, FIELD_W, EDGE_PX}};
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < EDGE_COUNT); i++) {
        status = bento_game_box_create(
            &clamp_ramp->game, &clamp_ramp->edges[i],
            (bento_real_t)places[i].x, (bento_real_t)places[i].y,
            (bento_real_t)places[i].w, (bento_real_t)places[i].h,
            COLOR_DIM);
    }
    return status;
}

static bento_status_t add_texts(clamp_ramp_t *clamp_ramp)
{
    bento_game_t *game = &clamp_ramp->game;
    bento_game_text_t rule;
    bento_status_t status;

    bento_game_text_init(&rule);
    status = bento_game_text_create(
        game, &rule,
        "speed = min(BASE + hold, MAX)       x = max(ซ้าย, min(ขวา, x))",
        TEXT_X, RULE_Y, BENTO_GAME_YELLOW);
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &clamp_ramp->live,
                                        "กดจอยค้าง ดูความเร็วไต่ขึ้น", TEXT_X,
                                        LIVE_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t build_screen(clamp_ramp_t *clamp_ramp)
{
    bento_game_t *game = &clamp_ramp->game;
    bento_status_t status = add_texts(clamp_ramp);

    if (status == BENTO_OK) {
        status = add_bar(clamp_ramp);
    }
    if (status == BENTO_OK) {
        /* the red tick is the ceiling MAX */
        status = bento_game_box_create(
            game, &clamp_ramp->ceiling, (bento_real_t)0,
            (bento_real_t)CEILING_Y, (bento_real_t)CEILING_W,
            (bento_real_t)CEILING_H, BENTO_GAME_RED);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &clamp_ramp->max_text, "",
                                   MAX_TEXT_X, INFO_Y, BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &clamp_ramp->place_text,
                                        "", PLACE_TEXT_X, INFO_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_edges(clamp_ramp);
    }
    if (status == BENTO_OK) {
        clamp_ramp->hero_x = (X_MIN + X_MAX) / 2;
        clamp_ramp->hero_y = (Y_MIN + Y_MAX) / 2;
        status = bento_game_box_create_styled(
            game, &clamp_ramp->hero, (bento_real_t)clamp_ramp->hero_x,
            (bento_real_t)clamp_ramp->hero_y, (bento_real_t)HERO_PX,
            (bento_real_t)HERO_PX, BENTO_GAME_CYAN,
            BENTO_GAME_BORDER_NONE, HERO_RADIUS, 0);
    }
    return status;
}

static bento_status_t set_max(clamp_ramp_t *clamp_ramp)
{
    const int32_t top = max_speed_of(clamp_ramp->max_choice);
    /* floor division on ints, as Python's top * 34 // PER_BOX */
    const int32_t tick_x =
        (BAR_X + ((top * BAR_GAP_PX) / PER_BOX)) - CEILING_W;
    char text[MAX_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "MAX %" PRId32 "  (A = สลับ)",
                   top);
    status = bento_game_text_set(&clamp_ramp->game,
                                 &clamp_ramp->max_text, text);
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &clamp_ramp->game, &clamp_ramp->ceiling,
            (bento_real_t)tick_x, clamp_ramp->ceiling.y);
    }
    return status;
}

/* ป้ายเปลี่ยนเมื่อข้อความเปลี่ยนเท่านั้น */
static bento_status_t show(bento_game_t *game,
                           const bento_game_text_t *widget, char *shown,
                           uint32_t size, const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(text, shown) != 0) {
        status = bento_game_text_set(game, widget, text);
        (void)snprintf(shown, size, "%s", text);
    }
    return status;
}

static bool any_touch(const bool touch[EDGE_COUNT])
{
    int32_t i;
    bool found = false;

    for (i = 0; i < EDGE_COUNT; i++) {
        found = found || touch[i];
    }
    return found;
}

/* เสียงเฉพาะตอนเพิ่งแตะ ไม่ดังรัว */
static bento_status_t show_edge(clamp_ramp_t *clamp_ramp, int32_t edge,
                                bool touched)
{
    bento_game_t *game = &clamp_ramp->game;
    const bento_status_t status = bento_game_box_set_color(
        game, &clamp_ramp->edges[edge],
        touched ? BENTO_GAME_ORANGE : COLOR_DIM);

    if (touched) {
        bento_game_sfx(game, "wall");
    }
    return status;
}

/* ขอบที่เพิ่งแตะเป็นสีส้ม และตัวกล่องด้วยเมื่อแตะขอบใดขอบหนึ่ง */
static bento_status_t show_touch(clamp_ramp_t *clamp_ramp,
                                 const bool touch[EDGE_COUNT])
{
    bento_game_t *game = &clamp_ramp->game;
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < EDGE_COUNT); i++) {
        if (touch[i] != clamp_ramp->shown_touch[i]) {
            status = show_edge(clamp_ramp, i, touch[i]);
        }
    }
    if ((status == BENTO_OK) &&
        (any_touch(touch) != any_touch(clamp_ramp->shown_touch))) {
        status = bento_game_box_set_color(
            game, &clamp_ramp->hero,
            any_touch(touch) ? BENTO_GAME_ORANGE : BENTO_GAME_CYAN);
    }
    for (i = 0; i < EDGE_COUNT; i++) {
        clamp_ramp->shown_touch[i] = touch[i];
    }
    return status;
}

static bento_status_t show_texts(clamp_ramp_t *clamp_ramp, bool moving,
                                 int32_t top, int32_t speed)
{
    char live[LIVE_CHARS];
    char place[PLACE_CHARS];
    bento_status_t status;

    if (moving) {
        (void)snprintf(live, sizeof(live),
                       "hold %" PRId32 "  ->  speed = min(%d + %" PRId32
                       ", %" PRId32 ") = %" PRId32,
                       clamp_ramp->hold, BASE_SPEED, clamp_ramp->hold,
                       top, speed);
    } else {
        (void)snprintf(live, sizeof(live), "%s",
                       "ปล่อยจอย: hold = 0  speed = 0");
    }
    status = show(&clamp_ramp->game, &clamp_ramp->live,
                  clamp_ramp->shown_live,
                  (uint32_t)sizeof(clamp_ramp->shown_live), live);
    (void)snprintf(place, sizeof(place), "x %" PRId32 "  y %" PRId32,
                   clamp_ramp->hero_x, clamp_ramp->hero_y);
    if (status == BENTO_OK) {
        status = show(&clamp_ramp->game, &clamp_ramp->place_text,
                      clamp_ramp->shown_place,
                      (uint32_t)sizeof(clamp_ramp->shown_place), place);
    }
    return status;
}

/* ไล่แถบเข้าหาค่าจริงทีละก้อน */
static bento_status_t step_bar(clamp_ramp_t *clamp_ramp, int32_t speed)
{
    const int32_t blocks = speed / PER_BOX;
    const int32_t want = (blocks < BAR_BOXES) ? blocks : BAR_BOXES;
    int32_t step;
    bento_status_t status = BENTO_OK;

    for (step = 0; (status == BENTO_OK) && (step < BAR_STEP); step++) {
        if (clamp_ramp->bar_on < want) {
            status = bento_game_box_show(
                &clamp_ramp->game,
                &clamp_ramp->bar[clamp_ramp->bar_on]);
            clamp_ramp->bar_on += 1;
        } else if (clamp_ramp->bar_on > want) {
            clamp_ramp->bar_on -= 1;
            status = bento_game_box_hide(
                &clamp_ramp->game,
                &clamp_ramp->bar[clamp_ramp->bar_on]);
        } else {
            /* the bar already shows the speed */
        }
    }
    return status;
}

/* คิด: แบบเดียวกับ s03 แล้วจำกัดขอบเองทั้งสองแกน */
static bento_status_t move_hero(clamp_ramp_t *clamp_ramp,
                                const bento_game_keys_t *keys,
                                int32_t speed)
{
    const int32_t x =
        clamp(clamp_ramp->hero_x + (keys->right ? speed : 0) -
                  (keys->left ? speed : 0),
              X_MIN, X_MAX);
    const int32_t y =
        clamp(clamp_ramp->hero_y + (keys->down ? speed : 0) -
                  (keys->up ? speed : 0),
              Y_MIN, Y_MAX);
    bool touch[EDGE_COUNT];
    bento_status_t status = BENTO_OK;

    touching(x, y, touch);
    if ((x != clamp_ramp->hero_x) || (y != clamp_ramp->hero_y)) {
        clamp_ramp->hero_x = x;
        clamp_ramp->hero_y = y;
        status =
            bento_game_box_move_to(&clamp_ramp->game, &clamp_ramp->hero,
                                   (bento_real_t)x, (bento_real_t)y);
    }
    if (status == BENTO_OK) {
        status = show_touch(clamp_ramp, touch);
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    clamp_ramp_t *clamp_ramp = &g_clamp;
    bento_game_keys_t keys;
    bool moving;
    int32_t top;
    int32_t speed;
    bento_status_t status;

    (void)user;        /* the state is g_clamp */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&clamp_ramp->game, &keys);
    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&clamp_ramp->game, BENTO_GAME_KEY_A,
                                &keys)) {
        clamp_ramp->max_choice = 1 - clamp_ramp->max_choice;
        status = set_max(clamp_ramp);
        bento_game_sfx(&clamp_ramp->game, "select");
    }
    moving = keys.left || keys.right || keys.up || keys.down;
    clamp_ramp->hold = moving ? (clamp_ramp->hold + 1) : 0;
    top = max_speed_of(clamp_ramp->max_choice);
    speed = moving ? ramp_speed(clamp_ramp->hold, top) : 0;
    if (status == BENTO_OK) {
        status = move_hero(clamp_ramp, &keys, speed);
    }
    if (status == BENTO_OK) {
        status = show_texts(clamp_ramp, moving, top, speed);
    }
    if (status == BENTO_OK) {
        status = step_bar(clamp_ramp, speed);
    }
    return status;
}

static void reset_state(clamp_ramp_t *clamp_ramp)
{
    int32_t i;

    (void)memset(clamp_ramp, 0, sizeof(*clamp_ramp));
    bento_game_init(&clamp_ramp->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_text_init(&clamp_ramp->live);
    for (i = 0; i < BAR_BOXES; i++) {
        bento_game_box_init(&clamp_ramp->bar[i]);
    }
    bento_game_box_init(&clamp_ramp->ceiling);
    bento_game_text_init(&clamp_ramp->max_text);
    bento_game_text_init(&clamp_ramp->place_text);
    for (i = 0; i < EDGE_COUNT; i++) {
        bento_game_box_init(&clamp_ramp->edges[i]);
    }
    bento_game_box_init(&clamp_ramp->hero);
}

int main(void)
{
    clamp_ramp_t *clamp_ramp = &g_clamp;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    reset_state(clamp_ramp);
    if (!self_test()) {
        quit = true; /* SystemExit: the brain is wrong, no screen */
    }
    if (!quit) {
        status =
            bento_game_title(&clamp_ramp->game, "CLAMP", NULL, &quit);
        report_joystick(&clamp_ramp->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(clamp_ramp);
    }
    if ((status == BENTO_OK) && !quit) {
        status = set_max(clamp_ramp);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&clamp_ramp->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
