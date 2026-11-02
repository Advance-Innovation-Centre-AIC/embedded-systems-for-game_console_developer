/*
 * flappy_step4.c -- สร้าง Flappy #4: ชนท่อ = จบ + ผ่านท่อ = ได้แต้ม
 *
 * ต่อจาก step3 (นก ฟิสิกส์ ท่อ 3 ท่อที่สร้างครั้งเดียวแล้วหมุนเวียนใช้ซ้ำ) คาบนี้เพิ่ม
 * ชนท่อหรือตกพื้น = เสียง "fall" + GAME OVER และนกผ่านท่อ = +1 แต้มพร้อมเสียง "point"
 * แต่ละท่อมีธง scored ไว้กันนับซ้ำ
 *
 * Flappy step 4, the solution: the bird flaps under gravity, three pipes made
 * once scroll left and are recycled to the right with a new random gap. A pipe
 * or the floor plays "fall", shows "GAME OVER" and ends the game loop; each
 * pipe passed scores one point with the "point" sound.
 *
 * Twin of flappy_step4.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/flappy_step4.py). Style: CLEAN_C.md.
 * Each pipe's dict becomes a pipe_t; random is bento_random, MicroPython's
 * generator draw for draw.
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

/* The bird, the pipes and the screen. */
enum {
    BIRD_X = 150,
    BIRD_START_Y = 180,
    BIRD_PX = 34,
    GAP = 152,       /* ช่องว่างที่นกลอด */
    PIPE_WIDTH = 64, /* ความกว้างของท่อ */
    SPACING = 270,   /* ระยะห่างระหว่างท่อ */
    GAP_MARGIN = 60, /* ช่องว่างไม่ชิดขอบบนหรือล่างเกินนี้ */
    PIPE_COUNT = 3,
    SCORE_X = 384,
    SCORE_Y = 16,
    SCORE_CHARS = 12, /* any int32_t and its NUL */
    OVER_X = 320,
    OVER_Y = 180,
    FPS = 50
};

typedef struct {
    bento_real_t x;
    int32_t gap_top;
    bool scored; /* ธง scored ไว้กันนับซ้ำ */
    bento_game_box_t top;
    bento_game_box_t bottom;
} pipe_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t bird;
    bento_game_text_t score_label;
    pipe_t pipes[PIPE_COUNT];
    bento_real_t bird_y;
    bento_real_t bird_velocity;
    int32_t score;
} flappy_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern flappy_t g_flappy;
flappy_t g_flappy;

static bento_status_t random_gap_top(flappy_t *flappy, int32_t *gap_top)
{
    return bento_random_randint(&flappy->random, GAP_MARGIN,
                                flappy->game.height - GAP_MARGIN - GAP,
                                gap_top);
}

static bento_status_t add_pipe(flappy_t *flappy, pipe_t *pipe,
                               int32_t start_x)
{
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
        status = bento_game_box_create(
            &flappy->game, &pipe->bottom, (bento_real_t)start_x,
            (bento_real_t)(pipe->gap_top + GAP),
            (bento_real_t)PIPE_WIDTH,
            (bento_real_t)(height - pipe->gap_top - GAP),
            BENTO_GAME_GB_DARK);
    }
    return status;
}

static void move_bird(flappy_t *flappy, const bento_game_keys_t *keys)
{
    const bento_real_t gravity = (bento_real_t)0.45; /* ดึงลงต่อเฟรม */
    const bento_real_t flap = (bento_real_t)-7.4;    /* ดีดขึ้น */

    if (keys->a || keys->up) {
        flappy->bird_velocity = flap;
        bento_game_sfx(&flappy->game, "flap");
    }
    flappy->bird_velocity += gravity;
    flappy->bird_y += flappy->bird_velocity;
    if (flappy->bird_y < (bento_real_t)0) {
        flappy->bird_y = (bento_real_t)0;
        flappy->bird_velocity = (bento_real_t)0;
    }
}

static bento_status_t show_game_over(flappy_t *flappy)
{
    bento_game_text_t over;

    bento_game_sfx(&flappy->game, "fall");
    bento_game_text_init(&over);
    return bento_game_text_create(&flappy->game, &over, "GAME OVER",
                                  OVER_X, OVER_Y, BENTO_GAME_RED);
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

/* เลยขอบซ้าย ก็ย้ายไปขวาสุด แล้วสุ่มช่องว่างใหม่ */
static bento_status_t recycle_pipe(flappy_t *flappy, pipe_t *pipe)
{
    const int32_t height = flappy->game.height;
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
        status = bento_game_box_resize(
            &flappy->game, &pipe->bottom, (bento_real_t)PIPE_WIDTH,
            (bento_real_t)(height - pipe->gap_top - GAP));
    }
    return status;
}

