/*
 * s13_06_deadzone.c -- คาบ 13 การ์ด 5: ตัวควบคุมสามระดับ + ช่องว่าง
 *
 * จอยขึ้น/ลงย้ายลูกบอล (เป้า) ไม้ AI ไล่ตามเองด้วยตัวควบคุมสามระดับ
 * ค่าคลาด e = เป้า - ตำแหน่งไม้ (มีสัญญาณรบกวน ±1 px แบบเซนเซอร์จริง)
 * e > BAND -> +1 ขยับลง · e < -BAND -> -1 ขยับขึ้น · นอกนั้น 0 อยู่นิ่ง
 * A = สลับ BAND ระหว่าง 6 กับ 0 · B = ล้างตัวนับ FLIPS
 * BAND 6 ไม้นิ่งในแถบเขียว ไฟ HOLD ติดค้าง · BAND 0 ไม้สั่นกึก ๆ
 * ไฟสลับแทบทุกเฟรม FLIPS พุ่ง และได้ยินเสียงติ๊กทุกครั้งที่กลับทิศ
 *
 * Up and down move the ball (the target); the AI paddle follows it
 * with a three-level controller. The error e = target - paddle, with
 * +-1 px of noise like a real sensor: e > BAND moves down (+1), e <
 * -BAND moves up (-1), anything else holds (0). A swaps BAND between
 * 6 and 0, B clears the FLIPS counter. With BAND 6 the paddle rests in
 * the green band and the HOLD lamp stays on; with BAND 0 it jitters,
 * the lamps swap almost every frame, FLIPS climbs and every reversal
 * ticks.
 *
 * Twin of s13_06_deadzone.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_06_deadzone.py).
 * Style: CLEAN_C.md. What C does differently:
 * - random is bento_random.h, its state in the program's struct;
 * - the lamps dict keyed by +1, 0, -1 is an array indexed by 1 - out;
 * - the shown dict is fields of the struct, None a NEVER_SHOWN value;
 * - a Sprite has no size in C: the ball and paddle sizes are enums;
 * - the state lives in one struct at file scope (512 bytes or more);
 * - title()'s Back (SystemExit) and Stop end the program with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    BAND_ON = 6, /* ช่องว่าง ±6 px แบบ pong_step5.py (กด A สลับกับ 0) */
    /* ไม้ AI ขยับเฟรมละ 5 px คงที่ — มีแค่ ขึ้น / หยุด / ลง ไม่มีครึ่ง ๆ กลาง ๆ */
    SPEED = 5,
    BALL_STEP = 4, /* กดจอยค้าง ลูก (เป้า) ขยับเฟรมละ 4 px */
    NOISE = 1,     /* ค่าที่วัดได้สั่น ±1 px เหมือนเซนเซอร์จริง (0 = วัดเป๊ะ) */
    TOP = 44,      /* ขอบบน/ล่างของสนาม */
    BOTTOM = 356,
    BALL_X = 90, /* คอลัมน์ของลูก และของไม้ AI */
    AI_X = 330,
    BALL = 14, /* ขนาดสไปรต์ ball 14x14 · paddle 14x90 */
    PADDLE_H = 90,
    HUD_X = 460
};
/* ---- end of settings ---- */

/* The tone and the field. */
enum {
    TONE_NOTE = 96,
    TONE_VELOCITY = 100,
    TONE_MS = 12,
    FPS = 30,
    NEVER_SHOWN = -32768
};

/* The drawing on the left. */
enum {
    LABEL_Y = 14,
    TARGET_LABEL_X = BALL_X - 26,
    AI_LABEL_X = AI_X - 4,
    FIELD_X = 30,
    FIELD_W = 400,
    LINE_W = AI_X + 40 - BALL_X,
    BAND_X = AI_X - 14,
    BAND_W = 42,
    MARK_X = AI_X - 8,
    MARK_W = 30,
    MARK_H = 3
};

/* The panel on the right. */
enum {
    BAND_TEXT_Y = 24,
    ERROR_TEXT_Y = 60,
    OUT_TEXT_Y = 96,
    LAMP_COUNT = 3,
    LAMP_GAP_PX = 105,
    LAMP_X = HUD_X + 10,
    LAMP_Y = 138,
    LAMP_PX = 30,
    LAMP_RADIUS = 15,
    LAMP_LABEL_Y = 176,
    VALVE_Y = 214,
    FLIPS_Y = 258,
    HELP_Y = 300,
    TEXT_CHARS =
        128,        /* a label's 126 bytes; Thai is 3 bytes a letter */
    SIGN_CHARS = 12 /* a sign, ten digits and the NUL */
};

