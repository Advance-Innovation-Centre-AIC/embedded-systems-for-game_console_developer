/*
 * s13_18_kit_props_hud.c -- คาบ 13 (คาบพิเศษ): ชุดของประกอบ + ป้าย HUD สำเร็จรูป
 * ยกมาจากเกมเต็มของจริง
 *
 * ของประกอบ 5 ชิ้น สร้างครบครั้งเดียวแล้วโชว์ทีละชิ้น: กระสุนชุดหมุนเวียน · อาหารที่วางไม่ทับตัวงู
 * · กล่องเมนูแบบเกม C · ป้ายจบเกมพร้อม Best · HUD ที่วาดใหม่เฉพาะตอนเลขเปลี่ยน
 * กด Start แล้วใช้ซ้าย/ขวาเปลี่ยนชิ้น A = ท่าของชิ้นนั้น
 * กระสุน: A ยิง กดรัวจนพูลหมดแล้วยิงไม่ออก · อาหาร: A วางใหม่ · เมนู: UP/DOWN เลื่อน A ยืนยัน
 * จบเกม: คะแนนวิ่งเอง A = จบตา ดู Best · HUD: A ได้แต้ม B เสียชีวิต
 *
 * Five ready-made props from the full games, made once and shown one at a time: a bullet
 * pool (fire with A until the pool is empty), food placed at random but never on the snake,
 * a C-game style menu that does not stop the game, a game-over panel with the Best score,
 * and a HUD that is written only when one of its numbers changes. Left / right picks the
 * prop, A (and B on the HUD) works it.
 *
 * Twin of s13_18_kit_props_hud.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_18_kit_props_hud.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - each piece's dict is a struct, and all of them are one kit_t at file scope (it is large);
 *   - the "all" and "base" widget lists are hide_piece() and show_base(), one case per piece;
 *   - the PIECES table of functions is a switch on the piece number;
 *   - Python's None in the HUD and the info line is a has_last / has_said flag;
 *   - a food is a Sprite or a Box, so food_t keeps both and a flag that says which one it is;
 *   - row._label.color() is bento_ui_set_color() on the label, its status ignored as before.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_random.h"
#include "bento_runtime.h"

/* a*b+c is two roundings in MicroPython: no fused multiply-add */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

/* The kit (change these). */
enum {
    FPS = 30,
    PIECES = 5,
    PIECE_BULLETS = 0,
    PIECE_FOOD = 1,
    PIECE_MENU = 2,
    PIECE_OVER = 3,
    PIECE_HUD = 4,
    BULLETS = 4,     /* make_bullets(4) */
    FOODS = 2,       /* the pixel apple and the red box */
    SNAKE_CELLS = 3, /* a make-believe snake for place_food to avoid */
    MENU_OPTIONS = 3,
    MENU_START = 1, /* the menu opens on NORMAL */
    PANEL_LINES = 3,
    LIVES = 3,
    SCORE_EVERY =
        20, /* the game-over demo scores a point every 20 frames */
    TEXT_CHARS =
        160 /* the longest line: Thai is 3 bytes a letter, two int32_t and the NUL */
};

/* Where things are, in px. */
enum {
    CELL = 16,
    COLS = BENTO_GAME_WIDTH / CELL, /* 49 columns */
    FOOD_FIRST_ROW =
        4, /* rows 4 .. 18: the top and bottom rows stay for the labels */
    FOOD_END_ROW = 19,
    SNAKE_FIRST_COL = 22,
    SNAKE_ROW = 10,
    GUN_HALF_W = 31,
    GUN_Y = 250,
    GUN_NOSE_X = 29, /* the ship is 62 px wide: shots leave its nose */
    GUN_NOSE_Y = 8,
    BULLET_PARK_X = -20,
    BULLET_PARK_Y = -50,
    BULLET_W = 5,
    BULLET_H = 12,
    BULLET_RADIUS = 2,
    BULLET_GONE_Y = -12,
    BULLET_VY = 8, /* a bullet climbs 8 px a frame */
    MENU_BASE_H = 70,
    MENU_ROW_H = 34,
    MENU_HALF_W = 160,
    MENU_W = 320,
    MENU_RADIUS = 10,
    MENU_EDGE_W = 2,
    MENU_HEAD_Y = 14,
    MENU_ROWS_Y = 50,
    MENU_CHAR_PX =
        4, /* about half a letter's width: centres a text by its length */
    MENU_ROW_PAD = 6, /* ">  " and "  <" around the option */
    PANEL_HALF_W = 180,
    PANEL_HALF_H = 70,
    PANEL_W = 360,
    PANEL_H = 140,
    PANEL_TEXT_X = 28,
    PANEL_TEXT_Y = 22,
    PANEL_LINE_H = 36,
    HUD_X = 10,
    HUD_Y = 8,
    LABEL_X = 12,
    CAPTION_UP =
        74, /* the caption and the info line sit above the bottom edge */
    INFO_UP = 50
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_SPACE = 0x10122C,
    COLOR_GRASS = 0x4CA838,
    COLOR_LASER = 0x80F4FF,
    COLOR_APPLE = 0xD82818,
    COLOR_MENU_BG = 0x101A38,
    COLOR_MENU_EDGE = 0x80F4FF,
    COLOR_MENU_DIM = 0x6C7CA8,
    COLOR_PANEL_BG = 0x101A38,
    COLOR_PANEL_EDGE = 0x80F4FF
};

