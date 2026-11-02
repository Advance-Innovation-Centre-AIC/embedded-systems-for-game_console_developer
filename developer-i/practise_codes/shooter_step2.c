/*
 * shooter_step2.c -- Shooter #1 (คาบ 10) step 2: บังคับยานซ้าย/ขวา (เร่ง + ไถล) ฉบับฝึก
 *
 * เรื่องของ step นี้: ทำให้ยาน "ขับได้และมีน้ำหนัก" เหมือนเข็นรถเข็นในห้าง กดค้างไว้จะค่อย ๆ
 * เร็วขึ้น (เร่ง/ACCEL) พอปล่อยปุ่มก็ไถลต่อแล้วช้าลงเอง (FRICTION) ชนขอบจอแล้วหยุด
 * ปุ่ม -> ความเร็ว (ship_speed) -> ตำแหน่ง (ship_x) -> จอ (bento_game_box_move_to)
 * ช่องที่ต้องเติมยังว่าง ยานจึงยังนิ่ง เฉลย: solution_codes/shooter_step2.py
 *
 * Shooter step 2, the practise skeleton: the title screen, then the green ship and
 * the HUD. Each frame reads the controller; the blank that turns the keys into a
 * speed and a position is still empty, so the ship stays where it is. The game
 * loop handles Back (quit) and Start (pause) by itself.
 *
 * Twin of shooter_step2.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/shooter_step2.py). Style: CLEAN_C.md.
 * What C does differently:
 * - ACCEL, MAX_SPEED and FRICTION are read only by the blank, so they are named in
 *   its comment (an unread constant fails the -Werror build);
 * - the module's globals are one shooter_t at file scope (512 bytes or more, C9);
 * - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end with 0;
 * - joystick.init() prints its line on the kits, so the C prints it there too.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The ship, the HUD and the frame rate. */
enum {
    FPS = 30,
    SHIP_X = 365,
    SHIP_Y = 352,
    SHIP_W = 62,
    SHIP_H = 24,
    HUD_X = 10,
    HUD_Y = 8,
    START_LIVES = 3
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_real_t ship_x; /* ตำแหน่ง x + ความเร็วของยาน */
    bento_real_t ship_speed;
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

static bento_status_t add_ship_and_hud(shooter_t *shooter)
{
    bento_status_t status = bento_game_box_create(
        &shooter->game, &shooter->ship, (bento_real_t)SHIP_X,
        (bento_real_t)SHIP_Y, (bento_real_t)SHIP_W,
        (bento_real_t)SHIP_H, BENTO_GAME_GREEN);

    shooter->ship_x = (bento_real_t)SHIP_X;
    shooter->ship_speed = (bento_real_t)0.0;
    shooter->score = 0;
    shooter->lives = START_LIVES;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&shooter->game, &shooter->hud,
                                        "Score: 0   Lives: 3", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    const bento_status_t status =
        bento_game_keys(&shooter->game, &keys);

    (void)user;        /* the state is g_shooter */
    *out_stop = false; /* this step never ends the game by itself */
    /* ปุ่มระบบ เอนจินจัดการเอง: Back = ออก · Start = พักเกม (Pause) */

    /* ----- เติมส่วนนี้เอง (งานของน้อง ๆ): 5 ขั้นของยานที่มีน้ำหนัก -----
     * ค่าฟิสิกส์เดียวกับเกมเต็ม (shooter_full.py:18): accel 1.4, max_speed 13.0 และ
     *   friction 0.80 ประกาศเป็น const bento_real_t ในฟังก์ชันนี้ บรรทัดละตัว
     * ใช้ keys ที่อ่านมาแล้วด้านบน (keys.left / keys.right)
     *   ① เร่ง: กดซ้าย "เร่งไปทางซ้าย" กดขวา "เร่งไปทางขวา" ปรับ shooter->ship_speed ด้วย accel
     *   ② ไถล: ถ้าไม่กดปุ่มใดเลย ให้ ship_speed คูณ friction (เหลือ 80%)
     *   ③ เพดาน: บีบ ship_speed ให้อยู่ในช่วง -max_speed .. max_speed
     *   ④ ขยับ + ขอบจอ: บวก ship_speed เข้า ship_x แล้วบีบให้อยู่ใน 0 .. (game.width - ship.w)
     *   ⑤ วาด: bento_game_box_move_to ส่ง ship_x และ y เดิมของยาน
     * ถ้าติดตรงไหน เปิดเฉลยเต็มดูได้ที่ solution_codes/shooter_step2 */
    return status;
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bento_status_t status;

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    report_joystick(&shooter->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_ship_and_hud(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
