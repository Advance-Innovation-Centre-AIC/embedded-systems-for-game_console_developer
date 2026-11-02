/*
 * s13_03_bounce_angle.c -- คาบ 13 การ์ด 2: สะท้อน และมุมกระทบ
 *
 * ลูกเด้งผนังบน ล่าง ขวา: ชนผนังแล้วดึงลูกกลับเข้าขอบและกลับ
 * เครื่องหมายความเร็ว (ต้องครบสองอย่าง) ผนังที่โดนกะพริบขาว 4 เฟรม
 * ไม้ซ้ายมือขยับด้วยจอยขึ้น/ลง ลูกโดนไม้ตรงไหนก็เปลี่ยนมุมตามนั้น
 * offset = (กลางลูก - กลางไม้) / ครึ่งไม้ แล้ว vy += 2 * offset
 * โดยมีเพดาน VY_MAX จุดสีบนขอบไม้บอกตำแหน่งที่เพิ่งโดน
 * (เขียว = กลาง · เหลือง · ส้ม = ปลายไม้) รับพลาด = เสิร์ฟใหม่
 *
 * The ball bounces off the top, bottom and right walls: a hit pulls
 * the ball back inside and flips the sign of its speed, and the wall
 * flashes white for 4 frames. The left paddle moves with the joystick
 * up and down; where the ball meets it sets the new angle, offset =
 * (ball centre - paddle centre) / half the paddle, then vy += 2 *
 * offset, capped at VY_MAX. A coloured mark on the paddle shows the
 * last hit point. A miss serves the ball again from the middle.
 *
 * Twin of s13_03_bounce_angle.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_03_bounce_angle.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the walls dict keyed by name is an array indexed by wall_t;
 * - a Sprite has no size in C: the program keeps the ball's and the
 *   paddle's size and tests them with bento_game_hit_rect;
 * - "%+.2f" is bento_fmt_real_precision with the sign added;
 * - the state lives in one struct at file scope (512 bytes or more);
 * - title()'s Back (SystemExit) and Stop end the program with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
/* ความเร็วแนวนอนของลูก (คงที่ ไฟล์นี้ดูเฉพาะ vy) */
static const bento_real_t BALL_VX = (bento_real_t)8.0;
/* vy ตอนเสิร์ฟ และเพดานความเร็วแนวตั้ง */
static const bento_real_t SERVE_VY = (bento_real_t)2.0;
static const bento_real_t VY_MAX = (bento_real_t)9.0;
enum {
    PADDLE_SPEED = 9, /* ไม้ขยับกี่พิกเซลต่อเฟรม */
    FLASH = 4,        /* ผนังกะพริบกี่เฟรม (นับเฟรม ไม่ใช้ sleep) */
    /* ขอบสนามด้านใน (ผนังอยู่นอกเส้นเหล่านี้) */
    TOP = 70,
    BOTTOM = 356,
    RIGHT = 776,
    /* ขนาดภาพ ball / paddle */
    BALL = 14,
    PADDLE_X = 20,
    PADDLE_W = 14,
    PADDLE_H = 90
};
/* ---- end of settings ---- */

/* The field, the paddle and the frame counts. */
enum {
    PADDLE_FACE_X = PADDLE_X + PADDLE_W, /* the paddle's right edge */
    WALL_THICK_PX = 6,
    RIGHT_WALL_W = 16,
    SERVE_X = 396,
    SERVE_Y = 200,
    MARK_W = 8,
    MARK_H = 14,
    MARK_PARK_X = -20,
    FPS = 30,
    /* three numbers of up to BENTO_FMT_BUF_SIZE bytes and the words */
    TEXT_CHARS = 144
};

/* The texts on top. */
enum {
    TEXT_X = 16,
    RULE_Y = 4,
    HUD_Y = 34,
    SCORE_X = 600
};

enum {
    COLOR_WALL = 0x8899AA
};

typedef enum {
    WALL_TOP = 0,
    WALL_BOTTOM,
    WALL_RIGHT,
    WALL_COUNT,
    WALL_NONE
} wall_t;

typedef struct {
    bento_game_t game;
    bento_game_text_t hud_hit;
    bento_game_text_t hud_score;
    bento_game_box_t walls[WALL_COUNT];
    bento_game_sprite_t paddle;
    bento_game_box_t mark;
    bento_game_sprite_t ball;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t speed_x;
    bento_real_t speed_y;
    int32_t paddle_y;
    int32_t hits;
    int32_t misses;
    wall_t flash_wall;
    int32_t flash_left;
} bounce_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern bounce_t g_bounce;
bounce_t g_bounce;

