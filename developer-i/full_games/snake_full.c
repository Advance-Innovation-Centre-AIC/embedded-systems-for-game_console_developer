/*
 * snake_full.c -- เกมงูคลาสสิกฉบับเต็ม (สีแบบ Game Boy)
 *
 * ต้นแบบที่นักเรียนสร้างตามทีละขั้นใน Block B ตรงกับเกม C บนบอร์ด:
 * หน้าเริ่ม "SNAKE" พร้อมทำนอง แล้วหน้าเลือกโหมด RELAXED / CLASSIC /
 * TURBO (ต่างกันที่จังหวะก้าว) สนามหญ้าลายตัดหญ้า HUD บอกโหมด คะแนน
 * Best และความยาว เลี้ยวด้วย D-pad แบบนับเฉพาะจังหวะกด กินแอปเปิลมี
 * เสียง ทุกลูกที่ 10 มีเสียงเหรียญ ชนกำแพงกับกัดตัวเองคนละเสียง
 * ปุ่ม Y เริ่มตาใหม่โหมดเดิมทันที Start = พัก / เล่นใหม่ Back = เลิกเล่น
 *
 * The reference build students rebuild step by step in Block B,
 * faithful to the on-board C game: the title "SNAKE" with its tune,
 * then the RELAXED / CLASSIC / TURBO mode picker (the modes differ in
 * the step time), a mown-grass field, a HUD with the mode, the score,
 * the best score and the length, turns on the press of the D-pad, an
 * eat sound and a coin sound every 10th apple, and different sounds
 * for a wall and for a bite of its own body. Y restarts the same mode
 * at once, Start pauses or plays again, Back ends the game.
 *
 * Twin of snake_full.py
 * (embedded-systems-for-game_console_developer/developer-i/full_games/snake_full.py),
 * from the library twin full-snake (py/full-snake.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the program's state is one snake_t; it is 512 bytes or more, so
 *     it lives at file scope (C9) and update() reads it there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - Python's lists are fixed arrays of the size Python reaches: the
 *     body holds 19 cells, 20 for the moment between insert and pop;
 *     the box lists hold indices into one array of 18 body boxes;
 *   - run(update, fps=1000 / ms) sleeps int(1000 / fps) per frame;
 *     C works out that period in the board's float width and passes
 *     it to bento_game_run_period();
 *   - the endless `while True` with its break and continue is a loop
 *     on `playing`, so every function has one exit;
 *   - a call that raises in MicroPython returns its status; title()'s
 *     Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"
#include "bento_time.h"

/* The field, counted in cells of 16 px (SNAKE_CELL_PX ของเกม C). */
enum {
    CELL_PX = 16,
    BOX_PX = CELL_PX - 2, /* a 2 px gap between neighbouring cells */
    COLS = BENTO_GAME_WIDTH / CELL_PX,  /* 49 คอลัมน์ */
    ROWS = BENTO_GAME_HEIGHT / CELL_PX, /* 24 แถว (เท่าเกม C) */
    STRIPE_H = 48,                      /* ลายหญ้ากว้าง 48 px */
    STRIPE_ROWS = 8,
    HIDDEN_PX = -100, /* where a spare box waits, off the screen */
    HUD_X = 10,
    HUD_Y = 8,
    /* the mode, three times 4 spaces and a label, any int32_t, NUL */
    HUD_BYTES = 80
};

/* The snake. The screen holds 32 widgets: 8 for the field, 4 for the
 * game over panel, so the head and 18 body boxes are the most. */
enum {
    MAX_LEN = 19,
    BODY_CAP = MAX_LEN + 1, /* one more between insert and pop */
    BOX_COUNT = MAX_LEN - 1,
    START_LEN = 5,
    START_COL = 10, /* จุดเกิดเดียวกับเกม C */
    START_ROW = 10
};

/* The modes, the sounds and the clock. */
enum {
    MODE_COUNT = 3,
    START_MODE = 1, /* เปิดมาชี้ CLASSIC ก่อน เหมือนเกม C */
    MS_PER_SECOND = 1000,
    COIN_EVERY = 10, /* ทุกลูกที่ 10 มีเสียงเหรียญ */
    COIN_LOW_NOTE = 83,
    COIN_LOW_MS = 60,
    COIN_GAP_MS = 45,
    COIN_HIGH_NOTE = 88,
    COIN_HIGH_MS = 160,
    TONE_VELOCITY = 100 /* tone()'s default velocity */
};

