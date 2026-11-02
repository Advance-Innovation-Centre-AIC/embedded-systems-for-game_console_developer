/*
 * s03_box_joystick.c -- ขยับกล่องด้วยจอย (ฉบับฝึกเติมโค้ด คาบ 2)
 *
 * ก้าวแรกของคอร์ส: หน้าเริ่ม "MOVE BOX" รอกด Start (Back = ออก)
 * แล้ววางกล่องสีฟ้า 40 x 40 ไว้ที่ (380, 180) จากนั้น game.run เรียก
 * update() 30 ครั้งต่อวินาที ช่องที่นักเรียนต้องเติม (ขยับกล่องตามจอย
 * เร่งเมื่อกดค้าง) ยังเป็น pass จึงอ่านจอยอย่างเดียว กล่องยังไม่ขยับ
 * กด Back เพื่อออก
 *
 * The course's first step: the title screen "MOVE BOX" waits for
 * Start (Back quits), then a cyan 40 x 40 box appears at (380, 180),
 * and the game loop calls update() 30 times a second. The student's
 * blank (move the box with the pad, faster while a key is held) is
 * still `pass`, so each frame only reads the pad and the box stays
 * where it is. Back ends the game.
 *
 * Twin of s03_box_joystick.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/s03_box_joystick.py),
 * from the library twin devi-s03-box-joystick
 * (py/devi-s03-box-joystick.py). Style: CLEAN_C.md.
 * What C must do differently:
 *   - the module globals (the game, the hero box, hold_frames) are one
 *     box_game_t at file scope, which update() reads there, never
 *     through a cast of run()'s user pointer (MISRA 11.5);
 *   - BASE_SPEED and MAX_SPEED are named but never read, as in the
 *     original, whose blank would read them;
 *   - title()'s Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* The hero box and the game's pace. */
enum {
    HERO_X = 380,
    HERO_Y = 180,
    HERO_SIZE_PX = 40,
    FPS = 30 /* run(update) without fps= runs 30 frames a second */
};

/* ความเร็ว (พิกเซลต่อเฟรม), for the blank the student fills in */
enum {
    BASE_SPEED = 6, /* ความเร็วเริ่มต้น ตอนเพิ่งแตะจอย */
    MAX_SPEED = 30  /* ความเร็วสูงสุดเมื่อกดค้างนานพอ */
};

typedef struct {
    bento_game_t game;
    bento_game_box_t hero;
    int32_t hold_frames; /* กดค้างมากี่เฟรมแล้ว (ยิ่งมาก ยิ่งเร็ว) */
} box_game_t;

/* The frame callback reads the state here rather than through a cast
 * of its void pointer (MISRA 11.5): file scope, zeroed by the loader
 * on every load, and set again at the start of main() (CLEAN_C.md C9,
 * deviation BENTO-MISRA-E03/E04). */
extern box_game_t g_box_game;
box_game_t g_box_game;

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

/* One frame. The student's part (move the hero with the pad) is still
 * blank, so the frame only reads the pad and the game goes on. */
static bento_status_t update(void *user, bool *out_stop)
{
    box_game_t *box_game = &g_box_game;
    bento_game_keys_t pressed;

    /* never ends the game: Start/Back are handled by the run loop */
    *out_stop = false;
    (void)user; /* the state is g_box_game: no cast (MISRA 11.5) */
    /* keys() answers all false when no pad answers, never a failure */
    (void)bento_game_keys(&box_game->game, &pressed);
    return BENTO_OK;
}

/* The hero box, then the game loop until Back. */
static bento_status_t play(box_game_t *box_game)
{
    bool finished = false;
    /* สร้างสี่เหลี่ยมตัวแรก: x, y, กว้าง, สูง, สี */
    bento_status_t status = bento_game_box_create(
        &box_game->game, &box_game->hero, (bento_real_t)HERO_X,
        (bento_real_t)HERO_Y, (bento_real_t)HERO_SIZE_PX,
        (bento_real_t)HERO_SIZE_PX, BENTO_GAME_CYAN);

    box_game->hold_frames = 0;
    if (status == BENTO_OK) {
        status = bento_game_run(&box_game->game, update, NULL, FPS,
                                &finished);
    }
    return status;
}

int main(void)
{
    box_game_t *box_game = &g_box_game;
    bento_ui_t *ui = bento_runtime_get_ui();
    bool quit = false;
    bento_status_t status;

    (void)memset(box_game, 0, sizeof(*box_game));
    bento_game_init(&box_game->game, ui, bento_runtime_is_running);
    bento_game_box_init(&box_game->hero);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&box_game->game, "MOVE BOX", NULL, &quit);
    report_joystick(&box_game->game);
    if ((status == BENTO_OK) && !quit) {
        status = play(box_game);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
