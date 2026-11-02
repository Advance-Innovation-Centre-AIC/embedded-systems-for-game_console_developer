/*
 * s13_14_sound_board.c -- คาบ 13 (คาบพิเศษ): แผงเสียง 21 เสียง ในเกมคือรางวัล ในโรงงานคือคำเตือน
 *
 * หนึ่งเสียง หนึ่งความหมาย จัดเสียงเป็น 4 กลุ่ม ตอบรับ / ได้ / เสีย / กระทำ
 * กด Start แล้วใช้จอย ขึ้น/ลง เลือกกลุ่ม ซ้าย/ขวา เลือกเสียง (ชื่อในวงเล็บ) A = เล่นเสียง
 * B = ทำนองสองโน้ต ดูเลข n กับความถี่ Hz จากสูตร f = 440 * 2^((n - 69) / 12) ขึ้นทีละโน้ต
 * X = จำลองสัญญาณเตือน สลับระดับทุกครั้งที่กด: ด่วน (ไฟแดงกะพริบ ดังถี่จนกด Y)
 * หรือ แจ้งทราบ (ไฟเหลือง ดังครั้งเดียว) Y = รับทราบ ทำนองไม่ใช้ sleep แต่นับเฟรม
 *
 * A sound board of the 21 built-in sounds in four groups (acknowledge, gain, loss, action).
 * Up/down picks a group, left/right a sound, A plays it. B plays a two-note jingle by counting
 * frames, showing each note's MIDI number and its frequency from f = 440 * 2^((n - 69) / 12).
 * X raises an alarm, HIGH and LOW in turn: HIGH beeps five times a second with a blinking red
 * lamp until Y acknowledges it, LOW beeps once with a yellow lamp. The pure logic (midi_hz,
 * row_text, alarm_beep) is checked once before the screen opens.
 *
 * Twin of s13_14_sound_board.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_14_sound_board.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the st dict is one board_t, at file scope because it is large;
 *   - the alarm level string ("", "HIGH", "LOW") is an enum;
 *   - math.pow is py_pow(): powf on the kits, pow on the reTerminal, as MicroPython;
 *   - round(x, 2) in the self test compares whole hundredths;
 *   - '%.1f' of a float is bento_fmt_real_precision(), the board's own float formatter;
 *   - raise SystemExit after a failed self test ends main() with 0.
 */
#include <math.h>
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

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    NOTE_GAP = 8,   /* โน้ตถัดไปห่าง 8 เฟรม (ราว 0.27 วินาที) */
    HIGH_EVERY = 6, /* เตือนด่วน: ดังทุก 6 เฟรม = 5 ครั้งต่อวินาที */
    HIGH_NOTE = 88,
    LOW_NOTE = 76
};
/* ---- end of settings ---- */

/* Sounds and rhythm. */
enum {
    GROUP_COUNT = 4,
    GROUP_MAX = 6,    /* the most sounds in one group */
    JINGLE_NOTES = 2, /* โด สูง -> ซอล สูง (เลข MIDI 72, 79) */
    JINGLE_MS = 220,
    BEEP_MS = 90,
    TONE_VELOCITY = 100,
    FLASH_FRAMES =
        6,        /* the bar lights for 6 frames: the press was heard */
    MIDI_A4 = 69, /* ลา 440 Hz */
    SEMITONES = 12, /* one octave doubles the frequency */
    FPS = 30
};

/* Where things are, in px, and the text buffers. */
enum {
    ROW_Y0 = 62,
    ROW_STEP = 38,
    BAR_X = 8,
    BAR_UP = 6,
    BAR_W = 776,
    BAR_H = 34,
    BAR_RADIUS = 6,
    LEGEND_X = 12,
    LEGEND_Y = 12,
    GROUP_X = 20,
    ROW_X = 110,
    FORMULA_Y = 230,
    NOTE_Y = 262,
    LAMP_X = 470,
    LAMP_Y = 226,
    LAMP_SIZE = 44,
    LAMP_RADIUS = 22,
    ALARM_X = 530,
    ROW_CHARS = 64,   /* the longest row is 46 bytes */
    NOTE_CHARS = 112, /* the note line with three int32_t and a float */
    NEXT_CHARS = 16
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_GRAY = 0x556070,
    COLOR_DARK_RED = 0x5C1F1F,
    COLOR_BAR = 0x2A3340,
    COLOR_BAR_LIT = 0x3E6A9A,
    COLOR_GROUP = 0x8899AA
};

