/*
 * shooter_step3.c -- Shooter #2 (คาบ 11): เลเซอร์จาก "ชุดกระสุนหมุนเวียน" 6 นัด ฉบับฝึก
 *
 * เทคนิคของคาบ: Object Pool คาบนี้เรียกว่า "ชุดหมุนเวียน" สร้างกระสุน 6 นัดครั้งเดียว
 * ก่อนเกมเริ่ม แล้วยืม-คืนใช้ซ้ำ ไม่สร้างใหม่ใน on_frame (จอมีได้ไม่เกิน 32 ชิ้นรวมทุกอย่าง)
 * ว่าง ⇔ bullet.y < -20 (จอดรอเหนือจอ) · ใช้อยู่ ⇔ bullet.y >= -20 (กำลังบิน)
 * hide แค่ซ่อนให้มองไม่เห็น สิ่งที่ทำให้ "ว่าง" คือค่า y ที่ต่ำกว่า -20
 * 5 ขั้น: ① สร้างชุดครั้งเดียว ② หาตัวว่าง ③ ยืม ④ อัปเดตทุกเฟรม ⑤ คืน
 * ที่ต้องเติม: "งานของคุณ 1" (คูลดาวน์ + ③) และ "งานของคุณ 2" (④ ⑤)
 * เฉลย: solution_codes/shooter_step3.py · ใบ้: สไลด์คาบ 11 หน้า "งานของเรา" และใบงานคาบ 11
 *
 * Shooter step 3, the practise skeleton: the title screen, then the ship that
 * steers with step 2's acceleration and friction, the HUD, and a pool of six
 * hidden bullets made once. The two blanks (fire with a cooldown; fly the bullets
 * up and park them) are still empty, so nothing is fired yet. The game loop
 * handles Back (quit) and Start (pause) by itself.
 *
 * Twin of shooter_step3.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/shooter_step3.py). Style: CLEAN_C.md.
 * What C does differently:
 * - find_free_bullet() is read only by the blank, and an uncalled static function
 *   fails the -Werror build, so blank 1 says how to write it;
 * - max(low, min(high, x)) is clamp(); reals are bento_real_t, MicroPython's float;
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
    START_LIVES =
        3 /* score กับ lives ยังไม่ใช้ในคาบนี้ (คาบ 12 ใช้นับคะแนน/ชีวิต) */
};

/* The bullet pool. */
enum {
    MAX_BULLETS =
        6, /* ขนาดชุดหมุนเวียน (เกมเต็มใช้ 4: shooter_full.py:30) */
    BULLET_W = 6,
    BULLET_H = 14,
    PARKED_Y = -50,   /* จอดรอเหนือจอ */
    FREE_LINE_Y = -20 /* ว่าง ⇔ y < -20 */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_game_box_t bullets[MAX_BULLETS];
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t fire_cooldown;
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
    shooter->fire_cooldown = 0;
    shooter->score = 0;
    shooter->lives = START_LIVES;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&shooter->game, &shooter->hud,
                                        "Score: 0   Lives: 3", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* ① สร้างชุดครั้งเดียว (ให้มาแล้ว ห้ามย้ายเข้าไปใน on_frame): 6 นัด จอดรอที่ y = -50 เหนือจอ */
static bento_status_t add_bullet_pool(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < MAX_BULLETS); i++) {
        status = bento_game_box_create(
            &shooter->game, &shooter->bullets[i], (bento_real_t)0,
            (bento_real_t)PARKED_Y, (bento_real_t)BULLET_W,
            (bento_real_t)BULLET_H, BENTO_GAME_CYAN);
    }
    /* ซ่อนไว้ก่อน (ที่ทำให้ว่างคือ y = -50 จากการสร้างข้างบน) */
    for (i = 0; (status == BENTO_OK) && (i < MAX_BULLETS); i++) {
        status =
            bento_game_box_hide(&shooter->game, &shooter->bullets[i]);
    }
    return status;
}

