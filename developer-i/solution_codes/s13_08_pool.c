/*
 * s13_08_pool.c -- คาบ 13 ส่วนที่ 3: ชุดหมุนเวียน (ยืม-คืน)
 *
 * สร้างของ 8 ตัวครั้งเดียวตอนเริ่ม แล้วยืม-คืนใช้ซ้ำ ไม่สร้างใหม่
 * ระหว่างเล่น ว่าง = y < -20 (จอดรอนอกจอ) กด A = ยืม 1 ตัว
 * ปล่อยขึ้นจากช่องของมันเอง ช่องเปลี่ยนเขียว -> ส้ม ตอนยืม และกลับ
 * เป็นเขียวตอนของบินพ้นจอ ขีดขาว = ช่องที่จะยืมต่อไป ครบ 8 แล้ว
 * กดอีก = เสียงปฏิเสธ
 *
 * Eight items are made once at the start and then borrowed and
 * returned, never made again during play. Free means y < -20, parked
 * above the screen. A borrows one, which rises from its own locker;
 * the locker turns from green to orange, and back to green when the
 * item has flown off the top. The white bar marks the locker the next
 * borrow takes. With all 8 out, A plays the deny sound.
 *
 * Twin of s13_08_pool.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_08_pool.py).
 * Style: CLEAN_C.md. What C does differently:
 * - find_free() answers an index, or -1 where Python answers None;
 * - item.w, which a C Sprite does not have, is an array of widths;
 * - the shown dict is fields of the struct, -1 for "never shown"
 *   and for None;
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

/* The pool and its lockers (แก้ได้). */
enum {
    POOL = 8, /* ของ 1 ตัวกิน 2 ชิ้น คือตัวมันกับช่องของมัน */
    SPEED_COUNT = 3,
    LOCK_X0 = 34,
    LOCK_Y = 316,
    LOCK_W = 44,
    LOCK_H = 30,
    LOCK_GAP = 54,
    LOCK_RADIUS = 4,
    LAUNCH_Y = LOCK_Y - 22, /* จุดปล่อยของ เหนือช่องของตัวเอง */
    CURSOR_GAP = 5,
    CURSOR_H = 4,
    PARKED_Y = -20, /* above this an item is free */
    ITEM_PARK = -100,
    ENEMY_W = 16,
    ENEMY2_W = 21,
    FPS = 30,
    TEXT_CHARS = 64
};

/* The texts and the return note. */
enum {
    HUD_X = 500,
    TITLE_Y = 20,
    COUNT_Y = 58,
    MESSAGE_Y = 96,
    LEGEND_Y = 150,
    CURSOR_TEXT_Y = 186,
    STORY_Y = 240,
    NOTE_RETURN = 76,
    NOTE_MS = 40,
    VELOCITY = 100
};

enum {
    COLOR_FREE = BENTO_GAME_GREEN,
    COLOR_USED = BENTO_GAME_ORANGE,
    COLOR_STORY = 0x9AA8B8
};

typedef struct {
    bento_game_t game;
    bento_game_box_t lockers[POOL];
    bento_game_box_t cursor;
    bento_game_sprite_t items[POOL];
    int32_t item_w[POOL];
    bento_game_text_t hud_count;
    bento_game_text_t hud_message;
    int32_t shown_free;
    int32_t shown_next;
} pool_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern pool_t g_pool;
pool_t g_pool;

/* จอดรอนอกจอ = ว่าง */
static bool is_free(const bento_game_sprite_t *item)
{
    return item->y < (bento_real_t)PARKED_Y;
}

/* ไล่จากช่องซ้ายสุด เจอตัวว่างตัวแรกก็ใช้เลย; -1 = ใช้อยู่ครบ */
static int32_t find_free(const pool_t *pool)
{
    int32_t found = -1;
    int32_t i;

    for (i = 0; (i < POOL) && (found < 0); i++) {
        if (is_free(&pool->items[i])) {
            found = i;
        }
    }
    return found;
}

static int32_t count_free(const pool_t *pool)
{
    int32_t count = 0;
    int32_t i;

    for (i = 0; i < POOL; i++) {
        if (is_free(&pool->items[i])) {
            count += 1;
        }
    }
    return count;
}

static int32_t locker_x(int32_t i)
{
    return LOCK_X0 + (i * LOCK_GAP);
}

