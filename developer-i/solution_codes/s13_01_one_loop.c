/*
 * s13_01_one_loop.c -- คาบ 13 ส่วนที่ 1: หัวใจดวงเดียว = เข้า -> คิด -> ออก ทุกเฟรม
 *
 * ทุกระบบเรียลไทม์คือวงวนเดียว: game.run() เรียก on_frame() 30 ครั้งต่อวินาที
 * เข้า (วัด) อ่านปุ่มและดูว่าปุ่มไหน "เพิ่งกด" · คิด (ตัดสิน) ตำแหน่งใหม่ =
 * เดิม + ก้าว แล้วจำกัดขอบ · ออก (ทำ + โชว์) ขยับยาน อัปเดตป้าย เสียงสั้น
 * กด Start · จอย 4 ทิศ = ขับยาน · A B X Y = ปุ่มโน้ต (โด ถึง โด สูง)
 * แถบบน 3 ช่องบอก เข้า / คิด / ออก กดค้างเสียงดังแค่ครั้งแรก (edge)
 *
 * One loop is the heart of every real-time system: input, think, output,
 * thirty times a second. Steer the ship with the joystick; every
 * direction and A, B, X, Y play one note of a C major scale, once per
 * press. The three panels on top show what was read (the held keys),
 * what was decided (the new position, and "at the edge" when it was
 * clamped) and what was done (the frame count).
 *
 * Twin of s13_01_one_loop.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s13_01_one_loop.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the NOTE dict and ORDER tuple are one table of keys, names and notes;
 *   - next_position() returns "at the edge" and hands the new position back
 *     through a pointer (Python returns a tuple); note None is a bool;
 *   - the shown dict is two text buffers, compared with strcmp;
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

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    FPS = 30, /* เฟรมต่อวินาที (game.run เรียก on_frame กี่ครั้งต่อวินาที) */
    STEP = 6, /* ยานขยับกี่พิกเซลต่อเฟรมเมื่อกดจอย */
    LAMP_FRAMES = 8, /* ไฟเสียงติดค้างกี่เฟรมหลังโน้ตดัง (นับเฟรม ไม่ใช้ sleep) */
    AREA_X = 8,      /* สนามที่ยานวิ่งได้ */
    AREA_Y = 74,
    AREA_W = 776,
    AREA_H = 290,
    SHIP_W = 62, /* ขนาดภาพ ship */
    SHIP_H = 28,
    X_MIN = AREA_X + 4,
    X_MAX = AREA_X + AREA_W - SHIP_W - 4,
    Y_MIN = AREA_Y + 4,
    Y_MAX = AREA_Y + AREA_H - SHIP_H - 4
};
/* ---- end of settings ---- */

enum {
    KEY_COUNT = 8,
    COLUMNS = 3,         /* เข้า · คิด · ออก */
    TONE_VELOCITY = 100, /* tone()'s default */
    TONE_MS = 120
};

/* The three panels on top, the sound lamp, and their texts. */
enum {
    COLUMN_W = 248,
    COLUMN_H = 64,
    COLUMN_Y = 2,
    COLUMN_BORDER_PX = 2,
    COLUMN_RADIUS = 6,
    HEAD_DX = 12,
    HEAD_Y = 8,
    VALUE_Y = 36,
    IN_X = 20,
    THINK_X = 284,
    OUT_X = 548,
    LAMP_X = 712,
    LAMP_Y = 12,
    LAMP_PX = 16,
    LAMP_RADIUS = 8,
    SOUND_X = 734,
    SOUND_Y = 8,
    FIELD_BORDER_PX = 1,
    TEXT_BYTES = 64 /* the longest text: two int32_t and Thai words */
};

/* Colours, 0xRRGGBB. */
enum {
    PANEL = 0x16222E,
    GRAY = 0x556070,
    FIELD = 0x0E1620,
    FIELD_EDGE = 0x334455
};

typedef struct {
    bento_game_key_t key;
    const char *name; /* the key's name in upper case */
    int32_t note;     /* MIDI note */
} note_key_t;

/* ORDER with its NOTE: โด เร มี ฟา ซอล ลา ที โด(สูง) */
static const note_key_t KEYS[KEY_COUNT] = {
    {BENTO_GAME_KEY_LEFT, "LEFT", 60},
    {BENTO_GAME_KEY_RIGHT, "RIGHT", 65},
    {BENTO_GAME_KEY_UP, "UP", 64},
    {BENTO_GAME_KEY_DOWN, "DOWN", 62},
    {BENTO_GAME_KEY_A, "A", 67},
    {BENTO_GAME_KEY_B, "B", 69},
    {BENTO_GAME_KEY_X, "X", 71},
    {BENTO_GAME_KEY_Y, "Y", 72}};

typedef struct {
    int32_t x;
    int32_t y;
} point_t;

typedef struct {
    bento_game_t game;
    bento_game_text_t in_text;
    bento_game_text_t think_text;
    bento_game_text_t out_text;
    bento_game_box_t lamp;
    bento_game_sprite_t ship;
    point_t ship_at;
    int32_t frame;
    int32_t lamp_left;
    char in_shown[TEXT_BYTES];
    char think_shown[TEXT_BYTES];
} one_loop_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern one_loop_t g_loop;
one_loop_t g_loop;

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
static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    const int32_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