enum {
    COLOR_LAMP_OFF = 0x3A4450,
    COLOR_FIELD = 0x18202A,
    COLOR_FIELD_EDGE = 0x3A4A5A,
    COLOR_BAND = 0x2E8B57,
    COLOR_NOTE = 0x9AA8B8
};

/* Lamp i shows out = 1 - i: +1 OPEN, 0 HOLD, -1 CLOSE. */
static const char *const MEANING[LAMP_COUNT] = {"OPEN", "HOLD",
                                                "CLOSE"};

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t line;
    bento_game_box_t band_box;
    bento_game_sprite_t ball;
    bento_game_sprite_t paddle;
    bento_game_box_t mark;
    bento_game_text_t hud_band;
    bento_game_text_t hud_error;
    bento_game_text_t hud_out;
    bento_game_box_t lamps[LAMP_COUNT];
    bento_game_text_t hud_flips;
    bento_real_t ball_y; /* มุมบนของลูก */
    bento_real_t ai_y;   /* มุมบนของไม้ */
    int32_t band;
    int32_t flips;
    int32_t last_direction;
    int32_t shown_ball;
    int32_t shown_ai;
    int32_t shown_band;
    int32_t shown_out;
    int32_t shown_flips;
    char shown_error[TEXT_CHARS];
} deadzone_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern deadzone_t g_deadzone;
deadzone_t g_deadzone;

/* การ์ด 5: +1 = ลง/เปิดเพิ่ม · 0 = นิ่ง/หยุด · -1 = ขึ้น/ปิดลง */
static int32_t controller(bento_real_t error, int32_t band)
{
    int32_t out = 0; /* ใกล้พอแล้ว -> อยู่นิ่ง */

    if (error > (bento_real_t)band) {
        out = 1;
    } else if (error < (bento_real_t)-band) {
        out = -1;
    } else {
        /* inside the band */
    }
    return out;
}

/* ใส่เครื่องหมาย + ให้เลขบวก จะได้เห็นทิศของค่าคลาด */
static void format_signed(char *text, uint32_t size, int32_t value)
{
    (void)snprintf(text, size, "%s%" PRId32, (value > 0) ? "+" : "",
                   value);
}

static uint32_t lamp_of(int32_t out)
{
    const int32_t lamp = 1 - out;

    return (uint32_t)lamp;
}

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