typedef enum {
    LEVEL_NONE = 0,
    LEVEL_HIGH,
    LEVEL_LOW
} level_t;

typedef struct {
    const char *name;
    int32_t count;
} group_t;

/* รวม 21 เสียง: ชื่อกลุ่มกับจำนวน แล้วเสียงของกลุ่มที่แถวเดียวกัน */
static const group_t GROUPS[GROUP_COUNT] = {
    {"ตอบรับ", 5}, {"ได้", 5}, {"เสีย", 5}, {"กระทำ", 6}};
static const char *const GROUP_SOUNDS[GROUP_COUNT][GROUP_MAX] = {
    {"select", "move", "back", "deny", "start", ""},
    {"eat", "point", "hit", "win", "pong_score", ""},
    {"die", "fall", "lose", "lose_life", "gameover", ""},
    {"fire", "turn", "flap", "wall", "paddle", "explode"}};

typedef struct {
    bento_game_t game;
    bento_game_box_t bar;
    bento_game_text_t rows[GROUP_COUNT];
    bento_game_text_t hud_note;
    bento_game_box_t lamp;
    bento_game_text_t hud_alarm;
    bento_game_text_t hud_next;
    int32_t row;
    int32_t column;
    int32_t flash;
    int32_t jingle; /* the jingle's frame, -1 = not playing */
    level_t level;
    int32_t frame;
    level_t next;
    bool lit;
} board_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern board_t g_board;
board_t g_board;

/* ---- the brain: pure, no screen ---- */

/* Floor division and modulo, as Python does them (the divisor is positive). */
static int32_t floor_div(int32_t value, int32_t divisor)
{
    const int32_t quotient = value / divisor;

    return ((value % divisor) < 0) ? (quotient - 1) : quotient;
}

static int32_t floor_mod(int32_t value, int32_t divisor)
{
    return value - (floor_div(value, divisor) * divisor);
}

/* x ** y and math.pow in the board's MicroPython float: powf on the
 * kits, pow on the reTerminal (MicroPython's float is double there) */
static bento_real_t py_pow(bento_real_t base, bento_real_t exponent)
{
#if BENTO_FMT_REAL_DOUBLE
    return pow(base, exponent);
#else
    return powf(base, exponent);
#endif
}

/* 12 up = the frequency doubles (one octave higher). */
static bento_real_t midi_hz(int32_t note)
{
    const bento_real_t a4_hz = (bento_real_t)440.0;
    const bento_real_t octaves =
        (bento_real_t)(note - MIDI_A4) / (bento_real_t)SEMITONES;

    return a4_hz * py_pow((bento_real_t)2, octaves);
}

static void append_text(char *out, uint32_t cap, const char *piece)
{
    const uint32_t used = (uint32_t)strlen(out);
    const uint32_t length = (uint32_t)strlen(piece);

    if ((used + length) < cap) {
        (void)memcpy(&out[used], piece, (size_t)length + 1U);
    }
}

/* The picked name in brackets, the others padded alike, so the text never shifts. */
static void row_text(const char *const *names, int32_t count,
                     int32_t picked, char *out, uint32_t cap)
{
    char item[ROW_CHARS];
    int32_t i;

    out[0] = '\0';
    for (i = 0; i < count; i++) {
        (void)snprintf(item, sizeof(item),
                       (i == picked) ? "[%s]" : " %s ", names[i]);
        if (i > 0) {
            append_text(out, cap, " ");
        }
        append_text(out, cap, item);
    }
}

/* HIGH beeps every `every` frames and never stops by itself; LOW only on its first frame. */
static bool alarm_beep(level_t level, int32_t frame, int32_t every)
{
    bool beep = false;

    if (level == LEVEL_HIGH) {
        beep = (frame % every) == 0;
    } else if (level == LEVEL_LOW) {
        beep = frame == 0;
    } else {
        /* normal: silent */
    }
    return beep;
}

/* ---- check the brain before the screen opens ---- */

/* round(midi_hz(note), 2) against want_hundredths, as whole hundredths. */
static bool check_hz(int32_t note, int32_t want_hundredths)
{
    const bento_real_t hundred = (bento_real_t)100.0;
    const bento_real_t half = (bento_real_t)0.5;
    const bento_real_t scaled = midi_hz(note) * hundred;
    const bento_real_t rounded = scaled + half;
    const int32_t got = (int32_t)rounded;
    char got_text[BENTO_FMT_BUF_SIZE];
    char want_text[BENTO_FMT_BUF_SIZE];
    uint32_t length = 0U;

    if (got != want_hundredths) {
        (void)bento_fmt_real(
            got_text, sizeof(got_text), (bento_real_t)got / hundred,
            &length); /* the buffer is always big enough */
        (void)bento_fmt_real(want_text, sizeof(want_text),
                             (bento_real_t)want_hundredths / hundred,
                             &length);
        (void)printf("ยังไม่ผ่าน: ได้ %s ควรได้ %s\n", got_text, want_text);
    }
    return got == want_hundredths;
}

