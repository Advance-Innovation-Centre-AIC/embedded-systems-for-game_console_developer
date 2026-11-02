/*
 * pong_step5.c -- สร้าง Pong #5 (คาบ 13): เกมจริง คู่ต่อสู้ AI +
 * โหมดความยาก
 *
 * โครงเว้น 30%: หน้าเริ่ม "PONG" รอกด Start (Back = ออก) แล้ววางไม้ตี
 * สองฝั่ง ลูกบอล ป้ายคะแนน และคำแนะนำ เริ่มที่หน้าเลือกโหมด EASY /
 * NORMAL / HARD (UP/DOWN เลือก, A หรือ Start เริ่ม) แต่ละโหมดกำหนด
 * ความเร็ว AI ตัวคูณความเร็วลูก และแต้มที่ต้องชนะ ระหว่างเล่น ไม้ซ้าย
 * ขยับด้วย UP/DOWN แบบเร่งและไถลหยุดเอง ลูกเด้งบน/ล่างและตีกลับได้
 * จากไม้ทั้งสองฝั่ง ลูกหลุดข้างไหนอีกฝั่งได้แต้มแล้วเสิร์ฟใหม่ ใครถึง
 * แต้มชนะก่อนจบเกม กด Start กลางเกมเพื่อกลับหน้าเลือกโหมด ช่องที่
 * นักเรียนต้องเติม (สมอง AI ที่ไล่ตามลูก) ยังเป็น pass ไม้ขวาจึงอยู่นิ่ง
 *
 * A worksheet with 30% left blank: the title screen "PONG" waits for
 * Start (Back quits), then both paddles, the ball, the score line
 * and a hint. It opens on the mode picker EASY / NORMAL / HARD (UP
 * and DOWN choose, A or Start begins); each mode sets the AI's speed,
 * the ball's speed factor and the winning score. In a match UP and
 * DOWN speed the left paddle up and it slides to a stop on release;
 * the ball bounces off the top, the bottom and either paddle. A ball
 * that leaves one side scores for the other side and is served
 * again, and the first to the winning score ends the game. Start in
 * a match goes back to the picker. The student's blank (the AI that
 * follows the ball) is still `pass`, so the right paddle stays put.
 *
 * Twin of pong_step5.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/pong_step5.py),
 * from the library twin devi-pong-5 (py/devi-pong-5.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the module globals are one pong_t; it is 512 bytes or more, so it
 *     lives at file scope (C9) and the frame reads it there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - MODES, a list of tuples, is a static const table of game_mode_t;
 *   - Python's float is bento_real_t, the board's MicroPython float,
 *     and a * b + c is never fused (FP_CONTRACT below); min() and max()
 *     are smaller_of() and larger_of(), which keep Python's choice on
 *     a tie;
 *   - the "%" texts are snprintf() into a buffer sized for any int32_t;
 *   - PADDLE_START_Y is read from game.HEIGHT before title() starts the
 *     game, which is BENTO_GAME_HEIGHT; the middle of the screen is
 *     CENTER_X and CENTER_Y, integer enums, as the floor division
 *     there is;
 *   - `return False` from the frame is *out_stop = true;
 *   - title()'s Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
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

/* a * b + c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The paddles, the ball, the texts and the game's pace. */
/* The middle of the screen: half of game.WIDTH and game.HEIGHT by
 * floor division; title()'s start() keeps the default 792 x 398. */
enum {
    CENTER_X = BENTO_GAME_WIDTH / 2,
    CENTER_Y = BENTO_GAME_HEIGHT / 2
};

enum {
    PADDLE_X = 20, /* the right paddle keeps the same gap to its edge */
    PADDLE_W = 14,
    PADDLE_H = 90,
    BALL_SIZE = 14,
    PADDLE_START_Y = (BENTO_GAME_HEIGHT / 2) - (PADDLE_H / 2),
    SCORE_X = 16,
    SCORE_Y = 12,
    HINT_HALF_W = 130, /* the hint sits about centred */
    HINT_BELOW_MIDDLE = 40,
    RESULT_HALF_W = 70, /* the result sits about centred */
    /* "Player: ", any int32_t, "   AI: ", any int32_t and the NUL */
    TEXT_BYTES = 48,
    FPS = 60
};

