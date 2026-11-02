/*
 * s03_box_joystick.c -- สี่เหลี่ยมที่ขยับตามจอยและไม่หลุดจอ (คาบ 3)
 *
 * "win แรก" ของคอร์ส: คุมของบนจอได้ด้วยมือ ด้วยทักษะ 4 อย่างที่ใช้ซ้ำทุกเกม
 * start() · Box · keys() · run() กด Start เพื่อเล่น จอย 4 ทิศขยับกล่องสีฟ้า
 * กดค้างยิ่งนานกล่องยิ่งเร็ว (สูงสุด 30 พิกเซลต่อเฟรม) ปล่อยแล้วกลับไปช้า
 * กล่องไม่หลุดขอบจอ Back = ออก · Start = พักเกม
 *
 * The first win of the course: a cyan box that follows the joystick and
 * never leaves the screen. Press Start to play; the longer a direction is
 * held, the faster the box moves (6 px per frame, up to 30); letting go
 * slows it again. Back quits, Start pauses (the game loop handles both).
 *
 * Twin of s03_box_joystick.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s03_box_joystick.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - update() reads the file-scope state, not its void * argument;
 *   - game.run(update) without fps is bentogame's default, 30 fps, named FPS;
 *   - hold_frames stops counting at MAX_SPEED (past it the speed is the same),
 *     so the int32_t never overflows where Python's int would keep growing;
 *   - a bentogame call that raises in MicroPython returns its status here;
 *     title()'s Back (SystemExit) and the IDE's Stop end the program with 0.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The box and how fast it moves. */
enum {
    HERO_X = 380,
    HERO_Y = 180,
    HERO_PX = 40,
    BASE_SPEED = 6, /* ความเร็วเริ่มต้น (พิกเซลต่อเฟรม) ตอนเพิ่งแตะจอย */
    MAX_SPEED = 30, /* ความเร็วสูงสุดเมื่อกดค้างนานพอ */
    FPS = 30        /* run() without fps: bentogame's default */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t hero;
    bento_game_keys_t pressed; /* the joystick read this frame */
    int32_t hold_frames;       /* กดค้างมากี่เฟรมแล้ว (ยิ่งมาก ยิ่งเร็ว) */
} box_game_t;

/* The state bento_game_run's frame function reads: file scope, zeroed by
 * the loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern box_game_t g_box_game;
box_game_t g_box_game;

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

/* Box.move() keeps the box on the screen by itself. */
static bento_status_t move_hero(box_game_t *box_game,
                                const bento_game_keys_t *pressed,
                                int32_t speed)
{
    const bento_real_t step = (bento_real_t)speed;
    const bento_real_t none = (bento_real_t)0;
    bento_status_t status = BENTO_OK;

    if (pressed->left) {
        status = bento_game_box_move(&box_game->game, &box_game->hero,
                                     -step, none);
    }
    if ((status == BENTO_OK) && pressed->right) {
        status = bento_game_box_move(&box_game->game, &box_game->hero,
                                     step, none);
    }
    if ((status == BENTO_OK) && pressed->up) {
        status = bento_game_box_move(&box_game->game, &box_game->hero,
                                     none, -step);
    }
    if ((status == BENTO_OK) && pressed->down) {
        status = bento_game_box_move(&box_game->game, &box_game->hero,
                                     none, step);
    }
    return status;
}

/* One frame: read the joystick, speed up while it is held, move the box. */
static bento_status_t update(void *user, bool *out_stop)
{
    box_game_t *box_game = &g_box_game;
    const bento_game_keys_t *pressed = &box_game->pressed;
    bento_status_t status;
    bool moving;
    int32_t speed;

    /* the state is the file-scope object (no void * cast) */
    (void)user;
    *out_stop = false; /* this game never ends itself: Back ends it */
    bento_game_keys_init(&box_game->pressed);
    status = bento_game_keys(&box_game->game, &box_game->pressed);
    moving =
        pressed->left || pressed->right || pressed->up || pressed->down;
    if (!moving) {
        box_game->hold_frames = 0; /* ปล่อยจอย → รีเซ็ตกลับช้า */
    } else if (box_game->hold_frames < MAX_SPEED) {
        box_game->hold_frames += 1; /* กดค้าง → สะสมขึ้นเรื่อย ๆ */
    } else {
        /* already at the top speed */
    }
    /* ยิ่งกดค้างนาน speed ยิ่งมากขึ้น แต่ไม่เกิน MAX_SPEED */
    speed = BASE_SPEED + box_game->hold_frames;
    if (speed > MAX_SPEED) {
        speed = MAX_SPEED;
    }
    if (status == BENTO_OK) {
        status = move_hero(box_game, pressed, speed);
    }
    return status;
}

int main(void)
{
    box_game_t *box_game = &g_box_game;
    bool quit = false;
    bento_status_t status;

    (void)memset(box_game, 0, sizeof(*box_game));
    bento_game_init(&box_game->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_box_init(&box_game->hero);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&box_game->game, "MOVE BOX", NULL, &quit);
    report_joystick(&box_game->game);
    if ((status == BENTO_OK) && !quit) {
        /* สร้างสี่เหลี่ยมตัวแรก: x, y, กว้าง, สูง, สี */
        status = bento_game_box_create(
            &box_game->game, &box_game->hero, (bento_real_t)HERO_X,
            (bento_real_t)HERO_Y, (bento_real_t)HERO_PX,
            (bento_real_t)HERO_PX, BENTO_GAME_CYAN);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        box_game->hold_frames = 0;
        /* วิ่งทุกเฟรมจนกว่าจะกด Back */
        status = bento_game_run(&box_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