/* ความเร็วบินขึ้น (px ต่อเฟรม) วนตามเลขช่อง 0, 1, 2, 0, 1, ... */
static int32_t item_speed(int32_t i)
{
    static const int32_t speeds[SPEED_COUNT] = {2, 3, 4};

    return speeds[i % SPEED_COUNT];
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

/* สร้างครบ 8 ตัวตรงนี้ที่เดียว ในวงวนหลักไม่สร้างเพิ่มเลย */
static bento_status_t add_items(pool_t *pool)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < POOL); i++) {
        const bool even = (i % 2) == 0;

        status = bento_game_sprite_create(
            &pool->game, &pool->items[i], even ? "enemy" : "enemy2",
            (bento_real_t)ITEM_PARK, (bento_real_t)ITEM_PARK);
        pool->item_w[i] = even ? ENEMY_W : ENEMY2_W;
        if (status == BENTO_OK) {
            status =
                bento_game_sprite_hide(&pool->game, &pool->items[i]);
        }
    }
    return status;
}

static bento_status_t add_lockers(pool_t *pool)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < POOL); i++) {
        status = bento_game_box_create_styled(
            &pool->game, &pool->lockers[i], (bento_real_t)locker_x(i),
            (bento_real_t)LOCK_Y, (bento_real_t)LOCK_W,
            (bento_real_t)LOCK_H, COLOR_FREE, (int32_t)BENTO_GAME_WHITE,
            LOCK_RADIUS, 0);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &pool->game, &pool->cursor, (bento_real_t)locker_x(0),
            (bento_real_t)(LOCK_Y + LOCK_H + CURSOR_GAP),
            (bento_real_t)LOCK_W, (bento_real_t)CURSOR_H,
            BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t add_label(pool_t *pool, const char *text,
                                int32_t y, uint32_t color)
{
    bento_game_text_t label;

    bento_game_text_init(&label);
    return bento_game_text_create(&pool->game, &label, text, HUD_X, y,
                                  color);
}

static bento_status_t add_texts(pool_t *pool)
{
    char text[TEXT_CHARS];
    bento_status_t status;

    (void)snprintf(text, sizeof(text), "POOL %d    free = y < -20",
                   POOL);
    status = add_label(pool, text, TITLE_Y, BENTO_GAME_CYAN);
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(&pool->game, &pool->hud_count, "",
                                   HUD_X, COUNT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(&pool->game, &pool->hud_message,
                                        "A = ยืม 1 ตัว", HUD_X, MESSAGE_Y,
                                        BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = add_label(pool, "เขียว = ว่าง    ส้ม = ใช้อยู่", LEGEND_Y,
                           BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_label(pool, "ขีดขาว = ช่องที่จะยืมต่อไป", CURSOR_TEXT_Y,
                           BENTO_GAME_WHITE);
    }
    (void)snprintf(text, sizeof(text), "ลานจอด %d ช่อง / ถาดบนสายพาน",
                   POOL);
    if (status == BENTO_OK) {
        status = add_label(pool, text, STORY_Y, COLOR_STORY);
    }
    return status;
}

static bento_status_t say_locker(pool_t *pool, int32_t i, bool borrowed)
{
    char text[TEXT_CHARS];

    if (borrowed) {
        (void)snprintf(text, sizeof(text), "ยืมช่อง %" PRId32 " ไปแล้ว",
                       i + 1);
    } else {
        (void)snprintf(text, sizeof(text), "คืนช่อง %" PRId32 " แล้ว",
                       i + 1);
    }
    return bento_game_text_set(&pool->game, &pool->hud_message, text);
}

/* ยืม: กด A หนึ่งครั้ง = ขอหนึ่งตัว */
static bento_status_t borrow(pool_t *pool)
{
    const int32_t i = find_free(pool);
    bento_status_t status;

    if (i < 0) {
        status = bento_game_text_set(&pool->game, &pool->hud_message,
                                     "เต็ม! รอของบินพ้นจอก่อน");
        bento_game_sfx(&pool->game, "deny");
    } else {
        bento_game_sprite_t *item = &pool->items[i];

        const int32_t x =
            locker_x(i) + ((LOCK_W - pool->item_w[i]) / 2);

        status = bento_game_sprite_move_to(
            &pool->game, item, (bento_real_t)x, (bento_real_t)LAUNCH_Y);
        if (status == BENTO_OK) {
            status = bento_game_sprite_show(&pool->game, item);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                &pool->game, &pool->lockers[i], COLOR_USED);
        }
        if (status == BENTO_OK) {
            status = say_locker(pool, i, true);
        }
        bento_game_sfx(&pool->game, "select");
    }
    return status;
}

/* เพิ่งพ้น -20 = คืนเข้าชุดแล้ว */
static bento_status_t give_back(pool_t *pool, int32_t i)
{
    bento_status_t status =
        bento_game_sprite_hide(&pool->game, &pool->items[i]);

    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &pool->game, &pool->lockers[i], COLOR_FREE);
    }
    if (status == BENTO_OK) {
        status = say_locker(pool, i, false);
    }
    bento_game_tone(&pool->game, NOTE_RETURN, BENTO_GAME_WAVE_DEFAULT,
                    VELOCITY, NOTE_MS);
    return status;
}

