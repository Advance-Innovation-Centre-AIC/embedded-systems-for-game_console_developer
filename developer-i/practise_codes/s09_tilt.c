/*
 * s09_tilt.c -- เอียงเล่น: ขยับกล่องตามการเอียงบอร์ด (ฉบับฝึก)
 *
 * สิ่งที่จะได้ฝึก: อ่าน accelerometer (BMI270 ผ่าน I2C) ด้วย bento_sensors, แปลง (map)
 * ค่าความเอียงเป็นความเร็วการเลื่อนของกล่อง แล้วขยับกล่องตามการเอียงจริงของบอร์ด
 * เติมช่องว่างที่มีคำว่า "เติม:" ให้ครบ แล้วลองเอียงบอร์ดดู กด Back เพื่อออก
 * วิธีรัน: เปิดไฟล์นี้ใน BENTO IDE แล้วกด "Program to Device"
 *
 * Tilt to play, the practise version. It clears the screen, puts a 40 px cyan box
 * in the middle with a Thai caption, then loops at about 30 fps: Back clears the
 * screen and ends the program. Each pass turns the acceleration into g and cuts
 * the dead zone. The blanks are still empty, so no sensor is read, the
 * acceleration stays zero and the box stays where it is.
 *
 * Twin of s09_tilt.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/s09_tilt.py). Style: CLEAN_C.md.
 * What C does differently:
 * - the three axes are one tilt_t; reals are bento_real_t, MicroPython's float width;
 * - SPEED is 120.0 in the .py, a whole number, so an enum holds it exactly;
 * - Stop during a sleep (KeyboardInterrupt) ends the program with 0;
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
#include "bento_time.h"

/* The box, the caption and the loop. */
enum {
    BOX_SIZE = 40, /* สร้างกล่องไว้กลางจอ ขนาด 40x40 */
    CAPTION_X = 20,
    CAPTION_Y = 20,
    FRAME_MS = 30, /* ~30 fps */
    TILT_SPEED = 120 /* ตัวคูณ: ยิ่งเอียงมาก กล่องยิ่งวิ่งไว (ช่อง map ใช้) */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t box;
    bento_game_text_t caption;
} tilt_screen_t;

/* ความเร่ง 3 แกน หน่วย g */
typedef struct {
    bento_real_t x;
    bento_real_t y;
    bento_real_t z;
} tilt_t;

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

/* start() opens the joystick, whose init prints this line on the kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

static bento_status_t add_box_and_caption(tilt_screen_t *screen)
{
    bento_game_t *game = &screen->game;
    bento_status_t status = bento_game_box_create(
        game, &screen->box, (bento_real_t)(game->width / 2), (bento_real_t)(game->height / 2),
        (bento_real_t)BOX_SIZE, (bento_real_t)BOX_SIZE, BENTO_GAME_CYAN);

    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &screen->caption, "เอียงบอร์ดเพื่อขยับกล่อง",
                                        CAPTION_X, CAPTION_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The acceleration of the three axes, in g. */
static tilt_t read_tilt(void)
{
    const bento_real_t gravity_ms2 = (bento_real_t)9.81;
    /* เติม: อ่านความเร่ง 3 แกนด้วย bento_sensors_bmi270_acceleration(&ax, &ay, &az)
     *       ได้ค่าหน่วย m/s² มาแทนศูนย์ทั้งสามตัวนี้ */
    tilt_t tilt = {(bento_real_t)0.0, (bento_real_t)0.0, (bento_real_t)0.0};

    /* แปลงเป็น g (วางราบ แกนหนึ่ง ≈ 1.0) — บรรทัดนี้ให้ไว้แล้ว */
    tilt.x = tilt.x / gravity_ms2;
    tilt.y = tilt.y / gravity_ms2;
    tilt.z = tilt.z / gravity_ms2;
    return tilt;
}

/* ตัด noise ในช่วง deadzone ให้เป็น 0 (กล่องจะนิ่งเมื่อวางราบ) */
static bento_real_t cut_dead_zone(bento_real_t value)
{
    /* ค่าเอียงเล็ก ๆ ถือว่ายังนิ่ง กันกล่องสั่น */
    const bento_real_t dead_zone = (bento_real_t)0.10;
    const bento_real_t size = (value < (bento_real_t)0.0) ? -value : value;

    return (size < dead_zone) ? (bento_real_t)0.0 : value;
}

/* The tilt of a board at rest reads zero: the dead zone is cut off. */
static tilt_t read_level_tilt(void)
{
    tilt_t tilt = read_tilt();

    tilt.x = cut_dead_zone(tilt.x);
    tilt.y = cut_dead_zone(tilt.y);
    return tilt;
}

/* time.sleep_ms(); Stop arrives here as BENTO_E_CANCELLED. */
static bento_status_t wait_frame(void)
{
    bento_status_t status = bento_time_sleep_ms(FRAME_MS);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* Until Back: Back clears the screen and leaves. */
static bento_status_t run_tilt_loop(tilt_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    bool leaving = false;

    while ((status == BENTO_OK) && !leaving) {
        bento_game_keys_t keys;

        status = bento_game_keys(&screen->game, &keys); /* อ่านปุ่มระบบ (BACK = ออก) */
        if ((status == BENTO_OK) && keys.back) {
            status = bento_game_clear(&screen->game);
            leaving = true;
        } else if (status == BENTO_OK) {
            const tilt_t tilt = read_level_tilt();

            /* เติม: map ค่าความเอียงเป็นระยะเลื่อนต่อเฟรม แล้วเรียก
             *       bento_game_box_move(&screen->game, &screen->box, dx, dy)
             *       ใบ้: dx = tilt.x * TILT_SPEED, dy = tilt.y * TILT_SPEED (กันขอบจอให้แล้ว) */
            (void)tilt; /* read by the blank above once you fill it in */
            status = wait_frame();
        } else {
            /* the status ends the loop */
        }
    }
    return status;
}

int main(void)
{
    tilt_screen_t screen;
    bento_status_t status;

    (void)memset(&screen, 0, sizeof(screen));
    bento_game_init(&screen.game, bento_runtime_get_ui(), bento_runtime_is_running);
    /* เคลียร์จอ + เตรียมระบบ */
    status = bento_game_start(&screen.game, BENTO_GAME_WIDTH, BENTO_GAME_HEIGHT);
    report_joystick(&screen.game);
    /* เติม: ปลุกเซนเซอร์ทั้งชุดก่อนใช้งาน ด้วย bento_sensors_init(&failed_mask) */
    if (status == BENTO_OK) {
        status = add_box_and_caption(&screen);
    }
    if (status == BENTO_OK) {
        status = run_tilt_loop(&screen);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