static bento_real_t clamp_real(bento_real_t value, bento_real_t low,
                               bento_real_t high)
{
    const bento_real_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static int32_t clamp_int(int32_t value, int32_t low, int32_t high)
{
    const int32_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static bento_real_t absolute(bento_real_t value)
{
    return (value < (bento_real_t)0) ? -value : value;
}

/* โดนกลางไม้ = 0 · ปลายบน = -1 · ปลายล่าง = +1 */
static bento_real_t hit_offset(bento_real_t ball_y, int32_t paddle_y)
{
    const bento_real_t half_ball = (bento_real_t)BALL / (bento_real_t)2;
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;
    const bento_real_t ball_centre = ball_y + half_ball;
    const bento_real_t paddle_centre =
        (bento_real_t)paddle_y + half_paddle;

    return clamp_real((ball_centre - paddle_centre) / half_paddle,
                      (bento_real_t)-1.0, (bento_real_t)1.0);
}

/* การ์ด 2: มุมกระทบ แล้วจำกัดเพดาน (การ์ด 4) */
static bento_real_t paddle_vy(bento_real_t speed_y, bento_real_t offset)
{
    return clamp_real(speed_y + ((bento_real_t)2 * offset), -VY_MAX,
                      VY_MAX);
}

/* "%+.<digits>f": MicroPython's digits, with a '+' on a positive. */
static void format_signed(char *text, uint32_t size, bento_real_t value,
                          int32_t digits)
{
    char digits_text[BENTO_FMT_BUF_SIZE];
    uint32_t length = 0U;

    digits_text[0] = '\0';
    /* BENTO_FMT_BUF_SIZE holds any result, so it cannot fail */
    (void)bento_fmt_real_precision(digits_text,
                                   (uint32_t)sizeof(digits_text), value,
                                   digits, &length);
    (void)snprintf(text, size, "%s%s",
                   (digits_text[0] == '-') ? "" : "+", digits_text);
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

static bento_status_t add_walls(bounce_t *bounce)
{
    bento_game_t *game = &bounce->game;
    const int32_t wall_top = (int32_t)TOP - WALL_THICK_PX;
    const bento_real_t wall_y = (bento_real_t)wall_top;
    bento_status_t status = bento_game_box_create(
        game, &bounce->walls[WALL_TOP], (bento_real_t)0, wall_y,
        (bento_real_t)BENTO_GAME_WIDTH, (bento_real_t)WALL_THICK_PX,
        COLOR_WALL);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &bounce->walls[WALL_BOTTOM], (bento_real_t)0,
            (bento_real_t)BOTTOM, (bento_real_t)BENTO_GAME_WIDTH,
            (bento_real_t)WALL_THICK_PX, COLOR_WALL);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &bounce->walls[WALL_RIGHT], (bento_real_t)RIGHT,
            wall_y, (bento_real_t)RIGHT_WALL_W,
            (bento_real_t)((BOTTOM - TOP) + (2 * WALL_THICK_PX)),
            COLOR_WALL);
    }
    return status;
}

static bento_status_t build_screen(bounce_t *bounce)
{
    bento_game_t *game = &bounce->game;
    bento_game_text_t rule;
    bento_status_t status;

    bento_game_text_init(&rule);
    status = bento_game_text_create(
        game, &rule, "ชนผนัง:  vy = -vy        ชนไม้:  vy += 2 * offset",
        TEXT_X, RULE_Y, BENTO_GAME_YELLOW);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &bounce->hud_hit, "รับลูกด้วยไม้ แล้วดูตัวเลขตรงนี้", TEXT_X,
            HUD_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &bounce->hud_score,
                                        "รับได้ 0   พลาด 0", SCORE_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_walls(bounce);
    }
    if (status == BENTO_OK) {
        bounce->paddle_y = (TOP + BOTTOM - PADDLE_H) / 2;
        status = bento_game_sprite_create(
            game, &bounce->paddle, "paddle", (bento_real_t)PADDLE_X,
            (bento_real_t)bounce->paddle_y);
    }
    if (status == BENTO_OK) {
        /* parked off screen until the first hit */
        status = bento_game_box_create(
            game, &bounce->mark, (bento_real_t)MARK_PARK_X,
            (bento_real_t)0, (bento_real_t)MARK_W, (bento_real_t)MARK_H,
            BENTO_GAME_GREEN);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(game, &bounce->ball, "ball",
                                          (bento_real_t)SERVE_X,
                                          (bento_real_t)SERVE_Y);
    }
    return status;
}