static bool check_row(const char *got, const char *want)
{
    const bool same = strcmp(got, want) == 0;

    if (!same) {
        (void)printf("ยังไม่ผ่าน: ได้ %s ควรได้ %s\n", got, want);
    }
    return same;
}

static bool check_beep(bool got, bool want)
{
    if (got != want) {
        (void)printf("ยังไม่ผ่าน: ได้ %s ควรได้ %s\n",
                     got ? "True" : "False", want ? "True" : "False");
    }
    return got == want;
}

static bool self_test(void)
{
    /* round(midi_hz(n), 2) in hundredths; alarm_beep(level, frame, HIGH_EVERY) */
    static const int32_t hz_cases[][2] = {
        {69, 44000}, {72, 52325}, {81, 88000}};
    static const level_t beep_levels[] = {LEVEL_HIGH, LEVEL_HIGH,
                                          LEVEL_LOW, LEVEL_LOW};
    static const int32_t beep_frames[] = {12, 13, 0, 6};
    static const bool beep_wanted[] = {true, false, true, false};
    static const char *const two_names[] = {"a", "b"};
    const size_t hz_count = sizeof(hz_cases) / sizeof(hz_cases[0]);
    const size_t beep_count =
        sizeof(beep_wanted) / sizeof(beep_wanted[0]);
    char row[ROW_CHARS];
    bool passed = true;
    int32_t i;

    for (i = 0; passed && (i < (int32_t)hz_count); i++) {
        passed = check_hz(hz_cases[i][0], hz_cases[i][1]);
    }
    if (passed) {
        row_text(two_names, 2, 1, row, (uint32_t)sizeof(row));
        passed = check_row(row, " a  [b]");
    }
    for (i = 0; passed && (i < (int32_t)beep_count); i++) {
        passed = check_beep(
            alarm_beep(beep_levels[i], beep_frames[i], HIGH_EVERY),
            beep_wanted[i]);
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของแผงเสียงถูกทุกกรณี\n");
    }
    return passed;
}

/* ---- the screen: made once (the bar first, the texts on top of it) ---- */

static int32_t row_y(int32_t row)
{
    return ROW_Y0 + (row * ROW_STEP);
}

static bento_status_t add_text(board_t *board, bento_game_text_t *text,
                               const char *utf8, int32_t x, int32_t y,
                               uint32_t color)
{
    bento_game_text_init(text);
    return bento_game_text_create(&board->game, text, utf8, x, y,
                                  color);
}