/* Colours, 0xRRGGBB, from the C game. */
enum {
    GRASS = 0x4CA838,      /* สีสนามหญ้า */
    GRASS_MOWN = 0x409030, /* สีแถบหญ้าที่เพิ่งตัด */
    APPLE = 0xD82818       /* แอปเปิลสีแดง */
};

/* The modes' names, read by the picker and by the HUD. */
static const char *const MODE_NAMES[MODE_COUNT] = {"RELAXED", "CLASSIC",
                                                   "TURBO"};
/* The best-score table's name for this game. */
static const char BEST_NAME[] = "snake";

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random_source;
    int32_t mode;
    const char *death_message; /* NULL before the first round ends */
    bool restart_now;          /* Y: the same mode again at once */
    cell_t body[BODY_CAP];     /* head first */
    int32_t body_length;
    bento_game_box_t head;
    bento_game_box_t body_boxes[BOX_COUNT];
    int32_t shown[BOX_COUNT]; /* boxes on the body, neck to tail */
    int32_t shown_count;
    int32_t spare[BOX_COUNT]; /* hidden boxes the body grows into */
    int32_t spare_count;
    bento_game_box_t food;
    cell_t food_cell;
    bento_game_text_t hud;
    int32_t score;
    cell_t step; /* one cell per frame, to the right at first */
} snake_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern snake_t g_snake;
snake_t g_snake;

/* One Emulator build serves both board models, so ask the board for
 * its name; a board that cannot say keeps the build target's answer.
 */
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

/* The PSoC Edge kits' joystick.init() prints this line when start()
 * opens the pad; the reTerminal's prints nothing. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* A sleep that turns the IDE's Stop into BENTO_E_CANCELLED. */