/* The wall the ball hit flashes white for FLASH frames. */
static bento_status_t flash(bounce_t *bounce, wall_t wall)
{
    bento_status_t status = BENTO_OK;

    if (bounce->flash_wall != WALL_NONE) {
        status = bento_game_box_set_color(
            &bounce->game, &bounce->walls[bounce->flash_wall],
            COLOR_WALL);
    }
    bounce->flash_wall = wall;
    bounce->flash_left = FLASH;
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &bounce->game, &bounce->walls[wall], BENTO_GAME_WHITE);
    }
    bento_game_sfx(&bounce->game, "wall");
    return status;
}

/* เข้า: ขยับไม้แล้วจำกัดขอบ (จุดโดนเลื่อนตามไม้) */
static bento_status_t move_paddle(bounce_t *bounce,
                                  const bento_game_keys_t *keys)
{
    const int32_t step =
        (keys->down ? PADDLE_SPEED : 0) - (keys->up ? PADDLE_SPEED : 0);
    const int32_t new_y =
        clamp_int(bounce->paddle_y + step, TOP, BOTTOM - PADDLE_H);
    bento_status_t status = BENTO_OK;

    if (new_y != bounce->paddle_y) {
        if (bounce->hits != 0) {
            const int32_t moved = new_y - bounce->paddle_y;

            status = bento_game_box_move_to(
                &bounce->game, &bounce->mark, bounce->mark.x,
                bounce->mark.y + (bento_real_t)moved);
        }
        bounce->paddle_y = new_y;
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(
                &bounce->game, &bounce->paddle, (bento_real_t)PADDLE_X,
                (bento_real_t)new_y);
        }
    }
    return status;
}

/* นับถอยหลังแล้วคืนสีผนัง */
static bento_status_t count_flash(bounce_t *bounce)
{
    bento_status_t status = BENTO_OK;

    if (bounce->flash_left > 0) {
        bounce->flash_left -= 1;
        if (bounce->flash_left == 0) {
            status = bento_game_box_set_color(
                &bounce->game, &bounce->walls[bounce->flash_wall],
                COLOR_WALL);
            bounce->flash_wall = WALL_NONE;
        }
    }
    return status;
}

/* คิด: สะสมค่า แล้วสะท้อน = ดึงกลับเข้าขอบ + กลับเครื่องหมาย */
static bento_status_t move_ball(bounce_t *bounce)
{
    const int32_t lowest_px = BOTTOM - BALL;
    const int32_t rightmost_px = RIGHT - BALL;
    const bento_real_t lowest = (bento_real_t)lowest_px;
    const bento_real_t rightmost = (bento_real_t)rightmost_px;
    bento_status_t status = BENTO_OK;

    bounce->ball_x += bounce->speed_x;
    bounce->ball_y += bounce->speed_y;
    if (bounce->ball_y <= (bento_real_t)TOP) {
        bounce->ball_y = (bento_real_t)TOP;
        bounce->speed_y = -bounce->speed_y;
        status = flash(bounce, WALL_TOP);
    } else if (bounce->ball_y >= lowest) {
        bounce->ball_y = lowest;
        bounce->speed_y = -bounce->speed_y;
        status = flash(bounce, WALL_BOTTOM);
    } else {
        /* still between the walls */
    }
    if ((status == BENTO_OK) && (bounce->ball_x >= rightmost)) {
        bounce->ball_x = rightmost;
        bounce->speed_x = -bounce->speed_x;
        status = flash(bounce, WALL_RIGHT);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_move_to(&bounce->game, &bounce->ball,
                                      bounce->ball_x, bounce->ball_y);
    }
    return status;
}

static bool ball_hits_paddle(const bounce_t *bounce)
{
    return bento_game_hit_rect(
        bounce->ball.x, bounce->ball.y, (bento_real_t)BALL,
        (bento_real_t)BALL, bounce->paddle.x, bounce->paddle.y,
        (bento_real_t)PADDLE_W, (bento_real_t)PADDLE_H);
}

/* เขียว = กลางไม้ · เหลือง · ส้ม = ปลายไม้ */
static uint32_t mark_color(bento_real_t offset)
{
    const bento_real_t green_limit = (bento_real_t)0.34;
    const bento_real_t yellow_limit = (bento_real_t)0.67;
    const bento_real_t distance = absolute(offset);
    uint32_t color = BENTO_GAME_ORANGE;

    if (distance < green_limit) {
        color = BENTO_GAME_GREEN;
    } else if (distance < yellow_limit) {
        color = BENTO_GAME_YELLOW;
    } else {
        /* the paddle's end */
    }
    return color;
}

