/*
 * s13_12_sprite_gallery.c -- คาบ 13 (คาบพิเศษ): ห้องแสดงภาพ 20 แบบ
 *
 * ภาพจากเกมตัวเต็ม 20 แบบ แต่สร้างภาพจริงแค่ 5 ชิ้น แล้วเปลี่ยน
 * หน้าตาด้วย .frame(ชื่อ) ช่องที่ j โชว์ภาพลำดับ (index + j - 2) % 20
 * วนครบรอบไม่มีหลุดขอบ ภาพไม่รู้ขนาดตัวเอง เราจดขนาดไว้ในตาราง
 * แล้วจัดกลาง ภาพกลางที่มีหลายเฟรมสลับเฟรมทุก 8 เฟรม กด Start
 * แล้วจอยซ้าย/ขวาเลื่อนดูภาพทีละแบบ กด A ฟังเสียงที่คู่กับภาพนั้น
 *
 * A gallery of the 20 pictures of the full games, made from only 5
 * real sprites whose look changes with .frame(name). Slot j shows
 * picture (index + j - 2) % 20, wrapping round. A picture does not
 * know its size, so the table keeps it and the program centres each
 * one. A middle picture with several frames swaps frame every 8
 * frames. Press Start, then left and right step through the pictures;
 * A plays the sound that goes with the picture in its game. The brain
 * is checked by a self test before the screen.
 *
 * Twin of s13_12_sprite_gallery.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_12_sprite_gallery.py).
 * Style: CLEAN_C.md. What C does differently:
 * - SPRITES is a table of structs; GAME_COLOR's keys are game_kind_t;
 * - anim_frames() fills a frames_t (up to 4 names, from static tables)
 *   instead of building a tuple and the "boom%d" names;
 * - the st dict is members of one struct at file scope;
 * - slot_index uses floor modulo, as Python's % does;
 * - each self test case compares the text Python's print shows for
 *   the answer ("(365, 146)", "('ball',)"), which is also what a
 *   failure prints;
 * - raise SystemExit after a failed self test, title()'s Back and Stop
 *   end the program with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The gallery (แก้ได้). */
enum {
    SPRITE_COUNT = 20,
    SLOTS = 5,
    CENTRE_SLOT = 2, /* ช่องกลาง = ช่องที่ 2 */
    ANIM_EVERY = 8,  /* สลับเฟรมทุก 8 เฟรม (ราว 4 ครั้งต่อวินาที) */
    SLOT_Y = 160,
    HEAD_COUNT = 4,
    FPS = 30,
    TEXT_CHARS = 96
};

/* The frames round the slots and the labels. */
enum {
    CENTRE_HALF_W = 70,
    CENTRE_HALF_H = 60,
    CENTRE_W = 140,
    CENTRE_H = 120,
    CENTRE_RADIUS = 10,
    CENTRE_BORDER_W = 3,
    SIDE_HALF = 52,
    SIDE_SIZE = 104,
    SIDE_RADIUS = 8,
    TITLE_X = 12,
    TOP_Y = 8,
    INDEX_X = 690,
    TAG_X = 240,
    TAG_Y = 266,
    TAG_W = 130,
    TAG_H = 26,
    TAG_RADIUS = 6,
    LABEL_X = 250,
    NAME_Y = 238,
    GAME_Y = 269,
    SIZE_Y = 300,
    FRAME_Y = 330,
    HINT_X = 470
};

/* The self test's inputs, as the .py writes them. */
enum {
    CASE_SLOT_FIRST = 0,
    CASE_SLOT_LAST,
    CASE_CENTRED,
    CASE_BOOM_FRAMES,
    CASE_BALL_FRAMES,
    CASE_FRAME_AT
};

enum {
    TEST_LAST_INDEX = 19,
    TEST_LAST_SLOT = 4,
    TEST_CENTRE_X = 396,
    TEST_CENTRE_Y = 160,
    TEST_SHIP_W = 62,
    TEST_SHIP_H = 28,
    TEST_TICK = 17,
    TEST_EVERY = 8
};

enum {
    COLOR_CENTRE = 0x1E2A36,
    COLOR_SIDE = 0x181C24,
    COLOR_SIDE_BORDER = 0x445566,
    COLOR_HINT = 0x8899AA
};

typedef enum {
    GAME_SHOOTER = 0,
    GAME_PONG,
    GAME_FLAPPY,
    GAME_SNAKE,
    GAME_COUNT
} game_kind_t;