static bento_status_t pause_ms(uint32_t ms)
{
    bento_status_t status = bento_time_sleep_ms(ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* The screen position of a column or a row. */
static bento_real_t cell_px(int32_t index)
{
    return (bento_real_t)(index * CELL_PX);
}

/* A body-sized box on a cell. */
static bento_status_t add_cell_box(snake_t *snake,
                                   bento_game_box_t *box, cell_t cell,
                                   uint32_t color)
{
    return bento_game_box_create(
        &snake->game, box, cell_px(cell.col), cell_px(cell.row),
        (bento_real_t)BOX_PX, (bento_real_t)BOX_PX, color);
}

/* Is the cell on the snake's body? */
static bool in_body(const snake_t *snake, cell_t cell)
{
    bool found = false;
    int32_t i;

    for (i = 0; (i < snake->body_length) && !found; i++) {
        found = (snake->body[i].col == cell.col) &&
                (snake->body[i].row == cell.row);
    }
    return found;
}

/* Is the cell inside the field? */
static bool inside_field(cell_t cell)
{
    return (0 <= cell.col) && (cell.col < COLS) && (0 <= cell.row) &&
           (cell.row < ROWS);
}

/* เกม C มีเสียงเหรียญ "cha-ching" ทุกแอปเปิลลูกที่ 10 (ไฟล์ PCM ที่
 * Python เรียกตรง ๆ ไม่ได้) เลยแต่งโน้ต 2 ตัวให้ได้อารมณ์เดียวกัน */
static bento_status_t coin_sound(snake_t *snake)
{
    bento_status_t status;

    bento_game_tone(&snake->game, COIN_LOW_NOTE,
                    BENTO_GAME_WAVE_DEFAULT, TONE_VELOCITY,
                    COIN_LOW_MS);
    status = pause_ms(COIN_GAP_MS);
    if (status == BENTO_OK) {
        bento_game_tone(&snake->game, COIN_HIGH_NOTE,
                        BENTO_GAME_WAVE_DEFAULT, TONE_VELOCITY,
                        COIN_HIGH_MS);
    }
    return status;
}

/* The HUD. Best moves live during play, as in the C game. */
static bento_status_t show_hud(snake_t *snake)
{
    char line[HUD_BYTES];
    int32_t best = bento_game_best(&snake->game, BEST_NAME);

    if (snake->score > best) {
        best = snake->score;
    }
    (void)snprintf(line, sizeof(line),
                   "%s    Score: %" PRId32 "    Best: %" PRId32
                   "    Len: %" PRId32,
                   MODE_NAMES[snake->mode], snake->score, best,
                   snake->body_length);
    return bento_game_text_set(&snake->game, &snake->hud, line);
}

/* สุ่มช่องไปเรื่อย ๆ จนกว่าจะเจอช่องที่งูไม่ได้ทับอยู่ */
static bento_status_t place_food(snake_t *snake)
{
    bento_status_t status = BENTO_OK;
    bool placed = false;

    while ((status == BENTO_OK) && !placed) {
        cell_t cell = {0, 0};

        status = bento_random_randint(&snake->random_source, 0,
                                      COLS - 1, &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&snake->random_source, 0,
                                          ROWS - 1, &cell.row);
        }
        if ((status == BENTO_OK) && !in_body(snake, cell)) {
            snake->food_cell = cell;
            status = bento_game_box_move_to(&snake->game, &snake->food,
                                            cell_px(cell.col),
                                            cell_px(cell.row));
            placed = true;
        }
    }
    return status;
}

/* The grass and its mown stripes on every other band of 48 px. */
static bento_status_t add_field(snake_t *snake)
{
    bento_game_t *game = &snake->game;
    bento_game_box_t ground;
    bento_game_box_t stripe;
    int32_t row;
    bento_status_t status = bento_game_background(game, &ground, GRASS);

    for (row = 1; (row < STRIPE_ROWS) && (status == BENTO_OK);
         row += 2) {
        status = bento_game_box_create(
            game, &stripe, (bento_real_t)0,
            (bento_real_t)(row * STRIPE_H), (bento_real_t)game->width,
            (bento_real_t)STRIPE_H, GRASS_MOWN);
    }
    return status;
}

/* The snake of a new round: its cells, the head, the boxes on the
 * body and the hidden spares (the C game makes them all at once too).
 */
static bento_status_t add_snake(snake_t *snake)
{
    int32_t i;
    bento_status_t status;

    for (i = 0; i < START_LEN; i++) {
        snake->body[i].col = START_COL - i;
        snake->body[i].row = START_ROW;
    }
    snake->body_length = START_LEN;
    status = add_cell_box(snake, &snake->head, snake->body[0],
                          BENTO_GAME_GB_DARKEST);
    snake->shown_count = 0;
    for (i = 1; (i < snake->body_length) && (status == BENTO_OK); i++) {
        const int32_t box = snake->shown_count;

        status = add_cell_box(snake, &snake->body_boxes[box],
                              snake->body[i], BENTO_GAME_GB_DARK);
        snake->shown[box] = box;
        snake->shown_count += 1;
    }
    snake->spare_count = 0;
    for (i = snake->body_length; (i < MAX_LEN) && (status == BENTO_OK);
         i++) {
        const int32_t box = snake->shown_count + snake->spare_count;

        status = bento_game_box_create(
            &snake->game, &snake->body_boxes[box],
            (bento_real_t)HIDDEN_PX, (bento_real_t)HIDDEN_PX,
            (bento_real_t)BOX_PX, (bento_real_t)BOX_PX,
            BENTO_GAME_GB_DARK);
        if (status == BENTO_OK) {
            status = bento_game_box_hide(&snake->game,
                                         &snake->body_boxes[box]);
        }
        snake->spare[snake->spare_count] = box;
        snake->spare_count += 1;
    }
    return status;
}

/* จัดฉาก (ต้องสร้างใหม่ทุกตา เพราะ replay ล้างจอทั้งหมด), then the
 * round's own state. */
static bento_status_t build_round(snake_t *snake)
{
    const cell_t corner = {0, 0};
    const cell_t to_the_right = {1, 0};
    bento_status_t status = add_field(snake);

    if (status == BENTO_OK) {
        status = add_snake(snake);
    }
    if (status == BENTO_OK) {
        status = add_cell_box(snake, &snake->food, corner, APPLE);
    }
    snake->food_cell = corner;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&snake->game, &snake->hud, "",
                                        HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    snake->score = 0;
    snake->step = to_the_right;
    snake->death_message = "GAME OVER";
    snake->restart_now = false;
    return status;
}

/* เลี้ยวแบบจับ "จังหวะกด": every key is read first, then the first in
 * order wins, and a turn back onto itself is refused. */
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
    const cell_t step = snake->step;

    if (up && (step.row == 0)) {
        snake->step.col = 0;
        snake->step.row = -1;
    } else if (down && (step.row == 0)) {
        snake->step.col = 0;
        snake->step.row = 1;
    } else if (left && (step.col == 0)) {
        snake->step.col = -1;
        snake->step.row = 0;
    } else if (right && (step.col == 0)) {
        snake->step.col = 1;
        snake->step.row = 0;
    } else {
        /* no turn this frame */
    }
}

