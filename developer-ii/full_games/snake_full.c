/*
 * snake_full.c -- Snake ฉบับเต็ม (REFERENCE) สี Game Boy
 *
 * เกมงูฉบับเต็มที่นักศึกษาสร้างทีละขั้นใน Block B ตรงกับเกม C บนบอร์ด (page_game_snake.c):
 * สนามหญ้า เมนูเลือกโหมด RELAXED/CLASSIC/TURBO, HUD Score/Best/Len, เลี้ยวแบบจับจังหวะกด,
 * เสียงกิน/เหรียญ, เสียงชนกำแพงกับกัดตัวเองแยกกัน, Y = เริ่มตาใหม่โหมดเดิมทันที,
 * Start = เล่นใหม่โดยไม่ต้องรีบูต
 *
 * The full Snake: a grass field, a mode menu (the mode sets the step time), a
 * Score/Best/Len HUD, edge-triggered turns with no U-turn, the eat sound and a
 * two-note coin every 10th apple, separate sounds for a wall hit and a self
 * bite, Y to restart the same mode at once, and Start to play again from the
 * game-over panel. Only the head and one body box move per step.
 *
 * Twin of snake_full.py (embedded-systems-for-game_console_developer/
 * developer-ii/full_games/snake_full.py). Style: CLEAN_C.md.
 * Python's lists become arrays with a length; random is bento_random,
 * MicroPython's generator draw for draw. run(fps=1000 / ms) becomes
 * bento_game_run_period with Python's frame period int(1000 / fps).
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

/* The grid, the widget budget and the screen. */
enum {
    CELL = 16, /* ขนาดช่องเท่าเกม C เป๊ะ (SNAKE_CELL_PX = 16px) */
    COLS = BENTO_GAME_WIDTH / CELL,
    ROWS = BENTO_GAME_HEIGHT / CELL,
    /* งบวิดเจ็ต: หัว 1 + ลำตัว 18 แล้วตัวหยุดยาว แต่คะแนนยังนับต่อ */
    MAX_LEN = 19,
    /* one more while the new head is in and the tail not yet out */
    BODY_CAP = MAX_LEN + 1,
    BOX_COUNT = MAX_LEN - 1,
    START_LEN = 5,
    START_COL = 10,
    START_ROW = 10,
    STRIPE_PX = 48, /* ลายหญ้ากว้าง 48px แบบเกม C */
    STRIPE_ROWS = 8,
    HIDDEN_PX = -100,
    HUD_X = 10,
    HUD_Y = 8,
    /* the mode name and three int32_t with their labels */
    HUD_CHARS = 72,
    MODE_COUNT = 3,
    CLASSIC_MODE = 1, /* เปิดมาชี้ CLASSIC ก่อน เหมือนเกม C */
    COIN_EVERY = 10,
    MS_PER_SECOND = 1000
};

/* The coin's two notes. */
enum {
    COIN_NOTE_LOW = 83,
    COIN_NOTE_HIGH = 88,
    COIN_VELOCITY = 100,
    COIN_LOW_MS = 60,
    COIN_GAP_MS = 45,
    COIN_HIGH_MS = 160
};

/* Colours, 0xRRGGBB, ลอกจากเกม C. */
enum {
    GRASS = 0x4CA838,
    GRASS_MOWN = 0x409030,
    APPLE = 0xD82818
};

/* ชื่อโหมด + คาบเวลาเกมเดิน 1 ก้าว (ms) ชุดเดียวกับตาราง s_snake_modes ในเกม C */
static const char *const MODE_NAMES[MODE_COUNT] = {"RELAXED", "CLASSIC",
                                                   "TURBO"};