static const char BEST_NAME[] = "kit"; /* this file's Best slot */
static const char MENU_HEADING[] = "SELECT MODE";
static const char *const MENU_NAMES[MENU_OPTIONS] = {"EASY", "NORMAL",
                                                     "HARD"};

typedef struct {
    int32_t col;
    int32_t row;
} cell_t;

typedef struct {
    bento_game_box_t boxes[BULLETS];
    bool on[BULLETS];
    bento_real_t x[BULLETS];
    bento_real_t y[BULLETS];
} bullets_t;

typedef struct {
    bool is_sprite;
    bento_game_sprite_t sprite;
    bento_game_box_t box;
    cell_t cell;
} food_t;

typedef struct {
    food_t foods[FOODS];
    cell_t body[SNAKE_CELLS];
    bento_game_sprite_t segments[SNAKE_CELLS];
    int32_t placed;
} food_piece_t;

typedef struct {
    int32_t selected;
    bento_game_box_t box;
    bento_game_text_t head;
    bento_game_text_t rows[MENU_OPTIONS];
} menu_t;

typedef struct {
    bento_game_text_t text;
    bool has_last;
    int32_t last_score;
    int32_t last_best;
    int32_t last_lives;
    int32_t draws;
} hud_t;

typedef struct {
    bento_game_box_t box;
    bento_game_text_t lines[PANEL_LINES];
} panel_t;

typedef struct {
    hud_t hud;
    panel_t panel;
    int32_t score;
    int32_t frames;
    bool over;
} over_piece_t;

typedef struct {
    hud_t hud;
    int32_t score;
    int32_t lives;
    int32_t frames;
} hud_piece_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_box_t background;
    bento_game_sprite_t gun;
    bullets_t bullets;
    food_piece_t food;
    menu_t menu;
    over_piece_t over;
    hud_piece_t hud;
    bento_game_text_t caption;
    bento_game_text_t info;
    int32_t piece;
    bool has_said; /* false = nothing said yet (Python's None) */
    char said[TEXT_CHARS];
} kit_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern kit_t g_kit;
kit_t g_kit;

static int32_t text_chars(const char *text)
{
    return (int32_t)strlen(text);
}

/* The info line is written only when its message changes. */
static bento_status_t say(kit_t *kit, const char *message)
{
    bento_status_t status = BENTO_OK;

    if (!kit->has_said || (strcmp(kit->said, message) != 0)) {
        status = bento_game_text_set(&kit->game, &kit->info, message);
        (void)snprintf(kit->said, sizeof(kit->said), "%s", message);
        kit->has_said = true;
    }
    return status;
}

static bento_status_t set_background(kit_t *kit, uint32_t color)
{
    return bento_game_box_set_color(&kit->game, &kit->background,
                                    color);
}

/* ---- bullets: a laser pool, made once and hidden ---- */

/* Bullet j leaves the gun at (x, y). */
static bento_status_t launch_bullet(kit_t *kit, int32_t j,
                                    bento_real_t x, bento_real_t y)
{
    bullets_t *bullets = &kit->bullets;
    bento_status_t status;

    bullets->on[j] = true;
    bullets->x[j] = x;
    bullets->y[j] = y;
    status =
        bento_game_box_move_to(&kit->game, &bullets->boxes[j], x, y);
    if (status == BENTO_OK) {
        status = bento_game_box_show(&kit->game, &bullets->boxes[j]);
    }
    bento_game_sfx(&kit->game, "fire");
    return status;
}