/* The new head goes in front of the body. */
static void insert_head(snake_t *snake, cell_t next_head)
{
    int32_t i;

    for (i = snake->body_length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->body_length += 1;
}

/* On the apple: a point, its sound, a longer body while there are
 * boxes left (the score still counts after that), a new apple. */
static bento_status_t eat_food(snake_t *snake, cell_t next_head,
                               bool *out_grew)
{
    bento_status_t status = BENTO_OK;

    if ((next_head.col == snake->food_cell.col) &&
        (next_head.row == snake->food_cell.row)) {
        snake->score += 1;
        bento_game_sfx(&snake->game, "eat");
        if ((snake->score % COIN_EVERY) == 0) {
            status = coin_sound(snake);
        }
        if ((status == BENTO_OK) && (snake->body_length <= MAX_LEN)) {
            *out_grew = true;
        }
        if (status == BENTO_OK) {
            status = place_food(snake);
        }
    }
    return status;
}

/* The tail box moves onto the old head and becomes the neck (or a
 * spare box does, when the body grew), so only two boxes move. */
static bento_status_t move_boxes(snake_t *snake, cell_t old_head,
                                 cell_t next_head, bool grew)
{
    int32_t box = 0;
    int32_t i;
    bento_status_t status = bento_game_box_move_to(
        &snake->game, &snake->head, cell_px(next_head.col),
        cell_px(next_head.row));

    if (status == BENTO_OK) {
        if (grew) {
            snake->spare_count -= 1;
            box = snake->spare[snake->spare_count];
        } else {
            snake->shown_count -= 1;
            box = snake->shown[snake->shown_count];
        }
        status = bento_game_box_move_to(
            &snake->game, &snake->body_boxes[box],
            cell_px(old_head.col), cell_px(old_head.row));
    }
    if ((status == BENTO_OK) && grew) {
        status =
            bento_game_box_show(&snake->game, &snake->body_boxes[box]);
    }
    if (status == BENTO_OK) {
        for (i = snake->shown_count; i > 0; i--) {
            snake->shown[i] = snake->shown[i - 1];
        }
        snake->shown[0] = box;
        snake->shown_count += 1;
    }
    return status;
}

/* The body moves one cell: the new head in front, maybe the apple,
 * the tail off unless it grew, then the HUD and the boxes. */
static bento_status_t advance(snake_t *snake, cell_t old_head,
                              cell_t next_head)
{
    bool grew = false;
    bento_status_t status;

    insert_head(snake, next_head);
    status = eat_food(snake, next_head, &grew);
    if ((status == BENTO_OK) && !grew) {
        snake->body_length -= 1;
    }
    if (status == BENTO_OK) {
        status = show_hud(snake);
    }
    if (status == BENTO_OK) {
        status = move_boxes(snake, old_head, next_head, grew);
    }
    return status;
}

/* One step: a wall or a bite ends the round (checked in the C game's
 * order), otherwise the body advances. */
static bento_status_t step_snake(snake_t *snake, bool *out_stop)
{
    const cell_t old_head = snake->body[0];
    const cell_t next_head = {old_head.col + snake->step.col,
                              old_head.row + snake->step.row};
    bento_status_t status = BENTO_OK;

    if (!inside_field(next_head)) {
        bento_game_sfx(&snake->game, "die");
        snake->death_message = "Wall hit";
        *out_stop = true;
    } else if (in_body(snake, next_head)) {
        bento_game_sfx(&snake->game, "lose"); /* คนละเสียงกับชนกำแพง */
        snake->death_message = "Self collision";
        *out_stop = true;
    } else {
        status = advance(snake, old_head, next_head);
    }
    return status;
}

/* One frame: Y restarts the round, otherwise steer and step. */
static bento_status_t update(void *user, bool *out_stop)
{
    snake_t *snake = &g_snake;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_snake: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&snake->game, &keys);
    /* Y = เริ่มตาใหม่โหมดเดิมทันที (แบบเกม C) */
    if (bento_game_pressed_once(&snake->game, BENTO_GAME_KEY_Y,
                                &keys)) {
        snake->restart_now = true;
        *out_stop = true;
    } else {
        steer(snake, &keys);
        status = step_snake(snake, out_stop);
    }
    return status;
}