static const int32_t MODE_MS[MODE_COUNT] = {150, 115, 70};
static const char BEST_NAME[] = "snake";

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    int32_t mode;
    /* NULL = None: the menu heading is "SELECT MODE" */
    const char *death_text;
    bool restart_now;      /* ตั้งเป็นจริงเมื่อผู้เล่นกด Y */
    cell_t body[BODY_CAP]; /* หัวอยู่หน้าสุด */
    int32_t length;
    bento_game_box_t head;
    bento_game_box_t body_boxes[BOX_COUNT];
    /* กล่องลำตัวที่โชว์อยู่ เรียงจากคอไปหาง (indices) */
    int32_t boxes[BOX_COUNT];
    int32_t box_count;
    int32_t spare[BOX_COUNT]; /* กล่องลำตัวสำรอง ซ่อนไว้ */
    int32_t spare_count;
    bento_game_box_t food;
    cell_t food_cell;
    bento_game_text_t hud;
    int32_t score;
    int32_t step_x;
    int32_t step_y;
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

static bento_real_t cell_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL);
}

/* เกม C มีเสียงเหรียญ "cha-ching" ทุกแอปเปิลลูกที่ 10 (ไฟล์ PCM ที่ Python เรียกตรง ๆ ไม่ได้)
 * เลยแต่งโน้ต 2 ตัวให้ได้อารมณ์เดียวกัน */
static bento_status_t coin_sound(snake_t *snake)
{
    bento_status_t status;

    bento_game_tone(&snake->game, COIN_NOTE_LOW,
                    BENTO_GAME_WAVE_DEFAULT, COIN_VELOCITY,
                    COIN_LOW_MS);
    status = bento_time_sleep_ms(COIN_GAP_MS);
    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    if (status == BENTO_OK) {
        bento_game_tone(&snake->game, COIN_NOTE_HIGH,
                        BENTO_GAME_WAVE_DEFAULT, COIN_VELOCITY,
                        COIN_HIGH_MS);
    }
    return status;
}

/* ช่อง Best ขยับสดระหว่างเล่นแบบเกม C: คะแนนแซงเมื่อไหร่ Best ตามทันที */
static bento_status_t show_hud(snake_t *snake)
{
    const int32_t best = bento_game_best(&snake->game, BEST_NAME);
    char text[HUD_CHARS];

    (void)snprintf(text, sizeof(text),
                   "%s    Score: %" PRId32 "    Best: %" PRId32
                   "    Len: %" PRId32,
                   MODE_NAMES[snake->mode], snake->score,
                   (best > snake->score) ? best : snake->score,
                   snake->length);
    return bento_game_text_set(&snake->game, &snake->hud, text);
}

static bool in_body(const snake_t *snake, cell_t cell)
{
    bool found = false;
    int32_t i;

    for (i = 0; (i < snake->length) && !found; i++) {
        found = (snake->body[i].col == cell.col) &&
                (snake->body[i].row == cell.row);
    }
    return found;
}

/* สุ่มช่องไปเรื่อย ๆ จนกว่าจะเจอช่องที่งูไม่ได้ทับอยู่ */
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
        free_cell = (status == BENTO_OK) && !in_body(snake, cell);
    }
    if (status == BENTO_OK) {
        snake->food_cell = cell;
        status = bento_game_box_move_to(&snake->game, &snake->food,
                                        cell_px(cell.col),
                                        cell_px(cell.row));
    }
    return status;
}

/* เลี้ยวแบบจับ "จังหวะกด" (กดค้างไม่นับซ้ำ): อ่านครบทุกปุ่มก่อน แล้วค่อยเลือกตามลำดับ
 * และห้ามกลับหลังหัน 180 องศา */
