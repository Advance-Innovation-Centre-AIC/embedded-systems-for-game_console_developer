/*
 * flappy_step5.c -- Flappy #5: เข้าใกล้เกมจริงบนคอนโซล (โหมดความยาก + best)
 *
 * step4 เล่นจบได้แล้ว คาบนี้ทำให้ใกล้ "เกมในตู้จริง" ขึ้น: เมนูเลือกความยาก
 * EASY/NORMAL/HARD (UP/DOWN เลือก A เริ่ม) จำคะแนนดีที่สุด (best) ข้ามรอบ
 * และใช้ฟิสิกส์แบบ delta-time (วัดเวลาจริงที่ผ่านไป เฟรมกระตุกความเร็วนกก็คงที่)
 * เล่นจบแล้วค้างจอ GAME OVER ครู่หนึ่ง แล้วกลับไปเมนูเลือกโหมดใหม่
 * โครงเว้น 30%: ฟิสิกส์ delta-time + ชน + นับคะแนน และลูปเมนูเลือกโหมดเว้นไว้ให้เติมเอง
 *
 * Flappy step 5, the practise skeleton: the mode table, the round (bird,
 * HUD, three pipes), the score panel and the console loop are given. The
 * frame (delta-time physics, pipes, hits, score) and the menu loop are left
 * as TODOs: until then the menu takes NORMAL at once and the bird hangs still
 * until Back ends the round, after which the menu comes back.
 *
 * Twin of flappy_step5.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/flappy_step5.py). Style: CLEAN_C.md.
 * Each pipe's dict and the bird_state dict become structs; random is
 * bento_random, MicroPython's generator draw for draw.
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
#include "bento_time.h"

/* The bird, the pipes, the screen and the timing. */
enum {
    BIRD_X = 150,
    BIRD_BOX_Y = 180,
    BIRD_START_Y = 198,
    BIRD_PX = 34,
    PIPE_WIDTH = 64,
    SPACING = 270,
    GAP_MARGIN = 60,
    PIPE_COUNT = 3,
    HUD_X = 300,
    HUD_Y = 16,
    HUD_CHARS = 48, /* two int32_t, the labels and the mode name */
    OVER_X = 290,
    OVER_Y = 180,
    MODE_X = 280,
    MODE_Y = 150,
    HINT_X = 250,
    HINT_Y = 200,
    LINE_CHARS = 40,
    MODE_COUNT = 3,
    NORMAL_MODE = 1,     /* ค่าเริ่มต้น = NORMAL */
    OVER_PAUSE_MS = 700, /* ค้างจอ GAME OVER ก่อนกลับเมนู */
    MS_PER_SECOND = 1000,
    FPS = 60
};

/* ตารางโหมด (ชื่อ, gap, speed px/s, grav px/s^2, flap px/s) มิเรอร์ s_flap_modes[] */
typedef struct {
    const char *name;
    int32_t gap;
    bento_real_t speed;
    bento_real_t gravity;
    bento_real_t flap;
} difficulty_t;

static const difficulty_t MODES[MODE_COUNT] = {
    {"EASY", 175, (bento_real_t)200.0, (bento_real_t)900.0,
     (bento_real_t)-360.0},
    {"NORMAL", 152, (bento_real_t)240.0, (bento_real_t)1100.0,
     (bento_real_t)-390.0},
    {"HARD", 120, (bento_real_t)300.0, (bento_real_t)1300.0,
     (bento_real_t)-410.0},
};

typedef struct {
    bento_real_t x;
    int32_t gap_top;
    bool scored;
    bento_game_box_t top;
    bento_game_box_t bottom;
} pipe_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    const difficulty_t *mode;
    bento_game_box_t bird;
    bento_game_text_t hud_label;
    pipe_t pipes[PIPE_COUNT];
    bento_real_t bird_y;
    bento_real_t bird_velocity;
    int32_t score;
    int32_t best_score;
    uint32_t last_clock_ms;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