/* false when no bullet is free: nothing fires, and no new bullet is made */
static bool fire_bullet(kit_t *kit, bento_real_t x, bento_real_t y,
                        bento_status_t *status)
{
    bool fired = false;
    int32_t j;

    for (j = 0; (j < BULLETS) && !fired; j++) {
        if (!kit->bullets.on[j]) {
            *status = launch_bullet(kit, j, x, y);
            fired = true;
        }
    }
    return fired;
}

/* Bullet j climbs; past the top edge it goes back to the pool. */
static bento_status_t move_bullet(kit_t *kit, int32_t j)
{
    bullets_t *bullets = &kit->bullets;
    bento_status_t status;

    bullets->y[j] -= (bento_real_t)BULLET_VY;
    if (bullets->y[j] < (bento_real_t)BULLET_GONE_Y) {
        bullets->on[j] = false;
        status = bento_game_box_hide(&kit->game, &bullets->boxes[j]);
    } else {
        status = bento_game_box_move_to(&kit->game, &bullets->boxes[j],
                                        bullets->x[j], bullets->y[j]);
    }
    return status;
}

/* Every frame: up they go; past the top edge they go back to the pool. */
static bento_status_t step_bullets(kit_t *kit)
{
    bullets_t *bullets = &kit->bullets;
    bento_status_t status = BENTO_OK;
    int32_t j;

    for (j = 0; (status == BENTO_OK) && (j < BULLETS); j++) {
        if (bullets->on[j]) {
            status = move_bullet(kit, j);
        }
    }
    return status;
}

/* ---- food: placed at random, never on the snake ---- */

static bool on_snake(const food_piece_t *food, cell_t cell)
{
    bool found = false;
    int32_t i;

    for (i = 0; i < SNAKE_CELLS; i++) {
        if ((food->body[i].col == cell.col) &&
            (food->body[i].row == cell.row)) {
            found = true;
        }
    }
    return found;
}

static bento_status_t move_food(kit_t *kit, food_t *food,
                                bento_real_t x, bento_real_t y)
{
    return food->is_sprite
               ? bento_game_sprite_move_to(&kit->game, &food->sprite, x,
                                           y)
               : bento_game_box_move_to(&kit->game, &food->box, x, y);
}

static bento_status_t show_food(kit_t *kit, const food_t *food,
                                bool visible)
{
    bento_status_t status;

    if (food->is_sprite) {
        status =
            visible ? bento_game_sprite_show(&kit->game, &food->sprite)
                    : bento_game_sprite_hide(&kit->game, &food->sprite);
    } else {
        status = visible ? bento_game_box_show(&kit->game, &food->box)
                         : bento_game_box_hide(&kit->game, &food->box);
    }
    return status;
}

/* Draw cells until one is off the snake (rows first_row .. end_row - 1). */
static bento_status_t place_food(kit_t *kit, food_t *food,
                                 int32_t first_row, int32_t end_row)
{
    bento_status_t status = BENTO_OK;
    bool placed = false;

    while ((status == BENTO_OK) && !placed) {
        cell_t cell = {0, 0};

        status =
            bento_random_randint(&kit->random, 0, COLS - 1, &cell.col);
        if (status == BENTO_OK) {
            status = bento_random_randint(&kit->random, first_row,
                                          end_row - 1, &cell.row);
        }
        if ((status == BENTO_OK) && !on_snake(&kit->food, cell)) {
            food->cell = cell;
            status =
                move_food(kit, food, (bento_real_t)(cell.col * CELL),
                          (bento_real_t)(cell.row * CELL));
            placed = true;
        }
    }
    return status;
}

/* ---- the menu: the C game's panel, which does not stop the game ---- */

/* The chosen row is ">  name  <" in light blue, the others grey. */
static bento_status_t paint_menu(kit_t *kit)
{
    menu_t *menu = &kit->menu;
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < MENU_OPTIONS); i++) {
        const bool chosen = (i == menu->selected);

        if (chosen) {
            (void)snprintf(text, sizeof(text), ">  %s  <",
                           MENU_NAMES[i]);
        } else {
            (void)snprintf(text, sizeof(text), "   %s", MENU_NAMES[i]);
        }
        status = bento_game_text_set(&kit->game, &menu->rows[i], text);
        /* the label colour is a try/except in Python, as in game.menu: its refusal is ignored */
        (void)bento_ui_set_color(kit->game.ui, menu->rows[i].id,
                                 chosen ? (uint32_t)COLOR_MENU_EDGE
                                        : (uint32_t)COLOR_MENU_DIM);
    }
    return status;
}