static void steer(snake_t *snake, const bento_game_keys_t *keys)
{
    bento_game_t *game = &snake->game;
    const bool up =
        bento_game_pressed_once(game, BENTO_GAME_KEY_UP, keys);
    const bool down =
        bento_game_pressed_once(game, BENTO_GAME_KEY_DOWN, keys);
    const bool left =
        bento_game_pressed_once(game, BENTO_GAME_KEY_LEFT, keys);
    const bool right =
        bento_game_pressed_once(game, BENTO_GAME_KEY_RIGHT, keys);

    if (up && (snake->step_y == 0)) {
        snake->step_x = 0;
        snake->step_y = -1;
    } else if (down && (snake->step_y == 0)) {
        snake->step_x = 0;
        snake->step_y = 1;
    } else if (left && (snake->step_x == 0)) {
        snake->step_x = -1;
        snake->step_y = 0;
    } else if (right && (snake->step_x == 0)) {
        snake->step_x = 1;
        snake->step_y = 0;
    } else {
        /* no turn */
    }
}

/* ชนขอบสนามหรือชนตัวเอง = จบตา (ลำดับการเช็คเดียวกับเกม C) */
static bool round_ends(snake_t *snake, cell_t next_head)
{
    bool ends = true;

    if (!((0 <= next_head.col) && (next_head.col < COLS) &&
          (0 <= next_head.row) && (next_head.row < ROWS))) {
        bento_game_sfx(&snake->game, "die");
        snake->death_text = "Wall hit";
    } else if (in_body(snake, next_head)) {
        bento_game_sfx(&snake->game,
                       "lose"); /* คนละเสียงกับชนกำแพง แบบเกม C */
        snake->death_text = "Self collision";
    } else {
        ends = false;
    }
    return ends;
}

/* The data side of a step: the new head, eating, and the tail. */
static bento_status_t move_body(snake_t *snake, cell_t next_head,
                                bool *grew)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->length++;
    *grew = false;
    if ((next_head.col == snake->food_cell.col) &&
        (next_head.row == snake->food_cell.row)) {
        snake->score++;
        bento_game_sfx(&snake->game, "eat");
        if ((snake->score % COIN_EVERY) == 0) {
            status = coin_sound(snake);
        }
        /* ยังไม่ชนเพดานงบวิดเจ็ต -> ตัวยาวขึ้นจริง */
        *grew = snake->length <= MAX_LEN;
        if (status == BENTO_OK) {
            status = place_food(snake);
        }
    }
    if (!*grew) {
        snake->length--;
    }
    return status;
}

/* เคล็ดงูคลาสสิก: ที่เปลี่ยนจริงมีแค่หัวกับหาง เลยย้าย "กล่องหาง" ไปสวมตำแหน่งหัวเก่า
 * แทนการขยับทุกกล่อง เร็วกว่ากันมาก */
static bento_status_t move_boxes(snake_t *snake, cell_t old_head,
                                 cell_t next_head, bool grew)
{
    int32_t box;
    int32_t i;
    bento_status_t status = bento_game_box_move_to(
        &snake->game, &snake->head, cell_px(next_head.col),
        cell_px(next_head.row));

    if (grew) {
        snake->spare_count--;
        box = snake->spare[snake->spare_count];
    } else {
        snake->box_count--;
        box = snake->boxes[snake->box_count];
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &snake->game, &snake->body_boxes[box],
            cell_px(old_head.col), cell_px(old_head.row));
    }
    if ((status == BENTO_OK) && grew) {
        status =
            bento_game_box_show(&snake->game, &snake->body_boxes[box]);
    }
    for (i = snake->box_count; i > 0; i--) {
        snake->boxes[i] = snake->boxes[i - 1];
    }
    snake->boxes[0] = box; /* กลายเป็นคอ (ต่อจากหัว) */
    snake->box_count++;
    return status;
}