/* Stop arrives at a sleep, as Python's KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t ms)
{
    bento_status_t status = bento_time_sleep_ms(ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

static bento_status_t random_gap_top(flappy_t *flappy, int32_t *gap_top)
{
    return bento_random_randint(
        &flappy->random, GAP_MARGIN,
        flappy->game.height - GAP_MARGIN - flappy->mode->gap, gap_top);
}

static bento_status_t add_pipe(flappy_t *flappy, pipe_t *pipe,
                               int32_t start_x)
{
    const int32_t gap = flappy->mode->gap;
    const int32_t height = flappy->game.height;
    bento_status_t status = random_gap_top(flappy, &pipe->gap_top);

    pipe->x = (bento_real_t)start_x;
    pipe->scored = false;
    bento_game_box_init(&pipe->top);
    bento_game_box_init(&pipe->bottom);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &flappy->game, &pipe->top, (bento_real_t)start_x,
            (bento_real_t)0, (bento_real_t)PIPE_WIDTH,
            (bento_real_t)pipe->gap_top, BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        const int32_t bottom_y = pipe->gap_top + gap;
        const int32_t bottom_h = height - pipe->gap_top - gap;

        status = bento_game_box_create(
            &flappy->game, &pipe->bottom, (bento_real_t)start_x,
            (bento_real_t)bottom_y, (bento_real_t)PIPE_WIDTH,
            (bento_real_t)bottom_h, BENTO_GAME_GB_DARK);
    }
    return status;
}

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    *out_stop = false; /* the TODO below is where it may end the loop */
    /* ----- เติมส่วนนี้เอง (งานของคุณ): ฟิสิกส์ delta-time + ชน + นับคะแนน -----
     * ลองทำทีละขั้นนะ ชื่อเครื่องมือบอกไว้ให้แล้ว ส่วนตัวเลข/สูตรลองปรับเอง:
     *   1) วัด dt (วินาที): ใช้ bento_time_ticks_ms() + bento_time_ticks_diff() เทียบกับ
     *      เวลาเฟรมก่อน (เก็บไว้ใน g_flappy.last_clock_ms) แล้วหารพันให้เป็นวินาที
     *      เผื่อ clamp ค่าสูงสุดกันเฟรมค้าง
     *   2) อ่านปุ่ม: bento_game_keys ถ้ากดกระโดด (a หรือ up) ตั้ง velocity = mode->flap
     *      แล้ว bento_game_sfx "flap"
     *   3) ฟิสิกส์: เพิ่ม velocity ด้วย gravity * dt, เพิ่ม y ด้วย velocity * dt
     *      กันทะลุเพดาน/พื้น ตกพื้น (y เกิน game.height - bird.h) ให้ bento_game_sfx
     *      "fall" แล้วตั้ง *out_stop = true; วาดนกด้วย bento_game_box_move_to
     *   4) วน pipes แต่ละท่อ: เลื่อน x ด้วย speed * dt, ท่อที่หลุดซ้ายให้รีไซเคิลไปขวาสุด
     *      (สุ่ม gap_top + bento_game_box_resize ใหม่), ขยับท่อด้วย bento_game_box_move_to;
     *      ถ้า bento_game_hit(นก, ท่อ) ให้ตาย; ถ้านกผ่านท่อ (ยังไม่ scored) ให้ score++,
     *      bento_game_sfx "point", อัปเดต best_score แล้ว bento_game_text_set ป้าย HUD */
    return BENTO_OK;
}

