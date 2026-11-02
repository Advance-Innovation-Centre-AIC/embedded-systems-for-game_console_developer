/*
 * mygame.c -- โครงเกมเปล่าสำหรับ Capstone (คาบ 14)
 *
 * หน้าเริ่ม "MY GAME" รอกด Start (Back = ออก) แล้ววางกล่องผู้เล่น
 * สีฟ้าขนาด 30 x 30 ไว้กลางจอ บรรทัดถัดไปคือช่องว่าง ____ ของเป้า
 * ที่นักเรียนยังไม่ได้เติม ชื่อ ____ ไม่มีอยู่จริง โปรแกรมจึงจบตรงนั้น
 * เติมช่องทั้งสามส่วน (STATE, TICK, WIN / LOSE) ให้ครบแล้วเกมจะเล่นได้
 *
 * The title screen "MY GAME" waits for Start (Back quits), then the
 * cyan 30 x 30 player box appears in the middle of the screen. The
 * next line is the blank `____` for the target, which the student
 * has not filled in yet: the name `____` does not exist, so the
 * program ends there. Fill in the three parts (STATE, TICK, WIN /
 * LOSE) and the game becomes playable.
 *
 * Twin of mygame.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/mygame.py),
 * from the library twin devi-capstone (py/devi-capstone.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - the NameError at the target's blank is main() returning
 *     BENTO_E_NOT_FOUND right after the player box, at the same point;
 *   - the unreachable rest (the score, the score label, update() with
 *     its TICK and WIN / LOSE blanks, run(update, fps=30)) is not
 *     written: no line of it runs in MicroPython either;
 *   - the middle of the screen (game.WIDTH and game.HEIGHT halved by
 *     floor division) is CENTER_X and CENTER_Y, integer enums;
 *   - `import random` maps to nothing: no number is drawn before the
 *     error;
 *   - title()'s Back (SystemExit) and the IDE's Stop at a sleep
 *     (KeyboardInterrupt, BENTO_E_CANCELLED) both end with 0;
 *   - the PSoC Edge kits' joystick.init() prints its line from
 *     MicroPython's C side; here the program prints it after title().
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* ค่าเกม (ลองปรับเป็นของกลุ่มเราได้); the speed and the goal are for
 * the blanks the student fills in */
/* The middle of the screen: half of game.WIDTH and game.HEIGHT by
 * floor division; title()'s start() keeps the default 792 x 398. */
enum {
    CENTER_X = BENTO_GAME_WIDTH / 2,
    CENTER_Y = BENTO_GAME_HEIGHT / 2
};

enum {
    PLAYER_SPEED = 8,
    GOAL = 5,
    PLAYER_SIZE_PX = 30
};

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

/* 1) STATE: the player box in the middle, then the target's blank. */
static bento_status_t build_state(bento_game_t *game)
{
    bento_game_box_t player;
    bento_status_t status;

    bento_game_box_init(&player);
    status = bento_game_box_create(
        game, &player, (bento_real_t)CENTER_X, (bento_real_t)CENTER_Y,
        (bento_real_t)PLAYER_SIZE_PX, (bento_real_t)PLAYER_SIZE_PX,
        BENTO_GAME_CYAN);
    if (status == BENTO_OK) {
        /* the blank is a name nobody defined: a NameError */
        status = BENTO_E_NOT_FOUND;
    }
    return status;
}

int main(void)
{
    bento_ui_t *ui = bento_runtime_get_ui();
    bento_game_t game;
    bool quit = false;
    bento_status_t status;

    bento_game_init(&game, ui, bento_runtime_is_running);
    /* หน้าเริ่ม: Start = เล่น, Back = ออก (title ทำ start ให้ในตัว) */
    status = bento_game_title(&game, "MY GAME", NULL, &quit);
    report_joystick(&game);
    if ((status == BENTO_OK) && !quit) {
        status = build_state(&game);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
