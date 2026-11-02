/*
 * flappy_step5.c -- Flappy #5: เข้าใกล้เกมจริงบนคอนโซล (โหมดความยาก + best)
 *
 * step4 เล่นจบได้แล้ว คาบนี้ทำให้ใกล้ "เกมในตู้จริง" ขึ้น: เมนูเลือกความยาก
 * EASY/NORMAL/HARD (UP/DOWN เลือก A เริ่ม) จำคะแนนดีที่สุด (best) ข้ามรอบ
 * และใช้ฟิสิกส์แบบ delta-time (วัดเวลาจริงที่ผ่านไป เฟรมกระตุกความเร็วนกก็คงที่)
 * เล่นจบแล้วค้างจอ GAME OVER ครู่หนึ่ง แล้วกลับไปเมนูเลือกโหมดใหม่
 *
 * Flappy step 5, the solution: a mode menu (gap, pipe speed, gravity and flap
 * per mode), delta-time physics in pixels per second with dt clamped to 50 ms,
 * the best score kept across rounds, and a loop like the console's: choose a
 * mode, play, die, see the score, back to the menu.
 *
 * Twin of flappy_step5.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/flappy_step5.py). Style: CLEAN_C.md.
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
    NORMAL_MODE = 1, /* ค่าเริ่มต้น = NORMAL */
    MOVE_PAUSE_MS = 180,
    POLL_MS = 30,
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

static bento_status_t show_hud(flappy_t *flappy)
{
    char text[HUD_CHARS];

    (void)snprintf(
        text, sizeof(text), "%" PRId32 "  Best: %" PRId32 "  [%s]",
        flappy->score, flappy->best_score, flappy->mode->name);
    return bento_game_text_set(&flappy->game, &flappy->hud_label, text);
}

/* เวลาจริงที่ผ่านไป (วินาที) กันเฟรมค้างยาวทะลุท่อ */
static bento_real_t frame_seconds(flappy_t *flappy)
{
    const bento_real_t longest = (bento_real_t)0.05;
    const uint32_t now = bento_time_ticks_ms();
    bento_real_t dt = (bento_real_t)bento_time_ticks_diff(
                          now, flappy->last_clock_ms) /
                      (bento_real_t)MS_PER_SECOND;

    flappy->last_clock_ms = now;
    if (dt > longest) {
        dt = longest;
    }
    return dt;
}

/* ฟิสิกส์แบบ delta-time: โน้มถ่วงต่อวินาที */
static void move_bird(flappy_t *flappy, const bento_game_keys_t *keys,
                      bento_real_t dt)
{
    bento_real_t change;

    if (keys->a || keys->up) {
        flappy->bird_velocity = flappy->mode->flap;
        bento_game_sfx(&flappy->game, "flap");
    }
    change = flappy->mode->gravity * dt;
    flappy->bird_velocity += change;
    change = flappy->bird_velocity * dt;
    flappy->bird_y += change;
    if (flappy->bird_y < (bento_real_t)0) {
        flappy->bird_y = (bento_real_t)0;
        flappy->bird_velocity = (bento_real_t)0;
    }
}

static bento_real_t rightmost_x(const flappy_t *flappy)
{
    bento_real_t right = flappy->pipes[0].x;
    int32_t i;

    for (i = 1; i < PIPE_COUNT; i++) {
        if (flappy->pipes[i].x > right) {
            right = flappy->pipes[i].x;
        }
    }
    return right;
}

static bento_status_t recycle_pipe(flappy_t *flappy, pipe_t *pipe)
{
    const int32_t gap = flappy->mode->gap;
    bento_status_t status;

    pipe->x = rightmost_x(flappy) + (bento_real_t)SPACING;
    status = random_gap_top(flappy, &pipe->gap_top);
    pipe->scored = false;
    if (status == BENTO_OK) {
        status = bento_game_box_resize(&flappy->game, &pipe->top,
                                       (bento_real_t)PIPE_WIDTH,
                                       (bento_real_t)pipe->gap_top);
    }
    if (status == BENTO_OK) {
        const int32_t bottom_h =
            flappy->game.height - pipe->gap_top - gap;

        status = bento_game_box_resize(&flappy->game, &pipe->bottom,
                                       (bento_real_t)PIPE_WIDTH,
                                       (bento_real_t)bottom_h);
    }
    return status;
}

