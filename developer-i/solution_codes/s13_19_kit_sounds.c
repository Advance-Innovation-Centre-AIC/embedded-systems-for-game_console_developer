/*
 * s13_19_kit_sounds.c -- คาบ 13 (คาบพิเศษ): สูตรเสียงประกอบจากเกมเต็มของจริง เหตุการณ์ไหนใช้เสียงไหน
 * + ทำนองแบบไม่ใช้ sleep
 *
 * หนึ่งเสียง หนึ่งความหมาย ดังเฉพาะตอนเกิดเหตุ game.sfx("ชื่อ") ตรงบรรทัดที่เหตุเกิด
 * tone() ไม่ต่อคิว และห้าม sleep ในวงวน ทำนองจึงใช้ตัวนับเฟรม ถึงคิวค่อยเล่นโน้ตถัดไป
 * กด Start (ฟังทำนองเปิดเกม) ซ้าย/ขวา = เปลี่ยนเกม UP/DOWN = เลือกแถว A = เล่น
 * ป้ายล่างบอกว่ากำลังเล่นอะไร มาจากบรรทัดไหนของเกมเต็ม หน้าสุดท้าย MELODY ดูไฟวิ่งตามระดับโน้ต
 *
 * Which event plays which sound in the four full games and bentogame, page by page: left/right
 * changes the page, up/down picks a row, A plays it, and the bottom line names the sound and
 * the line of the full game that plays it. The last page, MELODY, plays three tunes with a
 * frame-counting player instead of sleeping, and a lamp moves with each note's pitch.
 *
 * Twin of s13_19_kit_sounds.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_19_kit_sounds.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the tuples of pages, rows and tune steps are static const tables of structs;
 *   - the RECIPES dict is a lookup by name, so the SNAKE page's COIN row plays the tune too;
 *   - player_step()'s None is a false answer, and view's None a has_said flag;
 *   - the view and player dicts are one kit_t, at file scope because it is large.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The pages and the screen (change these). */
enum {
    PAGES = 6,
    ROWS_MAX = 5,  /* the most rows on one page */
    ROW_TEXTS = 6, /* the row texts on screen */
    ROWS_Y = 60,   /* แถวแรกอยู่ที่ y = 60 */
    ROW_H = 34,    /* ห่างกันแถวละ 34 px */
    BAR_X = 20,
    BAR_UP = 5,
    BAR_W = 752,
    BAR_SHRINK = 4,
    BAR_RADIUS = 6,
    TEXT_X = 24,
    HEADER_Y = 16,
    ROW_X = 36,
    NOW_Y = 290,
    HINT_Y = 330,
    LAMP_X = 700,
    LAMP_Y = 286,
    LAMP_SIZE = 24,
    LAMP_RADIUS = 12,
    LAMP_FRAMES = 6, /* the note lamp stays lit 6 frames */
    LAMP_SOUND_X = 560,
    LAMP_NOTE_X = 420, /* middle C; each note up moves the lamp 10 px */
    LAMP_NOTE_STEP = 10,
    MIDDLE_C = 60,
    TONE_VELOCITY = 100,
    FPS = 30,
    LINE_CHARS =
        128 /* the longest line is about 80 bytes (Thai is 3 a letter) */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_SPACE = 0x10122C,
    COLOR_BAR = 0x24406A,
    COLOR_BAR_EDGE = 0x80F4FF,
    COLOR_LAMP_OFF = 0x334455
};

/* (the sound name for game.sfx, the event in the game, file:line that plays it) */
typedef struct {
    const char *sound;
    const char *event;
    const char *where;
} row_t;

typedef struct {
    const char *name;
    int32_t count;
    row_t rows[ROWS_MAX];
} page_t;

/* One step of a tune: a MIDI note (0 = a rest), its ms, frames until the next step. */
typedef struct {
    int32_t note;
    int32_t ms;
    int32_t frames;
} step_t;