/* เข้า: แปลงปุ่มที่กดอยู่ให้เป็นข้อความ เช่น "LEFT UP A" (ไม่กดเลย = "-") */
static void held_names(const bento_game_keys_t *keys, char *text,
                       uint32_t capacity)
{
    uint32_t used = 0U;
    uint32_t i;

    text[0] = '\0';
    for (i = 0U; i < (uint32_t)KEY_COUNT; i++) {
        if (bento_game_is_key_down(keys, KEYS[i].key)) {
            (void)snprintf(&text[used], capacity - used, "%s%s",
                           (used == 0U) ? "" : " ", KEYS[i].name);
            used = (uint32_t)strlen(text);
        }
    }
    if (used == 0U) {
        (void)snprintf(text, capacity, "-");
    }
}

/* คิด: ก้าวตามจอย แล้วจำกัดขอบ; answers "ชนขอบไหม" */
static bool next_position(point_t now, const bento_game_keys_t *keys,
                          point_t *next)
{
    const int32_t want_x =
        now.x + (keys->right ? STEP : 0) - (keys->left ? STEP : 0);
    const int32_t want_y =
        now.y + (keys->down ? STEP : 0) - (keys->up ? STEP : 0);

    next->x = clamp(want_x, X_MIN, X_MAX);
    next->y = clamp(want_y, Y_MIN, Y_MAX);
    return (next->x != want_x) || (next->y != want_y);
}

/* อัปเดตป้ายเฉพาะตอนข้อความเปลี่ยน (ทุก set() = หนึ่งข้อความข้ามแกนไปวาดจอ) */
static bento_status_t show(bento_game_t *game,
                           const bento_game_text_t *widget, char *shown,
                           const char *text)
{
    bento_status_t status = BENTO_OK;

    if (strcmp(text, shown) != 0) {
        status = bento_game_text_set(game, widget, text);
        (void)snprintf(shown, TEXT_BYTES, "%s", text);
    }
    return status;
}

/* ---- 4) หน้าจอ: สร้างครั้งเดียว ---- */
static bento_status_t add_columns(bento_game_t *game)
{
    static const int32_t column_x[] = {8, 272, 536};
    static const char *const heads[] = {"เข้า = วัด", "คิด = ตัดสิน",
                                        "ออก = ทำ + โชว์"};
    static const uint32_t colors[] = {
        BENTO_GAME_CYAN, BENTO_GAME_YELLOW, BENTO_GAME_GREEN};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    for (i = 0U; (status == BENTO_OK) && (i < (uint32_t)COLUMNS); i++) {
        bento_game_box_t frame_box;
        bento_game_text_t head;

        bento_game_box_init(&frame_box);
        bento_game_text_init(&head);
        status = bento_game_box_create_styled(
            game, &frame_box, (bento_real_t)column_x[i],
            (bento_real_t)COLUMN_Y, (bento_real_t)COLUMN_W,
            (bento_real_t)COLUMN_H, PANEL, (int32_t)colors[i],
            COLUMN_RADIUS, COLUMN_BORDER_PX);
        if (status == BENTO_OK) {
            status = bento_game_text_create(game, &head, heads[i],
                                            column_x[i] + HEAD_DX,
                                            HEAD_Y, colors[i]);
        }
    }
    return status;
}

/* The three value texts, the sound lamp and its caption. */
static bento_status_t add_values(one_loop_t *loop)
{
    bento_game_t *game = &loop->game;
    bento_game_text_t caption;
    bento_status_t status = bento_game_text_create(
        game, &loop->in_text, "-", IN_X, VALUE_Y, BENTO_GAME_WHITE);

    bento_game_text_init(&caption);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &loop->think_text, "", THINK_X,
                                   VALUE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &loop->out_text, "", OUT_X,
                                   VALUE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &loop->lamp, (bento_real_t)LAMP_X,
            (bento_real_t)LAMP_Y, (bento_real_t)LAMP_PX,
            (bento_real_t)LAMP_PX, GRAY, BENTO_GAME_BORDER_NONE,
            LAMP_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &caption, "เสียง", SOUND_X,
                                        SOUND_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The field and the ship in its middle. */
static bento_status_t add_field(one_loop_t *loop)
{
    bento_game_box_t field;
    bento_status_t status;

    bento_game_box_init(&field);
    status = bento_game_box_create_styled(
        &loop->game, &field, (bento_real_t)AREA_X, (bento_real_t)AREA_Y,
        (bento_real_t)AREA_W, (bento_real_t)AREA_H, FIELD, FIELD_EDGE,
        1, FIELD_BORDER_PX);
    loop->ship_at.x = (X_MIN + X_MAX) / 2;
    loop->ship_at.y = (Y_MIN + Y_MAX) / 2;
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(&loop->game, &loop->ship, "ship",
                                     (bento_real_t)loop->ship_at.x,
                                     (bento_real_t)loop->ship_at.y);
    }
    return status;
}

