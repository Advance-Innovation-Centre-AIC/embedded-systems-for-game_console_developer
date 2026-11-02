/*
 * pong_full.c -- เกมเต็มของ Pong (ผู้เล่น vs AI)
 *
 * ลอก "หน้าตาและจังหวะ" มาจากเกม C บนเครื่อง (page_game_pong.c) ให้ครบที่สุด:
 * โต๊ะสักหลาดเขียว + เส้นเน็ต, ไม้กับลูกเป็น sprite ชุดเดียวกับเกม C,
 * เมนูเลือกความยาก, HUD Player/AI/Best, เสียงครบทุกจังหวะ และจบเกมแล้ว
 * กด Start เล่นต่อได้ทันทีไม่ต้องรีบูตเครื่อง
 * ปุ่ม: UP/DOWN ขยับไม้   Y เริ่มตาใหม่   Start พักเกม   Back ออก
 *
 * The full Pong, the player against the AI, with the look and rhythm of
 * the on-board C game: a green felt table with a net, paddle and ball
 * sprites, an EASY / NORMAL / HARD menu, a Player / AI / Best line and a
 * sound for every event. The paddle speeds up while UP or DOWN is held
 * and glides to a stop on friction; where the ball meets the paddle sets
 * its angle. After a match, Start plays again from the menu.
 *
 * Twin of pong_full.py (embedded-systems-for-game_console_developer/developer-i/full_games/pong_full.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the module globals are one pong_t at file scope (it is large), and
 *     update() reads it there, not through its void * argument;
 *   - MODES is a table of structs; the sprites' .w / .h, which Python sets
 *     by hand for game.hit(), are the constants given to
 *     bento_game_hit_rect with the sprites' x / y;
 *   - (vx * vx + vy * vy) ** 0.5 is pow() in double, rounded to
 *     bento_real_t: on a PSoC Edge kit it can differ from MicroPython's
 *     powf in the last bit;
 *   - Python's `return False` in update() sets *out_stop;
 *   - a bentogame call that raises in MicroPython returns its status here;
 *     title()'s Back (SystemExit) and the IDE's Stop end the program with 0.
 */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* ---- ค่าคงที่ชุดเดียวกับเกม C เป๊ะ (พิกเซล) ---- */
enum {
    PADDLE_W = 14,
    PADDLE_H = 90,
    BALL_SIZE = 14,
    PADDLE_X = 14,     /* ระยะไม้ห่างจากขอบจอ */
    SERVE_SPREAD = 22, /* มุมเสิร์ฟสุ่ม: vy สุ่มใน -2.2 ถึง 2.2 */
    SCORE_MARGIN = 25, /* ลูกเลยขอบจอไปเท่านี้ = ได้แต้ม */
    AI_DEADBAND = 5,   /* AI พอใจเมื่อห่างจากลูกไม่เกินเท่านี้ */
    EDGE = 3,          /* ความหนาขอบโต๊ะ */
    NET_DASHES = 14,   /* game.net()'s defaults */
    NET_DASH_H = 12,
    HUD_X = 16,
    HUD_Y = 4,
    HELP_DY = 46, /* ขยับขึ้นหนึ่งบรรทัดหลบป้ายคุมของ run() */
    HELP_CHAR_PX = 4,
    MODE_COUNT = 3,
    START_MODE = 1, /* เปิดมาชี้ NORMAL ก่อน เหมือนเกม C */
    FPS = 60,
    HUD_BYTES = 72,  /* three int32_t and the words */
    HELP_BYTES = 80, /* a mode name, an int32_t and the words */
    SERVE_TENTHS = 10
};

/* Colours, 0xRRGGBB. */
enum {
    FELT = 0x1C7A34, /* สีโต๊ะปิงปอง (สักหลาดเขียว) สีเดียวกับเกม C */
    CHALK = 0xF0F8F0 /* สีขอบโต๊ะและเส้นเน็ต */
};