/* The mode picker; its heading says why the last round ended. Back
 * there ends the game (เหมือนเกม C). */
static bento_status_t pick_mode(snake_t *snake, bool *out_playing)
{
    const char *heading = (snake->death_message != NULL)
                              ? snake->death_message
                              : "SELECT MODE";
    int32_t picked = 0;
    bento_status_t status =
        bento_game_menu(&snake->game, MODE_NAMES, MODE_COUNT, heading,
                        snake->mode, &picked);

    if (status == BENTO_OK) {
        if (picked < 0) {
            *out_playing = false;
        } else {
            snake->mode = picked;
        }
    }
    return status;
}

/* run(update, fps=1000 / ms): run() sleeps int(1000 / fps) per frame,
 * worked out in the board's float as MicroPython does. */
static int32_t frame_period_ms(int32_t mode)
{
    /* the step time of RELAXED, CLASSIC, TURBO (ตาราง s_snake_modes) */
    static const int32_t step_ms[MODE_COUNT] = {150, 115, 70};
    const bento_real_t second_ms = (bento_real_t)MS_PER_SECOND;
    const bento_real_t fps = second_ms / (bento_real_t)step_ms[mode];

    return (int32_t)(second_ms / fps);
}

/* After the loop: Back in play keeps the score and ends the game, Y
 * plays the same mode again, a lost round shows game over, where
 * Start plays again and Back ends the game. */
static bento_status_t end_round(snake_t *snake, bool finished,
                                bool *out_playing, bool *out_restart)
{
    bool again = false;
    bento_status_t status = BENTO_OK;

    if (!finished) {
        /* save_best answers the best score, not a status */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        *out_playing = false;
    } else if (snake->restart_now) {
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        status = bento_game_replay(&snake->game);
        *out_restart = true;
    } else {
        status = bento_game_over(&snake->game, &snake->score, BEST_NAME,
                                 snake->death_message, &again);
        *out_playing = again;
    }
    return status;
}

/* One round: the field, an apple and the HUD, the frames, the end. */
static bento_status_t play_round(snake_t *snake, bool *out_playing,
                                 bool *out_restart)
{
    bool finished = false;
    bento_status_t status = build_round(snake);

    *out_restart = false;
    if (status == BENTO_OK) {
        status = place_food(snake);
    }
    if (status == BENTO_OK) {
        status = show_hud(snake);
    }
    /* Start = พักเกม, Back = ออก: game.run() จัดการให้ */
    if (status == BENTO_OK) {
        status = bento_game_run_period(&snake->game, update, NULL,
                                       frame_period_ms(snake->mode),
                                       &finished);
    }
    if (status == BENTO_OK) {
        status = end_round(snake, finished, out_playing, out_restart);
    }
    return status;
}

/* Rounds until the player stops; Y skips the picker. */
static bento_status_t play_rounds(snake_t *snake)
{
    bool playing = true;
    bool restart_same = false; /* Y: the same mode, no picker */
    bento_status_t status = BENTO_OK;

    snake->mode = START_MODE;
    snake->death_message = NULL;
    while ((status == BENTO_OK) && playing) {
        if (!restart_same) {
            status = pick_mode(snake, &playing);
        }
        if ((status == BENTO_OK) && playing) {
            status = play_round(snake, &playing, &restart_same);
        }
    }
    return status;
}

int main(void)
{
    snake_t *snake = &g_snake;
    bento_ui_t *ui = bento_runtime_get_ui();
    bool quit = false;
    bento_status_t status;

    (void)memset(snake, 0, sizeof(*snake));
    bento_game_init(&snake->game, ui, bento_runtime_is_running);
    status = bento_random_init(&snake->random_source);
    if (status == BENTO_OK) {
        status = bento_game_title_melody(&snake->game, "SNAKE", NULL,
                                         NULL, 0U, &quit);
        report_joystick(&snake->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = play_rounds(snake);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
