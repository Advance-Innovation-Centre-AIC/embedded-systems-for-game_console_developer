/*
 * snake_step5.c -- สร้าง Snake #5 (step สุดท้าย): อัปเกรดเป็นสไปรท์จริง + เลือกความยาก
 *
 * เกมเหมือน step4 ทุกอย่าง แต่เปลี่ยน "ผิวเกม" จากกล่องสี่เหลี่ยมเป็นรูปวาดงูจริง (pixel-art)
 * หัวงูหันหน้าตามทางเดิน มีเมนูเลือกความยาก RELAXED/CLASSIC/TURBO (UP/DOWN เลือก A เริ่ม)
 * และเก็บสถิติ Best สไปรท์ลำตัวสร้างเป็น pool ครั้งเดียว ห้ามสร้างใหม่ในทุกเฟรม
 * โครงเว้น 30%: ตรรกะงู เดิน/ตาย/กินโต เว้นไว้ให้เติมเอง
 *
 * Snake step 5, the practise skeleton: the sprites (the head turns with the
 * direction, the body is a pool made once), the mode menu and the score line
 * are given. The snake logic (die, walk, eat and grow) is left as TODOs; the
 * placeholder only drops the tail, so the snake shrinks one cell per frame and,
 * on the third frame, the redraw finds no head: Python raises IndexError there
 * and this twin returns BENTO_E_NOT_FOUND, ending the program the same way.
 *
 * Twin of snake_step5.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/snake_step5.py). Style: CLEAN_C.md.
 * Python's list becomes an array with a length; random is bento_random,
 * MicroPython's generator draw for draw.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_runtime.h"

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_time.h"

/* The grid, the pool and the screen. */
enum {
    CELL_PX = 16, /* ตรงกับเกม C จริง (SNAKE_CELL_PX = 16) */
    GRID_COLS = BENTO_GAME_WIDTH / CELL_PX,
    GRID_ROWS = BENTO_GAME_HEIGHT / CELL_PX,
    BODY_POOL_SIZE = 26, /* ปล้องลำตัว (<= 32 - head - food - score) */
    MAX_SNAKE_LENGTH = BODY_POOL_SIZE + 1,
    START_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    SCORE_X = 10,
    SCORE_Y = 8,
    MENU_X = 250,
    MENU_Y = 170,
    MENU_CHARS = 48, /* the longest mode line and its NUL */
    MODE_COUNT = 3,
    CLASSIC_MODE = 1, /* CLASSIC เป็นค่าตั้งต้น */
    MENU_POLL_MS = 125 /* 1 รอบ ~8/วิ (กันลั่นปุ่ม + ไม่ busy-hang) */
};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    cell_t body[MAX_SNAKE_LENGTH + 1]; /* หัวอยู่ตัวแรก; room for the head before the pop */
    int32_t length;
    bento_game_sprite_t body_pool[BODY_POOL_SIZE];
    bento_game_sprite_t head_sprite;
    bento_game_sprite_t food_sprite;
    bento_game_text_t score_text;
    cell_t food_cell;
    int32_t step_col;
    int32_t step_row;
    int32_t score;
    int32_t best_score;
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
        if ((snake->body[i].col == cell.col) && (snake->body[i].row == cell.row)) {
            covered = true;
        }
    }
    return covered;
}

static bento_real_t cell_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL_PX);
}

static bento_status_t place_food_at_random_empty_cell(snake_t *snake)
{
    cell_t cell = {0, 0};
    bool free_cell = false;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && !free_cell) {
        status = bento_random_randint(&snake->random, 0, GRID_COLS - 1, &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&snake->random, 0, GRID_ROWS - 1, &cell.row);
        }
        free_cell = (status == BENTO_OK) && !snake_covers(snake, cell);
    }
    if (status == BENTO_OK) {
        snake->food_cell = cell;
        status = bento_game_sprite_move_to(&snake->game, &snake->food_sprite, cell_px(cell.col),
                                           cell_px(cell.row));
    }
    return status;
}

/* หัวหันทิศ: ทิศเดิน (step_col, step_row) -> ชื่อสไปรท์หัว */
static const char *head_sprite_for(int32_t step_col, int32_t step_row)
{
    const char *name = "snake_head_d";

    if (step_col == 1) {
        name = "snake_head_r";
    } else if (step_col == -1) {
        name = "snake_head_l";
    } else if (step_row == -1) {
        name = "snake_head_u";
    } else {
        /* (0, 1): down */
    }
    return name;
}

