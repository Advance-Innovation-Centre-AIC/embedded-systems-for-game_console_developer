/*
 * snake_step4.c -- สร้าง Snake #4 (คาบ 6): "แพ้ได้แล้ว" ชนกำแพง/ชนตัวเอง = GAME OVER
 *
 * เกมที่แพ้ไม่ได้ก็ยังไม่ค่อยเป็นเกม คาบนี้เติม "เงื่อนไขแพ้": หัวงูหลุดขอบกริด
 * หรือหัวไปทับช่องที่ตัวงูอยู่แล้ว -> เสียง "die" + ป้าย "GAME OVER" + ออกจาก game loop
 * ตรวจก่อนขยับงูจริง ที่เหลือ (อาหาร กินแล้วโต คะแนน) เหมือน step3
 *
 * Snake step 4, the solution: the snake from step 3 can now lose. A next head
 * outside the grid or on the snake itself plays "die", shows "GAME OVER" in red
 * and ends the game loop; otherwise the snake moves, eats and grows as before.
 *
 * Twin of snake_step4.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/snake_step4.py). Style: CLEAN_C.md.
 * Python's list becomes an array with a length, at most MAX_LEN cells: the ui's
 * widget table (64) runs out of boxes before a longer snake, in both languages.
 * random is bento_random, MicroPython's generator draw for draw.
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

/* The grid and the game. */
enum {
    CELL_PX = 26,
    GRID_COLS = BENTO_GAME_WIDTH / CELL_PX,
    GRID_ROWS = BENTO_GAME_HEIGHT / CELL_PX,
    START_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    /* the ui's widget table: no box is left for a longer snake */
    MAX_LEN = 64,
    SCORE_X = 10,
    SCORE_Y = 8,
    SCORE_CHARS = 20, /* "Score: " and any int32_t */
    OVER_X = 320,
    OVER_Y = 180,
    FPS = 9
};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    /* one more while the new head is in and the tail not yet out */
    cell_t body[MAX_LEN + 1];
    int32_t length;
    bento_game_box_t squares[MAX_LEN];
    int32_t square_count;
    bento_game_box_t food; /* อาหาร = ช่องสว่างสุด */
    bento_game_text_t score_text;
    int32_t step_col;
    int32_t step_row;
    int32_t score;
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

static bool snake_covers(const snake_t *snake, cell_t cell)
{
    bool covered = false;
    int32_t i;

    for (i = 0; i < snake->length; i++) {
        if ((snake->body[i].col == cell.col) &&
            (snake->body[i].row == cell.row)) {
            covered = true;
        }
    }
    return covered;
}

/* หาช่องว่าง (ไม่ทับตัวงู) แล้ววางอาหารตรงนั้น */
static bento_status_t place_food_at_random_empty_cell(snake_t *snake)
{
    cell_t cell = {0, 0};
    bool free_cell = false;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && !free_cell) {
        status = bento_random_randint(&snake->random, 0, GRID_COLS - 1,
                                      &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&snake->random, 0,
                                          GRID_ROWS - 1, &cell.row);
        }
        /* ห้ามทับตัวงู */
        free_cell = (status == BENTO_OK) && !snake_covers(snake, cell);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_box_move_to(&snake->game, &snake->food,
                                   (bento_real_t)(cell.col * CELL_PX),
                                   (bento_real_t)(cell.row * CELL_PX));
    }
    return status;
}

static void steer(snake_t *snake, const bento_game_keys_t *keys)
{
    if (keys->left && (snake->step_col == 0)) {
        snake->step_col = -1;
        snake->step_row = 0;
    }
    if (keys->right && (snake->step_col == 0)) {
        snake->step_col = 1;
        snake->step_row = 0;
    }
    if (keys->up && (snake->step_row == 0)) {
        snake->step_col = 0;
        snake->step_row = -1;
    }
    if (keys->down && (snake->step_row == 0)) {
        snake->step_col = 0;
        snake->step_row = 1;
    }
}

