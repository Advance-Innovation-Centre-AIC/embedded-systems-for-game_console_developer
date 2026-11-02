/*
 * snake_sprite_full.c -- Snake ฉบับเต็ม วาดด้วยสไปรต์ C จริงของบอร์ด (spike-b)
 *
 * Twin of snake_sprite_full.py
 * (embedded-systems-for-game_console_developer/developer-i/full_games/snake_sprite_full.py).
 *
 * เกมงูครบวงจรแบบเกม C ในเครื่อง (page_game_snake.c): บนฮาร์ดแวร์ ตัวงู หัวงู
 * และแอปเปิลคือพิกเซลชุดเดียวกับเกม Snake ในเครื่อง ส่วนบนซิมพีซีถอยไปเป็นกล่อง
 * เขียว GB มีสนามหญ้า เมนูเลือกความยาก RELAXED/CLASSIC/TURBO, Score/Best/Len
 * บน HUD, เสียงกิน + เสียงพิเศษทุกแอปเปิลลูกที่ 10, เสียงตายแยกชนกำแพง/กัดตัวเอง
 * ตายแล้วเล่นซ้ำได้ทันที เลี้ยวแบบจับจังหวะกด ปุ่ม Y เริ่มตาใหม่โหมดเดิม
 *
 * The full Snake drawn with the board's own C sprites: on the hardware
 * the body, head and apple are the same pixels as the built-in Snake;
 * on the PC sim they fall back to GB-green boxes. It has a grass field,
 * the RELAXED / CLASSIC / TURBO picker, a Score / Best / Len HUD, an
 * eat sound with a reward on every 10th apple, separate wall and
 * self-bite sounds, play again without a reboot, turns on the press,
 * and Y to restart the same mode at once.
 *
 * C twin of the Example Explorer program full-snakesprite
 * (py/full-snakesprite.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the round's state is one struct at file scope (it holds the
 *     game, too big for the stack), set again at the start of main;
 *   - the body list is a fixed array of cells sized for the most
 *     Python reaches (24 cells, 25 between insert and pop);
 *   - HEAD_BY_DIR is a function, MODES a table of name and period;
 *   - update() is the function the game's run calls each frame; it
 *     reads g_snake (no user pointer), and `return False` sets
 *     *out_stop;
 *   - run(update, fps=1000 / tick_ms) gets Python's frame period,
 *     int(1000 / fps), computed in the board's float width;
 *   - a call that raises in MicroPython returns its status; title()'s
 *     BACK and the IDE's Stop end the program with 0;
 *   - the joystick line joystick.init() prints on the PSoC Edge kits
 *     is printed after title(), as the kits' MicroPython does.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

enum { /* the grid, as page_game_snake.c (SNAKE_CELL_PX = 16) */
    CELL = 16,
    COLS = BENTO_GAME_WIDTH / CELL,  /* 49 */
    ROWS = BENTO_GAME_HEIGHT / CELL, /* 24 */
    START_LEN = 5,
    START_COL = 10,
    START_ROW = 10
};
enum { /* the widget budget: 32 on the screen, all counted */
    POOL = 23,
    MAX_BODY = POOL + 1,
    BODY_CAP = MAX_BODY + 1 /* one more between insert and pop */
};
enum { /* the mown strips at y = 48, 144, 240, 336 */
    STRIP_Y = 48,
    STRIP_H = 48,
    STRIP_STEP = 96,
    STRIPS = 4
};
enum { /* the modes and the score */
    MODE_COUNT = 3,
    START_MODE = 1,
    BONUS_EVERY = 10,
    MS_PER_SECOND = 1000
};
enum { /* the HUD: place, and room for "TURBO" and three int32_t */
    HUD_X = 12,
    HUD_Y = 8,
    HUD_CHARS = 96
};
enum { /* colours, 0xRRGGBB */
    GRASS_BASE = 0x4CA838,
    GRASS_MOWN = 0x409030
};
enum { /* why the round ended */
    END_NONE = 0,
    END_WALL = 1,
    END_SELF = 2
};

typedef struct {
    const char *name;
    int32_t tick_ms;
} speed_mode_t;