/* up and down are pressed_once answers; the choice wraps round at both ends. */
static bento_status_t step_menu(kit_t *kit, bool up, bool down)
{
    menu_t *menu = &kit->menu;
    bento_status_t status = BENTO_OK;

    if (up || down) {
        if (up) {
            menu->selected =
                (menu->selected + MENU_OPTIONS - 1) % MENU_OPTIONS;
        } else {
            menu->selected = (menu->selected + 1) % MENU_OPTIONS;
        }
        bento_game_sfx(&kit->game, "move");
        status = paint_menu(kit);
    }
    return status;
}

/* ---- the game-over panel with Best, which does not stop the game ---- */

static bento_status_t show_over_panel(kit_t *kit, panel_t *panel,
                                      int32_t score)
{
    /* save first, then show, as game_over does */
    const int32_t best =
        bento_game_save_best(&kit->game, &score, BEST_NAME);
    char text[TEXT_CHARS];
    bento_status_t status =
        bento_game_text_set(&kit->game, &panel->lines[0], "GAME OVER");

    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "Score %d      Best %d",
                       (int)score, (int)best);
        status =
            bento_game_text_set(&kit->game, &panel->lines[1], text);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_set(&kit->game, &panel->lines[2],
                                     "A = เล่นใหม่");
    }
    if (status == BENTO_OK) {
        status = bento_game_box_show(&kit->game, &panel->box);
    }
    return status;
}

static bento_status_t hide_over_panel(kit_t *kit, panel_t *panel)
{
    bento_status_t status =
        bento_game_box_hide(&kit->game, &panel->box);
    int32_t i;

    /* a Text cannot hide: it gets an empty text instead */
    for (i = 0; (status == BENTO_OK) && (i < PANEL_LINES); i++) {
        status = bento_game_text_set(&kit->game, &panel->lines[i], "");
    }
    return status;
}

/* ---- the HUD: written again only when a number changes ---- */

static bento_status_t make_hud(kit_t *kit, hud_t *hud)
{
    bento_game_text_init(&hud->text);
    hud->has_last = false;
    hud->draws = 0;
    return bento_game_text_create(&kit->game, &hud->text, "", HUD_X,
                                  HUD_Y, BENTO_GAME_WHITE);
}

static bento_status_t update_hud(kit_t *kit, hud_t *hud, int32_t score,
                                 int32_t lives)
{
    const int32_t saved_best = bento_game_best(&kit->game, BEST_NAME);
    /* Best follows at once when the score passes it, as in the C game */
    const int32_t best = (saved_best > score) ? saved_best : score;
    bento_status_t status = BENTO_OK;

    if (!hud->has_last || (score != hud->last_score) ||
        (best != hud->last_best) || (lives != hud->last_lives)) {
        char text[TEXT_CHARS];

        hud->has_last = true;
        hud->last_score = score;
        hud->last_best = best;
        hud->last_lives = lives;
        (void)snprintf(text, sizeof(text),
                       "Score: %d    Best: %d    Lives: %d", (int)score,
                       (int)best, (int)lives);
        status = bento_game_text_set(&kit->game, &hud->text, text);
        hud->draws += 1;
    }
    return status;
}

/* ---- each piece: enter, then one step a frame ---- */