/* (ชื่อภาพ, เกม, กว้าง, สูง, เสียงที่คู่กัน) */
typedef struct {
    const char *name;
    game_kind_t game;
    int32_t width;
    int32_t height;
    const char *sound;
} sprite_row_t;

/* ขนาดจริงของภาพในเฟิร์มแวร์ หน่วย px */
static const sprite_row_t SPRITES[SPRITE_COUNT] = {
    {"ship", GAME_SHOOTER, 62, 28, "fire"},
    {"enemy", GAME_SHOOTER, 16, 14, "hit"},
    {"enemy2", GAME_SHOOTER, 21, 10, "hit"},
    {"enemy3", GAME_SHOOTER, 18, 14, "hit"},
    {"boom1", GAME_SHOOTER, 29, 24, "explode"},
    {"boom2", GAME_SHOOTER, 29, 24, "explode"},
    {"boom3", GAME_SHOOTER, 29, 24, "explode"},
    {"boom4", GAME_SHOOTER, 29, 24, "explode"},
    {"boom5", GAME_SHOOTER, 29, 24, "explode"},
    {"boom6", GAME_SHOOTER, 29, 24, "explode"},
    {"ball", GAME_PONG, 14, 14, "wall"},
    {"paddle", GAME_PONG, 14, 90, "paddle"},
    {"bird", GAME_FLAPPY, 44, 29, "flap"},
    {"pipe_cap", GAME_FLAPPY, 82, 18, "point"},
    {"snake_head_r", GAME_SNAKE, 16, 16, "turn"},
    {"snake_head_d", GAME_SNAKE, 16, 16, "turn"},
    {"snake_head_l", GAME_SNAKE, 16, 16, "turn"},
    {"snake_head_u", GAME_SNAKE, 16, 16, "turn"},
    {"snake_body", GAME_SNAKE, 16, 16, "die"},
    {"snake_food", GAME_SNAKE, 16, 16, "eat"}};

static const uint32_t GAME_COLORS[GAME_COUNT] = {
    0x5A3FB0U, 0x2F6F9FU, 0x2E7D32U, BENTO_GAME_GB_DARK};
/* จุดกลางของช่อง 5 ช่อง */
static const int32_t SLOT_X[SLOTS] = {96, 246, 396, 546, 696};

/* The frames of one picture: Python's tuple of names. */
typedef struct {
    const char *names[HEAD_COUNT];
    uint32_t count;
} frames_t;

typedef struct {
    bento_game_t game;
    bento_game_text_t hud_index;
    bento_game_sprite_t slots[SLOTS];
    bento_game_box_t tag;
    bento_game_text_t hud_name;
    bento_game_text_t hud_game;
    bento_game_text_t hud_size;
    bento_game_text_t hud_frame;
    int32_t index;
    int32_t tick;
    const char *frame;
} gallery_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern gallery_t g_gallery;
gallery_t g_gallery;

/* value % SPRITE_COUNT with Python's floor modulo: never below 0 */
static int32_t wrap_index(int32_t value)
{
    const int32_t remainder = value % SPRITE_COUNT;

    return (remainder < 0) ? (remainder + SPRITE_COUNT) : remainder;
}

/* วนรอบ: ซ้ายของภาพแรกคือภาพสุดท้าย */
static int32_t slot_index(int32_t index, int32_t slot)
{
    return wrap_index((index + slot) - CENTRE_SLOT);
}

/* ภาพวางจากมุมซ้ายบน จึงต้องถอยครึ่งตัว */
static void centred(int32_t centre_x, int32_t centre_y, int32_t width,
                    int32_t height, int32_t *out_x, int32_t *out_y)
{
    *out_x = centre_x - (width / 2);
    *out_y = centre_y - (height / 2);
}

/* The snake's heads as a frames_t. */
static frames_t head_frames(void)
{
    static const char *const heads[HEAD_COUNT] = {
        "snake_head_r", "snake_head_d", "snake_head_l", "snake_head_u"};
    frames_t frames;
    uint32_t i;

    for (i = 0U; i < (uint32_t)HEAD_COUNT; i++) {
        frames.names[i] = heads[i];
    }
    frames.count = (uint32_t)HEAD_COUNT;
    return frames;
}

/* ระเบิดเป็นคู่ (1,2) (3,4) (5,6) · หัวงูมี 4 ทิศ
 * · ภาพอื่นมีเฟรมเดียว */
