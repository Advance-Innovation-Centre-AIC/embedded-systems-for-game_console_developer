/*
 * snake_sprite_full.c -- Snake ฉบับเต็ม วาดด้วยสไปรต์ C จริงของบอร์ด (spike-b)
 *
 * บนฮาร์ดแวร์ ตัวงู หัวงู และแอปเปิลคือพิกเซลชุดเดียวกับเกม Snake ในเครื่อง
 * (ฝั่ง C เป็นเจ้าของรูป เราส่งแค่ชื่อกับตำแหน่ง) บนซิมจะถอยไปเป็นกล่องเขียว GB
 * ตามเกม C ครบวงจร: สนามหญ้า เมนูความยาก RELAXED/CLASSIC/TURBO, HUD Score/Best/Len,
 * เสียงกิน + เสียงพิเศษทุกแอปเปิลลูกที่ 10, เสียงตายแยกชนกำแพง/กัดตัวเอง,
 * ตายแล้วเล่นซ้ำได้ทันที, เลี้ยวแบบจับจังหวะกด, Y = เริ่มโหมดเดิมใหม่
 *
 * The full Snake drawn with the board's own sprites: a grass field, a mode
 * menu (the mode sets the step time), a Score/Best/Len HUD, the head turning
 * with the direction, a body pool made once per round, the "point" sound
 * every 10th apple, separate sounds for a wall hit and a self bite, Y to
 * restart the same mode, and a game-over panel that names the cause.
 *
 * Twin of snake_sprite_full.py (embedded-systems-for-game_console_developer/
 * developer-ii/full_games/snake_sprite_full.py). Style: CLEAN_C.md.
 * Python's lists become arrays with a length; random is bento_random,
 * MicroPython's generator draw for draw. run(fps=1000 / tick_ms) becomes
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

/* The grid, the widget budget and the screen. */
enum {
    CELL = 16, /* ขนาดช่องตาราง (SNAKE_CELL_PX = 16) */
    COLS = BENTO_GAME_WIDTH / CELL,
    ROWS = BENTO_GAME_HEIGHT / CELL,
    START_LEN = 5, /* งูเริ่มยาว 5 ท่อน เท่าเกม C */
    START_COL = 10,
    START_ROW = 10,
    POOL = 23, /* งบ widget: ลำตัวที่เตรียมไว้ล่วงหน้าใน pool */
    MAX_BODY = POOL + 1,
    STRIPE_FIRST_Y = 48, /* แถบหญ้าตัดที่ y = 48, 144, 240, 336 */
    STRIPE_STEP_Y = 96,
    STRIPE_H = 48,
    STRIPE_COUNT = 4,
    HUD_X = 12,
    HUD_Y = 8,
    /* the mode name and three int32_t with their labels */
    HUD_CHARS = 64,
    MODE_COUNT = 3,
    CLASSIC_MODE = 1, /* เริ่มชี้ CLASSIC เหมือนเกม C */
    POINT_EVERY = 10,
    MS_PER_SECOND = 1000
};

/* Colours, 0xRRGGBB: พื้นเขียวอ่อน สลับแถบหญ้าเพิ่งตัด */
enum {
    GRASS_BASE = 0x4CA838,
    GRASS_MOWN = 0x409030
};

/* Why the round ended. */
typedef enum {
    END_NONE = 0,
    END_WALL,
    END_SELF
} end_reason_t;

/* คาบเวลาต่อ 1 ก้าวของงู (ms) ลอกมาจาก s_snake_modes */
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
    /* หัวอยู่หน้าสุด; one more while the head is in, the tail not out */
    cell_t body[MAX_BODY + 1];
    int32_t length;
    bento_game_box_t grass_strips[STRIPE_COUNT];
    bento_game_sprite_t body_segments[POOL];
    bento_game_sprite_t head;
    bento_game_sprite_t food;
    bento_game_text_t hud;
    cell_t food_cell;
    int32_t step_x;
    int32_t step_y;
    int32_t score;
    end_reason_t end_reason;
    bool restart_now; /* ตั้งเป็นจริงเมื่อผู้เล่นกด Y */
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

static bento_real_t cell_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL);
}

