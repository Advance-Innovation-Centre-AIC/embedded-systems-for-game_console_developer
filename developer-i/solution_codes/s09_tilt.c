/*
 * s09_tilt.c -- เอียงเล่น: ขยับกล่องตามการเอียงบอร์ด
 *
 * Twin of s09_tilt.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s09_tilt.py)
 *
 * อ่าน accelerometer (BMI270 ผ่าน I2C) แปลงความเอียงเป็นความเร็ว
 * แล้วขยับกล่องสีฟ้ากลางจอตามการเอียงจริงของบอร์ด
 * ลองเอียงซ้าย-ขวา-หน้า-หลัง กล่องจะวิ่งตามมือ กด BACK เพื่อออก
 *
 * Reads the accelerometer (BMI270 over I2C), maps the tilt to a speed
 * and moves the cyan box in the middle of the screen as the board
 * really tilts. Tilt left, right, forward and back and the box
 * follows your hand; press BACK to leave.
 *
 * C twin of the Example Explorer program devi-s09-tilt
 * (py/devi-s09-tilt.py), with the blanks filled as this solution fills
 * them. Style: CLEAN_C.md. What C must do differently:
 *   - sensors.init() prints its own report lines in MicroPython;
 *     bento_sensors_init() prints none (its result is ignored in both);
 *   - a busy sensor bus is ignored, as MicroPython ignores it;
 *   - floats are bento_real_t, the width of each board's MicroPython
 *     float;
 *   - Stop during a sleep ends the program, as KeyboardInterrupt.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_sensors.h"
#include "bento_time.h"

/* The box, the caption and the frame time. */
enum {
    BOX_SIZE = 40,
    CAPTION_X = 20,
    CAPTION_Y = 20,
    FRAME_MS = 30 /* about 30 frames a second */
};

/* What the program changes after it starts. */
typedef struct {
    bento_game_t game;
    bento_game_box_t box;
    bento_game_text_t caption;
} tilt_game_t;

/* The reTerminal's joystick prints nothing when it opens; the kits
 * print a line. One Emulator build serves both, so ask the board. */
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

/* start() calls joystick.init(), which prints this line on the kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* The box in the middle of the screen start() made. */
static bento_status_t add_box(tilt_game_t *tilt)
{
    /* floor division of positive ints */
    const int32_t middle_x = tilt->game.width / 2;
    const int32_t middle_y = tilt->game.height / 2;

    return bento_game_box_create(
        &tilt->game, &tilt->box, (bento_real_t)middle_x,
        (bento_real_t)middle_y, (bento_real_t)BOX_SIZE,
        (bento_real_t)BOX_SIZE, BENTO_GAME_CYAN);
}

static bento_status_t build_screen(tilt_game_t *tilt)
{
    uint32_t failed_mask = 0U;
    bento_status_t status = bento_game_start(
        &tilt->game, BENTO_GAME_WIDTH, BENTO_GAME_HEIGHT);

    report_joystick(&tilt->game);
    if (status == BENTO_OK) {
        /* the sensors that failed are only printed in MicroPython */
        status = bento_sensors_init(&failed_mask);
    }
    if (status == BENTO_OK) {
        status = add_box(tilt);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(&tilt->game, &tilt->caption,
                                        "เอียงบอร์ดเพื่อขยับกล่อง", CAPTION_X,
                                        CAPTION_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* A small tilt counts as level, so the box does not shake. */
static bento_real_t cut_dead_zone(bento_real_t tilt_g)
{
    const bento_real_t dead_zone = (bento_real_t)0.10;
    const bento_real_t size =
        (tilt_g < (bento_real_t)0) ? -tilt_g : tilt_g;

    return (size < dead_zone) ? (bento_real_t)0.0 : tilt_g;
}

/* One frame: read the tilt in g, cut the dead zone, move the box by
 * the tilt times the speed (move() keeps it on the screen). */
static bento_status_t follow_tilt(tilt_game_t *tilt)
{
    const bento_real_t gravity_ms2 = (bento_real_t)9.81;
    /* the more the board tilts, the faster the box runs */
    const bento_real_t speed = (bento_real_t)120.0;
    float accel_x = 0.0F;
    float accel_y = 0.0F;
    float accel_z = 0.0F;
    bento_status_t status =
        bento_sensors_bmi270_acceleration(&accel_x, &accel_y, &accel_z);

    if (status == BENTO_E_BUSY) {
        status = BENTO_OK; /* MicroPython ignores a busy bus too */
    }
    if (status == BENTO_OK) {
        const bento_real_t tilt_x =
            cut_dead_zone((bento_real_t)accel_x / gravity_ms2);
        const bento_real_t tilt_y =
            cut_dead_zone((bento_real_t)accel_y / gravity_ms2);

        status = bento_game_box_move(&tilt->game, &tilt->box,
                                     tilt_x * speed, tilt_y * speed);
    }
    return status;
}

/* One frame: BACK clears the screen and leaves; otherwise the box
 * follows the tilt and the frame waits its time. */
static bento_status_t run_frame(tilt_game_t *tilt, bool *leave)
{
    bento_game_keys_t keys;
    bento_status_t status;

    bento_game_keys_init(&keys);
    status = bento_game_keys(&tilt->game, &keys);
    if ((status == BENTO_OK) && keys.back) {
        status = bento_game_clear(&tilt->game);
        *leave = true;
    } else if (status == BENTO_OK) {
        status = follow_tilt(tilt);
        if (status == BENTO_OK) {
            status = bento_time_sleep_ms(FRAME_MS);
        }
        if ((status == BENTO_OK) && !bento_runtime_is_running()) {
            status = BENTO_E_CANCELLED;
        }
    } else {
        /* the status ends the loop */
    }
    return status;
}

static bento_status_t run_loop(tilt_game_t *tilt)
{
    bool leave = false;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && !leave) {
        status = run_frame(tilt, &leave);
    }
    return status;
}

int main(void)
{
    tilt_game_t tilt;
    bento_status_t status;

    (void)memset(&tilt, 0, sizeof(tilt));
    bento_game_init(&tilt.game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&tilt.box);
    bento_game_text_init(&tilt.caption);
    status = build_screen(&tilt);
    if (status == BENTO_OK) {
        status = run_loop(&tilt);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
