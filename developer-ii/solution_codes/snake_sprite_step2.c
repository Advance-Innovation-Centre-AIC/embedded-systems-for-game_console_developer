/*
 * snake_sprite_step2.c -- Snake #4 (คาบ 5): กริด-เดิน + กินแล้วโตด้วย sprite pool + ชน = ตาย
 *
 * ต่อยอดจากคาบ 4 (เปลี่ยนกล่องเป็นสไปรท์) คาบนี้ใส่ "สมอง" ครบลูป:
 * เดิน = เติมหัวใหม่ + ตัดหาง, โต = เติมหัวใหม่แต่ไม่ตัดหาง,
 * ตาย = หัวชนกำแพงหรือหัวไปอยู่ในตัวงู, ลำตัว = sprite pool สร้างครั้งเดียวแล้ว show/hide/move ซ้ำ
 *
 * Snake with sprites, step 2, the solution: the snake walks one cell per frame
 * on a 16 px grid, steered by the joystick. Eating the food scores, plays "eat"
 * and grows the snake (up to the size of the body pool); a wall or a self bite
 * plays "die", shows "GAME OVER" and ends the game loop.
 *
 * Twin of snake_sprite_step2.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/snake_sprite_step2.py). Style: CLEAN_C.md.
 * Python's list becomes an array with a length; random is bento_random,
 * MicroPython's generator draw for draw.
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

/* The grid, the pool and the screen. */
enum {
    CELL = 16, /* ตรงกับ C game (SNAKE_CELL_PX = 16) */
    COLS = BENTO_GAME_WIDTH / CELL,
    ROWS = BENTO_GAME_HEIGHT / CELL,
    POOL = 26, /* จำนวนปล้องลำตัว (<= 32 - หัว - อาหาร - score) */
    MAX_BODY = POOL + 1,
    START_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    SCORE_X = 10,
    SCORE_Y = 8,
    SCORE_CHARS = 20, /* "Score: " and any int32_t */
    OVER_X = 330,
    OVER_Y = 180,
    FPS = 9 /* 9 เฟรม/วิ = จังหวะ Snake คลาสสิก */
};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    /* หัวอยู่ช่องแรก; room for the head before the pop */
    cell_t body[MAX_BODY + 1];
    int32_t length;
    bento_game_sprite_t body_segment_pool[POOL];
    bento_game_sprite_t head;
    bento_game_sprite_t food;
    bento_game_text_t score_text;
    cell_t food_cell;
    int32_t step_x; /* 1,0=ขวา -1,0=ซ้าย 0,-1=ขึ้น 0,1=ลง */
    int32_t step_y;
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

static bento_real_t cell_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL);
}

/* สุ่มอาหารเป็น "ช่อง" (ไม่ใช่พิกเซล) แล้วค่อยคูณ CELL ตอนวาด ห้ามทับตัวงู */
static bento_status_t place_food(snake_t *snake)
{
    cell_t cell = {0, 0};
    bool free_cell = false;
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) && !free_cell) {
        status = bento_random_randint(&snake->random, 0, COLS - 1,
                                      &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&snake->random, 0, ROWS - 1,
                                          &cell.row);
        }
        free_cell = (status == BENTO_OK) && !snake_covers(snake, cell);
    }
    if (status == BENTO_OK) {
        snake->food_cell = cell;
        status = bento_game_sprite_move_to(&snake->game, &snake->food,
                                           cell_px(cell.col),
                                           cell_px(cell.row));
    }
    return status;
}

/* One pool sprite: shown on body cell index + 1, or hidden. */
static bento_status_t draw_segment(snake_t *snake, int32_t index)
{
    bento_game_sprite_t *segment = &snake->body_segment_pool[index];
    bento_status_t status;

    if ((index + 1) < snake->length) {
        const cell_t cell = snake->body[index + 1];

        status = bento_game_sprite_show(&snake->game, segment);
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(&snake->game, segment,
                                               cell_px(cell.col),
                                               cell_px(cell.row));
        }
    } else {
        status = bento_game_sprite_hide(&snake->game, segment);
    }
    return status;
}

/* ย้ายเฉพาะ sprite ที่ใช้: show ปล้องที่มีจริง hide ที่เหลือ */
static bento_status_t draw(snake_t *snake)
{
    int32_t i;
    bento_status_t status = bento_game_sprite_move_to(
        &snake->game, &snake->head, cell_px(snake->body[0].col),
        cell_px(snake->body[0].row));

    /* ปล้องตัว: body[1..] เอาจาก pool มาโชว์เท่าที่ใช้ ที่เหลือซ่อน */
    for (i = 0; (status == BENTO_OK) && (i < POOL); i++) {
        status = draw_segment(snake, i);
    }
    return status;
}

