/*
 * mygame.c -- เฉลย MVP: โครงเกมเปล่าที่ "เติม 3 ช่องครบแล้ว" สำหรับคาบ 13 Showcase
 *
 * ผู้เล่นกล่อง cyan วิ่งด้วย joystick ไปชนเป้า yellow ให้ครบ 5 แต้มแล้วชนะ
 * ใช้เป็น "ตัวอย่างเต็ม" ให้เทียบหลังเติม practise_codes/mygame เอง
 * เขียนจริงแค่ 3 ช่อง: STATE, TICK, WIN/LOSE
 *
 * The MVP solution: a cyan player box moves with the joystick; touching the
 * yellow target scores a point with the "point" sound and moves the target to
 * a new pseudo-random place; five points show "YOU WIN!" with the "win"
 * sound and end the game. Back quits and Start pauses (the game loop).
 *
 * Twin of mygame.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/mygame.py). Style: CLEAN_C.md.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

enum {
    PLAYER_SPEED = 8, /* ขยับทีละกี่พิกเซลต่อเฟรม */
    FPS = 30,         /* ความถี่ของลูป */
    WIN_SCORE = 5,    /* ชนเป้าครบเท่านี้ = ชนะ */
    PLAYER_PX = 32,
    TARGET_START = 120,
    TARGET_PX = 28,
    LABEL_X = 12,
    LABEL_Y = 10,
    WIN_TEXT_SHIFT = 70,
    SCORE_CHARS = 20, /* "Score: " and any int32_t */
    /* the target's next place: a light pseudo-random step */
    STEP_X_TIMES = 7,
    STEP_X_PLUS = 113,
    STEP_Y_TIMES = 5,
    STEP_Y_PLUS = 71
};

typedef struct {
    bento_game_t game;
    bento_game_box_t player;
    bento_game_box_t target;
    bento_game_text_t label;
    int32_t score;
} my_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern my_game_t g_my_game;
my_game_t g_my_game;

/* อ่านปุ่มแล้วขยับผู้เล่น (box move คุมไม่ให้หลุดจอ) */
static bento_status_t move_player(my_game_t *my_game,
                                  const bento_game_keys_t *keys)
{
    const bento_real_t step = (bento_real_t)PLAYER_SPEED;
    const bento_real_t still = (bento_real_t)0;
    bento_game_t *game = &my_game->game;
    bento_game_box_t *player = &my_game->player;
    bento_status_t status = BENTO_OK;

    if (keys->left) {
        status = bento_game_box_move(game, player, -step, still);
    }
    if ((status == BENTO_OK) && keys->right) {
        status = bento_game_box_move(game, player, step, still);
    }
    if ((status == BENTO_OK) && keys->up) {
        status = bento_game_box_move(game, player, still, -step);
    }
    if ((status == BENTO_OK) && keys->down) {
        status = bento_game_box_move(game, player, still, step);
    }
    return status;
}

/* สุ่มตำแหน่งเป้าใหม่แบบเบา ๆ */
static bento_status_t move_target(my_game_t *my_game)
{
    const bento_game_box_t *target = &my_game->target;
    const int32_t span_x = my_game->game.width - (int32_t)target->w;
    const int32_t span_y = my_game->game.height - (int32_t)target->h;
    const int32_t new_x =
        (((int32_t)target->x * STEP_X_TIMES) + STEP_X_PLUS) % span_x;
    const int32_t new_y =
        (((int32_t)target->y * STEP_Y_TIMES) + STEP_Y_PLUS) % span_y;

    return bento_game_box_move_to(&my_game->game, &my_game->target,
                                  (bento_real_t)new_x,
                                  (bento_real_t)new_y);
}

static bento_status_t show_win(my_game_t *my_game)
{
    const int32_t x_px = (my_game->game.width / 2) - WIN_TEXT_SHIFT;
    bento_game_text_t win;
    bento_status_t status;

    bento_game_text_init(&win);
    status = bento_game_text_create(&my_game->game, &win, "YOU WIN!",
                                    x_px, my_game->game.height / 2,
                                    BENTO_GAME_GREEN);
    bento_game_sfx(&my_game->game, "win");
    return status;
}

/* 3) WIN / LOSE: ชนเป้า = +1 แต้ม, ครบ WIN_SCORE = ชนะ */
static bento_status_t check_target(my_game_t *my_game, bool *won)
{
    char text[SCORE_CHARS];
    bento_status_t status;

    my_game->score += 1;
    (void)snprintf(text, sizeof(text), "Score: %" PRId32,
                   my_game->score);
    status = bento_game_text_set(&my_game->game, &my_game->label, text);
    bento_game_sfx(&my_game->game, "point");
    if (status == BENTO_OK) {
        status = move_target(my_game);
    }
    if ((status == BENTO_OK) && (my_game->score >= WIN_SCORE)) {
        status = show_win(my_game);
        *won = true;
    }
    return status;
}

/* ถูกเรียกทุกเฟรม; *out_stop = true จบเกม */
static bento_status_t update(void *user, bool *out_stop)
{
    my_game_t *my_game = &g_my_game;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_my_game at file scope (C9): no void * conversion */
    (void)user;
    /* 2) TICK: กติกาทุกเฟรม อ่านปุ่มแล้วขยับผู้เล่น */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&my_game->game, &keys);
    if (status == BENTO_OK) {
        status = move_player(my_game, &keys);
    }
    /* BACK = ออกเกม, START = หยุดชั่วคราว game loop จัดการให้อัตโนมัติ
     * update() จึงจบเกมเฉพาะตอน "ชนะ/แพ้จริง" เท่านั้น */
    if ((status == BENTO_OK) &&
        bento_game_hit(&my_game->player, &my_game->target)) {
        status = check_target(my_game, out_stop);
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

/* 1) STATE: เกมต้องจำอะไรบ้าง (สร้างครั้งเดียว ก่อนลูป) */
static bento_status_t add_state(my_game_t *my_game)
{
    const int32_t center_x = my_game->game.width / 2;
    const int32_t center_y = my_game->game.height / 2;
    bento_status_t status;

    my_game->score = 0;
    bento_game_box_init(&my_game->player);
    bento_game_box_init(&my_game->target);
    status = bento_game_box_create(
        &my_game->game, &my_game->player, (bento_real_t)center_x,
        (bento_real_t)center_y, (bento_real_t)PLAYER_PX,
        (bento_real_t)PLAYER_PX, BENTO_GAME_CYAN);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &my_game->game, &my_game->target,
            (bento_real_t)TARGET_START, (bento_real_t)TARGET_START,
            (bento_real_t)TARGET_PX, (bento_real_t)TARGET_PX,
            BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        bento_game_text_init(&my_game->label);
        status = bento_game_text_create(&my_game->game, &my_game->label,
                                        "Score: 0", LABEL_X, LABEL_Y,
                                        BENTO_GAME_WHITE);
    }
    return status;
}

int main(void)
{
    my_game_t *my_game = &g_my_game;
    bool quit = false;
    bento_status_t status;

    (void)memset(my_game, 0, sizeof(*my_game));
    bento_game_init(&my_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&my_game->game, "MY GAME", NULL, &quit);
    /* title() calls start(), whose joystick.init() prints this line */
    if ((my_game->game.joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_state(my_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&my_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