/* One item in use flies up; it goes back once past the top. */
static bento_status_t fly_item(pool_t *pool, int32_t i)
{
    bento_game_sprite_t *item = &pool->items[i];
    bento_status_t status = bento_game_sprite_move_to(
        &pool->game, item, item->x,
        item->y - (bento_real_t)item_speed(i));

    if ((status == BENTO_OK) && is_free(item)) {
        status = give_back(pool, i);
    }
    return status;
}

/* อัปเดตเฉพาะตัวที่ใช้อยู่ + คืนเมื่อพ้นขอบบน */
static bento_status_t fly_items(pool_t *pool)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < POOL); i++) {
        if (!is_free(&pool->items[i])) {
            status = fly_item(pool, i);
        }
    }
    return status;
}

/* The white bar moves under the next free locker, or hides. */
static bento_status_t show_cursor(pool_t *pool, int32_t next_free)
{
    bento_status_t status;

    if (next_free < 0) {
        status = bento_game_box_hide(&pool->game, &pool->cursor);
    } else {
        status = bento_game_box_move_to(
            &pool->game, &pool->cursor,
            (bento_real_t)locker_x(next_free),
            (bento_real_t)(LOCK_Y + LOCK_H + CURSOR_GAP));
        if (status == BENTO_OK) {
            status = bento_game_box_show(&pool->game, &pool->cursor);
        }
    }
    return status;
}

/* โชว์: เปลี่ยนเฉพาะเมื่อจำนวนว่างหรือช่องถัดไปเปลี่ยน */
static bento_status_t show_pool(pool_t *pool)
{
    const int32_t free_count = count_free(pool);
    const int32_t next_free = find_free(pool);
    bento_status_t status = BENTO_OK;

    if (free_count != pool->shown_free) {
        char text[TEXT_CHARS];

        (void)snprintf(text, sizeof(text),
                       "FREE %" PRId32 "    USED %" PRId32, free_count,
                       POOL - free_count);
        status =
            bento_game_text_set(&pool->game, &pool->hud_count, text);
        pool->shown_free = free_count;
    }
    if ((status == BENTO_OK) && (next_free != pool->shown_next)) {
        status = show_cursor(pool, next_free);
        pool->shown_next = next_free;
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    pool_t *pool = &g_pool;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;        /* the state is g_pool */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&pool->game, &keys);
    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&pool->game, BENTO_GAME_KEY_A, &keys)) {
        status = borrow(pool);
    }
    if (status == BENTO_OK) {
        status = fly_items(pool);
    }
    if (status == BENTO_OK) {
        status = show_pool(pool);
    }
    return status;
}

static void reset_state(pool_t *pool)
{
    int32_t i;

    (void)memset(pool, 0, sizeof(*pool));
    bento_game_init(&pool->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    for (i = 0; i < POOL; i++) {
        bento_game_box_init(&pool->lockers[i]);
        bento_game_sprite_init(&pool->items[i]);
    }
    bento_game_box_init(&pool->cursor);
    bento_game_text_init(&pool->hud_count);
    bento_game_text_init(&pool->hud_message);
    pool->shown_free = -1;
    pool->shown_next = 0; /* the bar starts under locker 1 */
}

int main(void)
{
    pool_t *pool = &g_pool;
    bool quit = false;
    bento_status_t status;

    reset_state(pool);
    status = bento_game_title(&pool->game, "POOL", NULL, &quit);
    report_joystick(&pool->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_lockers(pool);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_items(pool);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_texts(pool);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&pool->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