typedef struct {
    const char *name;
    bento_real_t ai_speed; /* ความเร็ว AI ต่อเฟรม */
    bento_real_t ball_mul; /* ตัวคูณความเร็วลูก */
    int32_t win_score;     /* แต้มชนะ */
} pong_mode_t;

/* โหมดความยาก ตรงกับตาราง s_pong_modes ของเกม C: NORMAL คือจังหวะมาตรฐาน */
static const pong_mode_t MODES[MODE_COUNT] = {
    {"EASY", (bento_real_t)2.2, (bento_real_t)0.85, 7},
    {"NORMAL", (bento_real_t)3.2, (bento_real_t)1.0, 10},
    {"HARD", (bento_real_t)4.6, (bento_real_t)1.2, 10}};

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t net[NET_DASHES];
    bento_game_sprite_t left_paddle;
    bento_game_sprite_t right_paddle;
    bento_game_sprite_t ball;
    bento_game_text_t hud;
    const pong_mode_t *mode;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
    bento_real_t left_y;
    bento_real_t right_y;
    bento_real_t left_vy;
    int32_t left_score;
    int32_t right_score;
    int32_t session_best;
    int32_t paddle_start_y;
} pong_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern pong_t g_pong;
pong_t g_pong;

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

/* max(low, min(high, value)) */
static bento_real_t clamp(bento_real_t low, bento_real_t high,
                          bento_real_t value)
{
    const bento_real_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static bento_real_t absolute(bento_real_t value)
{
    return (value < (bento_real_t)0) ? -value : value;
}

/* วางลูกกลางโต๊ะ แล้วเสิร์ฟออกด้วยมุมสุ่มเล็กน้อย (เหมือน lv_rand ของเกม C) */
static bento_status_t serve(pong_t *pong, bool to_right)
{
    /* ก่อนคูณตามโหมด */
    const bento_real_t ball_speed0 = (bento_real_t)6.2;
    const bento_real_t two = (bento_real_t)2;
    bento_real_t speed;
    int32_t spread = 0;
    bento_status_t status;

    pong->ball_x = ((bento_real_t)pong->game.width / two) -
                   ((bento_real_t)BALL_SIZE / two);
    pong->ball_y = ((bento_real_t)pong->game.height / two) -
                   ((bento_real_t)BALL_SIZE / two);
    speed = ball_speed0 * pong->mode->ball_mul;
    pong->ball_vx = to_right ? speed : -speed;
    status = bento_random_randint(&pong->random, -SERVE_SPREAD,
                                  SERVE_SPREAD, &spread);
    pong->ball_vy = (bento_real_t)spread / (bento_real_t)SERVE_TENTHS;
    return status;
}

/* จำกัดความเร็ว "รวม" ของลูก (ทั้งแกน x และ y) ไม่ให้ทะลุเพดาน */
static void clamp_ball_speed(pong_t *pong)
{
    const bento_real_t ball_maxv = (bento_real_t)14.0;
    const bento_real_t square_x = pong->ball_vx * pong->ball_vx;
    const bento_real_t square_y = pong->ball_vy * pong->ball_vy;
    const bento_real_t square_sum = square_x + square_y;
    const bento_real_t speed =
        (bento_real_t)pow((double)square_sum, 0.5);

    if (speed > ball_maxv) {
        const bento_real_t factor = ball_maxv / speed;

        pong->ball_vx *= factor;
        pong->ball_vy *= factor;
    }
}

/* Best นับจากฝั่งที่ทำแต้มสูงสุด (นับ AI ด้วย) แบบเดียวกับเกม C */
static bento_status_t update_hud(pong_t *pong)
{
    char text[HUD_BYTES];

    if (pong->left_score > pong->session_best) {
        pong->session_best = pong->left_score;
    }
    if (pong->right_score > pong->session_best) {
        pong->session_best = pong->right_score;
    }
    (void)snprintf(
        text, sizeof(text),
        "Player: %" PRId32 "    AI: %" PRId32 "    Best: %" PRId32,
        pong->left_score, pong->right_score, pong->session_best);
    return bento_game_text_set(&pong->game, &pong->hud, text);
}

/* ปุ่ม Y ของเกม C: ล้างแต้มแล้วเริ่มแมตช์ใหม่ทันทีในโหมดเดิม */
static bento_status_t new_round(pong_t *pong)
{
    bento_status_t status;

    pong->left_score = 0;
    pong->right_score = 0;
    pong->left_y = (bento_real_t)pong->paddle_start_y;
    pong->right_y = (bento_real_t)pong->paddle_start_y;
    pong->left_vy = (bento_real_t)0;
    status = update_hud(pong);
    if (status == BENTO_OK) {
        status = serve(pong, true);
    }
    return status;
}

/* ไม้ผู้เล่น: กดค้างเร่งความเร็ว ปล่อยแล้วหน่วงด้วยแรงเสียดทาน (มือเดียวกับเกม C) */
static bento_status_t steer_player(pong_t *pong,
                                   const bento_game_keys_t *keys)
{
    const bento_real_t paddle_accel = (bento_real_t)1.5;
    const bento_real_t paddle_maxv = (bento_real_t)13.0;
    const bento_real_t paddle_friction = (bento_real_t)0.78;
    const bento_real_t lowest =
        (bento_real_t)(pong->game.height - PADDLE_H);

    if (keys->up && !keys->down) {
        pong->left_vy -= paddle_accel;
    } else if (keys->down && !keys->up) {
        pong->left_vy += paddle_accel;
    } else {
        pong->left_vy *= paddle_friction;
    }
    pong->left_vy = clamp(-paddle_maxv, paddle_maxv, pong->left_vy);
    pong->left_y += pong->left_vy;
    if ((pong->left_y <= (bento_real_t)0) || (pong->left_y >= lowest)) {
        pong->left_y = clamp((bento_real_t)0, lowest, pong->left_y);
        pong->left_vy = (bento_real_t)0; /* ชนขอบแล้วหยุดนิ่ง ไม่มีเด้ง */
    }
    return bento_game_sprite_move_to(&pong->game, &pong->left_paddle,
                                     (bento_real_t)PADDLE_X,
                                     pong->left_y);
}

/* ลูกวิ่ง + เด้งขอบบนล่าง */
static void move_ball(pong_t *pong)
{
    const bento_real_t lowest =
        (bento_real_t)(pong->game.height - BALL_SIZE);

    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if (pong->ball_y <= (bento_real_t)0) {
        pong->ball_y = (bento_real_t)0;
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    } else if (pong->ball_y >= lowest) {
        pong->ball_y = lowest;
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    } else {
        /* the ball is between the walls */
    }
}

/* game.hit(ball, paddle) with the sizes the program gives the sprites */
static bool ball_hits(const pong_t *pong,
                      const bento_game_sprite_t *paddle)
{
    return bento_game_hit_rect(
        pong->ball.x, pong->ball.y, (bento_real_t)BALL_SIZE,
        (bento_real_t)BALL_SIZE, paddle->x, paddle->y,
        (bento_real_t)PADDLE_W, (bento_real_t)PADDLE_H);
}

/* Where the ball met the paddle, -1 at the top end to 1 at the bottom. */
static bento_real_t hit_offset(const pong_t *pong,
                               bento_real_t paddle_y)
{
    const bento_real_t two = (bento_real_t)2;
    const bento_real_t ball_middle =
        pong->ball_y + ((bento_real_t)BALL_SIZE / two);
    const bento_real_t paddle_middle =
        paddle_y + ((bento_real_t)PADDLE_H / two);

    return (ball_middle - paddle_middle) /
           ((bento_real_t)PADDLE_H / two);
}

/* ตีโดนไม้: สะท้อนกลับ + เร่งขึ้น + มุมสะท้อนตามจุดที่โดนหน้าไม้
 * (โดนปลายไม้มุมยิ่งแรง) */
static void bounce_off_paddles(pong_t *pong)
{
    const bento_real_t ball_speedup = (bento_real_t)0.35;
    const bento_real_t paddle_angle = (bento_real_t)2.0;
    const bento_real_t paddle_spin = (bento_real_t)0.28;

    if ((pong->ball_vx < (bento_real_t)0) &&
        ball_hits(pong, &pong->left_paddle)) {
        const bento_real_t norm = hit_offset(pong, pong->left_y);
        const int32_t paddle_right = PADDLE_X + PADDLE_W;

        pong->ball_x = (bento_real_t)paddle_right;
        pong->ball_vx = absolute(pong->ball_vx) + ball_speedup;
        pong->ball_vy += norm * paddle_angle;
        /* ไม้ที่กำลังวิ่งใส่สปินเพิ่มให้อีก */
        pong->ball_vy += pong->left_vy * paddle_spin;
        clamp_ball_speed(pong);
        bento_game_sfx(&pong->game, "paddle");
    } else if ((pong->ball_vx > (bento_real_t)0) &&
               ball_hits(pong, &pong->right_paddle)) {
        const bento_real_t norm = hit_offset(pong, pong->right_y);

        pong->ball_x = (bento_real_t)(pong->game.width - PADDLE_X -
                                      PADDLE_W - BALL_SIZE);
        pong->ball_vx = -(absolute(pong->ball_vx) + ball_speedup);
        pong->ball_vy += norm * paddle_angle;
        clamp_ball_speed(pong);
        bento_game_sfx(&pong->game, "paddle");
    } else {
        /* no paddle was hit */
    }
}

/* One side scored: the HUD, then the match's end or the next serve towards
 * the side that just scored. */
static bento_status_t award_point(pong_t *pong, int32_t *score,
                                  const char *win_sound, bool to_right,
                                  bool *out_stop)
{
    bento_status_t status;

    *score += 1;
    status = update_hud(pong);
    if ((status == BENTO_OK) && (*score >= pong->mode->win_score)) {
        bento_game_sfx(&pong->game, win_sound);
        *out_stop = true;
    } else if (status == BENTO_OK) {
        bento_game_sfx(&pong->game, "pong_score");
        status = serve(pong, to_right);
    } else {
        /* the status ends the frame */
    }
    return status;
}

/* ได้แต้มเมื่อลูกเลยขอบจอไป 25px (ตามเกม C) */
static bento_status_t check_score(pong_t *pong, bool *out_stop)
{
    bento_status_t status = BENTO_OK;

    if (pong->ball_x < (bento_real_t)(-SCORE_MARGIN)) {
        status = award_point(pong, &pong->right_score, "lose", true,
                             out_stop);
    } else if (pong->ball_x >
               (bento_real_t)(pong->game.width + SCORE_MARGIN)) {
        status = award_point(pong, &pong->left_score, "win", false,
                             out_stop);
    } else {
        /* the ball is still on the table */
    }
    return status;
}

/* ไม้ AI: ไล่ตามลูกด้วยความเร็วคงที่ตามโหมด (±5px คือระยะที่ AI พอใจแล้วหยุด) */
static bento_status_t move_ai(pong_t *pong)
{
    const bento_real_t two = (bento_real_t)2;
    const bento_real_t ball_middle =
        pong->ball_y + ((bento_real_t)BALL_SIZE / two);
    const bento_real_t paddle_middle =
        pong->right_y + ((bento_real_t)PADDLE_H / two);

    if (ball_middle > (paddle_middle + (bento_real_t)AI_DEADBAND)) {
        pong->right_y += pong->mode->ai_speed;
    } else if (ball_middle <
               (paddle_middle - (bento_real_t)AI_DEADBAND)) {
        pong->right_y -= pong->mode->ai_speed;
    } else {
        /* close enough: the AI rests */
    }
    pong->right_y = clamp((bento_real_t)0,
                          (bento_real_t)(pong->game.height - PADDLE_H),
                          pong->right_y);
    return bento_game_sprite_move_to(
        &pong->game, &pong->right_paddle,
        (bento_real_t)(pong->game.width - PADDLE_X - PADDLE_W),
        pong->right_y);
}

/* One frame of the match. */
static bento_status_t update(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&pong->game, &keys);
    /* Y = เริ่มตาใหม่ (กดค้างไม่นับซ้ำ) */
    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&pong->game, BENTO_GAME_KEY_Y, &keys)) {
        status = new_round(pong);
    }
    if (status == BENTO_OK) {
        status = steer_player(pong, &keys);
    }
    if (status == BENTO_OK) {
        move_ball(pong);
        bounce_off_paddles(pong);
        /* วาดลูก "หลัง" เคลียร์เรื่องชนไม้แล้วเท่านั้น (ลำดับเดียวกับเกม C) */
        status = bento_game_sprite_move_to(&pong->game, &pong->ball,
                                           pong->ball_x, pong->ball_y);
    }
    if (status == BENTO_OK) {
        status = check_score(pong, out_stop);
    }
    if ((status == BENTO_OK) && !*out_stop) {
        status = move_ai(pong);
    }
    return status;
}