static frames_t anim_frames(const char *name)
{
    static const char *const booms[] = {"boom1", "boom2", "boom3",
                                        "boom4", "boom5", "boom6"};
    static const char boom_prefix[] = "boom";
    static const char head_prefix[] = "snake_head";
    const size_t boom_length = sizeof(boom_prefix) - 1U;
    const size_t head_length = sizeof(head_prefix) - 1U;
    frames_t frames;
    uint32_t i;

    for (i = 0U; i < (uint32_t)HEAD_COUNT; i++) {
        frames.names[i] = NULL;
    }
    frames.names[0] = name;
    frames.count = 1U;
    if (strncmp(name, boom_prefix, boom_length) == 0) {
        const int32_t kind = (int32_t)name[boom_length] - (int32_t)'0';
        const int32_t first = ((kind % 2) == 1) ? kind : (kind - 1);

        frames.names[0] = booms[first - 1];
        frames.names[1] = booms[first];
        frames.count = 2U;
    } else if (strncmp(name, head_prefix, head_length) == 0) {
        frames = head_frames();
    } else {
        /* one frame: the picture itself */
    }
    return frames;
}

static const char *frame_at(const frames_t *frames, int32_t tick,
                            int32_t every)
{
    const int32_t step = tick / every;

    return frames->names[(uint32_t)step % frames->count];
}

/* Where the name sits in its frames (Python's tuple.index). */
static int32_t frame_index(const frames_t *frames, const char *name)
{
    int32_t found = 0;
    uint32_t i;

    for (i = 0U; i < frames->count; i++) {
        if (strcmp(frames->names[i], name) == 0) {
            found = (int32_t)i;
            break;
        }
    }
    return found;
}

/* The tuple as print() shows it: ('boom3', 'boom4') or ('ball',). */
static void format_frames(char *text, size_t size,
                          const frames_t *frames)
{
    size_t used = 1U;
    uint32_t i;

    (void)snprintf(text, size, "(");
    for (i = 0U; (i < frames->count) && (used < size); i++) {
        (void)snprintf(&text[used], size - used, "%s'%s'",
                       (i > 0U) ? ", " : "", frames->names[i]);
        used = strlen(text);
    }
    if (used < size) {
        (void)snprintf(&text[used], size - used, "%s)",
                       (frames->count == 1U) ? "," : "");
    }
}

/* The answer of self test case `which`, as print() shows it. */
static void case_text(int32_t which, char *text, size_t size)
{
    frames_t frames;
    int32_t x = 0;
    int32_t y = 0;

    switch (which) {
    case CASE_SLOT_FIRST:
        (void)snprintf(text, size, "%" PRId32, slot_index(0, 0));
        break;
    case CASE_SLOT_LAST:
        (void)snprintf(text, size, "%" PRId32,
                       slot_index(TEST_LAST_INDEX, TEST_LAST_SLOT));
        break;
    case CASE_CENTRED:
        centred(TEST_CENTRE_X, TEST_CENTRE_Y, TEST_SHIP_W, TEST_SHIP_H,
                &x, &y);
        (void)snprintf(text, size, "(%" PRId32 ", %" PRId32 ")", x, y);
        break;
    case CASE_BOOM_FRAMES:
        frames = anim_frames("boom4");
        format_frames(text, size, &frames);
        break;
    case CASE_BALL_FRAMES:
        frames = anim_frames("ball");
        format_frames(text, size, &frames);
        break;
    default:
        frames = head_frames();
        (void)snprintf(text, size, "%s",
                       frame_at(&frames, TEST_TICK, TEST_EVERY));
        break;
    }
}