static bento_status_t move_pipe(flappy_t *flappy, pipe_t *pipe)
{
    const bento_real_t speed = (bento_real_t)3.4; /* ท่อเลื่อนซ้าย */
    bento_status_t status = BENTO_OK;

    pipe->x -= speed;
    if (pipe->x < (bento_real_t)-PIPE_WIDTH) {
        status = recycle_pipe(flappy, pipe);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(&flappy->game, &pipe->top,
                                        pipe->x, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &flappy->game, &pipe->bottom, pipe->x,
            (bento_real_t)(pipe->gap_top + GAP));
    }
    return status;
}

/* ชนท่อ -> จบ; ผ่านท่อ -> +1 */
static bento_status_t check_pipe(flappy_t *flappy, pipe_t *pipe,
                                 bool *over)
{
    bento_status_t status = BENTO_OK;

    if (bento_game_hit(&flappy->bird, &pipe->top) ||
        bento_game_hit(&flappy->bird, &pipe->bottom)) {
        status = show_game_over(flappy);
        *over = true;
    } else if (!pipe->scored && ((pipe->x + (bento_real_t)PIPE_WIDTH) <
                                 flappy->bird.x)) {
        char text[SCORE_CHARS];

        pipe->scored = true;
        flappy->score++;
        bento_game_sfx(&flappy->game, "point");
        (void)snprintf(text, sizeof(text), "%" PRId32, flappy->score);
        status = bento_game_text_set(&flappy->game,
                                     &flappy->score_label, text);
    } else {
        /* the pipe is still ahead, or already counted */
    }
    return status;
}

static bento_status_t move_pipes(flappy_t *flappy, bool *over)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = 0; (status == BENTO_OK) && !*over && (i < PIPE_COUNT);
         i++) {
        status = move_pipe(flappy, &flappy->pipes[i]);
        if (status == BENTO_OK) {
            status = check_pipe(flappy, &flappy->pipes[i], over);
        }
    }
    return status;
}

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    flappy_t *flappy = &g_flappy;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_flappy at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&flappy->game, &keys);
    if (status == BENTO_OK) {
        move_bird(flappy, &keys);
    }
    if ((status == BENTO_OK) &&
        (flappy->bird_y >
         ((bento_real_t)flappy->game.height - flappy->bird.h))) {
        status = show_game_over(flappy);
        *out_stop = true;
    } else if (status == BENTO_OK) {
        status = bento_game_box_move_to(&flappy->game, &flappy->bird,
                                        flappy->bird.x, flappy->bird_y);
        if (status == BENTO_OK) {
            status = move_pipes(flappy, out_stop);
        }
    } else {
        /* the status ends the game loop */
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

/* title() calls start(), whose joystick.init() prints this line on the
 * kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* The bird, the score and the three pipes, starting off screen right. */
static bento_status_t build_screen(flappy_t *flappy)
{
    int32_t i;
    bento_status_t status;

    bento_game_box_init(&flappy->bird);
    status = bento_game_box_create(
        &flappy->game, &flappy->bird, (bento_real_t)BIRD_X,
        (bento_real_t)BIRD_START_Y, (bento_real_t)BIRD_PX,
        (bento_real_t)BIRD_PX, BENTO_GAME_GB_LIGHTEST);
    flappy->bird_y = (bento_real_t)BIRD_START_Y;
    flappy->bird_velocity = (bento_real_t)0;
    flappy->score = 0;
    if (status == BENTO_OK) {
        bento_game_text_init(&flappy->score_label);
        status = bento_game_text_create(
            &flappy->game, &flappy->score_label, "0", SCORE_X, SCORE_Y,
            BENTO_GAME_WHITE);
    }
    for (i = 0; (status == BENTO_OK) && (i < PIPE_COUNT); i++) {
        status = add_pipe(flappy, &flappy->pipes[i],
                          flappy->game.width + (i * SPACING));
    }
    return status;
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
        report_joystick(&flappy->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(flappy);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&flappy->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