/* The event -> sound tables of the four full games, read from their code, then MELODY. */
static const page_t PAGE_TABLE[PAGES] = {
    {"SHOOTER",
     5,
     {{"fire", "ยิงกระสุน", "shooter_full.py:142"},
      {"explode", "ระเบิด ทุกครั้งที่บึ้ม", "shooter_full.py:152"},
      {"hit", "ยิงโดนศัตรู (ซ้อนเสียงระเบิด)", "shooter_full.py:242"},
      {"lose_life", "ศัตรูชนยาน ยังรอด", "shooter_full.py:259"},
      {"gameover", "ชีวิตหมด", "shooter_full.py:229, 256"}}},
    {"FLAPPY",
     3,
     {{"flap", "กระพือปีก", "flappy_full.py:177"},
      {"point", "บินผ่านท่อ ได้ 1 คะแนน", "flappy_full.py:213"},
      {"fall", "ตกพื้น หรือชนท่อ", "flappy_full.py:197, 221"},
      {"", "", ""},
      {"", "", ""}}},
    {"PONG",
     5,
     {{"wall", "ลูกเด้งขอบบน/ล่าง", "pong_full.py:101, 105"},
      {"paddle", "ตีโดนไม้", "pong_full.py:115, 122"},
      {"pong_score", "ได้แต้ม แมตช์ยังไม่จบ", "pong_full.py:134, 142"},
      {"win", "ผู้เล่นชนะแมตช์", "pong_full.py:140"},
      {"lose", "AI ชนะแมตช์", "pong_full.py:132"}}},
    {"SNAKE",
     5,
     {{"eat", "กินแอปเปิล", "snake_full.py:127"},
      {"COIN", "ทุกลูกที่ 10 เสียงเหรียญ 2 โน้ต", "snake_full.py:128-129"},
      {"point", "ทุกลูกที่ 10 (ฉบับสไปรต์)", "snake_sprite_full.py:132-133"},
      {"die", "ชนกำแพง", "snake_full.py:119"},
      {"lose", "กัดตัวเอง (คนละเสียงกับชนกำแพง)", "snake_full.py:121"}}},
    {"SYSTEM",
     4,
     {{"start", "กด Start หน้าเริ่ม / เล่นใหม่", "bentogame.py:915, 1072"},
      {"select", "พักเกม / ยืนยันเมนู", "bentogame.py:991, 1154"},
      {"move", "เลื่อนเมนูขึ้นลง", "bentogame.py:1147, 1151"},
      {"back", "กด Back ในเมนู", "bentogame.py:1142"},
      {"", "", ""}}},
    {"MELODY",
     3,
     {{"TITLE", "ทำนองเปิดเกม (ทุกเกมเต็มใช้)", "bentogame.py:869, 907"},
      {"COIN", "เหรียญ 2 โน้ตของ snake_full", "snake_full.py:32-37"},
      {"TEAM", "ทำนองของทีม แต่งเองได้", "bentogame.py:871"},
      {"", "", ""},
      {"", "", ""}}}};

/* The frame-counting tune player: no sleep. */
typedef struct {
    const step_t *steps;
    int32_t count;
    int32_t index;
    int32_t wait;
} player_t;

typedef struct {
    bento_game_t game;
    bento_game_box_t bar;
    bento_game_text_t header;
    bento_game_text_t rows[ROW_TEXTS];
    bento_game_text_t now;
    bento_game_box_t lamp;
    player_t player;
    int32_t page;
    int32_t picked;
    int32_t lamp_left;
    const char *name;
    bool has_said;
    char said[LINE_CHARS];
} kit_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern kit_t g_kit;
kit_t g_kit;

/* ---- the tune player ---- */

/* Start a new tune (over one still playing). */
static void play(player_t *player, const step_t *steps, int32_t count)
{
    player->steps = steps;
    player->count = count;
    player->index = 0;
    player->wait = 0;
}

/* Called every frame. True with *out_note when a step starts now (0 = a rest). */
static bool player_step(bento_game_t *game, player_t *player,
                        int32_t *out_note)
{
    bool started = false;

    if (player->index >= player->count) {
        /* no tune, or it has ended */
    } else if (player->wait > 0) {
        player->wait -=
            1; /* not its turn yet: count down and let the frame go on */
    } else {
        const step_t *step = &player->steps[player->index];

        if (step->note != 0) {
            bento_game_tone(game, step->note, BENTO_GAME_WAVE_DEFAULT,
                            TONE_VELOCITY, step->ms);
        }
        player->index += 1;
        player->wait = step->frames - 1;
        *out_note = step->note;
        started = true;
    }
    return started;
}

/* RECIPES[name]: false for a name that is a built-in sound. */
static bool find_recipe(const char *name, const step_t **out_steps,
                        int32_t *out_count)
{
    /* The tunes of the full games, in frames at 30 fps (one frame = 33 ms). */
    static const step_t coin[] = {{83, 60, 1}, {88, 160, 1}};
    static const step_t title[] = {
        {60, 90, 3}, {64, 90, 3}, {67, 90, 3}, {72, 90, 3}};
    static const step_t team[] = {{64, 110, 4},
                                  {0, 110, 4},
                                  {64, 110, 4},
                                  {67, 110, 4},
                                  {72, 110, 4}};
    const size_t coin_count = sizeof(coin) / sizeof(coin[0]);
    const size_t title_count = sizeof(title) / sizeof(title[0]);
    const size_t team_count = sizeof(team) / sizeof(team[0]);
    bool found = true;

    if (strcmp(name, "TITLE") == 0) {
        *out_steps = title;
        *out_count = (int32_t)title_count;
    } else if (strcmp(name, "COIN") == 0) {
        *out_steps = coin;
        *out_count = (int32_t)coin_count;
    } else if (strcmp(name, "TEAM") == 0) {
        *out_steps = team;
        *out_count = (int32_t)team_count;
    } else {
        found = false;
    }
    return found;
}