static bento_status_t play_bullets(kit_t *kit, bool fire)
{
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;
    int32_t flying = 0;
    int32_t j;

    if (fire &&
        !fire_bullet(kit, kit->gun.x + (bento_real_t)GUN_NOSE_X,
                     kit->gun.y - (bento_real_t)GUN_NOSE_Y, &status)) {
        bento_game_sfx(&kit->game, "deny");
    }
    if (status == BENTO_OK) {
        status = step_bullets(kit);
    }
    for (j = 0; j < BULLETS; j++) {
        flying += kit->bullets.on[j] ? 1 : 0;
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text),
                       "ในอากาศ %d/%d นัด   หมดพูล = ยิงไม่ออก",
                       (int)flying, BULLETS);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t play_food(kit_t *kit, bool place)
{
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;
    int32_t i;

    if (place) {
        for (i = 0; (status == BENTO_OK) && (i < FOODS); i++) {
            status = place_food(kit, &kit->food.foods[i],
                                FOOD_FIRST_ROW, FOOD_END_ROW);
        }
        bento_game_sfx(&kit->game, "eat");
        kit->food.placed += 1;
    }
    if (status == BENTO_OK) {
        (void)snprintf(text, sizeof(text), "วางใหม่ %d ครั้ง  ไม่เคยทับตัวงู",
                       (int)kit->food.placed);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t
play_menu(kit_t *kit, const bento_game_keys_t *keys, bool confirm)
{
    const bool up =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_UP, keys);
    const bool down =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_DOWN, keys);
    bento_status_t status = step_menu(kit, up, down);

    if ((status == BENTO_OK) && confirm) {
        char text[TEXT_CHARS];

        bento_game_sfx(&kit->game, "select");
        (void)snprintf(text, sizeof(text),
                       "เลือก %s แล้ว   ในเกมเต็มคือ mode = picked",
                       MENU_NAMES[kit->menu.selected]);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t play_over(kit_t *kit, bool press)
{
    over_piece_t *over = &kit->over;
    bento_status_t status = BENTO_OK;

    if (press && over->over) {
        /* play again: hide the panel, count from 0 again */
        status = hide_over_panel(kit, &over->panel);
        over->score = 0;
        over->over = false;
        bento_game_sfx(&kit->game, "start");
    } else if (press) {
        /* the round ends: save Best and show the panel (the game itself goes on) */
        status = show_over_panel(kit, &over->panel, over->score);
        over->over = true;
        bento_game_sfx(&kit->game, "gameover");
    } else {
        /* no press this frame */
    }
    over->frames += 1;
    if (!over->over && ((over->frames % SCORE_EVERY) == 0)) {
        over->score += 1; /* make believe that someone is playing */
    }
    if (status == BENTO_OK) {
        status = update_hud(kit, &over->hud, over->score, LIVES);
    }
    if (status == BENTO_OK) {
        status =
            say(kit, over->over ? "ป้ายจบเกม"
                                : "คะแนนวิ่งเอง  A = จบตา แล้วดู Best");
    }
    return status;
}

static bento_status_t
play_hud(kit_t *kit, const bento_game_keys_t *keys, bool point)
{
    hud_piece_t *hud = &kit->hud;
    bento_status_t status;

    if (point) {
        hud->score += 1;
        bento_game_sfx(&kit->game, "hit");
    }
    if (bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_B, keys)) {
        hud->lives -= 1;
        bento_game_sfx(&kit->game, "lose_life");
        if (hud->lives == 0) {
            /* no lives left: save Best, then start again; save_best answers the best score,
             * not a status */
            (void)bento_game_save_best(&kit->game, &hud->score,
                                       BEST_NAME);
            bento_game_sfx(&kit->game, "gameover");
            hud->score = 0;
            hud->lives = LIVES;
        }
    }
    status = update_hud(kit, &hud->hud, hud->score, hud->lives);
    hud->frames += 1;
    if ((status == BENTO_OK) && ((hud->frames % FPS) == 0)) {
        char text[TEXT_CHARS];

        /* this line, too, is written only once a second */
        (void)snprintf(text, sizeof(text),
                       "%d เฟรม  แต่วาด HUD ใหม่ %d ครั้ง",
                       (int)hud->frames, (int)hud->hud.draws);
        status = say(kit, text);
    }
    return status;
}

static bento_status_t enter_piece(kit_t *kit, int32_t piece)
{
    const uint32_t color = (piece == PIECE_FOOD)
                               ? (uint32_t)COLOR_GRASS
                               : (uint32_t)COLOR_SPACE;
    bento_status_t status = set_background(kit, color);
    int32_t j;

    if ((status == BENTO_OK) && (piece == PIECE_BULLETS)) {
        for (j = 0; j < BULLETS; j++) {
            kit->bullets.on[j] = false;
        }
    } else if ((status == BENTO_OK) && (piece == PIECE_FOOD)) {
        kit->food.placed = 0;
        status = play_food(kit, true);
    } else if ((status == BENTO_OK) && (piece == PIECE_MENU)) {
        status = bento_game_text_set(&kit->game, &kit->menu.head,
                                     MENU_HEADING);
        if (status == BENTO_OK) {
            status = paint_menu(kit);
        }
    } else if ((status == BENTO_OK) && (piece == PIECE_OVER)) {
        kit->over.score = 0;
        kit->over.frames = 0;
        kit->over.over = false;
        kit->over.hud.has_last = false;
    } else if (status == BENTO_OK) {
        kit->hud.score = 0;
        kit->hud.lives = LIVES;
        kit->hud.frames = 0;
        kit->hud.hud.has_last = false;
        kit->hud.hud.draws = 0;
    } else {
        /* the status ends the program */
    }
    return status;
}

static bento_status_t
play_piece(kit_t *kit, const bento_game_keys_t *keys, bool press)
{
    bento_status_t status;

    switch (kit->piece) {
    case PIECE_BULLETS:
        status = play_bullets(kit, press);
        break;
    case PIECE_FOOD:
        status = play_food(kit, press);
        break;
    case PIECE_MENU:
        status = play_menu(kit, keys, press);
        break;
    case PIECE_OVER:
        status = play_over(kit, press);
        break;
    default:
        status = play_hud(kit, keys, press);
        break;
    }
    return status;
}

/* ---- showing and hiding a whole piece ("all" and "base") ---- */

static bento_status_t hide_bullets(kit_t *kit)
{
    bento_status_t status =
        bento_game_sprite_hide(&kit->game, &kit->gun);
    int32_t j;

    for (j = 0; (status == BENTO_OK) && (j < BULLETS); j++) {
        status =
            bento_game_box_hide(&kit->game, &kit->bullets.boxes[j]);
    }
    return status;
}

static bento_status_t show_food_piece(kit_t *kit, bool visible)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < FOODS); i++) {
        status = show_food(kit, &kit->food.foods[i], visible);
    }
    for (i = 0; (status == BENTO_OK) && (i < SNAKE_CELLS); i++) {
        status = visible
                     ? bento_game_sprite_show(&kit->game,
                                              &kit->food.segments[i])
                     : bento_game_sprite_hide(&kit->game,
                                              &kit->food.segments[i]);
    }
    return status;
}