/* snake->body[1..] -> pool ลำตัว; the rest of the pool hides */
static bento_status_t redraw_body(snake_t *snake)
{
    int32_t pool_index;
    bento_status_t status = BENTO_OK;

    for (pool_index = 0; (status == BENTO_OK) && (pool_index < BODY_POOL_SIZE); pool_index++) {
        bento_game_sprite_t *piece = &snake->body_pool[pool_index];

        if ((pool_index + 1) < snake->length) {
            const cell_t cell = snake->body[pool_index + 1];

            status = bento_game_sprite_show(&snake->game, piece);
            if (status == BENTO_OK) {
                status = bento_game_sprite_move_to(&snake->game, piece, cell_px(cell.col),
                                                   cell_px(cell.row));
            }
        } else {
            status = bento_game_sprite_hide(&snake->game, piece);
        }
    }
    return status;
}

static bento_status_t redraw(snake_t *snake)
{
    bento_status_t status = BENTO_OK;

    if (snake->length == 0) {
        status = BENTO_E_NOT_FOUND; /* Python's IndexError: the list has no head left */
    }
    if (status == BENTO_OK) {
        /* หัวหันทิศ เหมือนเกม C */
        status = bento_game_sprite_frame(&snake->game, &snake->head_sprite,
                                         head_sprite_for(snake->step_col, snake->step_row));
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&snake->game, &snake->head_sprite,
                                           cell_px(snake->body[0].col),
                                           cell_px(snake->body[0].row));
    }
    if (status == BENTO_OK) {
        status = redraw_body(snake);
    }
    return status;
}

/* เมนูเลือกความยาก: UP/DOWN เลือก A เริ่ม เป็น loop ของเราเอง (ก่อนเข้า game loop)
 * จึงต้องหลับเองทุกรอบ มิฉะนั้นจะหมุนเปล่า (busy-loop) กิน CPU 100% */
