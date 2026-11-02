/*
 * snake_sprite_step1.c -- คาบ 4 (ช่วง B): กล่อง เป็น sprite จริง (ภาพนิ่ง)
 *
 * ขั้นนี้ logic การเดินยังไม่มี คาบนี้แค่เปลี่ยนตัววาด
 * ไฟล์นี้คือ snake_step1 (วาดสนาม ตัวงู อาหารแบบภาพนิ่ง) แต่เปลี่ยนวิธีวาดจากกล่อง
 * เป็นสไปรท์ (รูปพิกเซลอาร์ตตัวเดียวกับเกม C) และขนาดช่องจาก 26 เป็น 16 พิกเซล
 * ปล้องตัวงูเตรียมไว้ล่วงหน้าเป็น pool ครั้งเดียว (ห้ามสร้างสไปรท์ใหม่ในลูป)
 *
 * Snake with sprites, step 1: the still snake of snake_step1 drawn with sprites
 * on a 16 px grid. The body segments come from a pool made once, the head is
 * one sprite facing right, the food is a sprite. Press Start to quit. On a
 * build without sprites the library draws small GB-green boxes instead.
 *
 * Twin of snake_sprite_step1.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/snake_sprite_step1.py). Style: CLEAN_C.md.
 * The body list becomes a fixed array of three cells.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The grid and the still picture. */
enum {
    CELL = 16, /* ตรงกับเกม C เป๊ะ (SNAKE_CELL_PX = 16) */
    COLS = BENTO_GAME_WIDTH / CELL,
    ROWS = BENTO_GAME_HEIGHT / CELL,
    BODY_LEN = 3,
    START_COL = 6,
    START_ROW = 8,
    POOL = 26, /* กฎกัน 32 widget: ปล้องตัวงูเตรียมไว้ครั้งเดียว */
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
    cell_t body[BODY_LEN]; /* หัวอยู่ช่องแรก */
    bento_game_sprite_t seg_pool[POOL];
    bento_game_sprite_t head;
    bento_game_sprite_t food;
    bento_game_text_t score_text;
    int32_t score;
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

static bento_real_t cell_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL);
}

/* One pool sprite: shown on body cell index + 1, or hidden. */
static bento_status_t draw_segment(snake_t *snake, int32_t index)
{
    bento_game_sprite_t *segment = &snake->seg_pool[index];
    bento_status_t status;

    if ((index + 1) < BODY_LEN) {
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

/* วาดงูแบบภาพนิ่งจาก body โดยใช้ head + pool (ไม่สร้าง sprite ใหม่) */
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

/* ภาพยังนิ่ง แต่ยังให้ "กด Start = ออก" เหมือนทุกเกม */
static bento_status_t update(void *user, bool *out_stop)
{
    snake_t *snake = &g_snake;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_snake at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if ((status == BENTO_OK) && keys.start) {
        *out_stop = true;
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

/* สร้างครั้งเดียว: pool ปล้อง หัว (รูปหันขวา) อาหาร และคะแนน */
static bento_status_t add_sprites(snake_t *snake)
{
    int32_t i;
    bento_status_t status;

    for (i = 0; i < BODY_LEN; i++) {
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    status = bento_game_pool(&snake->game, snake->seg_pool, POOL,
                             "snake_body");
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->head);
        status = bento_game_sprite_create(
            &snake->game, &snake->head, "snake_head_r", (bento_real_t)0,
            (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        bento_game_sprite_init(&snake->food);
        status = bento_game_sprite_create(
            &snake->game, &snake->food, "snake_food", cell_px(FOOD_COL),
            cell_px(FOOD_ROW));
    }
    snake->score = 0;
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->score_text);
        status = bento_game_text_create(
            &snake->game, &snake->score_text, "Score: 0", SCORE_X,
            SCORE_Y, BENTO_GAME_WHITE);
    }
    return status;
}

int main(void)
{
    snake_t *snake = &g_snake;
    bool quit = false;
    bento_status_t status;

    (void)memset(snake, 0, sizeof(*snake));
    bento_game_init(&snake->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&snake->game, "SNAKE", NULL, &quit);
    report_joystick(&snake->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_sprites(snake);
    }
    if ((status == BENTO_OK) && !quit) {
        status = draw(snake); /* วาดงูหนึ่งครั้งก่อนเริ่ม loop (ให้งูโผล่ทันที) */
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&snake->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