/* value kept between low and high, as max(low, min(high, value)) */
static bento_real_t clamp(bento_real_t value, bento_real_t low,
                          bento_real_t high)
{
    const bento_real_t below_high = (value < high) ? value : high;

    return (below_high > low) ? below_high : low;
}

/* ยาน (จาก step2 คาบ 10): เร่ง ไถล เพดานความเร็ว ขอบจอ แล้ววาด */
static bento_status_t steer_ship(shooter_t *shooter,
                                 const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t friction = (bento_real_t)0.80;
    const bento_real_t right_edge =
        (bento_real_t)(shooter->game.width - SHIP_W);

    if (keys->left) {
        shooter->ship_speed -= accel;
    } else if (keys->right) {
        shooter->ship_speed += accel;
    } else {
        shooter->ship_speed *= friction;
    }
    shooter->ship_speed =
        clamp(shooter->ship_speed, -max_speed, max_speed);
    shooter->ship_x = clamp(shooter->ship_x + shooter->ship_speed,
                            (bento_real_t)0.0, right_edge);
    return bento_game_box_move_to(&shooter->game, &shooter->ship,
                                  shooter->ship_x,
                                  (bento_real_t)SHIP_Y);
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status = bento_game_keys(&shooter->game, &keys);

    (void)user;        /* the state is g_shooter */
    *out_stop = false; /* this step never ends the game by itself */
    /* (Back = ออก · Start = พักเกม: bento_game_run จัดการให้) */
    if (status == BENTO_OK) {
        status = steer_ship(shooter, &keys);
    }

    /* ----- งานของคุณ 1: คูลดาวน์ + ③ ยืม -----
     * ② หาตัวว่าง (ใน .py ให้มาแล้ว): เขียน static bento_game_box_t *find_free_bullet(shooter_t *)
     *    ไว้เหนือ on_frame ไล่ดูทีละนัด นัดแรกที่ y < FREE_LINE_Y คือตัวว่าง ไม่มีก็คืน NULL
     * 1) ทุกเฟรมลด fire_cooldown ลงทีละ 1 แต่อย่าให้ต่ำกว่า 0
     * 2) เช็กว่ากดยิงไหม: กด A (keys.a) หรือดันขึ้น (keys.up) "และ" fire_cooldown == 0
     * 3) ถ้าใช่ หาตัวว่างด้วย find_free_bullet ถ้าได้ (ไม่ใช่ NULL) ③ ยืม:
     *    3.1 โผล่กระสุนด้วย bento_game_box_show
     *    3.2 ย้ายไปที่หัวยานด้วย bento_game_box_move_to: x = กลางยานถอยครึ่งความกว้างกระสุน,
     *        y = เหนือยานเล็กน้อย
     *    3.3 เล่นเสียงยิงด้วย bento_game_sfx(&shooter->game, "fire")
     *    3.4 ตั้ง fire_cooldown เป็นค่าหน่วง (มากไป = ยิงช้า, น้อยไป = ชุดหมุนเวียนหมด) */

    /* ----- งานของคุณ 2: ④ อัปเดตทุกเฟรม + ⑤ คืน -----
     * 1) วนทุกกระสุน shooter->bullets[0 .. MAX_BULLETS - 1]
     * 2) ④ ถ้ากระสุนยัง "ใช้อยู่" (y >= FREE_LINE_Y) ให้ขยับขึ้นด้วย bento_game_box_move_to
     *    โดย x คงเดิม ส่วน y ลดลงทีละน้อยทุกเฟรม = พุ่งขึ้น (เลือกระยะเอง มาก = เร็ว)
     * 3) ⑤ หลังขยับ ถ้าพ้นเส้นแล้ว (y < FREE_LINE_Y) ให้ bento_game_box_hide = คืน
     * เขียนแต่ละขั้นใน if (status == BENTO_OK) แบบ steer_ship ข้างบน */
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
    /* หน้าชื่อเกม: Start = เล่น · Back = ออก (เรียก start ให้ในตัว) */
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    report_joystick(&shooter->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_ship_and_hud(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_bullet_pool(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