/* The modes: EASY, NORMAL, HARD; the game opens on NORMAL. */
enum {
    MODE_COUNT = 3,
    START_MODE = 1
};

/* One row of the mode table. */
typedef struct {
    const char *name;
    bento_real_t ai_speed; /* ความเร็วไล่ตามของ AI */
    bento_real_t ball_mul; /* ตัวคูณความเร็วลูก */
    int32_t win_score;
} game_mode_t;

/* ตารางโหมด (mirror ของ s_pong_modes[] ในเกม C) */
static const game_mode_t MODES[MODE_COUNT] = {
    {"EASY", (bento_real_t)2.2, (bento_real_t)0.85, 7},
    {"NORMAL", (bento_real_t)3.2, (bento_real_t)1.00, 10},
    {"HARD", (bento_real_t)4.6, (bento_real_t)1.20, 10}};

/* The serve's speed, read by the first ball and by every serve. */
static const bento_real_t SERVE_SPEED = (bento_real_t)6.2;
static const bento_real_t SERVE_VY = (bento_real_t)3.4;

typedef struct {
    bento_game_t game;
    bento_game_box_t player_paddle;
    bento_game_box_t ai_paddle;
    bento_game_box_t ball;
    bento_game_text_t score_text;
    bento_game_text_t hint;
    int32_t selected_mode;
    bool choosing_mode;
    bento_real_t ai_speed;
    bento_real_t ball_mul;
    int32_t win_score;
    bento_real_t ball_x;
    bento_real_t ball_y;
    bento_real_t ball_vx;
    bento_real_t ball_vy;
    bento_real_t player_y;
    bento_real_t player_speed;
    bento_real_t ai_y;
    int32_t player_score;
    int32_t ai_score;
    bool game_over;
    /* ปุ่มเฟรมก่อนหน้า เพื่อตรวจ "เพิ่งกด" (กันปุ่มค้างลั่นเมนูรัว ๆ) */
    bool prev_a;
    bool prev_up;
    bool prev_down;
} pong_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern pong_t g_pong;
pong_t g_pong;

/* One Emulator build serves both board models, so ask the board for
 * its name; a board that cannot say keeps the build target's answer.
 */
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

/* The PSoC Edge kits' joystick.init() prints this line when start()
 * opens the pad; the reTerminal's prints nothing. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* Python's min(a, b): a unless b is smaller. */
static bento_real_t smaller_of(bento_real_t first, bento_real_t second)
{
    return (second < first) ? second : first;
}

/* Python's max(a, b): a unless b is larger. */
static bento_real_t larger_of(bento_real_t first, bento_real_t second)
{
    return (second > first) ? second : first;
}

/* The picker's line on the score text. */
static bento_status_t show_menu(pong_t *pong)
{
    char line[TEXT_BYTES];

    (void)snprintf(line, sizeof(line), "SELECT MODE:  < %s >",
                   MODES[pong->selected_mode].name);
    return bento_game_text_set(&pong->game, &pong->score_text, line);
}

/* The score line during a match. */
static bento_status_t show_score(pong_t *pong)
{
    char line[TEXT_BYTES];

    (void)snprintf(line, sizeof(line),
                   "Player: %" PRId32 "   AI: %" PRId32,
                   pong->player_score, pong->ai_score);
    return bento_game_text_set(&pong->game, &pong->score_text, line);
}

/* The chosen mode's AI speed, ball factor and winning score. */
static void use_mode(pong_t *pong)
{
    const game_mode_t *mode = &MODES[pong->selected_mode];

    pong->ai_speed = mode->ai_speed;
    pong->ball_mul = mode->ball_mul;
    pong->win_score = mode->win_score;
}

/* The ball back in the middle, towards direction (1 right, -1 left). */
static void serve(pong_t *pong, int32_t direction)
{
    pong->ball_x = (bento_real_t)pong->game.width / (bento_real_t)2;
    pong->ball_y = (bento_real_t)pong->game.height / (bento_real_t)2;
    pong->ball_vx =
        SERVE_SPEED * pong->ball_mul * (bento_real_t)direction;
    pong->ball_vy = SERVE_VY;
}