/* One chalk box of the table's edge. */
static bento_status_t add_edge(pong_t *pong, int32_t x, int32_t y,
                               int32_t width, int32_t height)
{
    bento_game_box_t edge;

    bento_game_box_init(&edge);
    return bento_game_box_create(&pong->game, &edge, (bento_real_t)x,
                                 (bento_real_t)y, (bento_real_t)width,
                                 (bento_real_t)height, CHALK);
}

/* จัดฉากโต๊ะปิงปอง (สร้างพื้นก่อนเสมอ ของที่สร้างทีหลังจะทับอยู่ข้างบน) */
static bento_status_t add_table(pong_t *pong)
{
    const int32_t width = pong->game.width;
    const int32_t height = pong->game.height;
    bento_game_box_t felt;
    bento_status_t status;

    bento_game_box_init(&felt);
    status = bento_game_background(&pong->game, &felt, FELT);
    if (status == BENTO_OK) {
        status = add_edge(pong, 0, 0, width, EDGE); /* ขอบบน */
    }
    if (status == BENTO_OK) {
        /* ขอบล่าง */
        status = add_edge(pong, 0, height - EDGE, width, EDGE);
    }
    if (status == BENTO_OK) {
        status = add_edge(pong, 0, 0, EDGE, height); /* ขอบซ้าย */
    }
    if (status == BENTO_OK) {
        /* ขอบขวา */
        status = add_edge(pong, width - EDGE, 0, EDGE, height);
    }
    if (status == BENTO_OK) {
        /* เส้นเน็ตประกลางโต๊ะ */
        status =
            bento_game_net(&pong->game, pong->net, (uint32_t)NET_DASHES,
                           CHALK, NET_DASH_H);
    }
    return status;
}