static bento_status_t add_rows(board_t *board)
{
    bento_game_text_t group_name;
    bento_status_t status;
    int32_t i;

    bento_game_box_init(&board->bar);
    status = bento_game_box_create_styled(
        &board->game, &board->bar, (bento_real_t)BAR_X,
        (bento_real_t)(row_y(0) - BAR_UP), (bento_real_t)BAR_W,
        (bento_real_t)BAR_H, COLOR_BAR, BENTO_GAME_BORDER_NONE,
        BAR_RADIUS, 0);
    for (i = 0; (status == BENTO_OK) && (i < GROUP_COUNT); i++) {
        status = add_text(board, &group_name, GROUPS[i].name, GROUP_X,
                          row_y(i), COLOR_GROUP);
    }
    for (i = 0; (status == BENTO_OK) && (i < GROUP_COUNT); i++) {
        status = add_text(board, &board->rows[i], "", ROW_X, row_y(i),
                          BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t build_screen(board_t *board)
{
    bento_game_text_t fixed;
    /* one text holds at most 95 bytes when made (Thai is 3 bytes a letter); the board cuts more */
    bento_status_t status = add_text(
        board, &fixed, "A = เล่น   B = ทำนอง   X = เตือน   Y = รับทราบ",
        LEGEND_X, LEGEND_Y, BENTO_GAME_CYAN);

    if (status == BENTO_OK) {
        status = add_rows(board);
    }
    if (status == BENTO_OK) {
        status = add_text(board, &fixed, "f = 440 * 2^((n - 69) / 12)",
                          GROUP_X, FORMULA_Y, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = add_text(board, &board->hud_note, "B = ทำนอง 2 โน้ต",
                          GROUP_X, NOTE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        bento_game_box_init(&board->lamp);
        status = bento_game_box_create_styled(
            &board->game, &board->lamp, (bento_real_t)LAMP_X,
            (bento_real_t)LAMP_Y, (bento_real_t)LAMP_SIZE,
            (bento_real_t)LAMP_SIZE, COLOR_GRAY, (int32_t)COLOR_GROUP,
            LAMP_RADIUS, 2);
    }
    if (status == BENTO_OK) {
        status = add_text(board, &board->hud_alarm, "NORMAL", ALARM_X,
                          FORMULA_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(board, &board->hud_next, "", ALARM_X, NOTE_Y,
                          COLOR_GROUP);
    }
    return status;
}

static bento_status_t paint_rows(board_t *board)
{
    char text[ROW_CHARS];
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < GROUP_COUNT); i++) {
        row_text(GROUP_SOUNDS[i], GROUPS[i].count,
                 (i == board->row) ? board->column : -1, text,
                 (uint32_t)sizeof(text));
        status =
            bento_game_text_set(&board->game, &board->rows[i], text);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &board->game, &board->bar, (bento_real_t)BAR_X,
            (bento_real_t)(row_y(board->row) - BAR_UP));
    }
    return status;
}

static bento_status_t set_alarm(board_t *board, level_t level)
{
    static const uint32_t lamp_colors[] = {COLOR_GRAY, BENTO_GAME_RED,
                                           BENTO_GAME_YELLOW};
    static const char *const alarm_texts[] = {
        "NORMAL", "HIGH ALARM - press Y", "LOW: notify once"};
    /* A level's name as Python keeps it, by level_t. */
    static const char *const level_names[] = {"", "HIGH", "LOW"};
    char text[NEXT_CHARS];
    bento_status_t status;

    board->level = level;
    board->frame = 0;
    status = bento_game_box_set_color(&board->game, &board->lamp,
                                      lamp_colors[level]);
    if (status == BENTO_OK) {
        status = bento_game_text_set(&board->game, &board->hud_alarm,
                                     alarm_texts[level]);
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "X = raise %s",
                       level_names[board->next]);
        status =
            bento_game_text_set(&board->game, &board->hud_next, text);
    }
    return status;
}

/* ---- the main loop ---- */

/* -1, 0 or +1 for a pair of opposite keys. */
static int32_t key_axis(bool minus, bool plus)
{
    int32_t axis = 0;

    if (plus) {
        axis += 1;
    }
    if (minus) {
        axis -= 1;
    }
    return axis;
}

/* Up and down stop at the ends; left and right wrap round inside the row. */
static bento_status_t move_selection(board_t *board,
                                     const bento_game_keys_t *keys)
{
    bento_game_t *game = &board->game;
    const bool down =
        bento_game_pressed_once(game, BENTO_GAME_KEY_DOWN, keys);
    const bool up =
        bento_game_pressed_once(game, BENTO_GAME_KEY_UP, keys);
    const bool right =
        bento_game_pressed_once(game, BENTO_GAME_KEY_RIGHT, keys);
    const bool left =
        bento_game_pressed_once(game, BENTO_GAME_KEY_LEFT, keys);
    const int32_t row_step = key_axis(up, down);
    const int32_t column_step = key_axis(left, right);
    bento_status_t status = BENTO_OK;

    if ((row_step != 0) || (column_step != 0)) {
        int32_t count;
        int32_t column;

        board->row += row_step;
        board->row = (board->row < 0) ? 0 : board->row;
        board->row = (board->row > (GROUP_COUNT - 1))
                         ? (GROUP_COUNT - 1)
                         : board->row;
        count = GROUPS[board->row].count;
        column =
            (board->column < (count - 1)) ? board->column : (count - 1);
        board->column = floor_mod(column + column_step, count);
        status = paint_rows(board);
    }
    return status;
}

/* A plays the picked sound, and the bar flashes for 6 frames. */
static bento_status_t play_picked(board_t *board,
                                  const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    if (bento_game_pressed_once(&board->game, BENTO_GAME_KEY_A, keys)) {
        bento_game_sfx(&board->game,
                       GROUP_SOUNDS[board->row][board->column]);
        status = bento_game_box_set_color(&board->game, &board->bar,
                                          COLOR_BAR_LIT);
        board->flash = FLASH_FRAMES;
    }
    if ((status == BENTO_OK) && (board->flash > 0)) {
        board->flash -= 1;
        if (board->flash == 0) {
            status = bento_game_box_set_color(&board->game, &board->bar,
                                              COLOR_BAR);
        }
    }
    return status;
}

static bento_status_t show_note(board_t *board, int32_t number,
                                int32_t note)
{
    char hz_text[BENTO_FMT_BUF_SIZE];
    char text[NOTE_CHARS];
    uint32_t length = 0U;
    bento_status_t status = bento_fmt_real_precision(
        hz_text, sizeof(hz_text), midi_hz(note), 1, &length);

    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "note %d/%d   n = %d  ->  f = %s Hz",
                       (int)number, JINGLE_NOTES, (int)note, hz_text);
        status =
            bento_game_text_set(&board->game, &board->hud_note, text);
    }
    return status;
}