/* A new match in the chosen mode, served to the right. */
static bento_status_t start_match(pong_t *pong)
{
    char line[TEXT_BYTES];
    bento_status_t status;

    use_mode(pong);
    pong->player_score = 0;
    pong->ai_score = 0;
    pong->player_y = (bento_real_t)PADDLE_START_Y;
    pong->ai_y = (bento_real_t)PADDLE_START_Y;
    pong->player_speed = (bento_real_t)0.0;
    pong->choosing_mode = false;
    (void)snprintf(line, sizeof(line), "Player: 0   AI: 0  (%s)",
                   MODES[pong->selected_mode].name);
    status = bento_game_text_set(&pong->game, &pong->score_text, line);
    if (status == BENTO_OK) {
        status = bento_game_text_set(&pong->game, &pong->hint,
                                     "UP/DOWN move, START menu");
    }
    if (status == BENTO_OK) {
        serve(pong, 1);
    }
    return status;
}

/* หน้าจอเลือกโหมด: a new press of UP or DOWN moves the choice (+ 2 of
 * 3 is one step back), a new press of A or Start begins. */
static bento_status_t choose_mode(pong_t *pong,
                                  const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    if (keys->up && !pong->prev_up) {
        pong->selected_mode = (pong->selected_mode + 2) % MODE_COUNT;
        status = show_menu(pong);
    } else if (keys->down && !pong->prev_down) {
        pong->selected_mode = (pong->selected_mode + 1) % MODE_COUNT;
        status = show_menu(pong);
    } else {
        /* no new press on UP or DOWN */
    }
    if ((status == BENTO_OK) &&
        ((keys->a && !pong->prev_a) || keys->start)) {
        status = start_match(pong);
    }
    if (status == BENTO_OK) {
        pong->prev_a = keys->a;
        pong->prev_up = keys->up;
        pong->prev_down = keys->down;
    }
    return status;
}

/* กลับไปหน้าเลือกโหมด: the keys still held do not count as new
 * presses there. */
static bento_status_t back_to_menu(pong_t *pong)
{
    bento_status_t status;

    pong->choosing_mode = true;
    pong->prev_a = true;
    pong->prev_up = true;
    pong->prev_down = true;
    status = show_menu(pong);
    if (status == BENTO_OK) {
        status = bento_game_text_set(&pong->game, &pong->hint,
                                     "UP/DOWN choose mode, A start");
    }
    return status;
}

/* ไม้ผู้เล่น (ซ้าย): เร่งเมื่อกดค้าง ไถลหยุดเองเมื่อปล่อย */
static bento_status_t move_player(pong_t *pong,
                                  const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.5;
    const bento_real_t max_speed = (bento_real_t)12.0;
    const bento_real_t friction = (bento_real_t)0.78;
    const bento_real_t lowest =
        (bento_real_t)(pong->game.height - PADDLE_H);
    bento_real_t moved_y;

    if (keys->up && !keys->down) {
        pong->player_speed -= accel;
    } else if (keys->down && !keys->up) {
        pong->player_speed += accel;
    } else {
        pong->player_speed *= friction;
    }
    pong->player_speed = larger_of(
        -max_speed, smaller_of(max_speed, pong->player_speed));
    moved_y = pong->player_y + pong->player_speed;
    pong->player_y =
        larger_of((bento_real_t)0, smaller_of(lowest, moved_y));
    return bento_game_box_move_to(&pong->game, &pong->player_paddle,
                                  (bento_real_t)PADDLE_X,
                                  pong->player_y);
}

/* AI tracker (ไม้ขวา): the student's chase is still blank, so ai_y
 * only stays on the field. */
static bento_status_t move_ai(pong_t *pong)
{
    const bento_real_t lowest =
        (bento_real_t)(pong->game.height - PADDLE_H);

    pong->ai_y =
        larger_of((bento_real_t)0, smaller_of(lowest, pong->ai_y));
    return bento_game_box_move_to(&pong->game, &pong->ai_paddle,
                                  pong->ai_paddle.x, pong->ai_y);
}