/* ล้างจอเมนูก่อนเริ่มรอบ แล้ววางนก HUD และท่อ 3 ท่อ */
static bento_status_t build_round(flappy_t *flappy)
{
    char text[HUD_CHARS];
    int32_t i;
    bento_status_t status = bento_game_clear(&flappy->game);

    bento_game_box_init(&flappy->bird);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &flappy->game, &flappy->bird, (bento_real_t)BIRD_X,
            (bento_real_t)BIRD_BOX_Y, (bento_real_t)BIRD_PX,
            (bento_real_t)BIRD_PX, BENTO_GAME_GB_LIGHTEST);
    }
    flappy->bird_y = (bento_real_t)BIRD_START_Y;
    flappy->bird_velocity = (bento_real_t)0;
    flappy->score = 0;
    (void)snprintf(text, sizeof(text), "0  Best: %" PRId32 "  [%s]",
                   flappy->best_score, flappy->mode->name);
    bento_game_text_init(&flappy->hud_label);
    if (status == BENTO_OK) {
        status = bento_game_text_create(&flappy->game,
                                        &flappy->hud_label, text, HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    for (i = 0; (status == BENTO_OK) && (i < PIPE_COUNT); i++) {
        status = add_pipe(flappy, &flappy->pipes[i],
                          flappy->game.width + (i * SPACING));
    }
    return status;
}

/* เล่นหนึ่งรอบด้วยโหมดที่เลือก แล้วโชว์คะแนนตอนจบ */
static bento_status_t play_round(flappy_t *flappy, int32_t mode_index)
{
    bool finished = false;
    bento_status_t status;

    flappy->mode = &MODES[mode_index];
    status = build_round(flappy);
    flappy->last_clock_ms = bento_time_ticks_ms();
    if (status == BENTO_OK) {
        status = bento_game_run(&flappy->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    if (flappy->score > flappy->best_score) {
        flappy->best_score = flappy->score;
    }
    if (status == BENTO_OK) {
        char text[LINE_CHARS];
        bento_game_text_t over;

        (void)snprintf(text, sizeof(text),
                       "GAME OVER  -  score %" PRId32, flappy->score);
        bento_game_text_init(&over);
        status = bento_game_text_create(&flappy->game, &over, text,
                                        OVER_X, OVER_Y, BENTO_GAME_RED);
    }
    return status;
}

static bento_status_t draw_choice(flappy_t *flappy,
                                  const bento_game_text_t *mode_label,
                                  int32_t selected)
{
    char text[LINE_CHARS];

    (void)snprintf(text, sizeof(text), "MODE:  < %s >",
                   MODES[selected].name);
    return bento_game_text_set(&flappy->game, mode_label, text);
}

/* ----- เติมส่วนนี้เอง: หน้าจอเลือกโหมด (เลียน s_flap_modes UP/DOWN+A) ----- */
static bento_status_t choose_mode(flappy_t *flappy, int32_t *out_mode)
{
    bento_game_text_t mode_label;
    bento_game_text_t hint_label;
    bento_status_t status = bento_game_clear(&flappy->game);

    *out_mode = NORMAL_MODE;
    bento_game_text_init(&mode_label);
    bento_game_text_init(&hint_label);
    if (status == BENTO_OK) {
        status = bento_game_text_create(&flappy->game, &mode_label, "",
                                        MODE_X, MODE_Y,
                                        BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            &flappy->game, &hint_label, "UP/DOWN เลือกโหมด, A = เริ่ม",
            HINT_X, HINT_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = draw_choice(flappy, &mode_label, *out_mode);
    }
    /* ----- เติมส่วนนี้เอง (งานของคุณ): หน้าจอเลือกโหมด (รับ UP/DOWN + A) -----
     * อ่านปุ่มด้วย bento_game_keys ในลูป แล้ว:
     *   1) ปุ่มขึ้น (up) เลื่อน *out_mode ไปโหมดก่อนหน้า (วนรอบด้วย % MODE_COUNT)
     *      แล้วเรียก draw_choice()
     *   2) ปุ่มลง (down) เลื่อนไปโหมดถัดไป (วนรอบเช่นกัน) แล้วเรียก draw_choice()
     *   3) ปุ่มยืนยัน (a) ออกจากลูปเริ่มเล่นโหมดที่เลือก
     * เคล็ด: หลังเลื่อนให้หน่วงสั้น ๆ (pause_ms) กันปุ่มเด้งรัว ลองหาเวลาที่พอดีเอง
     * ตอนนี้คืนค่าเริ่มต้นไปก่อน เพื่อให้รันได้ */
    return status;
}

/* วนเหมือนคอนโซลจริง: เลือกโหมด -> เล่น -> ตาย -> กลับไปเมนูเลือกโหมดใหม่ */
static bento_status_t play_forever(flappy_t *flappy)
{
    bento_status_t status = BENTO_OK;

    while (status == BENTO_OK) {
        int32_t chosen_mode = NORMAL_MODE;

        status = choose_mode(flappy, &chosen_mode);
        if (status == BENTO_OK) {
            status = play_round(flappy, chosen_mode);
        }
        if (status == BENTO_OK) {
            status = pause_ms(OVER_PAUSE_MS);
        }
    }
    return status;
}

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

int main(void)
{
    flappy_t *flappy = &g_flappy;
    bool quit = false;
    bento_status_t status;

    (void)memset(flappy, 0, sizeof(*flappy));
    status = bento_random_init(&flappy->random);
    if (status == BENTO_OK) {
        bento_game_init(&flappy->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
        status = bento_game_title(&flappy->game, "FLAPPY", NULL, &quit);
        /* title() calls start(), whose joystick.init() prints this line */
        if ((flappy->game.joystick_init == BENTO_OK) &&
            !on_reterminal()) {
            (void)printf(
                "joystick: USB Host HID initialized on CM55\n");
        }
    }
    flappy->best_score = 0;
    if ((status == BENTO_OK) && !quit) {
        status = play_forever(flappy);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