/* ไม้กับลูกใช้ sprite ลายเดียวกับเกม C (ไม้ปิงปองจริง + ลูกมีไฮไลต์) */
static bento_status_t add_players(pong_t *pong)
{
    const int32_t width = pong->game.width;
    const int32_t middle_x = width / 2; /* floor division */
    const int32_t middle_y = pong->game.height / 2;
    bento_status_t status;

    pong->paddle_start_y = (pong->game.height / 2) - (PADDLE_H / 2);
    status = bento_game_sprite_create(
        &pong->game, &pong->left_paddle, "paddle",
        (bento_real_t)PADDLE_X, (bento_real_t)pong->paddle_start_y);
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(
            &pong->game, &pong->right_paddle, "paddle",
            (bento_real_t)(width - PADDLE_X - PADDLE_W),
            (bento_real_t)pong->paddle_start_y);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_create(
            &pong->game, &pong->ball, "ball", (bento_real_t)middle_x,
            (bento_real_t)middle_y);
    }
    return status;
}

/* The HUD and the help line: the C game's sentence and place (centred,
 * dark on the felt). */
static bento_status_t add_labels(pong_t *pong)
{
    char help[HELP_BYTES];
    bento_game_text_t help_text;
    bento_status_t status = bento_game_text_create(
        &pong->game, &pong->hud, "", HUD_X, HUD_Y, BENTO_GAME_WHITE);

    bento_game_text_init(&help_text);
    (void)snprintf(help, sizeof(help),
                   "%s  -  UP/DOWN move, Score %" PRId32
                   ", Y = new round",
                   pong->mode->name, pong->mode->win_score);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            &pong->game, &help_text, help,
            (pong->game.width / 2) -
                ((int32_t)strlen(help) * HELP_CHAR_PX),
            pong->game.height - HELP_DY, BENTO_GAME_GB_DARKEST);
    }
    return status;
}