/* HUD แบบเดียวกับเกม C: Best โชว์ค่าสดระหว่างเล่น ค่าจริงบันทึกตอน game over */
static bento_status_t update_hud(snake_t *snake)
{
    const int32_t best = bento_game_best(&snake->game, BEST_NAME);
    char text[HUD_CHARS];

    (void)snprintf(
        text, sizeof(text),
        "%s   Score: %" PRId32 "   Best: %" PRId32 "   Len: %" PRId32,
        MODE_NAMES[snake->mode], snake->score,
        (snake->score > best) ? snake->score : best, snake->length);
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

/* สุ่มจนกว่าจะได้ช่องว่างที่ไม่ทับตัวงู */
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
        status = bento_game_sprite_move_to(&snake->game, &snake->food,
                                           cell_px(cell.col),
                                           cell_px(cell.row));
    }
    return status;
}

/* หัวหันตามทิศ (step_x, step_y) เหมือนเกม C */
static const char *head_by_dir(int32_t step_x, int32_t step_y)
{
    const char *name = "snake_head_d";

    if (step_x == 1) {
        name = "snake_head_r";
    } else if (step_x == -1) {
        name = "snake_head_l";
    } else if (step_y == -1) {
        name = "snake_head_u";
    } else {
        /* (0, 1): down */
    }
    return name;
}

/* One pool sprite: shown on body cell index + 1, or hidden. */
static bento_status_t draw_segment(snake_t *snake, int32_t index)
{
    bento_game_sprite_t *segment = &snake->body_segments[index];
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

static bento_status_t draw(snake_t *snake)
{
    int32_t i;
    bento_status_t status = bento_game_sprite_frame(
        &snake->game, &snake->head,
        head_by_dir(snake->step_x, snake->step_y));

    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&snake->game, &snake->head,
                                           cell_px(snake->body[0].col),
                                           cell_px(snake->body[0].row));
    }
    /* body[1..] -> สไปรต์ในพูล */
    for (i = 0; (status == BENTO_OK) && (i < POOL); i++) {
        status = draw_segment(snake, i);
    }
    return status;
}

/* เลี้ยวแบบ "จับจังหวะกด" (กดค้างไม่นับซ้ำ) และกันการกลับหลังหัน */
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

static bool round_ends(snake_t *snake, cell_t next_head)
{
    bool ends = true;

    if ((next_head.col < 0) || (next_head.col >= COLS) ||
        (next_head.row < 0) || (next_head.row >= ROWS)) {
        snake->end_reason = END_WALL;
        bento_game_sfx(&snake->game, "die"); /* เสียงชนกำแพง */
    } else if (in_body(snake, next_head)) {
        snake->end_reason = END_SELF;
        /* กัดตัวเอง: คนละเสียงกับชนกำแพง แบบเกม C */
        bento_game_sfx(&snake->game, "lose");
    } else {
        ends = false;
    }
    return ends;
}

static bento_status_t eat_food(snake_t *snake)
{
    bento_status_t status;

    snake->score++;
    bento_game_sfx(&snake->game, "eat");
    if ((snake->score % POINT_EVERY) == 0) {
        /* แอปเปิลทุกลูกที่ 10 มีเสียงรางวัล แบบเกม C */
        bento_game_sfx(&snake->game, "point");
    }
    status = place_food(snake);
    if (snake->length > MAX_BODY) {
        /* พูลลำตัวเต็ม -> หยุดโต แต่คะแนนนับต่อ */
        snake->length--;
    }
    return status;
}

static bento_status_t walk(snake_t *snake, cell_t next_head)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    for (i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->length++;
    if ((next_head.col == snake->food_cell.col) &&
        (next_head.row == snake->food_cell.row)) {
        status = eat_food(snake);
    } else {
        snake->length--;
    }
    if (status == BENTO_OK) {
        status = draw(snake);
    }
    if (status == BENTO_OK) {
        status = update_hud(snake);
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
        cell_t next_head;

        steer(snake, &keys);
        /* (Back=ออก / Start=พักเกม game loop จัดการให้) */
        next_head = (cell_t){snake->body[0].col + snake->step_x,
                             snake->body[0].row + snake->step_y};
        if (round_ends(snake, next_head)) {
            *out_stop = true;
        } else {
            status = walk(snake, next_head);
        }
    } else {
        /* the status ends the game loop */
    }
    return status;
}

/* พื้นหลังต้องสร้างก่อนเพื่อนเสมอ แล้วแถบหญ้าตัด 4 แถบ */
static bento_status_t draw_field(snake_t *snake)
{
    bento_game_box_t field;
    int32_t i;
    bento_status_t status =
        bento_game_background(&snake->game, &field, GRASS_BASE);

    for (i = 0; (status == BENTO_OK) && (i < STRIPE_COUNT); i++) {
        bento_game_box_init(&snake->grass_strips[i]);
        status = bento_game_box_create(
            &snake->game, &snake->grass_strips[i], (bento_real_t)0,
            (bento_real_t)(STRIPE_FIRST_Y + (i * STRIPE_STEP_Y)),
            (bento_real_t)BENTO_GAME_WIDTH, (bento_real_t)STRIPE_H,
            GRASS_MOWN);
    }
    return status;
}