static bento_status_t hide_menu(kit_t *kit)
{
    bento_status_t status =
        bento_game_box_hide(&kit->game, &kit->menu.box);
    int32_t i;

    /* a Text cannot hide: it gets an empty text instead */
    if (status == BENTO_OK) {
        status = bento_game_text_set(&kit->game, &kit->menu.head, "");
    }
    for (i = 0; (status == BENTO_OK) && (i < MENU_OPTIONS); i++) {
        status =
            bento_game_text_set(&kit->game, &kit->menu.rows[i], "");
    }
    return status;
}

static bento_status_t hide_piece(kit_t *kit, int32_t piece)
{
    bento_status_t status;

    switch (piece) {
    case PIECE_BULLETS:
        status = hide_bullets(kit);
        break;
    case PIECE_FOOD:
        status = show_food_piece(kit, false);
        break;
    case PIECE_MENU:
        status = hide_menu(kit);
        break;
    case PIECE_OVER:
        status =
            bento_game_text_set(&kit->game, &kit->over.hud.text, "");
        if (status == BENTO_OK) {
            status = hide_over_panel(kit, &kit->over.panel);
        }
        break;
    default:
        status =
            bento_game_text_set(&kit->game, &kit->hud.hud.text, "");
        break;
    }
    return status;
}

/* The pieces a piece shows as it opens; the rest appear as it plays. */
static bento_status_t show_base(kit_t *kit, int32_t piece)
{
    bento_status_t status = BENTO_OK;

    if (piece == PIECE_BULLETS) {
        status = bento_game_sprite_show(&kit->game, &kit->gun);
    } else if (piece == PIECE_FOOD) {
        status = show_food_piece(kit, true);
    } else if (piece == PIECE_MENU) {
        status = bento_game_box_show(&kit->game, &kit->menu.box);
    } else {
        /* the game-over panel and the HUD start with nothing on show */
    }
    return status;
}

/* Hide the old piece, show the new one, then start its state again. */
static bento_status_t open_piece(kit_t *kit, int32_t piece)
{
    static const char *const names[PIECES] = {
        "BULLETS", "FOOD", "MENU", "GAME OVER + BEST", "HUD"};
    static const char *const hints[PIECES] = {
        "A = ยิง", "A = วางใหม่", "UP/DOWN + A", "A = จบตา / เล่นใหม่",
        "A = ได้แต้ม  B = เสียชีวิต"};
    bento_status_t status = hide_piece(kit, kit->piece);

    if (status == BENTO_OK) {
        kit->piece = piece;
        status = say(kit, "");
    }
    if (status == BENTO_OK) {
        status = show_base(kit, piece);
    }
    if (status == BENTO_OK) {
        status = enter_piece(kit, piece);
    }
    if (status == BENTO_OK) {
        char text[TEXT_CHARS];

        (void)snprintf(text, sizeof(text), "%d/%d  %s   %s",
                       (int)(piece + 1), PIECES, names[piece],
                       hints[piece]);
        status = bento_game_text_set(&kit->game, &kit->caption, text);
    }
    return status;
}