/* a slow step leaves time to think; the rest of the game is the same */
static const speed_mode_t MODES[MODE_COUNT] = {
    {"RELAXED", 150}, {"CLASSIC", 115}, {"TURBO", 70}};
static const char BEST_NAME[] = "snake";

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    int32_t mode;
    cell_t body[BODY_CAP];
    int32_t length;
    bento_game_box_t background;
    bento_game_box_t grass_strips[STRIPS];
    int32_t strip_count;
    bento_game_sprite_t segments[POOL];
    bento_game_sprite_t head;
    bento_game_sprite_t food;
    bento_game_text_t hud;
    int32_t step_x;
    int32_t step_y;
    int32_t score;
    cell_t food_cell;
    int32_t end_reason;
    bool restart_now;
} snake_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern snake_game_t g_snake;
snake_game_t g_snake;

static bento_real_t to_px(int32_t cells)
{
    return (bento_real_t)(cells * CELL);
}

/* the head picture faces the way the snake moves, as the C game */
static const char *head_by_direction(int32_t step_x, int32_t step_y)
{
    const char *name = "snake_head_d";

    if (step_x == 1) {
        name = "snake_head_r";
    } else if (step_x == -1) {
        name = "snake_head_l";
    } else if (step_y == -1) {
        name = "snake_head_u";
    } else {
        /* moving down keeps the default */
    }
    return name;
}

static bool in_body(const snake_game_t *snake, cell_t cell)
{
    bool found = false;

    for (int32_t i = 0; (i < snake->length) && !found; i++) {
        found = (snake->body[i].col == cell.col) &&
                (snake->body[i].row == cell.row);
    }
    return found;
}

/* the live best while playing; the real one is saved at game over */
static bento_status_t update_hud(snake_game_t *snake)
{
    const int32_t best = bento_game_best(&snake->game, BEST_NAME);
    char text[HUD_CHARS];

    (void)snprintf(text, sizeof(text),
                   "%s   Score: %d   Best: %d   Len: %d",
                   MODES[snake->mode].name, (int)snake->score,
                   (int)((best > snake->score) ? best : snake->score),
                   (int)snake->length);
    return bento_game_text_set(&snake->game, &snake->hud, text);
}

/* draw again until the cell is off the snake */
static bento_status_t place_food(snake_game_t *snake)
{
    bento_status_t status = BENTO_OK;
    bool placed = false;

    while ((status == BENTO_OK) && !placed) {
        cell_t cell = {0, 0};

        status = bento_random_randint(&snake->random, 0, COLS - 1,
                                      &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&snake->random, 0, ROWS - 1,
                                          &cell.row);
        }
        if ((status == BENTO_OK) && !in_body(snake, cell)) {
            snake->food_cell = cell;
            status = bento_game_sprite_move_to(
                &snake->game, &snake->food, to_px(cell.col),
                to_px(cell.row));
            placed = true;
        }
    }
    return status;
}

/* the rest of the body goes to the pool's sprites, unused ones hide */
static bento_status_t draw_segment(snake_game_t *snake, int32_t i)
{
    bento_game_sprite_t *segment = &snake->segments[i];
    bento_status_t status = BENTO_OK;

    if ((i + 1) < snake->length) {
        const cell_t cell = snake->body[i + 1];

        status = bento_game_sprite_show(&snake->game, segment);
        if (status == BENTO_OK) {
            status = bento_game_sprite_move_to(&snake->game, segment,
                                               to_px(cell.col),
                                               to_px(cell.row));
        }
    } else {
        status = bento_game_sprite_hide(&snake->game, segment);
    }
    return status;
}

static bento_status_t draw(snake_game_t *snake)
{
    bento_status_t status = bento_game_sprite_frame(
        &snake->game, &snake->head,
        head_by_direction(snake->step_x, snake->step_y));

    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&snake->game, &snake->head,
                                           to_px(snake->body[0].col),
                                           to_px(snake->body[0].row));
    }
    for (int32_t i = 0; (i < POOL) && (status == BENTO_OK); i++) {
        status = draw_segment(snake, i);
    }
    return status;
}