/* เข้า: every key is asked every frame, so each remembers its last frame;
 * the last key that was just pressed gives the note. */
static bool read_note(one_loop_t *loop, const bento_game_keys_t *keys,
                      int32_t *note)
{
    bool has_note = false;
    uint32_t i;

    for (i = 0U; i < (uint32_t)KEY_COUNT; i++) {
        if (bento_game_pressed_once(&loop->game, KEYS[i].key, keys)) {
            *note = KEYS[i].note; /* กดค้าง = True แค่เฟรมแรก */
            has_note = true;
        }
    }
    return has_note;
}

/* ออก: the three panels, only where the text changed (out every frame). */
static bento_status_t show_texts(one_loop_t *loop,
                                 const bento_game_keys_t *keys,
                                 bool at_edge)
{
    char text[TEXT_BYTES];
    bento_status_t status;

    held_names(keys, text, (uint32_t)sizeof(text));
    status = show(&loop->game, &loop->in_text, loop->in_shown, text);
    (void)snprintf(text, sizeof(text), "x %" PRId32 "  y %" PRId32 "%s",
                   loop->ship_at.x, loop->ship_at.y,
                   at_edge ? "  ชนขอบ" : "");
    if (status == BENTO_OK) {
        status = show(&loop->game, &loop->think_text, loop->think_shown,
                      text);
    }
    (void)snprintf(text, sizeof(text),
                   "เฟรม %" PRId32 "  (%" PRId32 " วิ)", loop->frame,
                   loop->frame / FPS);
    if (status == BENTO_OK) {
        status =
            bento_game_text_set(&loop->game, &loop->out_text, text);
    }
    return status;
}

/* ออก: a short note lights the lamp; the lamp counts down, then goes out. */
static bento_status_t sound_lamp(one_loop_t *loop, bool has_note,
                                 int32_t note)
{
    bento_status_t status = BENTO_OK;

    if (has_note) {
        /* เสียงสั้น ไม่ต่อคิว ไม่ต้อง sleep */
        bento_game_tone(&loop->game, note, BENTO_GAME_WAVE_DEFAULT,
                        TONE_VELOCITY, TONE_MS);
        if (loop->lamp_left == 0) {
            status = bento_game_box_set_color(&loop->game, &loop->lamp,
                                              BENTO_GAME_ORANGE);
        }
        loop->lamp_left = LAMP_FRAMES;
    } else if (loop->lamp_left > 0) { /* นับถอยหลังแล้วดับไฟ */
        loop->lamp_left -= 1;
        if (loop->lamp_left == 0) {
            status = bento_game_box_set_color(&loop->game, &loop->lamp,
                                              GRAY);
        }
    } else {
        /* the lamp is already out */
    }
    return status;
}

/* ---- 5) วงวนหลัก ---- */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    one_loop_t *loop = &g_loop;
    bento_game_keys_t keys;
    point_t next = {0, 0};
    int32_t note = 0;
    bool has_note;
    bool at_edge;
    bool moved;
    bento_status_t status;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    *out_stop = false; /* คืน False เมื่ออยากจบเกม: this one never does */
    /* ===== เข้า (วัด): อ่านปุ่มเฟรมนี้ + หาปุ่มที่ "เพิ่งกด" ===== */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&loop->game, &keys);
    has_note = read_note(loop, &keys, &note);
    /* ===== คิด (ตัดสิน): คำนวณสถานะใหม่ ยังไม่แตะจอเลย ===== */
    loop->frame += 1;
    at_edge = next_position(loop->ship_at, &keys, &next);
    moved = (next.x != loop->ship_at.x) || (next.y != loop->ship_at.y);
    loop->ship_at = next;
    /* ===== ออก (ทำ + โชว์): วาด เสียง และป้าย เฉพาะที่เปลี่ยน ===== */
    if ((status == BENTO_OK) && moved) {
        status = bento_game_sprite_move_to(&loop->game, &loop->ship,
                                           (bento_real_t)next.x,
                                           (bento_real_t)next.y);
    }
    if (status == BENTO_OK) {
        status = show_texts(loop, &keys, at_edge);
    }
    if (status == BENTO_OK) {
        status = sound_lamp(loop, has_note, note);
    }
    return status;
}

int main(void)
{
    one_loop_t *loop = &g_loop;
    bool quit = false;
    bento_status_t status;

    (void)memset(loop, 0, sizeof(*loop));
    bento_game_init(&loop->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_text_init(&loop->in_text);
    bento_game_text_init(&loop->think_text);
    bento_game_text_init(&loop->out_text);
    bento_game_box_init(&loop->lamp);
    bento_game_sprite_init(&loop->ship);
    status = bento_game_title(&loop->game, "ONE LOOP", NULL, &quit);
    report_joystick(&loop->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_columns(&loop->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_values(loop);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_field(loop);
    }
    loop->frame = 0;
    loop->lamp_left = 0;
    (void)snprintf(loop->in_shown, sizeof(loop->in_shown), "-");
    loop->think_shown[0] = '\0';
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&loop->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