/* One match from the table to its end; *out_finished false on Back. */
static bento_status_t play_match(pong_t *pong, bool *out_finished)
{
    bento_status_t status = add_table(pong);

    if (status == BENTO_OK) {
        status = add_players(pong);
    }
    if (status == BENTO_OK) {
        status = add_labels(pong);
    }
    pong->left_y = (bento_real_t)pong->paddle_start_y;
    pong->right_y = (bento_real_t)pong->paddle_start_y;
    pong->left_vy = (bento_real_t)0;
    pong->left_score = 0;
    pong->right_score = 0;
    if (status == BENTO_OK) {
        status = update_hud(pong);
    }
    if (status == BENTO_OK) {
        /* เกม C เปิดแมตช์ด้วยการเสิร์ฟไปทางฝั่ง AI */
        status = serve(pong, true);
    }
    if (status == BENTO_OK) {
        status = bento_game_run(&pong->game, update, NULL, FPS,
                                out_finished);
    }
    return status;
}

/* The match's result and its question: play again? */
static bento_status_t ask_again(pong_t *pong, bool *out_again)
{
    const bool player_won = pong->left_score > pong->right_score;
    const int32_t final_score =
        player_won ? pong->left_score : pong->right_score;

    return bento_game_over(&pong->game, &final_score, "pong",
                           player_won ? "Player wins!" : "AI wins",
                           out_again);
}