/* turn on the press, every direction every frame, and never back
 * onto the body: up and down only while moving sideways, and so on */
static void steer(snake_game_t *snake, const bento_game_keys_t *keys)
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
        /* no turn this frame */
    }
}

static bool outside_field(cell_t cell)
{
    return (cell.col < 0) || (cell.col >= COLS) || (cell.row < 0) ||
           (cell.row >= ROWS);
}

/* the new head goes first; the tail goes unless the snake ate */
static bento_status_t advance(snake_game_t *snake, cell_t next_head)
{
    bento_status_t status = BENTO_OK;

    for (int32_t i = snake->length; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    snake->body[0] = next_head;
    snake->length += 1;
    if ((next_head.col == snake->food_cell.col) &&
        (next_head.row == snake->food_cell.row)) {
        snake->score += 1;
        bento_game_sfx(&snake->game, "eat");
        if ((snake->score % BONUS_EVERY) == 0) {
            bento_game_sfx(&snake->game, "point");
        }
        status = place_food(snake);
        /* a full pool stops the growth; the score goes on */
        if (snake->length > MAX_BODY) {
            snake->length -= 1;
        }
    } else {
        snake->length -= 1;
    }
    if (status == BENTO_OK) {
        status = draw(snake);
    }
    if (status == BENTO_OK) {
        status = update_hud(snake);
    }
    return status;
}

/* one step: Y restarts, then turn, then end at a wall or a bite, or
 * move; BACK and START are the game's run's own */
static bento_status_t update(void *user, bool *out_stop)
{
    snake_game_t *snake = &g_snake;
    bento_game_keys_t keys;
    bento_status_t status = BENTO_OK;

    (void)user; /* the state is g_snake (file scope, C9) */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&snake->game, &keys);
    if (status != BENTO_OK) {
        /* keys() never fails; nothing to do */
    } else if (bento_game_pressed_once(&snake->game, BENTO_GAME_KEY_Y,
                                       &keys)) {
        snake->restart_now = true;
        *out_stop = true;
    } else {
        cell_t next_head = {0, 0};

        steer(snake, &keys);
        next_head.col = snake->body[0].col + snake->step_x;
        next_head.row = snake->body[0].row + snake->step_y;
        if (outside_field(next_head)) {
            snake->end_reason = END_WALL;
            bento_game_sfx(&snake->game, "die");
            *out_stop = true;
        } else if (in_body(snake, next_head)) {
            /* a different sound from the wall, as the C game */
            snake->end_reason = END_SELF;
            bento_game_sfx(&snake->game, "lose");
            *out_stop = true;
        } else {
            status = advance(snake, next_head);
        }
    }
    return status;
}

/* a replay clears the whole screen, so every round builds it again;
 * the background comes first, under everything */
static bento_status_t build_field(snake_game_t *snake)
{
    bento_game_t *game = &snake->game;
    bento_status_t status = bento_game_background(
        game, &snake->background, (uint32_t)GRASS_BASE);

    snake->strip_count = 0;
    for (int32_t y = STRIP_Y;
         (y < (ROWS * CELL)) && (status == BENTO_OK); y += STRIP_STEP) {
        status = bento_game_box_create(
            game, &snake->grass_strips[snake->strip_count],
            (bento_real_t)0, (bento_real_t)y, (bento_real_t)game->width,
            (bento_real_t)STRIP_H, (uint32_t)GRASS_MOWN);
        snake->strip_count += 1;
    }
    return status;
}

/* the head first, facing right; the sprites are made once, outside
 * update() */
