/*
 * snake_step1.c -- สร้าง Snake #1 (คาบ 4): วาดสนาม + ตัวงู + คะแนน (ยังไม่ขยับ)
 *
 * โครงเว้น 30% ขั้นนี้เน้นทดลองแก้ค่าที่มีอยู่ให้เป็นของคุณ ยังไม่ต้องเขียน logic ใหม่
 * เป้าหมาย: หน้าจอใกล้เคียง snake_game.png แบบภาพนิ่ง จอแบ่งเป็นช่องเท่า ๆ กัน (กริด)
 * งูคือกล่องเรียงต่อกันเป็นช่อง ๆ หัวงูอยู่ช่องแรกของรายการเสมอ
 *
 * Snake step 1, the practise skeleton: the title screen, then the snake as three
 * boxes on a 26 px grid (the head lighter), the food box and "Score: 0". The
 * update function does nothing yet, so the picture stays still; the game loop
 * handles Back (quit) and Start (pause) by itself.
 *
 * Twin of snake_step1.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/snake_step1.py). Style: CLEAN_C.md.
 * The snake list becomes a fixed array of three cells.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_runtime.h"

#include "bento_game.h"
#include "bento_gpio.h"

/* The grid and the still picture. */
enum {
    CELL_PX = 26, /* 1 ช่องกริด = 26 พิกเซล */
    GRID_COLS = BENTO_GAME_WIDTH / CELL_PX,
    GRID_ROWS = BENTO_GAME_HEIGHT / CELL_PX,
    SNAKE_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    FOOD_COL = 7,
    FOOD_ROW = 4,
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
    cell_t body[SNAKE_LEN];
    bento_game_box_t squares[SNAKE_LEN];
    bento_game_box_t food;
    bento_game_text_t score_text;
    int32_t score;
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

/* ภาพยังนิ่ง คาบหน้าเราจะเติม logic เดินใน update() */
static bento_status_t update(void *user, bool *out_stop)
{
    (void)user;
    (void)out_stop;
    return BENTO_OK; /* ทุกเฟรมยังไม่ต้องทำอะไร ภาพจึงนิ่ง */
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

static bento_status_t add_cell_box(snake_t *snake, bento_game_box_t *box, cell_t cell,
                                   uint32_t color)
{
    bento_game_box_init(box);
    return bento_game_box_create(&snake->game, box, (bento_real_t)(cell.col * CELL_PX),
                                 (bento_real_t)(cell.row * CELL_PX), (bento_real_t)(CELL_PX - 2),
                                 (bento_real_t)(CELL_PX - 2), color);
}

/* แปลงช่องกริดของแต่ละปล้องให้เป็นกล่องสีเขียวบนจอ (ช่อง x 26 พิกเซล = ตำแหน่งจริง) */
static bento_status_t draw_still_picture(snake_t *snake)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    /* งู = รายการช่อง (คอลัมน์, แถว) หัวงูอยู่ช่องแรกเสมอ */
    for (i = 0; i < SNAKE_LEN; i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    for (i = 0; (status == BENTO_OK) && (i < SNAKE_LEN); i++) {
        status = add_cell_box(snake, &snake->squares[i], snake->body[i], BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        /* หัวงูสีอ่อนกว่า ให้เห็นชัดว่าหัวอยู่ไหน */
        status = bento_game_box_set_color(&snake->game, &snake->squares[0], BENTO_GAME_GB_LIGHT);
    }
    if (status == BENTO_OK) {
        status = add_cell_box(snake, &snake->food, (cell_t){FOOD_COL, FOOD_ROW},
                              BENTO_GAME_GB_LIGHTEST);
    }
    snake->score = 0;
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(&snake->game, &snake->score_text, "Score: 0", SCORE_X,
                                        SCORE_Y, BENTO_GAME_WHITE);
    }
    /* ----- เติมส่วนนี้เอง (งานของคุณ): ทดลองแก้ภาพนิ่งให้เป็นของคุณ -----
     *    1) เปลี่ยนตำแหน่งงูเริ่มต้น โดยแก้ START_COL / START_ROW ด้านบน แล้วรันดูว่างูย้ายไหม
     *    2) เปลี่ยนสีหัวงู: ใส่สีอื่นใน bento_game_box_set_color ของ squares[0]
     *       เช่น BENTO_GAME_GB_LIGHTEST หรือ BENTO_GAME_WHITE
     *    3) ลองย้ายอาหาร: เปลี่ยน FOOD_COL กับ FOOD_ROW แล้วดูว่าจุดอาหารขยับตามไหม
     *    (ทุกข้อแก้ค่าที่มีอยู่แล้วได้เลย ไม่ต้องเพิ่มบรรทัดใหม่) */
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
        status = draw_still_picture(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        /* Back = ออก / Start = หยุดชั่วคราว จัดการให้โดย game loop */
        status = bento_game_run(&snake->game, update, snake, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