static bento_status_t choose_mode(snake_t *snake, int32_t *out_fps)
{
    static const char *const MODE_NAMES[MODE_COUNT] = {"RELAXED", "CLASSIC", "TURBO"};
    static const int32_t MODE_FPS[MODE_COUNT] = {6, 9, 14};
    int32_t selected = CLASSIC_MODE;
    bool chosen = false;
    bento_game_text_t menu_text;
    bento_status_t status;

    bento_game_text_init(&menu_text);
    status = bento_game_text_create(&snake->game, &menu_text, "", MENU_X, MENU_Y,
                                    BENTO_GAME_GB_LIGHTEST);
    while ((status == BENTO_OK) && !chosen) {
        char line[MENU_CHARS];
        bento_game_keys_t keys;

        (void)snprintf(line, sizeof(line), "MODE: < %s >   (UP/DOWN, A=start)",
                       MODE_NAMES[selected]);
        status = bento_game_text_set(&snake->game, &menu_text, line);
        bento_game_keys_init(&keys);
        if (status == BENTO_OK) {
            status = bento_game_keys(&snake->game, &keys);
        }
        if (keys.up) {
            selected = (selected + MODE_COUNT - 1) % MODE_COUNT;
        }
        if (keys.down) {
            selected = (selected + 1) % MODE_COUNT;
        }
        chosen = keys.a || keys.start;
        if ((status == BENTO_OK) && chosen) {
            status = bento_game_text_set(&snake->game, &menu_text, "");
            *out_fps = MODE_FPS[selected]; /* ค่า fps ของโหมดที่เลือก */
        } else if (status == BENTO_OK) {
            status = bento_time_sleep_ms(MENU_POLL_MS);
            if ((status == BENTO_OK) && !bento_runtime_is_running()) {
                status = BENTO_E_CANCELLED;
            }
        } else {
            /* the status ends the menu */
        }
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

static bento_status_t on_each_frame(void *user, bool *out_stop)
{
    /* the state main() handed to bento_game_run */
    snake_t *snake = user;
    bento_game_keys_t keys;
    bento_status_t status;

    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if (status == BENTO_OK) {
        cell_t next_head;

        steer(snake, &keys);
        /* (Back=ออก / Start=หยุดชั่วคราว game loop จัดการให้) */
        next_head = (cell_t){snake->body[0].col + snake->step_col,
                             snake->body[0].row + snake->step_row};
        (void)next_head; /* the TODO below is where it is used */
        (void)out_stop;
        /* ----- เติมส่วนนี้เอง (งานของคุณ): ตรรกะงู เดิน/ตาย/กินโต (เหมือน step4) -----
         * ตรรกะเดิมจาก step4 ทั้งหมด ผิวเกมเปลี่ยนเป็นสไปรท์ แต่ logic ไม่เปลี่ยนเลย ค่อย ๆ ทำตามขั้น:
         * 1) เช็คตาย: ถ้า next_head ออกนอกกริด (col หรือ row ต่ำกว่า 0 หรือถึง/เกิน
         *    GRID_COLS/GRID_ROWS) หรือ next_head ไปทับลำตัวตัวเอง (snake_covers) -> เล่นเสียงตาย
         *    ด้วย bento_game_sfx, อัปเดต best_score ถ้า score ทำได้ดีกว่าเดิม, โชว์ข้อความจบเกม
         *    ด้วย bento_game_text_create แล้วตั้ง *out_stop = true
         * 2) เดินไปข้างหน้า: เลื่อน snake->body ถอยไป 1 ช่อง ใส่ next_head ที่ body[0], length++
         * 3) กินอาหารไหม? ถ้า next_head ตรงกับ food_cell -> score++ และอัปเดต best_score,
         *    เล่นเสียงกินด้วย bento_game_sfx, อัปเดตป้ายด้วย bento_game_text_set (โชว์ทั้ง Score
         *    และ Best), สุ่มอาหารใหม่ด้วย place_food_at_random_empty_cell(); ถ้างูยาวเกิน
         *    MAX_SNAKE_LENGTH ให้ตัดหางทิ้ง (length--)
         *    --- ถ้าไม่ได้กิน -> ตัดหางทิ้ง (length--) หางเดินตามหัว ความยาวเท่าเดิม
         * 4) ปิดท้ายทุกเฟรม: เรียก redraw() เพื่อวาดหัว/ลำตัว/อาหาร ใหม่ */
        snake->length--; /* placeholder กันค้าง (ลบทิ้งเมื่อเริ่มเขียนข้อ 1-4) */
        status = redraw(snake);
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

/* สร้างครั้งเดียว: pool ลำตัว หัว อาหาร และป้ายคะแนน */
static bento_status_t add_sprites(snake_t *snake)
{
    int32_t i;
    bento_status_t status;

    for (i = 0; i < START_LEN; i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    snake->length = START_LEN;
    status = bento_game_pool(&snake->game, snake->body_pool, BODY_POOL_SIZE, "snake_body");
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->head_sprite);
        status = bento_game_sprite_create(&snake->game, &snake->head_sprite, "snake_head_r",
                                          (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->food_sprite);
        status = bento_game_sprite_create(&snake->game, &snake->food_sprite, "snake_food",
                                          (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(&snake->game, &snake->score_text, "Score: 0   Best: 0",
                                        SCORE_X, SCORE_Y, BENTO_GAME_WHITE);
    }
    snake->step_col = 1;
    snake->step_row = 0;
    return status;
}

/* เลือกโหมดก่อนเริ่มเล่น วางอาหาร วาดงู แล้วเข้า game loop */
static bento_status_t play(snake_t *snake)
{
    int32_t fps = 0;
    bento_status_t status = add_sprites(snake);

    if (status == BENTO_OK) {
        status = choose_mode(snake, &fps);
    }
    if (status == BENTO_OK) {
        status = place_food_at_random_empty_cell(snake);
    }
    if (status == BENTO_OK) {
        status = redraw(snake);
    }
    if (status == BENTO_OK) {
        bool finished = false;

        status = bento_game_run(&snake->game, on_each_frame, snake, fps, &finished);
    }
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
        bento_game_init(&snake->game, bento_runtime_get_ui(), bento_runtime_is_running);
        /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
        status = bento_game_title(&snake->game, "SNAKE", NULL, &quit);
        report_joystick(&snake->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = play(snake);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
