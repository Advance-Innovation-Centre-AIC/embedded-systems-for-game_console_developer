/*
 * s04_catch.c -- เกม "Catch" (MVP-1) คาบ 3 ฉบับฝึก โครงเว้น 30%
 *
 * ตะกร้า (กล่อง) วิ่งซ้าย-ขวารับ "ของ" ที่ตกลงมา รับโดน = ได้แต้ม เล่นครบ 30 วินาที
 * แล้วขึ้น GAME OVER + คะแนนรวม ไฟล์นี้เป็นโครงเว้นช่อง: ทุกช่องที่ต้องเติมยังว่าง
 * จึงเห็นหน้าเริ่ม แล้วเห็นตะกร้า ของ และป้ายคะแนนนิ่งอยู่ กด Back เพื่อออก
 * เฉลยอยู่ที่ starter/catch.py · ใบ้: ดูในใบงาน session
 *
 * The Catch game of lesson 3, the practise skeleton with 30% left blank. The title
 * screen waits for Start, then the basket, the falling item and the score label
 * appear and the frame loop runs at 30 fps. Every blank is still empty, so each
 * frame only reads the keys and counts the frame: nothing moves and only Back ends
 * the game.
 *
 * Twin of s04_catch.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/s04_catch.py). Style: CLEAN_C.md.
 * What C does differently:
 * - the module's globals are one catch_game_t at file scope (512 bytes or more, C9);
 * - random is never called before a blank is filled, so no random state is made;
 * - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end with 0;
 * - joystick.init() prints its line on the kits, so the C prints it there too.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The pieces on the screen. */
enum {
    BASKET_X = 360,
    BASKET_Y = 360,
    BASKET_W = 90,
    BASKET_H = 20,
    ITEM_X = 380,
    ITEM_Y = 0,
    ITEM_SIZE = 24,
    SCORE_X = 10,
    SCORE_Y = 8
};

/* The round's numbers: the blanks below use them. */
enum {
    FPS = 30,
    ROUND_SECONDS = 30,
    TOTAL = ROUND_SECONDS * FPS, /* 30 วินาที ที่ 30 fps = 900 เฟรม */
    BASE_SPEED = 6,              /* ความเร็วตะกร้าตอนเพิ่งแตะจอย */
    MAX_SPEED = 30               /* เร็วสูงสุดเมื่อกดค้างนานพอ */
};

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct {
    bento_game_t game;
    bento_game_box_t basket;
    bento_game_box_t item;
    bento_game_text_t score_text;
    int32_t score;
    int32_t frames;
    int32_t hold_frames; /* กดค้างมากี่เฟรมแล้ว (ยิ่งมาก ยิ่งเร็ว) */
} catch_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern catch_game_t g_catch;
catch_game_t g_catch;

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

static bento_status_t add_box(catch_game_t *catch_game,
                              bento_game_box_t *box, rect_t place,
                              uint32_t color)
{
    return bento_game_box_create(
        &catch_game->game, box, (bento_real_t)place.x,
        (bento_real_t)place.y, (bento_real_t)place.w,
        (bento_real_t)place.h, color);
}

/* สร้าง Box / Text ครั้งเดียว (ห้ามสร้างใหม่ทุกเฟรม) */
static bento_status_t add_pieces(catch_game_t *catch_game)
{
    const rect_t basket = {BASKET_X, BASKET_Y, BASKET_W,
                           BASKET_H}; /* ตะกร้า อยู่ล่างจอ */
    const rect_t item = {ITEM_X, ITEM_Y, ITEM_SIZE,
                         ITEM_SIZE}; /* ของ เริ่มบนสุด */
    bento_status_t status = add_box(catch_game, &catch_game->basket,
                                    basket, BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = add_box(catch_game, &catch_game->item, item,
                         BENTO_GAME_GB_LIGHTEST);
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

/* One frame. Each blank is a comment until you fill it in. */
static bento_status_t update(void *user, bool *out_stop)
{
    catch_game_t *catch_game = &g_catch;
    bento_game_keys_t pressed;
    bento_status_t status;

    (void)user; /* the state is g_catch */
    /* only the blank of the 30 second end stops the round */
    *out_stop = false;
    /* คุมตะกร้าด้วยจอย: กดค้างยิ่งนานยิ่งเร็ว (bento_game_box_move กันขอบจอให้เอง) */
    status = bento_game_keys(&catch_game->game, &pressed);
    /* ----- เติมส่วนนี้เอง (งานของคุณ): ขยับตะกร้าซ้าย-ขวาตามจอย -----
     *   1) ถ้ากด pressed.left หรือ pressed.right อยู่ เพิ่ม hold_frames ทีละ 1
     *      ถ้าไม่ได้กด รีเซ็ต hold_frames เป็น 0
     *   2) speed = BASE_SPEED + hold_frames แต่อย่าให้เกิน MAX_SPEED
     *   3) กด left: bento_game_box_move ตะกร้าไปทางลบ X / กด right: ไปทางบวก X */

    /* ----- เติมส่วนนี้เอง (งานของคุณ): ให้ของตกลงมา + รีสปอว์นเมื่อพ้นจอล่าง -----
     *   ใช้ bento_game_box_move_to (ไม่กันขอบ) ให้ item.y เลยจอล่างได้จริง
     *   1) เลื่อน item ลงทีละนิด: x เดิม y เพิ่มขึ้น (เลือกก้าวเอง)
     *   2) ถ้า item.y พ้น game.height แล้ว ย้ายกลับไป y = 0 ที่ x สุ่ม
     *      (bento_random ในช่วงความกว้างจอ เผื่อความกว้างของ item) */

    /* ----- เติมส่วนนี้เอง (งานของคุณ): ตรวจชน บวกแต้ม อัปเดตป้าย เล่นเสียง รีสปอว์น -----
     *   ถ้า bento_game_hit(&basket, &item): score += 1, bento_game_text_set ป้ายคะแนน,
     *   bento_game_sfx เสียงได้แต้ม แล้วรีสปอว์นของที่ x สุ่ม */

    /* จับเวลา 30 วินาที แล้วจบรอบ */
    catch_game->frames += 1;
    /* ----- เติมส่วนนี้เอง (งานของคุณ): หมดเวลา ขึ้น GAME OVER แล้วจบ -----
     *   เมื่อ frames ถึง TOTAL: bento_game_text_create "GAME OVER" กลางจอ, อีกป้ายโชว์คะแนนรวม,
     *   bento_game_sfx เสียงจบเกม แล้วตั้ง *out_stop = true ให้ bento_game_run หยุดลูป */
    return status;
}

int main(void)
{
    catch_game_t *catch_game = &g_catch;
    bool quit = false;
    bento_status_t status;

    (void)memset(catch_game, 0, sizeof(*catch_game));
    bento_game_init(&catch_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น  Back=ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&catch_game->game, "CATCH", NULL, &quit);
    report_joystick(&catch_game->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_pieces(catch_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        /* Back = ออก / Start = พักเกม: bento_game_run จัดการให้ */
        status = bento_game_run(&catch_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
