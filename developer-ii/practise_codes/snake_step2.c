/*
 * snake_step2.c -- สร้าง Snake #2 (คาบ 5): งูเดินได้ + บังคับด้วยจอย
 *
 * ใหม่จาก step1: game loop (update ทุกเฟรม) + เดินทีละช่อง + เลี้ยวด้วยจอย
 * ทุกจังหวะ (frame) งูจะคืบไปข้างหน้า 1 ช่อง เคล็ดลับคือไม่ต้องขยับทั้งตัว
 * แค่เติมหัวใหม่ข้างหน้าแล้วตัดหางทิ้ง 1 ช่อง โครงนี้เว้นส่วนเดินไว้ให้เติมเอง
 *
 * Snake step 2, the practise skeleton: the snake steers with the joystick (no
 * U-turn into its own neck) and every frame redraws each segment at its cell.
 * The step forward is left as TODOs, so until the student writes it the snake
 * stays where it is while its direction changes.
 *
 * Twin of snake_step2.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/snake_step2.py). Style: CLEAN_C.md.
 * The snake list becomes a fixed array of three cells.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_runtime.h"

#include "bento_game.h"
#include "bento_gpio.h"

/* The grid and the game. */
enum {
    CELL_PX = 26,
    GRID_COLS = BENTO_GAME_WIDTH / CELL_PX,
    GRID_ROWS = BENTO_GAME_HEIGHT / CELL_PX,
    SNAKE_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    SCORE_X = 10,
    SCORE_Y = 8,
    FPS = 9 /* เรียก update 9 ครั้ง/วินาที = จังหวะ Snake */
};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    cell_t body[SNAKE_LEN]; /* หัวอยู่ตัวแรก */
    bento_game_box_t squares[SNAKE_LEN];
    bento_game_text_t score_text;
    int32_t step_col; /* 1,0 = ขวา -1,0 = ซ้าย 0,-1 = ขึ้น 0,1 = ลง */
    int32_t step_row;
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

/* เลี้ยวตามจอย แต่ห้ามถอยหลังกลับทับตัวเอง (เลี้ยวได้เมื่อแกนนั้นกำลังนิ่ง = 0) */
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

/* วาดทุกปล้องให้ไปอยู่ช่องที่ถูกต้อง (หัวสีอ่อน ลำตัวสีเข้ม) */
static bento_status_t redraw_snake(snake_t *snake)
{
    int32_t index;
    bento_status_t status = BENTO_OK;

    for (index = 0; (status == BENTO_OK) && (index < SNAKE_LEN); index++) {
        const uint32_t color = (index == 0) ? BENTO_GAME_GB_LIGHT : BENTO_GAME_GB_DARK;

        status = bento_game_box_set_color(&snake->game, &snake->squares[index], color);
        if (status == BENTO_OK) {
            status = bento_game_box_move_to(&snake->game, &snake->squares[index],
                                            (bento_real_t)(snake->body[index].col * CELL_PX),
                                            (bento_real_t)(snake->body[index].row * CELL_PX));
        }
    }
    return status;
}

/* ฟังก์ชันนี้ถูกเรียกซ้ำทุกจังหวะ (= หัวใจของเกม) */
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
        /* Back = ออกจากเกม / Start = หยุดชั่วคราว game loop จัดการให้ทั้งคู่อยู่แล้ว */

        /* ----- เติมส่วนนี้เอง (งานของคุณ): ทำให้งูเดินไปข้างหน้า 1 ช่อง -----
         * TODO 1) หาช่องใหม่ของหัว: เอาหัวเดิม snake->body[0] (col, row) มาบวกกับทิศ
         *         step_col, step_row เก็บผลลัพธ์ไว้ในตัวแปร cell_t เช่น next_head
         * TODO 2) เติมหัวใหม่ไว้หน้าสุด: เลื่อนทุกช่องใน snake->body ถอยหลังไป 1 ตำแหน่ง
         *         (จากท้ายมาหน้า) แล้วใส่ next_head ที่ body[0]
         * TODO 3) ช่องสุดท้ายที่หลุดออกไปคือหางที่ถูกตัดทิ้ง รวมกันแล้วงูจะดูเหมือนเลื้อยไป 1 ช่อง
         * =================================================================== */

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
    if ((bento_gpio_board_info(&info) == BENTO_OK) && (info.name != NULL)) {
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

    for (i = 0; (status == BENTO_OK) && (i < SNAKE_LEN); i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
        bento_game_box_init(&snake->squares[i]);
        status = bento_game_box_create(
            &snake->game, &snake->squares[i], (bento_real_t)(snake->body[i].col * CELL_PX),
            (bento_real_t)(snake->body[i].row * CELL_PX), (bento_real_t)(CELL_PX - 2),
            (bento_real_t)(CELL_PX - 2), BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_set_color(&snake->game, &snake->squares[0], BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(&snake->game, &snake->score_text, "Score: 0", SCORE_X,
                                        SCORE_Y, BENTO_GAME_WHITE);
    }
    snake->step_col = 1;
    snake->step_row = 0;
    return status;
}

int main(void)
{
    snake_t *snake = &g_snake;
    bool quit = false;
    bento_status_t status;

    (void)memset(snake, 0, sizeof(*snake));
    bento_game_init(&snake->game, bento_runtime_get_ui(), bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&snake->game, "SNAKE", NULL, &quit);
    report_joystick(&snake->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_snake(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&snake->game, on_each_frame, snake, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