/* ---- the screen: every piece made once ---- */

static bento_status_t add_bullets(kit_t *kit)
{
    bento_game_t *game = &kit->game;
    const int32_t gun_left =
        (game->width / 2) - GUN_HALF_W; /* floor division: whole px */
    bento_status_t status = bento_game_sprite_create(
        game, &kit->gun, "ship", (bento_real_t)gun_left,
        (bento_real_t)GUN_Y);
    int32_t j;

    /* a 5 x 12 bar, white edge, round corners: made once, then hidden */
    for (j = 0; (status == BENTO_OK) && (j < BULLETS); j++) {
        bento_game_box_t *box = &kit->bullets.boxes[j];

        bento_game_box_init(box);
        status = bento_game_box_create_styled(
            game, box, (bento_real_t)BULLET_PARK_X,
            (bento_real_t)BULLET_PARK_Y, (bento_real_t)BULLET_W,
            (bento_real_t)BULLET_H, COLOR_LASER,
            (int32_t)BENTO_GAME_WHITE, BULLET_RADIUS, 0);
        if (status == BENTO_OK) {
            status = bento_game_box_hide(game, box);
        }
    }
    return status;
}

static bento_status_t add_food(kit_t *kit)
{
    bento_game_t *game = &kit->game;
    food_piece_t *food = &kit->food;
    bento_status_t status;
    int32_t i;

    for (i = 0; i < SNAKE_CELLS; i++) {
        food->body[i] = (cell_t){SNAKE_FIRST_COL + i, SNAKE_ROW};
    }
    /* the C game's pixel apple, then snake_full's red box */
    food->foods[0].is_sprite = true;
    status = bento_game_sprite_create(game, &food->foods[0].sprite,
                                      "snake_food", (bento_real_t)0,
                                      (bento_real_t)0);
    if (status == BENTO_OK) {
        food->foods[1].is_sprite = false;
        bento_game_box_init(&food->foods[1].box);
        status = bento_game_box_create(
            game, &food->foods[1].box, (bento_real_t)0, (bento_real_t)0,
            (bento_real_t)(CELL - 2), (bento_real_t)(CELL - 2),
            COLOR_APPLE);
    }
    for (i = 0; (status == BENTO_OK) && (i < SNAKE_CELLS); i++) {
        status = bento_game_sprite_create(
            game, &food->segments[i], "snake_body",
            (bento_real_t)(food->body[i].col * CELL),
            (bento_real_t)(food->body[i].row * CELL));
    }
    return status;
}

static bento_status_t add_menu(kit_t *kit)
{
    bento_game_t *game = &kit->game;
    menu_t *menu = &kit->menu;
    const int32_t panel_h = MENU_BASE_H + (MENU_ROW_H * MENU_OPTIONS);
    const int32_t top = (game->height / 2) - (panel_h / 2);
    const int32_t middle = game->width / 2;
    bento_status_t status;
    int32_t i;

    menu->selected = MENU_START;
    bento_game_box_init(&menu->box);
    bento_game_text_init(&menu->head);
    status = bento_game_box_create_styled(
        game, &menu->box, (bento_real_t)(middle - MENU_HALF_W),
        (bento_real_t)top, (bento_real_t)MENU_W, (bento_real_t)panel_h,
        COLOR_MENU_BG, COLOR_MENU_EDGE, MENU_RADIUS, MENU_EDGE_W);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &menu->head, MENU_HEADING,
            middle - (text_chars(MENU_HEADING) * MENU_CHAR_PX),
            top + MENU_HEAD_Y, BENTO_GAME_WHITE);
    }
    for (i = 0; (status == BENTO_OK) && (i < MENU_OPTIONS); i++) {
        const int32_t width =
            (text_chars(MENU_NAMES[i]) + MENU_ROW_PAD) * MENU_CHAR_PX;

        bento_game_text_init(&menu->rows[i]);
        status = bento_game_text_create(
            game, &menu->rows[i], "", middle - width,
            top + MENU_ROWS_Y + (i * MENU_ROW_H), BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = paint_menu(kit);
    }
    return status;
}