/* ตรวจสมองก่อนเปิดจอ */
static bool self_test(void)
{
    static const char *const names[] = {"slot_index",  "slot_index",
                                        "centred",     "anim_frames",
                                        "anim_frames", "frame_at"};
    static const char *const wants[] = {
        "18",        "1",           "(365, 146)", "('boom3', 'boom4')",
        "('ball',)", "snake_head_l"};
    const size_t name_count = sizeof(names) / sizeof(names[0]);
    const int32_t count = (int32_t)name_count;
    char got[TEXT_CHARS];
    bool passed = true;
    int32_t i;

    for (i = 0; passed && (i < count); i++) {
        case_text(i, got, sizeof(got));
        if (strcmp(got, wants[i]) != 0) {
            (void)printf("ยังไม่ผ่าน: %s ได้ %s ควรได้ %s\n", names[i], got,
                         wants[i]);
            passed = false;
        }
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของห้องภาพถูกทั้ง %" PRId32 " กรณี\n", count);
    }
    return passed;
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

/* The frames first, then the pictures, so the pictures are on top. */
static bento_status_t add_slot_frames(gallery_t *gallery)
{
    bento_status_t status = BENTO_OK;
    int32_t j;

    for (j = 0; (status == BENTO_OK) && (j < SLOTS); j++) {
        bento_game_box_t frame_box;

        bento_game_box_init(&frame_box);
        if (j == CENTRE_SLOT) {
            status = bento_game_box_create_styled(
                &gallery->game, &frame_box,
                (bento_real_t)(SLOT_X[j] - CENTRE_HALF_W),
                (bento_real_t)SLOT_Y - (bento_real_t)CENTRE_HALF_H,
                (bento_real_t)CENTRE_W, (bento_real_t)CENTRE_H,
                COLOR_CENTRE, (int32_t)BENTO_GAME_YELLOW, CENTRE_RADIUS,
                CENTRE_BORDER_W);
        } else {
            status = bento_game_box_create_styled(
                &gallery->game, &frame_box,
                (bento_real_t)(SLOT_X[j] - SIDE_HALF),
                (bento_real_t)SLOT_Y - (bento_real_t)SIDE_HALF,
                (bento_real_t)SIDE_SIZE, (bento_real_t)SIDE_SIZE,
                COLOR_SIDE, COLOR_SIDE_BORDER, SIDE_RADIUS, 0);
        }
    }
    return status;
}

/* The labels under the gallery and the key hint. */
static bento_status_t add_labels(gallery_t *gallery)
{
    bento_game_t *game = &gallery->game;
    bento_game_text_t hint;
    bento_status_t status = BENTO_OK;

    bento_game_text_init(&hint);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &gallery->hud_name, "",
                                   LABEL_X, NAME_Y, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &gallery->hud_game, "",
                                   LABEL_X, GAME_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &gallery->hud_size, "",
                                   LABEL_X, SIZE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &gallery->hud_frame, "",
                                        LABEL_X, FRAME_Y,
                                        BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &hint, "ซ้าย/ขวา = เลือกภาพ   A = ฟังเสียง", HINT_X,
            GAME_Y, COLOR_HINT);
    }
    return status;
}

static bento_status_t build_screen(gallery_t *gallery)
{
    bento_game_t *game = &gallery->game;
    bento_game_text_t heading;
    bento_status_t status;
    int32_t j;

    bento_game_text_init(&heading);
    status =
        bento_game_text_create(game, &heading, "ภาพจากเกมตัวเต็ม 20 แบบ",
                               TITLE_X, TOP_Y, BENTO_GAME_CYAN);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &gallery->hud_index, "",
                                   INDEX_X, TOP_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_slot_frames(gallery);
    }
    for (j = 0; (status == BENTO_OK) && (j < SLOTS); j++) {
        status = bento_game_sprite_create(
            game, &gallery->slots[j], SPRITES[slot_index(0, j)].name,
            (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        /* ป้ายสีของเกม เปลี่ยนสีตามเกม */
        status = bento_game_box_create_styled(
            game, &gallery->tag, (bento_real_t)TAG_X,
            (bento_real_t)TAG_Y, (bento_real_t)TAG_W,
            (bento_real_t)TAG_H, GAME_COLORS[GAME_SHOOTER],
            BENTO_GAME_BORDER_NONE, TAG_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = add_labels(gallery);
    }
    return status;
}

static bento_status_t show_frame_name(gallery_t *gallery,
                                      const char *name)
{
    char text[TEXT_CHARS];

    (void)snprintf(text, sizeof(text), ".frame(\"%s\")", name);
    return bento_game_text_set(&gallery->game, &gallery->hud_frame,
                               text);
}

/* 5 ช่อง x (frame + move_to) = 10 ข้อความ */
static bento_status_t place_slots(gallery_t *gallery)
{
    bento_status_t status = BENTO_OK;
    int32_t j;

    for (j = 0; (status == BENTO_OK) && (j < SLOTS); j++) {
        const sprite_row_t *row =
            &SPRITES[slot_index(gallery->index, j)];
        int32_t x = 0;
        int32_t y = 0;

        centred(SLOT_X[j], SLOT_Y, row->width, row->height, &x, &y);
        status = bento_game_sprite_frame(&gallery->game,
                                         &gallery->slots[j], row->name);
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(
                &gallery->game, &gallery->slots[j], (bento_real_t)x,
                (bento_real_t)y);
        }
    }
    return status;
}

/* เรียกเฉพาะตอนเลือกภาพใหม่ แล้วนิ่งจนกดอีกครั้ง */
static bento_status_t show_gallery(gallery_t *gallery)
{
    static const char *const game_names[GAME_COUNT] = {
        "Shooter", "Pong", "Flappy", "Snake"};
    const sprite_row_t *row = &SPRITES[gallery->index];
    bento_game_t *game = &gallery->game;
    frames_t frames;
    char text[TEXT_CHARS];
    bento_status_t status = place_slots(gallery);

    frames = anim_frames(row->name);

    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "%" PRId32 " / %d",
                       gallery->index + 1, SPRITE_COUNT);
        status = bento_game_text_set(game, &gallery->hud_index, text);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_set(game, &gallery->hud_name, row->name);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "เกม %s",
                       game_names[row->game]);
        status = bento_game_text_set(game, &gallery->hud_game, text);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(game, &gallery->tag,
                                          GAME_COLORS[row->game]);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "%" PRId32 " x %" PRId32 " px   เสียง %s",
                       row->width, row->height, row->sound);
        status = bento_game_text_set(game, &gallery->hud_size, text);
    }
    /* เริ่มนับจากเฟรมของภาพที่เลือกพอดี */
    gallery->tick = frame_index(&frames, row->name) * ANIM_EVERY;
    gallery->frame = row->name;
    if (status == BENTO_OK) {
        status = show_frame_name(gallery, row->name);
    }
    return status;
}