static bento_status_t show_hit(bounce_t *bounce, bento_real_t offset)
{
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;
    const bento_real_t reach =
        (offset * (bento_real_t)PADDLE_H) / (bento_real_t)2;
    const int32_t mark_y =
        (int32_t)(((bento_real_t)bounce->paddle_y + half_paddle) +
                  reach) -
        (MARK_H / 2);
    char offset_text[BENTO_FMT_BUF_SIZE + 1U];
    char change_text[BENTO_FMT_BUF_SIZE + 1U];
    char speed_text[BENTO_FMT_BUF_SIZE + 1U];
    char text[TEXT_CHARS];
    bento_status_t status = bento_game_box_set_color(
        &bounce->game, &bounce->mark, mark_color(offset));

    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&bounce->game, &bounce->mark,
                                        (bento_real_t)PADDLE_FACE_X,
                                        (bento_real_t)mark_y);
    }
    format_signed(offset_text, (uint32_t)sizeof(offset_text), offset,
                  2);
    format_signed(change_text, (uint32_t)sizeof(change_text),
                  (bento_real_t)2 * offset, 2);
    format_signed(speed_text, (uint32_t)sizeof(speed_text),
                  bounce->speed_y, 1);
    (void)snprintf(text, sizeof(text),
                   "offset %s  ->  vy += %s  ->  vy %s", offset_text,
                   change_text, speed_text);
    if (status == BENTO_OK) {
        status =
            bento_game_text_set(&bounce->game, &bounce->hud_hit, text);
    }
    bento_game_sfx(&bounce->game, "paddle");
    return status;
}

static void serve_again(bounce_t *bounce)
{
    bounce->misses += 1;
    bounce->ball_x = (bento_real_t)SERVE_X;
    bounce->ball_y = (bento_real_t)SERVE_Y;
    bounce->speed_x = -BALL_VX;
    bounce->speed_y =
        ((bounce->misses % 2) != 0) ? SERVE_VY : -SERVE_VY;
    bento_game_sfx(&bounce->game, "lose");
}

/* ออก: ชนไม้ = มุมกระทบ · หลุดซ้าย = พลาด เสิร์ฟใหม่ */
static bento_status_t catch_or_miss(bounce_t *bounce)
{
    bool changed = true;
    bento_status_t status = BENTO_OK;

    if ((bounce->speed_x < (bento_real_t)0) &&
        ball_hits_paddle(bounce)) {
        const bento_real_t offset =
            hit_offset(bounce->ball_y, bounce->paddle_y);

        bounce->ball_x = (bento_real_t)PADDLE_FACE_X;
        bounce->speed_x = absolute(bounce->speed_x);
        bounce->speed_y = paddle_vy(bounce->speed_y, offset);
        bounce->hits += 1;
        status = show_hit(bounce, offset);
    } else if (bounce->ball_x < (bento_real_t)-BALL) {
        serve_again(bounce);
    } else {
        changed = false;
    }
    if ((status == BENTO_OK) && changed) {
        char text[TEXT_CHARS];

        (void)snprintf(text, sizeof(text),
                       "รับได้ %" PRId32 "   พลาด %" PRId32, bounce->hits,
                       bounce->misses);
        status = bento_game_text_set(&bounce->game, &bounce->hud_score,
                                     text);
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    bounce_t *bounce = &g_bounce;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;        /* the state is g_bounce */
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&bounce->game, &keys);
    if (status == BENTO_OK) {
        status = move_paddle(bounce, &keys);
    }
    if (status == BENTO_OK) {
        status = count_flash(bounce);
    }
    if (status == BENTO_OK) {
        status = move_ball(bounce);
    }
    if (status == BENTO_OK) {
        status = catch_or_miss(bounce);
    }
    return status;
}

static void reset_state(bounce_t *bounce)
{
    uint32_t i;

    (void)memset(bounce, 0, sizeof(*bounce));
    bento_game_init(&bounce->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_text_init(&bounce->hud_hit);
    bento_game_text_init(&bounce->hud_score);
    for (i = 0U; i < (uint32_t)WALL_COUNT; i++) {
        bento_game_box_init(&bounce->walls[i]);
    }
    bento_game_sprite_init(&bounce->paddle);
    bento_game_box_init(&bounce->mark);
    bento_game_sprite_init(&bounce->ball);
    bounce->ball_x = (bento_real_t)SERVE_X;
    bounce->ball_y = (bento_real_t)SERVE_Y;
    bounce->speed_x = -BALL_VX;
    bounce->speed_y = SERVE_VY;
    bounce->flash_wall = WALL_NONE;
}

int main(void)
{
    bounce_t *bounce = &g_bounce;
    bool quit = false;
    bento_status_t status;

    reset_state(bounce);
    status = bento_game_title(&bounce->game, "BOUNCE", NULL, &quit);
    report_joystick(&bounce->game);
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(bounce);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&bounce->game, on_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
