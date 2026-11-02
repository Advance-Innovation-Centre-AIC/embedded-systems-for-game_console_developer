/*
 * flappy_step1.c -- Flappy #1: นกลอยนิ่งบนท้องฟ้า + คะแนน (ยังไม่ตก)
 *
 * ก่อนที่นกจะบินได้ เราต้องวาดนกขึ้นจอให้ได้ก่อน คาบนี้วางนกนิ่ง ๆ
 * ไว้กลางฟ้า แล้วโชว์คะแนน 0 ยังไม่มีฟิสิกส์หรือท่อ ส่วนที่เขียนเอง: เลือกความสูง
 * เริ่มต้นของนก กด Start เพื่อออกจากเกม
 *
 * Flappy step 1, the solution: the title screen, then the bird as a still
 * box at height 180 and the score "0". Press Start to quit.
 *
 * Twin of flappy_step1.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/flappy_step1.py). Style: CLEAN_C.md.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The bird, the score and the loop. */
enum {
    BIRD_X = 150,
    BIRD_PX = 34,
    BIRD_START_Y = 180, /* ความสูงของนก (พิกเซลจากขอบบน) */
    SCORE_X = 384,
    SCORE_Y = 16,
    FPS = 50 /* ลูปเกม 50 fps */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t bird; /* ตัวนก = กล่องสว่างสุด */
    bento_game_text_t score_label;
    bento_real_t bird_y;
    int32_t score;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    bento_game_keys_t keys;
    /* ทุกเฟรม เราวาดนกไว้ที่ความสูง bird_y ตอนนี้ยังไม่ขยับ เป็นภาพนิ่ง */
    bento_status_t status = bento_game_box_move_to(
        &flappy->game, &flappy->bird, flappy->bird.x, flappy->bird_y);

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    if (status == BENTO_OK) {
        status = bento_game_keys(&flappy->game, &keys);
    }
    if ((status == BENTO_OK) && keys.start) {
        /* กด Start เพื่อออกจากเกม (เหมือนทุกเกม) */
        *out_stop = true;
    }
    /* คาบหน้า: เพิ่มแรงโน้มถ่วงให้นกตก แล้วกด A เพื่อ "กระพือ" ขึ้น */
    return status;
}

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

static bento_status_t add_bird_and_score(flappy_t *flappy)
{
    bento_status_t status;

    /* ----- เติมส่วนนี้เอง: ตำแหน่งเริ่มต้นของนก ----- */
    flappy->bird_y = (bento_real_t)BIRD_START_Y;
    bento_game_box_init(&flappy->bird);
    status = bento_game_box_create(
        &flappy->game, &flappy->bird, (bento_real_t)BIRD_X,
        flappy->bird_y, (bento_real_t)BIRD_PX, (bento_real_t)BIRD_PX,
        BENTO_GAME_GB_LIGHTEST);
    flappy->score = 0;
    if (status == BENTO_OK) {
        bento_game_text_init(&flappy->score_label);
        status = bento_game_text_create(
            &flappy->game, &flappy->score_label, "0", SCORE_X, SCORE_Y,
            BENTO_GAME_WHITE);
    }
    return status;
}

int main(void)
{
    flappy_t *flappy = &g_flappy;
    bool quit = false;
    bento_status_t status;

    (void)memset(flappy, 0, sizeof(*flappy));
    bento_game_init(&flappy->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&flappy->game, "FLAPPY", NULL, &quit);
    report_joystick(&flappy->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_bird_and_score(flappy);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&flappy->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
