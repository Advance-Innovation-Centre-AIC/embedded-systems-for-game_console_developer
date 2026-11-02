/*
 * s07_tilt.c -- เครื่องจำลองฟิสิกส์: ลูกแก้วกลิ้งมีโมเมนตัม
 *
 * คาบนี้มองการเอียงบอร์ดเป็น "ความเร่ง" แล้ว integrate ทุกเฟรม: ความเร่งสะสมเป็น
 * ความเร็ว ความเร็วสะสมเป็นตำแหน่ง และแรงเสียดทานหน่วงให้ค่อย ๆ ช้าลง ลูกแก้วจึง
 * กลิ้งต่อเองแม้หยุดเอียง วิ่งเลยเป้า และเด้งขอบจอ กด BACK เพื่อออก
 * เติมช่องว่างที่มีคำว่า "เติม:" ให้ครบ แล้วลองเอียงบอร์ดดู
 *
 * The practise skeleton of the marble: the sensors, the ball, the BACK key,
 * the tilt in g with its dead zone and the move are given. Integrating the
 * tilt into the velocity, friction and the edge bounce are left as TODOs;
 * until then the velocity stays 0 and the ball stays in the middle.
 *
 * Twin of s07_tilt.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/s07_tilt.py). Style: CLEAN_C.md.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_sensors.h"
#include "bento_time.h"

enum {
    BALL_PX = 30, /* ลูกแก้วเริ่มกลางจอ ขนาด 30x30 */
    LABEL_X = 20,
    LABEL_Y = 20,
    FRAME_MS = 30 /* ~30 fps (ตรงกับ DT = 0.03) */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ball;
    bento_game_text_t label;
    bento_real_t vx; /* ความเร็วสะสม: "ความจำ" ที่ทำให้เกิดโมเมนตัม */
    bento_real_t vy;
} marble_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern marble_t g_marble;
marble_t g_marble;

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

/* เคลียร์จอ + เตรียมระบบ แล้วปลุกเซนเซอร์ทั้งชุด (รวม BMI270) ครั้งเดียวพอ */
static bento_status_t start(marble_t *marble)
{
    uint32_t failed = 0U;
    bento_status_t status = bento_game_start(
        &marble->game, BENTO_GAME_WIDTH, BENTO_GAME_HEIGHT);

    /* start()'s joystick.init() prints this line on the kits */
    if ((marble->game.joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
    if (status == BENTO_OK) {
        status = bento_sensors_init(&failed);
    }
    if (status == BENTO_OK) {
        const int32_t center_x = marble->game.width / 2;
        const int32_t center_y = marble->game.height / 2;

        bento_game_box_init(&marble->ball);
        status = bento_game_box_create(
            &marble->game, &marble->ball, (bento_real_t)center_x,
            (bento_real_t)center_y, (bento_real_t)BALL_PX,
            (bento_real_t)BALL_PX, BENTO_GAME_CYAN);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&marble->label);
        status = bento_game_text_create(&marble->game, &marble->label,
                                        "เอียงบอร์ดให้ลูกแก้วกลิ้ง", LABEL_X,
                                        LABEL_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* One axis in g; a small tilt counts as still, so noise does not add up. */
static bento_real_t tilt_g(float meters_per_s2)
{
    const bento_real_t gravity = (bento_real_t)9.81;
    const bento_real_t deadzone = (bento_real_t)0.10;
    bento_real_t g_value = (bento_real_t)meters_per_s2 / gravity;

    if ((g_value < deadzone) && (g_value > -deadzone)) {
        g_value = (bento_real_t)0;
    }
    return g_value;
}

/* อ่านความเร่ง 3 แกน หน่วย m/s^2 แล้วขยับลูกแก้ว */
static bento_status_t roll(marble_t *marble)
{
    const bento_real_t dt = (bento_real_t)0.03; /* 1 เฟรม (วินาที) */
    float ax = 0.0F;
    float ay = 0.0F;
    float az = 0.0F;
    bento_status_t status =
        bento_sensors_bmi270_acceleration(&ax, &ay, &az);

    if (status == BENTO_OK) {
        const bento_real_t gx = tilt_g(ax);
        const bento_real_t gy = tilt_g(ay);

        (void)gx; /* the TODOs below are where they are used */
        (void)gy;
        /* เติม: integrate ความเร่ง -> ความเร็ว (สะสมเข้า vx, vy ทุกเฟรม)
         *       ใบ้: vx = vx + gx * accel * dt (accel = 900.0) แล้วทำ vy แบบเดียวกัน
         * เติม: friction หน่วงความเร็วทุกเฟรม ให้ลูกแก้วค่อย ๆ ช้าลงเองเมื่อวางราบ
         *       ใบ้: vx = vx * friction (friction = 0.95) แล้วทำ vy แบบเดียวกัน */

        /* box move คุมไม่ให้หลุดจอ แล้วหยุดลูกแก้วไว้ที่ขอบพอดี */
        status = bento_game_box_move(&marble->game, &marble->ball,
                                     marble->vx * dt, marble->vy * dt);
    }
    /* เติม: เด้งขอบจอ ถ้าลูกแก้วชนซ้าย/ขวา ให้กลับทิศ vx (คูณ -bounce, bounce = 0.70)
     *       ถ้าชนบน/ล่าง ให้กลับทิศ vy
     *       ใบ้: ชนซ้าย/ขวาเช็ก ball.x <= 0 หรือ ball.x >= game.width - ball.w
     *            ชนบน/ล่างเช็ก  ball.y <= 0 หรือ ball.y >= game.height - ball.h */
    return status;
}

/* One frame: roll, wait 30 ms; Stop arrives at the sleep. */
static bento_status_t frame(marble_t *marble)
{
    bento_status_t status = roll(marble);

    if (status == BENTO_OK) {
        status = bento_time_sleep_ms(FRAME_MS);
    }
    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

static bento_status_t run_loop(marble_t *marble)
{
    bool quit = false;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && !quit) {
        bento_game_keys_t keys;

        bento_game_keys_init(&keys);
        status = bento_game_keys(&marble->game, &keys);
        quit = keys.back; /* BACK = ออก */
        if ((status == BENTO_OK) && quit) {
            status = bento_game_clear(&marble->game);
        } else if (status == BENTO_OK) {
            status = frame(marble);
        } else {
            /* the status ends the loop */
        }
    }
    return status;
}

int main(void)
{
    marble_t *marble = &g_marble;
    bento_status_t status;

    (void)memset(marble, 0, sizeof(*marble));
    bento_game_init(&marble->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    status = start(marble);
    marble->vx = (bento_real_t)0;
    marble->vy = (bento_real_t)0;
    if (status == BENTO_OK) {
        status = run_loop(marble);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
