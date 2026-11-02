/*
 * mygame.c -- เฉลยโครงเกม MVP สำหรับ Capstone (คาบ 14)
 *
 * เฉลยที่เติมครบทั้ง 3 ช่องของไฟล์เว้นช่อง practise_codes/mygame.py
 * กด Start แล้วขับกล่องผู้เล่นสีฟ้าด้วยจอยไปชนเป้าสีเหลือง ชนหนึ่งครั้ง
 * ได้ 1 คะแนน และเป้าย้ายไปที่สุ่มใหม่ ครบ 5 คะแนน = YOU WIN!
 * Back = ออก · Start = พักเกม
 *
 * The solved Capstone skeleton: steer the cyan player box with the
 * joystick onto the yellow target. Each hit scores a point, plays a
 * sound and moves the target to a new random spot; five points shows
 * "YOU WIN!" and ends the game.
 *
 * Twin of mygame.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/mygame.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - random_spot() returns the spot through two out-parameters and a
 *     status (Python returns a tuple);
 *   - update() is the function bento_game_run calls each frame; it reads the
 *     file-scope state, not its void * argument; Python's
 *     `return False` sets *out_stop;
 *   - a bentogame call that raises in MicroPython returns its status here;
 *     title()'s Back (SystemExit) and the IDE's Stop end the program with 0.
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

/* --- ค่าเกม --- */
enum {
    PLAYER_SPEED = 8,
    GOAL = 5,
    PLAYER_PX = 30,
    TARGET_PX = 24, /* เป้า 24x24: random_spot() เว้นขอบเท่านี้ */
    LABEL_X = 10,
    LABEL_Y = 10,
    LABEL_CHARS = 20, /* "Score: " and any int32_t */
    FPS = 30
};

typedef struct {
    bento_game_t game;
    bento_random_t random;
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

/* title() calls start(), whose joystick.init() prints this line on the
 * kits. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* สุ่มตำแหน่งเป้าให้อยู่ในจอ (เผื่อขอบ) */
static bento_status_t random_spot(my_game_t *my_game, int32_t *spot_x,
                                  int32_t *spot_y)
{
    bento_status_t status = bento_random_randint(
        &my_game->random, 0, my_game->game.width - TARGET_PX, spot_x);

    if (status == BENTO_OK) {
        status = bento_random_randint(&my_game->random, 0,
                                      my_game->game.height - TARGET_PX,
                                      spot_y);
    }
    return status;
}

/* Box.move() keeps the player on the screen by itself. */
static bento_status_t move_player(my_game_t *my_game,
                                  const bento_game_keys_t *pressed)
{
    const bento_real_t step = (bento_real_t)PLAYER_SPEED;
    const bento_real_t none = (bento_real_t)0;
    bento_status_t status = BENTO_OK;

    if (pressed->left) {
        status = bento_game_box_move(&my_game->game, &my_game->player,
                                     -step, none);
    }
    if ((status == BENTO_OK) && pressed->right) {
        status = bento_game_box_move(&my_game->game, &my_game->player,
                                     step, none);
    }
    if ((status == BENTO_OK) && pressed->up) {
        status = bento_game_box_move(&my_game->game, &my_game->player,
                                     none, -step);
    }
    if ((status == BENTO_OK) && pressed->down) {
        status = bento_game_box_move(&my_game->game, &my_game->player,
                                     none, step);
    }
    return status;
}

/* A hit: one more point, the target to a new spot, and the win at GOAL. */
static bento_status_t score_hit(my_game_t *my_game, bool *out_stop)
{
    char text[LABEL_CHARS];
    int32_t spot_x = 0;
    int32_t spot_y = 0;
    bento_status_t status;

    my_game->score += 1;
    (void)snprintf(text, sizeof(text), "Score: %" PRId32,
                   my_game->score);
    status = bento_game_text_set(&my_game->game, &my_game->label, text);
    if (status == BENTO_OK) {
        bento_game_sfx(&my_game->game, "point");
        status = random_spot(my_game, &spot_x, &spot_y);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_move_to(
            &my_game->game, &my_game->target, (bento_real_t)spot_x,
            (bento_real_t)spot_y);
    }
    if ((status == BENTO_OK) && (my_game->score >= GOAL)) {
        status = bento_game_text_set(&my_game->game, &my_game->label,
                                     "YOU WIN!");
        if (status == BENTO_OK) {
            bento_game_sfx(&my_game->game, "win");
            *out_stop = true;
        }
    }
    return status;
}

/* One frame: 2) TICK moves the player, 3) WIN / LOSE scores a hit. */
static bento_status_t update(void *user, bool *out_stop)
{
    my_game_t *my_game = &g_my_game;
    bento_game_keys_t pressed;
    bento_status_t status;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    bento_game_keys_init(&pressed);
    status = bento_game_keys(&my_game->game, &pressed);
    if (status == BENTO_OK) {
        status = move_player(my_game, &pressed);
    }
    if ((status == BENTO_OK) &&
        bento_game_hit(&my_game->player, &my_game->target)) {
        status = score_hit(my_game, out_stop);
    }
    return status;
}

/* 1) STATE: the player, the target at a random spot, the score label. */
static bento_status_t build_state(my_game_t *my_game)
{
    /* floor division */
    const int32_t middle_x = my_game->game.width / 2;
    const int32_t middle_y = my_game->game.height / 2;
    int32_t spot_x = 0;
    int32_t spot_y = 0;
    bento_status_t status = bento_game_box_create(
        &my_game->game, &my_game->player, (bento_real_t)middle_x,
        (bento_real_t)middle_y, (bento_real_t)PLAYER_PX,
        (bento_real_t)PLAYER_PX, BENTO_GAME_CYAN);

    if (status == BENTO_OK) {
        status = random_spot(my_game, &spot_x, &spot_y);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            &my_game->game, &my_game->target, (bento_real_t)spot_x,
            (bento_real_t)spot_y, (bento_real_t)TARGET_PX,
            (bento_real_t)TARGET_PX, BENTO_GAME_YELLOW);
    }
    my_game->score = 0;
    if (status == BENTO_OK) {
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
    status = bento_random_init(&my_game->random);
    bento_game_init(&my_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&my_game->player);
    bento_game_box_init(&my_game->target);
    bento_game_text_init(&my_game->label);
    if (status == BENTO_OK) {
        /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
        status =
            bento_game_title(&my_game->game, "MY GAME", NULL, &quit);
        report_joystick(&my_game->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_state(my_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&my_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
