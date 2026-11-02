/*
 * flappy_step2.c -- สร้าง Flappy #2: แรงโน้มถ่วง + กระพือปีก
 *
 * คาบนี้ทำให้นกมีชีวิต: โลกในเกมมีแรงโน้มถ่วงดึงนกลงทุกเฟรม ถ้าไม่ทำอะไร นกก็ร่วงลง
 * กด A (หรือ UP) นกจะกระพือปีกดีดตัวขึ้น กันชนเพดานบน และตกพื้น = จบเกม
 * โครงเว้น 30%: ฟิสิกส์ของนกเว้นไว้ให้เติมเอง
 *
 * Flappy step 2, the practise skeleton: the bird and the score are on the
 * screen and redrawn every frame. The physics (flap, gravity, ceiling, floor
 * = game over) is left as TODOs; until then the bird stays at height 180.
 *
 * Twin of flappy_step2.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/flappy_step2.py). Style: CLEAN_C.md.
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

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    bento_status_t status;

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    *out_stop = false; /* the TODO below is where it may end the loop */
    /* ----- เติมส่วนนี้เอง (งานของคุณ): ฟิสิกส์ของนก (กระพือ + โน้มถ่วง + ตกพื้นจบเกม) -----
     * TODO 0) ประกาศค่าคงที่ const bento_real_t สองตัว: gravity = 0.45 (แรงโน้มถ่วง
     *         ดึงลงต่อเฟรม) และ flap = -7.4 (แรงกระพือ ดีดขึ้น)
     * TODO 1) อ่านปุ่มด้วย bento_game_keys เก็บไว้ในตัวแปร แล้วเช็คว่ากด a หรือ up ไหม
     * TODO 2) ถ้ากด: ตั้ง bird_velocity ให้เท่ากับ flap (ดีดขึ้น = ความเร็วติดลบ)
     *         แล้วเล่นเสียงด้วย bento_game_sfx โดยส่งชื่อเสียง "flap" เข้าไป
     * TODO 3) ทุกเฟรม (นอก if กด): บวก gravity เข้า bird_velocity เพื่อให้โน้มถ่วงดึงลง
     *         แล้วบวก bird_velocity เข้า bird_y เพื่อเลื่อนนกตามความเร็ว
     * TODO 4) กันชนเพดาน: ถ้า bird_y น้อยกว่า 0 ให้รีเซ็ต bird_y และ bird_velocity เป็น 0
     * TODO 5) ตกพื้นจบเกม: ถ้า bird_y มากกว่า game.height - bird.h ให้แสดงข้อความ
     *         GAME OVER ด้วย bento_game_text_create แล้วตั้ง *out_stop = true */
    status = bento_game_box_move_to(&flappy->game, &flappy->bird,
                                    flappy->bird.x, flappy->bird_y);
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
