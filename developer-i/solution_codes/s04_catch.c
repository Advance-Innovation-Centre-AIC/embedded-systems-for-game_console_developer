/*
 * s04_catch.c -- เกม "Catch" (MVP-1) สำหรับคาบ 3
 *
 * ตะกร้า (กล่อง) วิ่งซ้าย-ขวารับ "ของ" ที่ตกลงมา รับโดน = ได้แต้ม
 * ของที่ตกพ้นล่างจอไปโผล่ที่ใหม่ด้านบน กดจอยค้างยิ่งนานตะกร้ายิ่งเร็ว
 * เล่นครบ 30 วินาที แล้วขึ้น GAME OVER + คะแนนรวม
 * Back = ออก · Start = พักเกม
 *
 * Catch, MVP-1: a basket runs left and right along the bottom of the
 * screen to catch an item that falls from the top. A catch scores a
 * point; a missed item comes back at a random place on top. After 30
 * seconds (900 frames at 30 fps) the round ends with GAME OVER and the
 * score.
 *
 * Twin of s04_catch.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s04_catch.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - update() is the function bento_game_run calls each frame; it reads the
 *     file-scope state, not its void * argument; Python's
 *     `return False` sets *out_stop;
 *   - hold_frames stops counting at MAX_SPEED (past it the speed is the same),
 *     so the int32_t never overflows where Python's int would keep growing;
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
#include "bento_random.h"
#include "bento_runtime.h"

/* The basket, the item and the score label. */
enum {
    BASKET_X = 360,
    BASKET_Y = 360, /* ตะกร้า อยู่ล่างจอ */
    BASKET_W = 90,
    BASKET_H = 20,
    ITEM_X = 380, /* ของ เริ่มบนสุด */
    ITEM_PX = 24,
    FALL_PX = 8, /* ของตกลงเฟรมละเท่านี้ */
    SCORE_X = 10,
    SCORE_Y = 8,
    OVER_X = 320,
    OVER_Y = 170,
    FINAL_X = 330,
    FINAL_Y = 210,
    SCORE_CHARS = 20 /* "Score: " and any int32_t */
};

/* The time and the basket's speed. */
enum {
    FPS = 30,
    TOTAL = 30 * FPS, /* 30 วินาที ที่ 30 fps = 900 เฟรม */
    BASE_SPEED = 6,   /* ความเร็วตะกร้าตอนเพิ่งแตะจอย */
    MAX_SPEED = 30    /* เร็วสูงสุดเมื่อกดค้างนานพอ */
};

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t basket;
    bento_game_box_t item;
    bento_game_text_t score_text;
    int32_t score;
    int32_t frames;
    int32_t hold_frames; /* กดค้างมากี่เฟรมแล้ว (ยิ่งมาก ยิ่งเร็ว) */
} catch_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern catch_game_t g_catch;
catch_game_t g_catch;

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

/* คุมตะกร้าด้วยจอย: กดค้างยิ่งนานยิ่งเร็ว (move() clamp ขอบจออัตโนมัติ) */
static bento_status_t steer_basket(catch_game_t *catch_game,
                                   const bento_game_keys_t *pressed)
{
    const bento_real_t none = (bento_real_t)0;
    bento_status_t status = BENTO_OK;
    int32_t speed;

    if (!pressed->left && !pressed->right) {
        catch_game->hold_frames = 0;
    } else if (catch_game->hold_frames < MAX_SPEED) {
        catch_game->hold_frames += 1;
    } else {
        /* already at the top speed */
    }
    speed = BASE_SPEED + catch_game->hold_frames;
    if (speed > MAX_SPEED) {
        speed = MAX_SPEED;
    }
    if (pressed->left) {
        status =
            bento_game_box_move(&catch_game->game, &catch_game->basket,
                                -(bento_real_t)speed, none);
    }
    if ((status == BENTO_OK) && pressed->right) {
        status =
            bento_game_box_move(&catch_game->game, &catch_game->basket,
                                (bento_real_t)speed, none);
    }
    return status;
}

/* The item back on top, at a random x. */
static bento_status_t respawn_item(catch_game_t *catch_game)
{
    int32_t new_x = 0;
    bento_status_t status =
        bento_random_randint(&catch_game->random, 0,
                             catch_game->game.width - ITEM_PX, &new_x);

    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &catch_game->game, &catch_game->item, (bento_real_t)new_x,
            (bento_real_t)0);
    }
    return status;
}

/* ของตกลงมา; ตกพ้นล่างจอ = พลาด แล้วไปโผล่ที่ใหม่ด้านบน. move_to() does
 * not clamp, so the item really passes the bottom edge. */