/* The jingle counts frames instead of sleeping: the next note plays when its frame comes. */
static bento_status_t step_jingle(board_t *board,
                                  const bento_game_keys_t *keys)
{
    static const int32_t jingle[JINGLE_NOTES] = {72, 79};
    bento_status_t status = BENTO_OK;

    if (bento_game_pressed_once(&board->game, BENTO_GAME_KEY_B, keys)) {
        board->jingle = 0;
    }
    if (board->jingle >= 0) {
        if ((board->jingle % NOTE_GAP) == 0) {
            const int32_t i = board->jingle / NOTE_GAP;

            bento_game_tone(&board->game, jingle[i],
                            BENTO_GAME_WAVE_DEFAULT, TONE_VELOCITY,
                            JINGLE_MS);
            status = show_note(board, i + 1, jingle[i]);
        }
        board->jingle += 1;
        if (board->jingle >= (JINGLE_NOTES * (int32_t)NOTE_GAP)) {
            board->jingle = -1; /* every note played: stop counting */
        }
    }
    return status;
}

/* X raises an alarm, HIGH and LOW in turn; Y acknowledges it. */
static bento_status_t step_alarm(board_t *board,
                                 const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;
    bool acknowledged;

    if (bento_game_pressed_once(&board->game, BENTO_GAME_KEY_X, keys)) {
        const level_t level = board->next;

        board->next = (level == LEVEL_HIGH) ? LEVEL_LOW : LEVEL_HIGH;
        status = set_alarm(board, level);
    }
    acknowledged =
        bento_game_pressed_once(&board->game, BENTO_GAME_KEY_Y, keys);
    if ((status == BENTO_OK) && acknowledged &&
        (board->level != LEVEL_NONE)) {
        bento_game_sfx(&board->game, "select");
        status = set_alarm(board, LEVEL_NONE);
    }
    if ((status == BENTO_OK) &&
        alarm_beep(board->level, board->frame, HIGH_EVERY)) {
        const bool high = board->level == LEVEL_HIGH;

        bento_game_tone(&board->game, high ? HIGH_NOTE : LOW_NOTE,
                        BENTO_GAME_WAVE_DEFAULT, TONE_VELOCITY,
                        BEEP_MS);
        if (high) {
            /* the red lamp blinks with the beep */
            board->lit = !board->lit;
            status = bento_game_box_set_color(
                &board->game, &board->lamp,
                board->lit ? BENTO_GAME_RED : COLOR_DARK_RED);
        }
    }
    board->frame += 1;
    return status;
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    board_t *board = &g_board;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: the game goes on */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &board->game,
        &keys); /* never fails: a silent pad is all keys up */
    status = move_selection(board, &keys);
    if (status == BENTO_OK) {
        status = play_picked(board, &keys);
    }
    if (status == BENTO_OK) {
        status = step_jingle(board, &keys);
    }
    if (status == BENTO_OK) {
        status = step_alarm(board, &keys);
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
    board_t *board = &g_board;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(board, 0, sizeof(*board));
    board->jingle = -1;
    board->level = LEVEL_NONE;
    board->next = LEVEL_HIGH;
    if (self_test()) {
        bento_game_init(&board->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status =
            bento_game_title(&board->game, "SOUND BOARD", NULL, &quit);
        report_joystick(&board->game);
        if ((status == BENTO_OK) && !quit) {
            status = build_screen(board);
        }
        if ((status == BENTO_OK) && !quit) {
            status = paint_rows(board);
        }
        if ((status == BENTO_OK) && !quit) {
            status = set_alarm(board, LEVEL_NONE);
        }
        if ((status == BENTO_OK) && !quit) {
            bool finished = false;

            status = bento_game_run(&board->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