/* สร้างครั้งเดียวต่อตา นอก update(): pool ลำตัว หัว อาหาร HUD */
static bento_status_t add_sprites(snake_t *snake)
{
    bento_status_t status = bento_game_pool(
        &snake->game, snake->body_segments, POOL, "snake_body");

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
        bento_game_text_init(&snake->hud);
        status = bento_game_text_create(&snake->game, &snake->hud, "",
                                        HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* จัดฉาก (ทำใหม่ทุกตา เพราะเล่นซ้ำจะล้างจอทั้งหมด) */
static bento_status_t build_round(snake_t *snake)
{
    int32_t i;
    bento_status_t status = draw_field(snake);

    for (i = 0; i < START_LEN; i++) {
        /* หัวอยู่หน้าสุด หันขวา */
        snake->body[i] = (cell_t){START_COL - i, START_ROW};
    }
    snake->length = START_LEN;
    if (status == BENTO_OK) {
        status = add_sprites(snake);
    }
    snake->step_x = 1;
    snake->step_y = 0;
    snake->score = 0;
    snake->food_cell = (cell_t){0, 0};
    snake->end_reason = END_NONE;
    snake->restart_now = false;
    if (status == BENTO_OK) {
        status = place_food(snake);
    }
    if (status == BENTO_OK) {
        status = draw(snake);
    }
    if (status == BENTO_OK) {
        status = update_hud(snake);
    }
    return status;
}

/* run(update, fps=1000 / tick_ms): run() sleeps int(1000 / fps). */
static int32_t frame_period_ms(int32_t mode)
{
    const bento_real_t fps =
        (bento_real_t)MS_PER_SECOND / (bento_real_t)MODE_MS[mode];

    return (int32_t)((bento_real_t)MS_PER_SECOND / fps);
}

/* ตานี้จบแล้ว: ลบแถบหญ้า 4 ชิ้นทิ้ง คืนงบให้ป้าย game over ขึ้นครบ */
static bento_status_t show_game_over(snake_t *snake, bool *again)
{
    int32_t i;
    const char *text = (snake->end_reason == END_WALL)
                           ? "GAME OVER - Wall hit"
                           : "GAME OVER - Self collision";

    for (i = 0; i < STRIPE_COUNT; i++) {
        bento_game_box_delete(&snake->game, &snake->grass_strips[i]);
    }
    /* Start = เล่นใหม่ (วนไปเลือกโหมด), Back = ออก */
    return bento_game_over(&snake->game, &snake->score, BEST_NAME, text,
                           again);
}

/* One round and what the player chose after it. */
static bento_status_t play_round(snake_t *snake, bool *playing,
                                 bool *restart_same)
{
    bool finished = false;
    bento_status_t status = build_round(snake);

    if (status == BENTO_OK) {
        status = bento_game_run_period(&snake->game, update, NULL,
                                       frame_period_ms(snake->mode),
                                       &finished);
    }
    if ((status == BENTO_OK) && !finished) {
        /* Back กลางเกม: คะแนนดี ๆ ไม่หาย (save_best answers the best) */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        *playing = false;
    } else if ((status == BENTO_OK) && snake->restart_now) {
        /* Y = วนไปเริ่มโหมดเดิมใหม่ทันที (save_best answers the best) */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        status = bento_game_replay(&snake->game);
        *restart_same = true;
    } else if (status == BENTO_OK) {
        status = show_game_over(snake, playing);
    } else {
        /* the status ends the game */
    }
    return status;
}

/* เลือกความยากก่อนเริ่มทุกตา; BACK จากหน้าเมนู = เลิกเล่น */
static bento_status_t pick_mode(snake_t *snake, bool *playing)
{
    int32_t picked = -1;
    const bento_status_t status =
        bento_game_menu(&snake->game, MODE_NAMES, MODE_COUNT, NULL,
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
    snake_t *snake = &g_snake;
    bool quit = false;
    bento_status_t status;

    (void)memset(snake, 0, sizeof(*snake));
    status = bento_random_init(&snake->random);
    if (status == BENTO_OK) {
        bento_game_init(&snake->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        /* หน้าเริ่ม: Start=เล่น Back=ออก */
        status = bento_game_title_melody(&snake->game, "SNAKE", NULL,
                                         NULL, 0U, &quit);
        /* title() calls start(), whose joystick.init() prints this line */
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