/* ---- the screen: made once; a new page writes new texts into the same rows ---- */

static bento_status_t add_text(kit_t *kit, bento_game_text_t *text,
                               const char *utf8, int32_t y,
                               uint32_t color)
{
    bento_game_text_init(text);
    return bento_game_text_create(&kit->game, text, utf8, TEXT_X, y,
                                  color);
}

static bento_status_t add_rows(kit_t *kit)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < ROW_TEXTS); i++) {
        bento_game_text_init(&kit->rows[i]);
        status = bento_game_text_create(&kit->game, &kit->rows[i], "",
                                        ROW_X, ROWS_Y + (i * ROW_H),
                                        BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t build_screen(kit_t *kit)
{
    bento_game_box_t space;
    bento_game_text_t hint;
    bento_status_t status =
        bento_game_background(&kit->game, &space, COLOR_SPACE);

    if (status == BENTO_OK) {
        /* the bar marks the picked row */
        bento_game_box_init(&kit->bar);
        status = bento_game_box_create_styled(
            &kit->game, &kit->bar, (bento_real_t)BAR_X,
            (bento_real_t)ROWS_Y - (bento_real_t)BAR_UP,
            (bento_real_t)BAR_W,
            (bento_real_t)ROW_H - (bento_real_t)BAR_SHRINK, COLOR_BAR,
            (int32_t)COLOR_BAR_EDGE, BAR_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = add_text(kit, &kit->header, "", HEADER_Y,
                          BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = add_rows(kit);
    }
    if (status == BENTO_OK) {
        status = add_text(kit, &kit->now, "", NOW_Y, BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        status = add_text(kit, &hint,
                          "ซ้าย/ขวา = เกมอื่น   UP/DOWN = เลือก   A = เล่น",
                          HINT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        /* the note lamp: lit while a sound plays, it moves with the note's pitch */
        bento_game_box_init(&kit->lamp);
        status = bento_game_box_create_styled(
            &kit->game, &kit->lamp, (bento_real_t)LAMP_X,
            (bento_real_t)LAMP_Y, (bento_real_t)LAMP_SIZE,
            (bento_real_t)LAMP_SIZE, COLOR_LAMP_OFF,
            BENTO_GAME_BORDER_NONE, LAMP_RADIUS, 0);
    }
    return status;
}

static bento_status_t move_bar(kit_t *kit)
{
    return bento_game_box_move_to(
        &kit->game, &kit->bar, (bento_real_t)BAR_X,
        (bento_real_t)((ROWS_Y - BAR_UP) + (kit->picked * ROW_H)));
}

static bento_status_t show_page(kit_t *kit)
{
    const page_t *page = &PAGE_TABLE[kit->page];
    char text[LINE_CHARS];
    bento_status_t status;
    int32_t i;

    (void)snprintf(text, sizeof(text), "%d/%d  %s   เหตุการณ์ -> เสียง",
                   (int)(kit->page + 1), PAGES, page->name);
    status = bento_game_text_set(&kit->game, &kit->header, text);
    for (i = 0; (status == BENTO_OK) && (i < ROW_TEXTS); i++) {
        text[0] = '\0';
        if (i < page->count) {
            (void)snprintf(text, sizeof(text), "%-11s %s",
                           page->rows[i].sound, page->rows[i].event);
        }
        status = bento_game_text_set(&kit->game, &kit->rows[i], text);
    }
    kit->picked = (kit->picked < (page->count - 1)) ? kit->picked
                                                    : (page->count - 1);
    if (status == BENTO_OK) {
        status = move_bar(kit);
    }
    return status;
}

/* The line is written only when its text changes. */
static bento_status_t say(kit_t *kit, const char *line)
{
    bento_status_t status = BENTO_OK;

    if (!kit->has_said || (strcmp(line, kit->said) != 0)) {
        status = bento_game_text_set(&kit->game, &kit->now, line);
        (void)snprintf(kit->said, sizeof(kit->said), "%s", line);
        kit->has_said = true;
    }
    return status;
}

/* The lamp lights for 6 frames and goes out by itself (counted in frames, no sleep). */
static bento_status_t light(kit_t *kit, int32_t x)
{
    bento_status_t status = BENTO_OK;

    if (kit->lamp_left == 0) {
        status = bento_game_box_set_color(&kit->game, &kit->lamp,
                                          BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&kit->game, &kit->lamp,
                                        (bento_real_t)x,
                                        (bento_real_t)LAMP_Y);
    }
    kit->lamp_left = LAMP_FRAMES;
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

/* Floor modulo, as Python's % (the divisor is positive). */
static int32_t floor_mod(int32_t value, int32_t divisor)
{
    const int32_t rest = value % divisor;

    return (rest < 0) ? (rest + divisor) : rest;
}

static bento_status_t turn_page(kit_t *kit,
                                const bento_game_keys_t *keys)
{
    bento_game_t *game = &kit->game;
    const bool right =
        bento_game_pressed_once(game, BENTO_GAME_KEY_RIGHT, keys);
    const bool left =
        bento_game_pressed_once(game, BENTO_GAME_KEY_LEFT, keys);
    const bool down =
        bento_game_pressed_once(game, BENTO_GAME_KEY_DOWN, keys);
    const bool up =
        bento_game_pressed_once(game, BENTO_GAME_KEY_UP, keys);
    const int32_t go = key_axis(left, right);
    const int32_t step = key_axis(up, down);
    bento_status_t status = BENTO_OK;

    if (go != 0) {
        kit->page = floor_mod(kit->page + go, PAGES);
        status = show_page(kit);
        bento_game_sfx(game, "move");
    } else if (step != 0) {
        kit->picked =
            floor_mod(kit->picked + step, PAGE_TABLE[kit->page].count);
        status = move_bar(kit);
    } else {
        /* no key: the page stays */
    }
    return status;
}

/* A built-in sound: one call, and done; the line names where the full game plays it. */
static bento_status_t play_sound(kit_t *kit, const row_t *row)
{
    char line[LINE_CHARS];
    bento_status_t status;

    bento_game_sfx(&kit->game, row->sound);
    status = light(kit, LAMP_SOUND_X);
    (void)snprintf(line, sizeof(line), "กำลังเล่น: %s   <- %s",
                   row->sound, row->where);
    if (status == BENTO_OK) {
        status = say(kit, line);
    }
    return status;
}

/* A plays the picked row of the page the frame began on: a tune, or a built-in sound. */
static bento_status_t play_row(kit_t *kit, const page_t *page,
                               const bento_game_keys_t *keys)
{
    const step_t *steps = NULL;
    int32_t count = 0;
    bento_status_t status = BENTO_OK;

    if (bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_A, keys)) {
        const row_t *row = &page->rows[kit->picked];

        kit->name = row->sound;
        if (find_recipe(row->sound, &steps, &count)) {
            /* a tune: the player plays it note by note */
            play(&kit->player, steps, count);
        } else {
            status = play_sound(kit, row);
        }
    }
    return status;
}

/* A tune just started a step: the lamp follows the pitch. */
static bento_status_t show_tune(kit_t *kit, int32_t note)
{
    char line[LINE_CHARS];
    bento_status_t status = BENTO_OK;

    if (note != 0) {
        status = light(kit, LAMP_NOTE_X +
                                ((note - MIDDLE_C) * LAMP_NOTE_STEP));
    }
    (void)snprintf(line, sizeof(line), "กำลังเล่น: %s  โน้ต %d/%d  (%d)",
                   kit->name, (int)kit->player.index,
                   (int)kit->player.count, (int)note);
    if (status == BENTO_OK) {
        status = say(kit, line);
    }
    return status;
}

/* One frame. on_frame always returns True. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    kit_t *kit = &g_kit;
    const page_t *page = &PAGE_TABLE[kit->page];
    bento_game_keys_t keys;
    int32_t note = 0;
    bento_status_t status;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: the game goes on */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &kit->game,
        &keys); /* never fails: a silent pad is all keys up */
    status = turn_page(kit, &keys);
    if (status == BENTO_OK) {
        status = play_row(kit, page, &keys);
    }
    if ((status == BENTO_OK) &&
        player_step(&kit->game, &kit->player, &note)) {
        status = show_tune(kit, note);
    }
    if ((status == BENTO_OK) && (kit->lamp_left > 0)) {
        kit->lamp_left -= 1;
        if (kit->lamp_left == 0) {
            status = bento_game_box_set_color(&kit->game, &kit->lamp,
                                              COLOR_LAMP_OFF);
        }
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
    kit_t *kit = &g_kit;
    bool quit = false;
    bento_status_t status;

    (void)memset(kit, 0, sizeof(*kit));
    kit->name = "";
    bento_game_init(&kit->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* the opening tune may sleep: the game loop has not started yet */
    status = bento_game_title_melody(&kit->game, "KIT SOUNDS", NULL,
                                     NULL, 0U, &quit);
    report_joystick(&kit->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(kit);
    }
    if ((status == BENTO_OK) && !quit) {
        status = show_page(kit);
    }
    if ((status == BENTO_OK) && !quit) {
        status = say(kit, "เลือกแถวแล้วกด A");
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&kit->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