static void insert_head(snake_t *snake, cell_t next_head)
{
    int32_t i;

    for (i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->length++;
}

static bento_status_t redraw_snake(snake_t *snake)
{
    int32_t index;
    bento_status_t status = BENTO_OK;

    for (index = 0; (status == BENTO_OK) && (index < snake->length);
         index++) {
        const uint32_t color =
            (index == 0) ? BENTO_GAME_GB_LIGHT : BENTO_GAME_GB_DARK;

        status = bento_game_box_set_color(
            &snake->game, &snake->squares[index], color);
        if (status == BENTO_OK) {
            status = bento_game_box_move_to(
                &snake->game, &snake->squares[index],
                (bento_real_t)(snake->body[index].col * CELL_PX),
                (bento_real_t)(snake->body[index].row * CELL_PX));
        }
    }
    return status;
}

static bento_status_t add_square(snake_t *snake, bento_game_box_t *box,
                                 cell_t cell, uint32_t color)
{
    bento_game_box_init(box);
    return bento_game_box_create(
        &snake->game, box, (bento_real_t)(cell.col * CELL_PX),
        (bento_real_t)(cell.row * CELL_PX), (bento_real_t)(CELL_PX - 2),
        (bento_real_t)(CELL_PX - 2), color);
}

/* กินอาหาร -> โต + คะแนน */
static bento_status_t eat_food(snake_t *snake)
{
    char text[SCORE_CHARS];
    bento_status_t status = BENTO_OK;

    snake->score++;
    bento_game_sfx(&snake->game, "eat"); /* เสียงงับ มาจาก C engine จริง */
    (void)snprintf(text, sizeof(text), "Score: %" PRId32, snake->score);
    status =
        bento_game_text_set(&snake->game, &snake->score_text, text);
    if ((status == BENTO_OK) && (snake->square_count >= MAX_LEN)) {
        /* where Python's next Box finds no widget left */
        status = BENTO_E_NO_RESOURCE;
    }
    if (status == BENTO_OK) {
        /* โต 1 ปล้อง */
        status = add_square(snake, &snake->squares[snake->square_count],
                            (cell_t){0, 0}, BENTO_GAME_GB_DARK);
        snake->square_count++;
    }
    if (status == BENTO_OK) {
        status = place_food_at_random_empty_cell(snake);
    }
    return status;
}

static bento_status_t eat_or_walk(snake_t *snake)
{
    const cell_t food = {(int32_t)snake->food.x / CELL_PX,
                         (int32_t)snake->food.y / CELL_PX};
    const bool ate_food = (snake->body[0].col == food.col) &&
                          (snake->body[0].row == food.row);
    bento_status_t status = BENTO_OK;

    if (ate_food) {
        status = eat_food(snake);
    } else {
        snake->length--; /* ไม่กิน -> ตัดหาง = เดินปกติ */
    }
    return status;
}

static bool outside_grid(cell_t cell)
{
    return (cell.col < 0) || (cell.col >= GRID_COLS) ||
           (cell.row < 0) || (cell.row >= GRID_ROWS);
}

/* เงื่อนไข "แพ้" (ตรวจก่อนขยับงูจริง): ชนกำแพง หรือหัวซ้อนทับตัวเอง */
static bool hits_something(const snake_t *snake, cell_t next_head)
{
    return outside_grid(next_head) || snake_covers(snake, next_head);
}

static bento_status_t show_game_over(snake_t *snake)
{
    bento_game_text_t over;

    bento_game_sfx(&snake->game, "die");
    bento_game_text_init(&over);
    return bento_game_text_create(&snake->game, &over, "GAME OVER",
                                  OVER_X, OVER_Y, BENTO_GAME_RED);
}

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    snake_t *snake = &g_snake;
    bento_game_keys_t keys;
    cell_t next_head = {0, 0};
    bool lost = false;
    bento_status_t status;

    /* the state is g_snake at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if (status == BENTO_OK) {
        steer(snake, &keys);
        /* (Back=ออก / Start=หยุดชั่วคราว game loop จัดการให้) */
        next_head = (cell_t){snake->body[0].col + snake->step_col,
                             snake->body[0].row + snake->step_row};
        /* ----- เติมส่วนนี้เอง: เงื่อนไข "แพ้" (ตรวจก่อนขยับงูจริง) ----- */
        lost = hits_something(snake, next_head);
    }
    if ((status == BENTO_OK) && lost) {
        status = show_game_over(snake);
        *out_stop = true; /* ออกจาก game loop */
    } else if (status == BENTO_OK) {
        insert_head(snake, next_head);
        status = eat_or_walk(snake);
        if (status == BENTO_OK) {
            status = redraw_snake(snake);
        }
    } else {
        /* the status ends the game loop */
    }
    return status;
}

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
    if ((bento_gpio_board_info(&info) == BENTO_OK) &&
        (info.name != NULL)) {
        answer = (strcmp(info.name, "Seeed reTerminal") == 0);
    }
    return answer;
}

/* title() calls start(), whose joystick.init() prints this line on the kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

static bento_status_t add_snake(snake_t *snake)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    snake->length = START_LEN;
    snake->square_count = START_LEN;
    for (i = 0; (status == BENTO_OK) && (i < START_LEN); i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
        status = add_square(snake, &snake->squares[i], snake->body[i],
                            BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(
            &snake->game, &snake->squares[0], BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = add_square(snake, &snake->food, (cell_t){0, 0},
                            BENTO_GAME_GB_LIGHTEST);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(
            &snake->game, &snake->score_text, "Score: 0", SCORE_X,
            SCORE_Y, BENTO_GAME_WHITE);
    }
    snake->step_col = 1;
    snake->step_row = 0;
    snake->score = 0;
    return status;
}

int main(void)
{
    snake_t *snake = &g_snake;
    bool quit = false;
    bento_status_t status;

    (void)memset(snake, 0, sizeof(*snake));
    status = bento_random_init(&snake->random);
    if (status == BENTO_OK) {
        bento_game_init(&snake->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
        status = bento_game_title(&snake->game, "SNAKE", NULL, &quit);
        report_joystick(&snake->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_snake(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        status = place_food_at_random_empty_cell(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&snake->game, on_each_frame, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
