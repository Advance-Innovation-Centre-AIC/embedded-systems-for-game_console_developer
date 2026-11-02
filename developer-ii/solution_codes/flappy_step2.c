/*
 * flappy_step2.c -- สร้าง Flappy #2: แรงโน้มถ่วง + กระพือปีก
 *
 * คาบนี้ทำให้นกมีชีวิต: โลกในเกมมีแรงโน้มถ่วงดึงนกลงทุกเฟรม ถ้าไม่ทำอะไร นกก็ร่วงลง
 * กด A (หรือ UP) นกจะกระพือปีกดีดตัวขึ้น กันชนเพดานบน และตกพื้น = จบเกม
 *
 * Flappy step 2, the solution: gravity pulls the bird down every frame, A or
 * UP flaps it up with the "flap" sound, the ceiling stops it, and touching
 * the floor shows "GAME OVER" and ends the game loop.
 *
 * Twin of flappy_step2.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/flappy_step2.py). Style: CLEAN_C.md.
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
    BIRD_START_Y = 180,
    OVER_X = 320,
    OVER_Y = 180,
    SCORE_X = 384,
    SCORE_Y = 16,
    FPS = 50 /* ลูปเกม 50 fps */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t bird; /* ตัวนก = กล่องสว่างสุด */
    bento_game_text_t score_label;
    bento_real_t bird_y;        /* ความสูงของนก */
    bento_real_t bird_velocity; /* + = ลง, - = ขึ้น */
    int32_t score;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

/* ฟิสิกส์ของ flappy: กระพือ + โน้มถ่วง + เพดาน */
static void move_bird(flappy_t *flappy, const bento_game_keys_t *keys)
{
    const bento_real_t gravity = (bento_real_t)0.45; /* ดึงลงต่อเฟรม */
    const bento_real_t flap = (bento_real_t)-7.4;    /* ดีดขึ้น */

    if (keys->a || keys->up) {
        flappy->bird_velocity = flap; /* กระพือ = ดีดขึ้น */
        bento_game_sfx(&flappy->game, "flap");
    }
    flappy->bird_velocity += gravity;        /* โน้มถ่วงดึงลงทุกเฟรม */
    flappy->bird_y += flappy->bird_velocity; /* เลื่อนนกตามความเร็ว */
    if (flappy->bird_y < (bento_real_t)0) {
        /* ชนเพดานบน -> หยุดความเร็ว */
        flappy->bird_y = (bento_real_t)0;
        flappy->bird_velocity = (bento_real_t)0;
    }
}

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    /* ----- เติมส่วนนี้เอง: ฟิสิกส์ของ flappy ----- */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&flappy->game, &keys);
    if (status == BENTO_OK) {
        move_bird(flappy, &keys);
    }
    if ((status == BENTO_OK) &&
        (flappy->bird_y >
         ((bento_real_t)flappy->game.height - flappy->bird.h))) {
        bento_game_text_t over;

        /* ตกพื้น -> จบเกม */
        bento_game_text_init(&over);
        status =
            bento_game_text_create(&flappy->game, &over, "GAME OVER",
                                   OVER_X, OVER_Y, BENTO_GAME_RED);
        *out_stop = true;
    } else if (status == BENTO_OK) {
        status = bento_game_box_move_to(&flappy->game, &flappy->bird,
                                        flappy->bird.x, flappy->bird_y);
    } else {
        /* the status ends the game loop */
    }
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

    bento_game_box_init(&flappy->bird);
    status = bento_game_box_create(
        &flappy->game, &flappy->bird, (bento_real_t)BIRD_X,
        flappy->bird_y, (bento_real_t)BIRD_PX, (bento_real_t)BIRD_PX,
        BENTO_GAME_GB_LIGHTEST);
    flappy->bird_y = (bento_real_t)BIRD_START_Y;
    flappy->bird_velocity = (bento_real_t)0;
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
