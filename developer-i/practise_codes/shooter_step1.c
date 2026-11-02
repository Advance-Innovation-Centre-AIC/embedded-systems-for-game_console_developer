/*
 * shooter_step1.c -- Shooter #1 (คาบ 10) step 1: วางยาน + ป้าย HUD (ยังไม่ขยับ) ฉบับฝึก
 *
 * เรื่องราวของ step นี้: เราแค่ "วางตัวละครลงสนาม" ก่อน คือยานอวกาศ 1 ลำ กับป้ายคะแนน
 * ด้านบน ยังเป็นภาพนิ่ง (ยังไม่อ่านปุ่ม ยังไม่ยิง ยังไม่มีศัตรู) HUD = ป้ายสถานะบนจอ
 * เช่น คะแนน ชีวิต ไม่ใช่ตัวละคร ช่องที่ต้องเติมยังว่าง จึงเห็นหน้าเริ่ม แล้วสนามว่าง
 * เฉลย: solution_codes/shooter_step1.py · ใบ้: ใบงานคาบ 10 + สไลด์ session-10 "step 1"
 *
 * Shooter step 1, the practise skeleton: the title screen, then the field where
 * you place the ship and the score HUD. The blank is still empty and the frame
 * function does nothing yet, so the field shows only the game loop's own key
 * hint. The game loop handles Back (quit) and Start (pause) by itself.
 *
 * Twin of shooter_step1.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/shooter_step1.py). Style: CLEAN_C.md.
 * What C does differently:
 * - ship = None and hud = None are a Box and a Text never created (id 0);
 * - the module's globals are one shooter_t at file scope (512 bytes or more, C9);
 * - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end with 0;
 * - joystick.init() prints its line on the kits, so the C prints it there too.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_runtime.h"

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"

enum {
    FPS = 30,
    SHIP_START_X = 365, /* ตำแหน่ง x ของยาน (เก็บเป็น float) */
    START_LIVES = 3
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ship; /* ยานของผู้เล่น (เดี๋ยวคุณสร้างเอง) */
    bento_game_text_t hud; /* ป้าย HUD (เดี๋ยวคุณสร้างเอง) */
    bento_real_t ship_x;
    int32_t score;
    int32_t lives;
} shooter_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

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
    if ((bento_gpio_board_info(&info) == BENTO_OK) && (info.name != NULL)) {
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

/* ภาพนิ่ง step 2 จะทำให้ยานขยับ on_frame ที่นี่ยังไม่ขยับอะไร */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    (void)user;
    (void)out_stop;
    return BENTO_OK; /* ยังไม่มี logic ภาพนิ่งก่อน */
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bento_status_t status;

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(), bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    report_joystick(&shooter->game);

    /* ตัวแปรพื้นฐานของเกม (เก็บไว้ให้โค้ดข้างล่างไม่พัง ห้ามลบ) */
    shooter->ship_x = (bento_real_t)SHIP_START_X;
    shooter->score = 0;
    shooter->lives = START_LIVES;

    /* ===== เติมส่วนนี้เอง (งานของคุณ): วางยานของผู้เล่น + ป้าย HUD =====
     * ① สร้างยานด้วย bento_game_box_create(&shooter->game, &shooter->ship, ...)
     *    เป็นสี่เหลี่ยมแนวนอน (กว้างกว่าสูง) วางกลางจอตามแนวนอน ใกล้ขอบล่าง สี BENTO_GAME_GREEN
     * ② สร้างป้าย HUD ด้วย bento_game_text_create(&shooter->game, &shooter->hud, ...)
     *    ข้อความ "Score: 0   Lives: 3" มุมบนซ้าย สี BENTO_GAME_WHITE
     * เขียนแต่ละขั้นใน if (status == BENTO_OK) แบบบรรทัดข้างล่าง
     * (ลองกะค่าพิกัด/ขนาดเองก่อนนะ ถ้าติดค่อยเปิดดู solution_codes/shooter_step1) */

    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        /* Back = ออก / Start = พักเกม (เอนจินจัดการเอง) */
        status = bento_game_run(&shooter->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