static bento_status_t drop_item(catch_game_t *catch_game)
{
    bento_status_t status = bento_game_box_move_to(
        &catch_game->game, &catch_game->item, catch_game->item.x,
        catch_game->item.y + (bento_real_t)FALL_PX);

    if ((status == BENTO_OK) &&
        (catch_game->item.y > (bento_real_t)catch_game->game.height)) {
        status = respawn_item(catch_game);
    }
    return status;
}

/* ชนแล้วได้แต้ม (หัวใจของคาบ) */
static bento_status_t catch_item(catch_game_t *catch_game)
{
    char text[SCORE_CHARS];
    bento_status_t status;

    catch_game->score += 1;
    (void)snprintf(text, sizeof(text), "Score: %" PRId32,
                   catch_game->score);
    status = bento_game_text_set(&catch_game->game,
                                 &catch_game->score_text, text);
    if (status == BENTO_OK) {
        /* เสียง "ได้แต้ม" (เงียบถ้า firmware ไม่รองรับ) */
        bento_game_sfx(&catch_game->game, "eat");
        status = respawn_item(catch_game);
    }
    return status;
}

/* จับเวลา 30 วินาทีครบแล้ว: GAME OVER และคะแนนรวม */
static bento_status_t show_game_over(catch_game_t *catch_game)
{
    char text[SCORE_CHARS];
    bento_game_text_t over;
    bento_game_text_t final_score;
    bento_status_t status;

    bento_game_text_init(&over);
    bento_game_text_init(&final_score);
    status =
        bento_game_text_create(&catch_game->game, &over, "GAME OVER",
                               OVER_X, OVER_Y, BENTO_GAME_RED);
    (void)snprintf(text, sizeof(text), "Score: %" PRId32,
                   catch_game->score);
    if (status == BENTO_OK) {
        status = bento_game_text_create(&catch_game->game, &final_score,
                                        text, FINAL_X, FINAL_Y,
                                        BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        bento_game_sfx(&catch_game->game, "gameover");
    }
    return status;
}

/* One frame: steer, drop, catch, and count down the 30 seconds. */
static bento_status_t update(void *user, bool *out_stop)
{
    catch_game_t *catch_game = &g_catch;
    bento_game_keys_t pressed;
    bento_status_t status;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    bento_game_keys_init(&pressed);
    status = bento_game_keys(&catch_game->game, &pressed);
    if (status == BENTO_OK) {
        status = steer_basket(catch_game, &pressed);
    }
    if (status == BENTO_OK) {
        status = drop_item(catch_game);
    }
    if ((status == BENTO_OK) &&
        bento_game_hit(&catch_game->basket, &catch_game->item)) {
        status = catch_item(catch_game);
    }
    if (status == BENTO_OK) {
        catch_game->frames += 1;
        if (catch_game->frames >= TOTAL) {
            status = show_game_over(catch_game);
            *out_stop = true; /* หยุด game.run() */
        }
    }
    return status;
}

/* สร้าง Box / Text ครั้งเดียว (ห้ามสร้างใหม่ทุกเฟรม) */
static bento_status_t build_pieces(catch_game_t *catch_game)
{
    bento_status_t status = bento_game_box_create(
        &catch_game->game, &catch_game->basket, (bento_real_t)BASKET_X,
        (bento_real_t)BASKET_Y, (bento_real_t)BASKET_W,
        (bento_real_t)BASKET_H, BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &catch_game->game, &catch_game->item, (bento_real_t)ITEM_X,
            (bento_real_t)0, (bento_real_t)ITEM_PX,
            (bento_real_t)ITEM_PX, BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        /* ป้ายคะแนน มุมซ้ายบน */
        status = bento_game_text_create(
            &catch_game->game, &catch_game->score_text, "Score: 0",
            SCORE_X, SCORE_Y, BENTO_GAME_WHITE);
    }
    catch_game->score = 0;
    catch_game->frames = 0;
    catch_game->hold_frames = 0;
    return status;
}

int main(void)
{
    catch_game_t *catch_game = &g_catch;
    bool quit = false;
    bento_status_t status;

    (void)memset(catch_game, 0, sizeof(*catch_game));
    status = bento_random_init(&catch_game->random);
    bento_game_init(&catch_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&catch_game->basket);
    bento_game_box_init(&catch_game->item);
    bento_game_text_init(&catch_game->score_text);
    if (status == BENTO_OK) {
        /* หน้าเริ่ม: Start=เล่น Back=ออก (title ทำ start ให้ในตัว) */
        status =
            bento_game_title(&catch_game->game, "CATCH", NULL, &quit);
        report_joystick(&catch_game->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_pieces(catch_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&catch_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