static bento_status_t move_pipe(flappy_t *flappy, pipe_t *pipe,
                                bento_real_t dt)
{
    const bento_real_t shift = flappy->mode->speed * dt;
    bento_status_t status = BENTO_OK;

    pipe->x -= shift;
    if (pipe->x < (bento_real_t)-PIPE_WIDTH) {
        status = recycle_pipe(flappy, pipe);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&flappy->game, &pipe->top,
                                        pipe->x, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        const int32_t bottom_y = pipe->gap_top + flappy->mode->gap;

        status =
            bento_game_box_move_to(&flappy->game, &pipe->bottom,
                                   pipe->x, (bento_real_t)bottom_y);
    }
    return status;
}

static bento_status_t check_pipe(flappy_t *flappy, pipe_t *pipe,
                                 bool *over)
{
    bento_status_t status = BENTO_OK;

    if (bento_game_hit(&flappy->bird, &pipe->top) ||
        bento_game_hit(&flappy->bird, &pipe->bottom)) {
        bento_game_sfx(&flappy->game, "fall");
        *over = true;
    } else if (!pipe->scored && ((pipe->x + (bento_real_t)PIPE_WIDTH) <
                                 flappy->bird.x)) {
        pipe->scored = true;
        flappy->score++;
        bento_game_sfx(&flappy->game, "point");
        if (flappy->score > flappy->best_score) {
            flappy->best_score = flappy->score;
        }
        status = show_hud(flappy);
    } else {
        /* the pipe is still ahead, or already counted */
    }
    return status;
}

static bento_status_t move_pipes(flappy_t *flappy, bento_real_t dt,
                                 bool *over)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && !*over && (i < PIPE_COUNT);
         i++) {
        status = move_pipe(flappy, &flappy->pipes[i], dt);
        if (status == BENTO_OK) {
            status = check_pipe(flappy, &flappy->pipes[i], over);
        }
    }
    return status;
}

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    /* ----- เติมส่วนนี้เอง: ฟิสิกส์แบบ delta-time ----- */
    const bento_real_t dt = frame_seconds(flappy);
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&flappy->game, &keys);
    if (status == BENTO_OK) {
        move_bird(flappy, &keys, dt);
    }
    if ((status == BENTO_OK) &&
        (flappy->bird_y >
         ((bento_real_t)flappy->game.height - flappy->bird.h))) {
        bento_game_sfx(&flappy->game, "fall");
        *out_stop = true;
    } else if (status == BENTO_OK) {
        status = bento_game_box_move_to(&flappy->game, &flappy->bird,
                                        flappy->bird.x, flappy->bird_y);
        if (status == BENTO_OK) {
            status = move_pipes(flappy, dt, out_stop);
        }
    } else {
        /* the status ends the game loop */
    }
    return status;
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

/* One menu poll: UP/DOWN move the choice round the list, A picks it. */
static bento_status_t poll_choice(flappy_t *flappy,
                                  const bento_game_text_t *mode_label,
                                  int32_t *selected, bool *chosen)
{
    bento_game_keys_t keys;
    bento_status_t status;

    bento_game_keys_init(&keys);
    status = bento_game_keys(&flappy->game, &keys);
    if ((status == BENTO_OK) && keys.up) {
        *selected = (*selected + MODE_COUNT - 1) % MODE_COUNT;
        status = draw_choice(flappy, mode_label, *selected);
        if (status == BENTO_OK) {
            status = pause_ms(MOVE_PAUSE_MS);
        }
    }
    if ((status == BENTO_OK) && keys.down) {
        *selected = (*selected + 1) % MODE_COUNT;
        status = draw_choice(flappy, mode_label, *selected);
        if (status == BENTO_OK) {
            status = pause_ms(MOVE_PAUSE_MS);
        }
    }
    *chosen = keys.a;
    return status;
}

/* ----- เติมส่วนนี้เอง: หน้าจอเลือกโหมด (เลียน s_flap_modes UP/DOWN+A) ----- */
static bento_status_t choose_mode(flappy_t *flappy, int32_t *out_mode)
{
    bento_game_text_t mode_label;
    bento_game_text_t hint_label;
    bool chosen = false;
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
    while ((status == BENTO_OK) && !chosen) {
        status = poll_choice(flappy, &mode_label, out_mode, &chosen);
        if ((status == BENTO_OK) && !chosen) {
            status = pause_ms(POLL_MS);
        }
    }
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