/* ลูกบอล + เด้งบน/ล่าง */
static bento_status_t move_ball(pong_t *pong)
{
    const bento_real_t bottom =
        (bento_real_t)(pong->game.height - BALL_SIZE);

    pong->ball_x += pong->ball_vx;
    pong->ball_y += pong->ball_vy;
    if ((pong->ball_y <= (bento_real_t)0) || (pong->ball_y >= bottom)) {
        pong->ball_vy = -pong->ball_vy;
        bento_game_sfx(&pong->game, "wall");
    }
    return bento_game_box_move_to(&pong->game, &pong->ball,
                                  pong->ball_x, pong->ball_y);
}

/* Where the ball met a paddle whose top is at paddle_y: about -1 at
 * the paddle's top end, 0 in its middle, 1 at its bottom end. */
static bento_real_t contact_offset(const pong_t *pong,
                                   bento_real_t paddle_y)
{
    const bento_real_t half_ball =
        (bento_real_t)BALL_SIZE / (bento_real_t)2;
    const bento_real_t half_paddle =
        (bento_real_t)PADDLE_H / (bento_real_t)2;

    return ((pong->ball_y + half_ball) - (paddle_y + half_paddle)) /
           half_paddle;
}

/* ตีไม้ซ้าย (มีสปินจากไม้) หรือไม้ขวา (AI) */
static void hit_paddles(pong_t *pong)
{
    const bento_real_t speedup = (bento_real_t)0.35;
    const bento_real_t ball_cap = (bento_real_t)14.0;
    const bento_real_t spin = (bento_real_t)0.28;
    bento_real_t hit_offset;

    if ((pong->ball_vx < (bento_real_t)0) &&
        bento_game_hit(&pong->ball, &pong->player_paddle)) {
        hit_offset = contact_offset(pong, pong->player_y);
        pong->ball_vx = smaller_of(-pong->ball_vx + speedup, ball_cap);
        pong->ball_vy += (hit_offset * (bento_real_t)2) +
                         (pong->player_speed * spin);
        bento_game_sfx(&pong->game, "paddle");
    } else if ((pong->ball_vx > (bento_real_t)0) &&
               bento_game_hit(&pong->ball, &pong->ai_paddle)) {
        hit_offset = contact_offset(pong, pong->ai_y);
        pong->ball_vx = -smaller_of(pong->ball_vx + speedup, ball_cap);
        pong->ball_vy += hit_offset * (bento_real_t)2;
        bento_game_sfx(&pong->game, "paddle");
    } else {
        /* neither paddle met the ball */
    }
}

/* คะแนน + เสิร์ฟใหม่: a ball out on one side scores for the other. */
static bento_status_t score_points(pong_t *pong)
{
    bento_status_t status = BENTO_OK;

    if (pong->ball_x < (bento_real_t)0) {
        pong->ai_score += 1;
        status = show_score(pong);
        if (status == BENTO_OK) {
            serve(pong, 1);
        }
    } else if (pong->ball_x > (bento_real_t)pong->game.width) {
        pong->player_score += 1;
        status = show_score(pong);
        if (status == BENTO_OK) {
            serve(pong, -1);
        }
    } else {
        /* the ball is still on the field */
    }
    return status;
}

/* จบเกม: the first to the mode's winning score ends the loop. */
static bento_status_t end_if_won(pong_t *pong, bool *out_stop)
{
    bento_game_t *game = &pong->game;
    bento_status_t status = BENTO_OK;

    if ((pong->player_score >= pong->win_score) ||
        (pong->ai_score >= pong->win_score)) {
        const bool player_won = pong->player_score > pong->ai_score;
        bento_game_text_t result;

        bento_game_text_init(&result);
        bento_game_sfx(game, player_won ? "win" : "lose");
        status = bento_game_text_create(
            game, &result, player_won ? "YOU WIN!" : "AI WINS",
            CENTER_X - RESULT_HALF_W, CENTER_Y,
            player_won ? BENTO_GAME_GREEN : BENTO_GAME_RED);
        if (status == BENTO_OK) {
            pong->game_over = true;
            *out_stop = true;
        }
    }
    return status;
}

/* One frame of a match. */
static bento_status_t
play_frame(pong_t *pong, const bento_game_keys_t *keys, bool *out_stop)
{
    bento_status_t status = move_player(pong, keys);

    if (status == BENTO_OK) {
        status = move_ai(pong);
    }
    if (status == BENTO_OK) {
        status = move_ball(pong);
    }
    if (status == BENTO_OK) {
        hit_paddles(pong);
        status = score_points(pong);
    }
    if (status == BENTO_OK) {
        status = end_if_won(pong, out_stop);
    }
    return status;
}