/* The game-over piece: its own HUD, then the 360 x 140 panel, hidden. */
static bento_status_t add_over(kit_t *kit)
{
    static const uint32_t line_colors[PANEL_LINES] = {
        BENTO_GAME_CYAN, BENTO_GAME_WHITE, BENTO_GAME_GB_LIGHT};
    bento_game_t *game = &kit->game;
    panel_t *panel = &kit->over.panel;
    const int32_t x = (game->width / 2) - PANEL_HALF_W;
    const int32_t y = (game->height / 2) - PANEL_HALF_H;
    bento_status_t status = make_hud(kit, &kit->over.hud);
    int32_t i;

    bento_game_box_init(&panel->box);
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &panel->box, (bento_real_t)x, (bento_real_t)y,
            (bento_real_t)PANEL_W, (bento_real_t)PANEL_H,
            COLOR_PANEL_BG, COLOR_PANEL_EDGE, MENU_RADIUS, MENU_EDGE_W);
    }
    for (i = 0; (status == BENTO_OK) && (i < PANEL_LINES); i++) {
        bento_game_text_init(&panel->lines[i]);
        status = bento_game_text_create(
            game, &panel->lines[i], "", x + PANEL_TEXT_X,
            y + PANEL_TEXT_Y + (i * PANEL_LINE_H), line_colors[i]);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_hide(game, &panel->box);
    }
    return status;
}

static bento_status_t add_labels(kit_t *kit)
{
    bento_game_t *game = &kit->game;
    bento_status_t status = make_hud(kit, &kit->hud.hud);

    /* made last, so the bottom labels sit on top of everything */
    bento_game_text_init(&kit->caption);
    bento_game_text_init(&kit->info);
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &kit->caption, "", LABEL_X, game->height - CAPTION_UP,
            BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &kit->info, "", LABEL_X,
                                        game->height - INFO_UP,
                                        BENTO_GAME_CYAN);
    }
    return status;
}

static bento_status_t build_screen(kit_t *kit)
{
    /* one background serves every piece; only its colour changes */
    bento_status_t status = bento_game_background(
        &kit->game, &kit->background, COLOR_SPACE);
    int32_t piece;

    if (status == BENTO_OK) {
        status = add_bullets(kit);
    }
    if (status == BENTO_OK) {
        status = add_food(kit);
    }
    if (status == BENTO_OK) {
        status = add_menu(kit);
    }
    if (status == BENTO_OK) {
        status = add_over(kit);
    }
    if (status == BENTO_OK) {
        status = add_labels(kit);
    }
    /* at the start every piece but the first is hidden */
    for (piece = PIECE_FOOD; (status == BENTO_OK) && (piece < PIECES);
         piece++) {
        status = hide_piece(kit, piece);
    }
    kit->piece = PIECE_BULLETS;
    kit->has_said = false;
    if (status == BENTO_OK) {
        status = open_piece(kit, PIECE_BULLETS);
    }
    return status;
}

/* -1, 0 or +1 for a pair of opposite keys. */
static int32_t key_axis(bool minus, bool plus)
{
    int32_t axis = 0;

    if (plus) {
        axis += 1;
    }
    if (minus) {
        axis -= 1;
    }
    return axis;
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    kit_t *kit = &g_kit;
    bento_game_keys_t keys;
    bool right;
    bool left;
    int32_t go;
    bento_status_t status;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: run() never stops by itself */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &kit->game,
        &keys); /* never fails: a silent pad is all keys up */
    right = bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_RIGHT,
                                    &keys);
    left =
        bento_game_pressed_once(&kit->game, BENTO_GAME_KEY_LEFT, &keys);
    go = key_axis(left, right);
    if (go != 0) {
        /* change piece only on the frame the key went down; floor modulo wraps round */
        status = open_piece(kit, (kit->piece + go + PIECES) % PIECES);
        if (status == BENTO_OK) {
            bento_game_sfx(&kit->game, "move");
        }
    } else {
        const bool press = bento_game_pressed_once(
            &kit->game, BENTO_GAME_KEY_A, &keys);

        status = play_piece(kit, &keys, press);
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

int main(void)
{
    kit_t *kit = &g_kit;
    bool quit = false;
    bento_status_t status;

    (void)memset(kit, 0, sizeof(*kit));
    status = bento_random_init(&kit->random);
    if (status == BENTO_OK) {
        bento_game_init(&kit->game, bento_runtime_get_ui(),
                        bento_runtime_is_running);
        status = bento_game_title(&kit->game, "KIT PROPS + HUD", NULL,
                                  &quit);
        report_joystick(&kit->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(kit);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&kit->game, update, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