/* เลี้ยว (กันถอยหลังทับตัว) */
static void steer(snake_t *snake, const bento_game_keys_t *keys)
{
    if (keys->left && (snake->step_x == 0)) {
        snake->step_x = -1;
        snake->step_y = 0;
    }
    if (keys->right && (snake->step_x == 0)) {
        snake->step_x = 1;
        snake->step_y = 0;
    }
    if (keys->up && (snake->step_y == 0)) {
        snake->step_x = 0;
        snake->step_y = -1;
    }
    if (keys->down && (snake->step_y == 0)) {
        snake->step_x = 0;
        snake->step_y = 1;
    }
}

/* ตรวจ "ตาย" ก่อน insert เสมอ: ชนกำแพง หรือหัวไปอยู่ในตัวงู (ชนตัวเอง) */
static bool hits_something(const snake_t *snake, cell_t next_head)
{
    return (next_head.col < 0) || (next_head.col >= COLS) ||
           (next_head.row < 0) || (next_head.row >= ROWS) ||
           snake_covers(snake, next_head);
}

static bento_status_t show_game_over(snake_t *snake)
{
    bento_game_text_t over;

    bento_game_sfx(&snake->game, "die");
    bento_game_text_init(&over);
    return bento_game_text_create(&snake->game, &over, "GAME OVER",
                                  OVER_X, OVER_Y, BENTO_GAME_RED);
}

/* เติมหัวใหม่เสมอ */
static void insert_head(snake_t *snake, cell_t next_head)
{
    int32_t i;

    for (i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->length++;
}

static bento_status_t eat_food(snake_t *snake)
{
    char text[SCORE_CHARS];
    bento_status_t status;

    snake->score++;
    bento_game_sfx(&snake->game, "eat");
    (void)snprintf(text, sizeof(text), "Score: %" PRId32, snake->score);
    status =
        bento_game_text_set(&snake->game, &snake->score_text, text);
    if (status == BENTO_OK) {
        status = place_food(snake);
    }
    if (snake->length > MAX_BODY) {
        /* pool เต็ม -> หยุดโต (เพดาน widget จริงของบอร์ด) */
        snake->length--;
    }
    return status;
}

static bento_status_t walk_or_eat(snake_t *snake, cell_t next_head)
{
    bento_status_t status = BENTO_OK;

    insert_head(snake, next_head);
    if ((next_head.col == snake->food_cell.col) &&
        (next_head.row == snake->food_cell.row)) {
        status = eat_food(snake);
    } else {
        snake->length--; /* ไม่กิน -> ตัดหาง (เดินปกติ) */
    }
    if (status == BENTO_OK) {
        status = draw(snake);
    }
    return status;
}

static bento_status_t update(void *user, bool *out_stop)
{
    snake_t *snake = &g_snake;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_snake at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if (status == BENTO_OK) {
        cell_t next_head;

        steer(snake, &keys);
        /* (Back=ออก / Start=หยุดชั่วคราว game loop จัดการให้) */
        next_head = (cell_t){snake->body[0].col + snake->step_x,
                             snake->body[0].row + snake->step_y};
        if (hits_something(snake, next_head)) {
            status = show_game_over(snake);
            *out_stop = true;
        } else {
            status = walk_or_eat(snake, next_head);
        }
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

/* สร้าง sprite ครั้งเดียว (หัว + pool ลำตัว + อาหาร) ห้ามสร้างใน update() */
static bento_status_t add_sprites(snake_t *snake)
{
    int32_t i;
    bento_status_t status;

    for (i = 0; i < START_LEN; i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    snake->length = START_LEN;
    status = bento_game_pool(&snake->game, snake->body_segment_pool,
                             POOL, "snake_body");
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->head);
        status = bento_game_sprite_create(
            &snake->game, &snake->head, "snake_head_r", (bento_real_t)0,
            (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->food);
        status = bento_game_sprite_create(&snake->game, &snake->food,
                                          "snake_food", (bento_real_t)0,
                                          (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(
            &snake->game, &snake->score_text, "Score: 0", SCORE_X,
            SCORE_Y, BENTO_GAME_WHITE);
    }
    snake->step_x = 1;
    snake->step_y = 0;
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
        status = add_sprites(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        status = place_food(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        status = draw(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&snake->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