static bento_status_t build_snake(snake_game_t *snake)
{
    bento_game_t *game = &snake->game;
    bento_status_t status = BENTO_OK;

    for (int32_t i = 0; i < START_LEN; i++) {
        snake->body[i].col = START_COL - i;
        snake->body[i].row = START_ROW;
    }
    snake->length = START_LEN;
    status = bento_game_pool(game, snake->segments, (uint32_t)POOL,
                             "snake_body");
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(game, &snake->head, "snake_head_r",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status =
            bento_game_sprite_create(game, &snake->food, "snake_food",
                                     (bento_real_t)0, (bento_real_t)0);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &snake->hud, "", HUD_X,
                                        HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t build_round(snake_game_t *snake)
{
    bento_status_t status = build_field(snake);

    if (status == BENTO_OK) {
        status = build_snake(snake);
    }
    snake->step_x = 1;
    snake->step_y = 0;
    snake->score = 0;
    snake->food_cell.col = 0;
    snake->food_cell.row = 0;
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

/* the mode's step time is the frame period: Python's fps is
 * 1000 / tick_ms and run() sleeps int(1000 / fps) */
static int32_t frame_period_ms(int32_t mode)
{
    const bento_real_t fps =
        (bento_real_t)MS_PER_SECOND / (bento_real_t)MODES[mode].tick_ms;

    return (int32_t)((bento_real_t)MS_PER_SECOND / fps);
}

/* the round is over: the four strips go to make room for the game
 * over panel's four widgets */
static bento_status_t end_round(snake_game_t *snake, bool *out_again)
{
    const char *message = (snake->end_reason == END_WALL)
                              ? "GAME OVER - Wall hit"
                              : "GAME OVER - Self collision";

    for (int32_t i = 0; i < snake->strip_count; i++) {
        bento_game_box_delete(&snake->game, &snake->grass_strips[i]);
    }
    return bento_game_over(&snake->game, &snake->score, BEST_NAME,
                           message, out_again);
}

/* one round; *out_playing false ends the game, *out_restart skips the
 * menu */
static bento_status_t play_round(snake_game_t *snake, bool *out_playing,
                                 bool *out_restart)
{
    bool finished = false;
    bento_status_t status = build_round(snake);

    if (status == BENTO_OK) {
        status = bento_game_run_period(&snake->game, update, NULL,
                                       frame_period_ms(snake->mode),
                                       &finished);
    }
    if ((status == BENTO_OK) && !finished) {
        /* Back during play keeps a good score; save_best answers the
         * best score, not a status */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        *out_playing = false;
    } else if ((status == BENTO_OK) && snake->restart_now) {
        /* save_best answers the best score, not a status */
        (void)bento_game_save_best(&snake->game, &snake->score,
                                   BEST_NAME);
        status = bento_game_replay(&snake->game);
        *out_restart = true;
    } else if (status == BENTO_OK) {
        status = end_round(snake, out_playing);
    } else {
        /* the status ends the game */
    }
    return status;
}

/* the difficulty is picked before every round; BACK there ends it */
static bento_status_t pick_mode(snake_game_t *snake, bool *out_playing)
{
    static const char *const options[MODE_COUNT] = {"RELAXED",
                                                    "CLASSIC", "TURBO"};
    int32_t picked = 0;
    const bento_status_t status =
        bento_game_menu(&snake->game, options, (uint32_t)MODE_COUNT,
                        NULL, snake->mode, &picked);

    if ((status == BENTO_OK) && (picked < 0)) {
        *out_playing = false;
    } else if (status == BENTO_OK) {
        snake->mode = picked;
    } else {
        /* the status ends the game */
    }
    return status;
}

static bento_status_t play_rounds(snake_game_t *snake)
{
    bento_status_t status = BENTO_OK;
    bool playing = true;
    bool restart_same = false;

    snake->mode = START_MODE;
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

/* Is this the reTerminal? Asked at run time, by the name the board
 * gives, so one Emulator build serves both of its board models. A
 * board that cannot say keeps the build target's answer. */
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
    snake_game_t *snake = &g_snake;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(snake, 0, sizeof(*snake));
    bento_game_init(&snake->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    status = bento_random_init(&snake->random);
    if (status == BENTO_OK) {
        status = bento_game_title_melody(&snake->game, "SNAKE", NULL,
                                         NULL, 0U, &quit);
        /* joystick.init(), which title() calls, prints this line on
         * the PSoC Edge kits and nothing on the reTerminal */
        if ((snake->game.joystick_init == BENTO_OK) &&
            !on_reterminal()) {
            (void)printf(
                "joystick: USB Host HID initialized on CM55\n");
        }
    }
    if ((status == BENTO_OK) && !quit) {
        status = play_rounds(snake);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