/* ภาพกลางที่มีหลายเฟรม: เขียนจอเฉพาะตอนเฟรมเปลี่ยน */
static bento_status_t animate_centre(gallery_t *gallery)
{
    frames_t frames;
    const char *now;
    bento_status_t status = BENTO_OK;

    frames = anim_frames(SPRITES[gallery->index].name);

    gallery->tick += 1;
    now = frame_at(&frames, gallery->tick, ANIM_EVERY);
    if (strcmp(now, gallery->frame) != 0) {
        status = bento_game_sprite_frame(
            &gallery->game, &gallery->slots[CENTRE_SLOT], now);
        if (status == BENTO_OK) {
            status = show_frame_name(gallery, now);
        }
        gallery->frame = now;
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    /* the state is the file-scope g_gallery (no void * conversion) */
    gallery_t *gallery = &g_gallery;
    bento_game_t *game = &gallery->game;
    bento_game_keys_t keys;
    bento_status_t status;
    int32_t step = 0;

    (void)user;
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(game, &keys);
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_LEFT, &keys)) {
        step = -1;
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_RIGHT, &keys)) {
        step = 1;
    }
    if ((status == BENTO_OK) && (step != 0)) {
        gallery->index = wrap_index(gallery->index + step);
        bento_game_sfx(game, "move");
        status = show_gallery(gallery);
    }
    if (bento_game_pressed_once(game, BENTO_GAME_KEY_A, &keys)) {
        bento_game_sfx(game, SPRITES[gallery->index].sound);
    }
    if (status == BENTO_OK) {
        status = animate_centre(gallery);
    }
    return status;
}

static void reset_state(gallery_t *gallery)
{
    int32_t j;

    (void)memset(gallery, 0, sizeof(*gallery));
    bento_game_init(&gallery->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    for (j = 0; j < SLOTS; j++) {
        bento_game_sprite_init(&gallery->slots[j]);
    }
    bento_game_box_init(&gallery->tag);
    bento_game_text_init(&gallery->hud_index);
    bento_game_text_init(&gallery->hud_name);
    bento_game_text_init(&gallery->hud_game);
    bento_game_text_init(&gallery->hud_size);
    bento_game_text_init(&gallery->hud_frame);
    gallery->frame = "";
}

int main(void)
{
    gallery_t *gallery = &g_gallery;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    reset_state(gallery);
    /* a failed self test is SystemExit: the program ends with 0 */
    if (!self_test()) {
        quit = true;
    }
    if (!quit) {
        status =
            bento_game_title(&gallery->game, "SPRITES 20", NULL, &quit);
        report_joystick(&gallery->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(gallery);
    }
    if ((status == BENTO_OK) && !quit) {
        status = show_gallery(gallery);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&gallery->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