/* One frame: the end of the game stops the loop, the picker takes
 * the keys while it is open, and Start in a match goes back to it. */
static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    pong_t *pong = &g_pong;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_pong: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&pong->game, &keys);
    if (pong->game_over) {
        *out_stop = true;
    } else if (pong->choosing_mode) {
        status = choose_mode(pong, &keys);
    } else if (keys.start) {
        status = back_to_menu(pong);
    } else {
        status = play_frame(pong, &keys, out_stop);
    }
    return status;
}

/* Both paddles and the ball. */
static bento_status_t add_boxes(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    bento_status_t status = bento_game_box_create(
        game, &pong->player_paddle, (bento_real_t)PADDLE_X,
        (bento_real_t)PADDLE_START_Y, (bento_real_t)PADDLE_W,
        (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);

    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ai_paddle,
            (bento_real_t)(game->width - PADDLE_X - PADDLE_W),
            (bento_real_t)PADDLE_START_Y, (bento_real_t)PADDLE_W,
            (bento_real_t)PADDLE_H, BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &pong->ball, (bento_real_t)CENTER_X,
            (bento_real_t)CENTER_Y, (bento_real_t)BALL_SIZE,
            (bento_real_t)BALL_SIZE, BENTO_GAME_GB_LIGHTEST);
    }
    return status;
}

/* The score line and the hint. */
static bento_status_t add_texts(pong_t *pong)
{
    bento_game_t *game = &pong->game;
    bento_status_t status = bento_game_text_create(
        game, &pong->score_text, "Player: 0   AI: 0", SCORE_X, SCORE_Y,
        BENTO_GAME_WHITE);

    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &pong->hint, "UP/DOWN choose mode, A start",
            CENTER_X - HINT_HALF_W, CENTER_Y + HINT_BELOW_MIDDLE,
            BENTO_GAME_CYAN);
    }
    return status;
}

/* The state before the first frame: the picker open on NORMAL, the
 * ball in the middle, the paddles at their start. */
static void reset_state(pong_t *pong)
{
    pong->selected_mode = START_MODE;
    pong->choosing_mode = true;
    use_mode(pong);
    pong->ball_x = (bento_real_t)CENTER_X;
    pong->ball_y = (bento_real_t)CENTER_Y;
    pong->ball_vx = -SERVE_SPEED;
    pong->ball_vy = SERVE_VY;
    pong->player_y = (bento_real_t)PADDLE_START_Y;
    pong->player_speed = (bento_real_t)0.0;
    pong->ai_y = (bento_real_t)PADDLE_START_Y;
    pong->player_score = 0;
    pong->ai_score = 0;
    pong->game_over = false;
    /* mask ปุ่มแรกที่เข้าหน้าจอ */
    pong->prev_a = true;
    pong->prev_up = true;
    pong->prev_down = true;
}

/* The field, the state and the picker's line, then the game loop. */
static bento_status_t play(pong_t *pong)
{
    bool finished = false;
    bento_status_t status = add_boxes(pong);

    if (status == BENTO_OK) {
        status = add_texts(pong);
    }
    reset_state(pong);
    if (status == BENTO_OK) {
        status = show_menu(pong);
    }
    if (status == BENTO_OK) {
        status = bento_game_run(&pong->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return status;
}

int main(void)
{
    pong_t *pong = &g_pong;
    bento_ui_t *ui = bento_runtime_get_ui();
    bool quit = false;
    bento_status_t status;

    (void)memset(pong, 0, sizeof(*pong));
    bento_game_init(&pong->game, ui, bento_runtime_is_running);
    bento_game_box_init(&pong->player_paddle);
    bento_game_box_init(&pong->ai_paddle);
    bento_game_box_init(&pong->ball);
    bento_game_text_init(&pong->score_text);
    bento_game_text_init(&pong->hint);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&pong->game, "PONG", NULL, &quit);
    report_joystick(&pong->game);
    if ((status == BENTO_OK) && !quit) {
        status = play(pong);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