/* เลือกโหมดใหม่ได้ทุกแมตช์ (เกม C ก็กลับมาหน้าเลือกโหมดหลังจบเกมเช่นกัน) */
static bento_status_t play_matches(pong_t *pong)
{
    static const char *const options[MODE_COUNT] = {"EASY", "NORMAL",
                                                    "HARD"};
    int32_t mode = START_MODE;
    bool again = true;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && again) {
        int32_t picked = -1;
        bool finished = false;

        status =
            bento_game_menu(&pong->game, options, (uint32_t)MODE_COUNT,
                            NULL, mode, &picked);
        /* BACK = เลิกเล่น */
        again = (status == BENTO_OK) && (picked >= 0);
        if (again) {
            mode = picked;
            pong->mode = &MODES[mode];
            status = play_match(pong, &finished);
            /* Back = เลิกเล่น */
            again = (status == BENTO_OK) && finished;
        }
        if (again) {
            status = ask_again(pong, &again);
        }
    }
    return status;
}

int main(void)
{
    pong_t *pong = &g_pong;
    bool quit = false;
    bento_status_t status;

    (void)memset(pong, 0, sizeof(*pong));
    status = bento_random_init(&pong->random);
    bento_game_init(&pong->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_sprite_init(&pong->left_paddle);
    bento_game_sprite_init(&pong->right_paddle);
    bento_game_sprite_init(&pong->ball);
    bento_game_text_init(&pong->hud);
    pong->mode = &MODES[START_MODE];
    if (status == BENTO_OK) {
        status = bento_game_title_melody(&pong->game, "PONG", NULL,
                                         NULL, 0U, &quit);
        report_joystick(&pong->game);
    }
    if ((status == BENTO_OK) && !quit) {
        /* Best ค้างไว้จนกว่าจะปิดเครื่อง เหมือนเกม C */
        pong->session_best = bento_game_best(&pong->game, "pong");
        status = play_matches(pong);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