/* One step that is not a restart: steer, then end the round or move. */
static bento_status_t
step(snake_t *snake, const bento_game_keys_t *keys, bool *out_stop)
{
    const cell_t old_head = snake->body[0];
    cell_t next_head;
    bool grew = false;
    bento_status_t status = BENTO_OK;

    steer(snake, keys);
    next_head = (cell_t){old_head.col + snake->step_x,
                         old_head.row + snake->step_y};
    if (round_ends(snake, next_head)) {
        *out_stop = true;
    } else {
        status = move_body(snake, next_head, &grew);
        /* เกม C ก็อัปเดต HUD ท้ายทุกก้าวแบบนี้ */
        if (status == BENTO_OK) {
            status = show_hud(snake);
        }
        if (status == BENTO_OK) {
            status = move_boxes(snake, old_head, next_head, grew);
        }
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
    if ((status == BENTO_OK) &&
        bento_game_pressed_once(&snake->game, BENTO_GAME_KEY_Y,
                                &keys)) {
        /* Y = เริ่มตาใหม่โหมดเดิมทันที (แบบเกม C) */
        snake->restart_now = true;
        *out_stop = true;
    } else if (status == BENTO_OK) {
        status = step(snake, &keys, out_stop);
    } else {
        /* the status ends the game loop */
    }
    return status;
}

static bento_status_t add_cell_box(snake_t *snake,
                                   bento_game_box_t *box, int32_t x_px,
                                   int32_t y_px, uint32_t color)
{
    bento_game_box_init(box);
    return bento_game_box_create(
        &snake->game, box, (bento_real_t)x_px, (bento_real_t)y_px,
        (bento_real_t)(CELL - 2), (bento_real_t)(CELL - 2), color);
}

/* แถบหญ้าสลับสีแบบสนามตัดหญ้า */
static bento_status_t draw_field(snake_t *snake)
{
    bento_game_box_t field;
    bento_game_box_t stripe;
    int32_t row;
    bento_status_t status =
        bento_game_background(&snake->game, &field, GRASS);

    for (row = 1; (status == BENTO_OK) && (row < STRIPE_ROWS);
         row += 2) {
        bento_game_box_init(&stripe);
        status = bento_game_box_create(
            &snake->game, &stripe, (bento_real_t)0,
            (bento_real_t)(row * STRIPE_PX),
            (bento_real_t)BENTO_GAME_WIDTH, (bento_real_t)STRIPE_PX,
            GRASS_MOWN);
    }
    return status;
}

/* The snake's boxes: the head, the shown body (neck first) and the hidden spares. */
static bento_status_t add_snake_boxes(snake_t *snake)
{
    int32_t i;
    bento_status_t status =
        add_cell_box(snake, &snake->head, snake->body[0].col * CELL,
                     snake->body[0].row * CELL, BENTO_GAME_GB_DARKEST);

    snake->box_count = 0;
    for (i = 1; (status == BENTO_OK) && (i < snake->length); i++) {
        status =
            add_cell_box(snake, &snake->body_boxes[snake->box_count],
                         snake->body[i].col * CELL,
                         snake->body[i].row * CELL, BENTO_GAME_GB_DARK);
        snake->boxes[snake->box_count] = snake->box_count;
        snake->box_count++;
    }
    snake->spare_count = 0;
    for (i = 0; (status == BENTO_OK) && (i < (MAX_LEN - snake->length));
         i++) {
        const int32_t box = snake->box_count + snake->spare_count;

        status = add_cell_box(snake, &snake->body_boxes[box], HIDDEN_PX,
                              HIDDEN_PX, BENTO_GAME_GB_DARK);
        if (status == BENTO_OK) {
            status = bento_game_box_hide(&snake->game,
                                         &snake->body_boxes[box]);
        }
        snake->spare[snake->spare_count] = box;
        snake->spare_count++;
    }
    return status;
}

/* จัดฉาก (ต้องสร้างใหม่ทุกตา เพราะ replay ล้างจอทั้งหมด) */
static bento_status_t build_round(snake_t *snake)
{
    int32_t i;
    bento_status_t status = draw_field(snake);

    for (i = 0; i < START_LEN; i++) {
        /* จุดเกิดเดียวกับเกม C */
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    snake->length = START_LEN;
    if (status == BENTO_OK) {
        status = add_snake_boxes(snake);
    }
    if (status == BENTO_OK) {
        status = add_cell_box(snake, &snake->food, 0, 0, APPLE);
    }
    snake->food_cell = (cell_t){0, 0};
    if (status == BENTO_OK) {
        bento_game_text_init(&snake->hud);
        status = bento_game_text_create(&snake->game, &snake->hud, "",
                                        HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    snake->score = 0;
    snake->step_x = 1; /* เริ่มวิ่งไปขวา เหมือนเกม C */
    snake->step_y = 0;
    snake->death_text = "GAME OVER";
    snake->restart_now = false;
    if (status == BENTO_OK) {
        status = place_food(snake);
    }
    if (status == BENTO_OK) {
        status = show_hud(snake);
    }
    return status;
}

/* run(update, fps=1000 / ms): Python's run() sleeps int(1000 / fps) per frame. */
static int32_t frame_period_ms(int32_t mode)
{
    const bento_real_t fps =
        (bento_real_t)MS_PER_SECOND / (bento_real_t)MODE_MS[mode];

    return (int32_t)((bento_real_t)MS_PER_SECOND / fps);
}

/* One round and what the player chose after it. */
static bento_status_t play_round(snake_t *snake, bool *playing,
                                 bool *restart_same)
{
    bool finished = false;
    bento_status_t status = build_round(snake);

    if (status == BENTO_OK) {
        /* (Start = พักเกม / Back = ออก game loop จัดการให้) */
        status = bento_game_run_period(&snake->game, update, NULL,
                                       frame_period_ms(snake->mode),
                                       &finished);
    }
    if ((status == BENTO_OK) && !finished) {
        /* Back กลางเกม: คะแนนดี ๆ ไม่หาย; save_best answers the best, not a status */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        *playing = false;
    } else if ((status == BENTO_OK) && snake->restart_now) {
        /* Y = วนไปเริ่มโหมดเดิมใหม่ทันที; save_best answers the best, not a status */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        status = bento_game_replay(&snake->game);
        *restart_same = true;
    } else if (status == BENTO_OK) {
        /* Start = เล่นใหม่ / Back = พอแค่นี้ */
        status = bento_game_over(&snake->game, &snake->score, BEST_NAME,
                                 snake->death_text, playing);
    } else {
        /* the status ends the game */
    }
    return status;
}

/* ตายแล้ววนกลับมาเลือกโหมดใหม่ได้ทุกตา หัวเมนูบอกด้วยว่าตาก่อนจบเพราะอะไร */
/* BACK จากหน้าเมนู = เลิกเล่น (เหมือนเกม C) */
static bento_status_t pick_mode(snake_t *snake, bool *playing)
{
    int32_t picked = -1;
    const char *heading =
        (snake->death_text != NULL) ? snake->death_text : "SELECT MODE";
    const bento_status_t status =
        bento_game_menu(&snake->game, MODE_NAMES, MODE_COUNT, heading,
                        snake->mode, &picked);

    *playing = picked >= 0;
    if ((status == BENTO_OK) && *playing) {
        snake->mode = picked;
    }
    return status;
}

static bento_status_t play(snake_t *snake)
{
    bool playing = true;
    bool restart_same = false;
    bento_status_t status = BENTO_OK;

    snake->mode = CLASSIC_MODE;
    snake->death_text = NULL;
    while ((status == BENTO_OK) && playing) {
        if (!restart_same) {
            status = pick_mode(snake, &playing);
        }
        restart_same = false;
        if ((status == BENTO_OK) && playing) {
            status = play_round(snake, &playing, &restart_same);
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
        status = bento_game_title_melody(&snake->game, "SNAKE", NULL,
                                         NULL, 0U, &quit);
        /* title() calls start(), whose joystick.init() prints this line on the kits */
        if ((snake->game.joystick_init == BENTO_OK) &&
            !on_reterminal()) {
            (void)printf(
                "joystick: USB Host HID initialized on CM55\n");
        }
    }
    if ((status == BENTO_OK) && !quit) {
        status = play(snake);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