/* title() calls start(), whose joystick.init() prints this line on
 * the kits and nothing on the reTerminal. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

static bento_status_t add_text(bento_game_t *game, const char *text,
                               int32_t x, int32_t y, uint32_t color)
{
    bento_game_text_t label;

    bento_game_text_init(&label);
    return bento_game_text_create(game, &label, text, x, y, color);
}

/* ของที่สร้างก่อนอยู่ล่าง */
static bento_status_t add_field(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    const int32_t field_h = BOTTOM - TOP;
    bento_game_box_t field;
    bento_status_t status = add_text(game, "TARGET", TARGET_LABEL_X,
                                     LABEL_Y, BENTO_GAME_YELLOW);

    if (status == BENTO_OK) {
        status =
            add_text(game, "AI", AI_LABEL_X, LABEL_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        bento_game_box_init(&field);
        status = bento_game_box_create_styled(
            game, &field, (bento_real_t)FIELD_X, (bento_real_t)TOP,
            (bento_real_t)FIELD_W, (bento_real_t)field_h, COLOR_FIELD,
            COLOR_FIELD_EDGE, 1, 1);
    }
    if (status == BENTO_OK) {
        /* the target line at the ball's centre */
        status = bento_game_box_create(
            game, &deadzone->line, (bento_real_t)BALL_X,
            (bento_real_t)0, (bento_real_t)LINE_W, (bento_real_t)2,
            BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &deadzone->band_box, (bento_real_t)BAND_X,
            (bento_real_t)0, (bento_real_t)BAND_W,
            (bento_real_t)(2 * BAND_ON), COLOR_BAND);
    }
    return status;
}

static bento_status_t add_players(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    bento_status_t status =
        bento_game_sprite_create(game, &deadzone->ball, "ball",
                                 (bento_real_t)BALL_X, (bento_real_t)0);

    if (status == BENTO_OK) {
        status = bento_game_sprite_create(game, &deadzone->paddle,
                                          "paddle", (bento_real_t)AI_X,
                                          (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        /* the paddle's centre tick */
        status = bento_game_box_create(
            game, &deadzone->mark, (bento_real_t)MARK_X,
            (bento_real_t)0, (bento_real_t)MARK_W, (bento_real_t)MARK_H,
            BENTO_GAME_WHITE);
    }
    return status;
}

/* Each lamp, then its label, as the original's loop. */
static bento_status_t add_lamps(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    char label[TEXT_CHARS];
    char sign[SIGN_CHARS];
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && (i < LAMP_COUNT); i++) {
        status = bento_game_box_create_styled(
            game, &deadzone->lamps[i],
            (bento_real_t)(LAMP_X + (i * LAMP_GAP_PX)),
            (bento_real_t)LAMP_Y, (bento_real_t)LAMP_PX,
            (bento_real_t)LAMP_PX, COLOR_LAMP_OFF,
            BENTO_GAME_BORDER_NONE, LAMP_RADIUS, 0);
        format_signed(sign, (uint32_t)sizeof(sign), 1 - i);
        (void)snprintf(label, sizeof(label), "%s %s", sign, MEANING[i]);
        if (status == BENTO_OK) {
            status = add_text(game, label, HUD_X + (i * LAMP_GAP_PX),
                              LAMP_LABEL_Y, BENTO_GAME_WHITE);
        }
    }
    return status;
}

static bento_status_t add_panel(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    bento_status_t status =
        bento_game_text_create(game, &deadzone->hud_band, "", HUD_X,
                               BAND_TEXT_Y, BENTO_GAME_WHITE);

    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &deadzone->hud_error, "",
                                        HUD_X, ERROR_TEXT_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &deadzone->hud_out, "", HUD_X,
                                   OUT_TEXT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_lamps(deadzone);
    }
    if (status == BENTO_OK) {
        status = add_text(game, "วาล์ว: เปิดเพิ่ม / หยุด / ปิดลง", HUD_X,
                          VALVE_Y, COLOR_NOTE);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_text_create(game, &deadzone->hud_flips, "",
                                   HUD_X, FLIPS_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_text(game, "UP/DOWN = เป้า  A = BAND", HUD_X,
                          HELP_Y, BENTO_GAME_GB_LIGHT);
    }
    return status;
}

static bento_status_t build_screen(deadzone_t *deadzone)
{
    bento_status_t status = add_field(deadzone);

    if (status == BENTO_OK) {
        status = add_players(deadzone);
    }
    if (status == BENTO_OK) {
        status = add_panel(deadzone);
    }
    return status;
}

/* รับปุ่ม (วัด) */
static void read_keys(deadzone_t *deadzone,
                      const bento_game_keys_t *keys)
{
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;
    const bento_real_t half_ball = (bento_real_t)BALL / (bento_real_t)2;

    if (keys->up) {
        const bento_real_t highest =
            ((bento_real_t)TOP + half_paddle) - half_ball;
        const bento_real_t want =
            deadzone->ball_y - (bento_real_t)BALL_STEP;

        deadzone->ball_y = (highest > want) ? highest : want;
    }
    if (keys->down) {
        const bento_real_t lowest =
            ((bento_real_t)BOTTOM - half_paddle) - half_ball;
        const bento_real_t want =
            deadzone->ball_y + (bento_real_t)BALL_STEP;

        deadzone->ball_y = (lowest < want) ? lowest : want;
    }
    if (bento_game_pressed_once(&deadzone->game, BENTO_GAME_KEY_A,
                                keys)) {
        deadzone->band = (deadzone->band != 0) ? 0 : BAND_ON;
        bento_game_sfx(&deadzone->game, "select");
    }
    if (bento_game_pressed_once(&deadzone->game, BENTO_GAME_KEY_B,
                                keys)) {
        deadzone->flips = 0;
    }
}

/* คิด (ตัดสิน): ค่าคลาดที่วัดได้ -> สามระดับ -> ไม้ขยับเท่า SPEED */
static bento_status_t decide(deadzone_t *deadzone, bento_real_t *error,
                             int32_t *out)
{
    /* อยากให้กลางไม้ตรงกลางลูก */
    const bento_real_t target =
        (deadzone->ball_y + ((bento_real_t)BALL / 2)) -
        ((bento_real_t)PADDLE_H / 2);
    int32_t noise = 0;
    const bento_status_t status =
        bento_random_randint(&deadzone->random, -NOISE, NOISE, &noise);

    *error = (target - deadzone->ai_y) + (bento_real_t)noise;
    *out = controller(*error, deadzone->band);
    deadzone->ai_y += (bento_real_t)(*out * SPEED);
    if (*out != 0) {
        /* กลับทิศ = วาล์วสลับหนึ่งครั้ง */
        if ((deadzone->last_direction != 0) &&
            (*out != deadzone->last_direction)) {
            deadzone->flips += 1;
            bento_game_tone(&deadzone->game, TONE_NOTE,
                            BENTO_GAME_WAVE_DEFAULT, TONE_VELOCITY,
                            TONE_MS);
        }
        deadzone->last_direction = *out;
    }
    return status;
}

static bento_status_t show_ball(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    const int32_t centre_y =
        (int32_t)(deadzone->ball_y + ((bento_real_t)BALL / 2));
    bento_status_t status = BENTO_OK;

    if (centre_y != deadzone->shown_ball) {
        status = bento_game_sprite_move_to(
            game, &deadzone->ball, (bento_real_t)BALL_X,
            (bento_real_t)(int32_t)deadzone->ball_y);
        if (status == BENTO_OK) {
            const int32_t line_y = centre_y - 1;

            status = bento_game_box_move_to(game, &deadzone->line,
                                            (bento_real_t)BALL_X,
                                            (bento_real_t)line_y);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_move_to(
                game, &deadzone->band_box, (bento_real_t)BAND_X,
                (bento_real_t)(centre_y - BAND_ON));
        }
        deadzone->shown_ball = centre_y;
    }
    return status;
}

static bento_status_t show_band(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (deadzone->band != deadzone->shown_band) {
        if (deadzone->band != 0) {
            status = bento_game_box_show(game, &deadzone->band_box);
        } else {
            status = bento_game_box_hide(game, &deadzone->band_box);
        }
        (void)snprintf(text, sizeof(text),
                       "BAND = %" PRId32 " px   (A)", deadzone->band);
        if (status == BENTO_OK) {
            status =
                bento_game_text_set(game, &deadzone->hud_band, text);
        }
        deadzone->shown_band = deadzone->band;
    }
    return status;
}

static bento_status_t show_paddle(deadzone_t *deadzone)
{
    bento_game_t *game = &deadzone->game;
    const int32_t top = (int32_t)deadzone->ai_y;
    const int32_t mark_y = (top + (PADDLE_H / 2)) - 1;
    bento_status_t status = BENTO_OK;

    if (top != deadzone->shown_ai) {
        status = bento_game_sprite_move_to(game, &deadzone->paddle,
                                           (bento_real_t)AI_X,
                                           (bento_real_t)top);
        if (status == BENTO_OK) {
            status = bento_game_box_move_to(game, &deadzone->mark,
                                            (bento_real_t)MARK_X,
                                            (bento_real_t)mark_y);
        }
        deadzone->shown_ai = top;
    }
    return status;
}

static bento_status_t show_error(deadzone_t *deadzone,
                                 bento_real_t error)
{
    char sign[SIGN_CHARS];
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    format_signed(sign, (uint32_t)sizeof(sign), (int32_t)error);
    (void)snprintf(text, sizeof(text), "ERROR = %s px", sign);
    if (strcmp(text, deadzone->shown_error) != 0) {
        status = bento_game_text_set(&deadzone->game,
                                     &deadzone->hud_error, text);
        (void)snprintf(deadzone->shown_error,
                       sizeof(deadzone->shown_error), "%s", text);
    }
    return status;
}

static bento_status_t show_out(deadzone_t *deadzone, int32_t out)
{
    static const uint32_t lamp_on[LAMP_COUNT] = {
        BENTO_GAME_GREEN, BENTO_GAME_CYAN, BENTO_GAME_ORANGE};
    bento_game_t *game = &deadzone->game;
    char sign[SIGN_CHARS];
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (out != deadzone->shown_out) {
        if (deadzone->shown_out != NEVER_SHOWN) {
            status = bento_game_box_set_color(
                game, &deadzone->lamps[lamp_of(deadzone->shown_out)],
                COLOR_LAMP_OFF);
        }
        if (status == BENTO_OK) {
            status = bento_game_box_set_color(
                game, &deadzone->lamps[lamp_of(out)],
                lamp_on[lamp_of(out)]);
        }
        format_signed(sign, (uint32_t)sizeof(sign), out);
        (void)snprintf(text, sizeof(text), "OUT = %s   %s", sign,
                       MEANING[lamp_of(out)]);
        if (status == BENTO_OK) {
            status =
                bento_game_text_set(game, &deadzone->hud_out, text);
        }
        deadzone->shown_out = out;
    }
    return status;
}

static bento_status_t show_flips(deadzone_t *deadzone)
{
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (deadzone->flips != deadzone->shown_flips) {
        (void)snprintf(text, sizeof(text),
                       "FLIPS = %" PRId32 "   (B = 0)",
                       deadzone->flips);
        status = bento_game_text_set(&deadzone->game,
                                     &deadzone->hud_flips, text);
        deadzone->shown_flips = deadzone->flips;
    }
    return status;
}

/* ออก (โชว์): อัปเดตเฉพาะที่เปลี่ยน */
static bento_status_t on_frame(void *user, bool *out_stop)
{
    deadzone_t *deadzone = &g_deadzone;
    bento_game_keys_t keys;
    bento_real_t error = (bento_real_t)0;
    int32_t out = 0;
    bento_status_t status;

    (void)user;        /* the state is g_deadzone */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&deadzone->game, &keys);
    if (status == BENTO_OK) {
        read_keys(deadzone, &keys);
        status = decide(deadzone, &error, &out);
    }
    if (status == BENTO_OK) {
        status = show_ball(deadzone);
    }
    if (status == BENTO_OK) {
        status = show_band(deadzone);
    }
    if (status == BENTO_OK) {
        status = show_paddle(deadzone);
    }
    if (status == BENTO_OK) {
        status = show_error(deadzone, error);
    }
    if (status == BENTO_OK) {
        status = show_out(deadzone, out);
    }
    if (status == BENTO_OK) {
        status = show_flips(deadzone);
    }
    return status;
}

static void reset_state(deadzone_t *deadzone)
{
    const bento_real_t start_ball_y = (bento_real_t)180.0;
    const bento_real_t start_ai_y = (bento_real_t)60.0;
    int32_t i;

    (void)memset(deadzone, 0, sizeof(*deadzone));
    bento_game_init(&deadzone->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&deadzone->line);
    bento_game_box_init(&deadzone->band_box);
    bento_game_sprite_init(&deadzone->ball);
    bento_game_sprite_init(&deadzone->paddle);
    bento_game_box_init(&deadzone->mark);
    bento_game_text_init(&deadzone->hud_band);
    bento_game_text_init(&deadzone->hud_error);
    bento_game_text_init(&deadzone->hud_out);
    for (i = 0; i < LAMP_COUNT; i++) {
        bento_game_box_init(&deadzone->lamps[i]);
    }
    bento_game_text_init(&deadzone->hud_flips);
    deadzone->ball_y = start_ball_y;
    deadzone->ai_y = start_ai_y;
    deadzone->band = BAND_ON;
    deadzone->shown_ball = NEVER_SHOWN;
    deadzone->shown_ai = NEVER_SHOWN;
    deadzone->shown_band = NEVER_SHOWN;
    deadzone->shown_out = NEVER_SHOWN;
    deadzone->shown_flips = NEVER_SHOWN;
}

int main(void)
{
    deadzone_t *deadzone = &g_deadzone;
    bool quit = false;
    bento_status_t status;

    reset_state(deadzone);
    status = bento_random_init(&deadzone->random);
    if (status == BENTO_OK) {
        status =
            bento_game_title(&deadzone->game, "DEADZONE", NULL, &quit);
        report_joystick(&deadzone->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(deadzone);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&deadzone->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
