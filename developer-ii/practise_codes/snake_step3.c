/*
 * snake_step3.c -- สร้าง Snake #3 (คาบ 5): อาหาร + กินแล้วโต + คะแนน + เสียง
 *
 * โครงเว้น 30% ใหม่จาก step2: สุ่มอาหาร หัวชนอาหาร -> ต่อปล้อง + คะแนน + "Nice bite!"
 * + เสียง "eat" เคล็ดลับ "กินแล้วโต": ปกติเติมหัว + ตัดหาง (ยาวเท่าเดิม) แต่ถ้ากินอาหาร
 * จะเติมหัวแต่ไม่ตัดหาง งูจึงยาวขึ้น 1 ช่องทันที ส่วนเช็คการกินเว้นไว้ให้เติมเอง
 *
 * Snake step 3, the practise skeleton: the food sits on a random free cell and
 * the snake moves one cell per frame, steered by the joystick. The eat test is
 * left as TODOs: until the student writes it the placeholder always drops the
 * tail, so the snake stays three cells long and walks off the screen.
 *
 * Twin of snake_step3.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/snake_step3.py). Style: CLEAN_C.md.
 * Python's list becomes an array with a length, at most MAX_LEN cells: the ui's
 * widget table (64) runs out of boxes before a longer snake, in both languages.
 * random is bento_random, MicroPython's generator draw for draw.
 */
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
    MAX_LEN =
        64, /* the ui's widget table: no box is left for a longer snake */
    SCORE_X = 10,
    SCORE_Y = 8,
    FPS = 9
};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    cell_t body
        [MAX_LEN +
         1]; /* one more while the new head is in and the tail not yet out */
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
        free_cell = (status == BENTO_OK) &&
                    !snake_covers(snake, cell); /* ห้ามทับตัวงู */
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

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    /* the state main() handed to bento_game_run */
    snake_t *snake = user;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)out_stop;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if (status == BENTO_OK) {
        steer(snake, &keys);
        /* (Back=ออก / Start=หยุดชั่วคราว game loop จัดการให้) */
        insert_head(snake,
                    (cell_t){snake->body[0].col + snake->step_col,
                             snake->body[0].row + snake->step_row});

        /* ----- เติมส่วนนี้เอง (งานของคุณ): หัวงูกินอาหารไหม + ถ้ากินก็โต/ได้แต้ม/มีเสียง -----
         * TODO: เขียน if/else เช็คว่าหัวงูไปอยู่ช่องเดียวกับอาหารหรือไม่
         *   1) หา "ช่อง" ของอาหารจากพิกเซลของมัน: เอา snake->food.x, snake->food.y มาหารด้วย
         *      CELL_PX แล้วเทียบกับ snake->body[0] ว่าเป็นช่องเดียวกันไหม -> ได้ ate_food
         *   2) ถ้ากิน (ate_food):
         *        - บวกแต้ม snake->score++
         *        - เล่นเสียงงับด้วย bento_game_sfx (ชื่อเสียงคือ "eat")
         *        - อัปเดตป้ายคะแนนด้วย bento_game_text_set ให้ขึ้น "Score: <แต้ม>"
         *        - ต่อปล้องใหม่ด้วย bento_game_box_create ที่ squares[square_count]
         *          แล้วเพิ่ม square_count (ทำให้งูยาวขึ้น 1)
         *        - โชว์คำว่า "Nice bite!" ด้วย bento_game_text_create (ราว ๆ กลางจอ)
         *        - เรียก place_food_at_random_empty_cell() เพื่อวางอาหารชิ้นใหม่
         *   3) ถ้าไม่กิน (else): snake->length-- คือตัดหาง = เดินปกติ ยาวเท่าเดิม
         *      สำคัญ! เคล็ดลับ "กินแล้วโต" = เติมหัวเสมอ แต่ตัดหางเฉพาะตอนไม่กิน
         * placeholder ให้รันได้ก่อน: ย้ายบรรทัดตัดหางด้านล่างไปไว้ในกิ่ง else เมื่อเขียนเสร็จ */
        snake->length--;
        /* =============================================================================== */

        status = redraw_snake(snake);
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

static bento_status_t add_square(snake_t *snake, bento_game_box_t *box,
                                 cell_t cell, uint32_t color)
{
    bento_game_box_init(box);
    return bento_game_box_create(
        &snake->game, box, (bento_real_t)(cell.col * CELL_PX),
        (bento_real_t)(cell.row * CELL_PX), (bento_real_t)(CELL_PX - 2),
        (bento_real_t)(CELL_PX - 2), color);
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

        status = bento_game_run(&snake->game, on_each_frame, snake, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
