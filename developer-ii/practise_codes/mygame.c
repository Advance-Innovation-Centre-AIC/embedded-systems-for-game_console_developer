/*
 * mygame.c -- โครงเกมเปล่า ๆ ที่ "รันได้เลย" สำหรับคาบ MVP Clinic (คาบ 11)
 *
 * เปิดมาแล้วมี "ผู้เล่น 1 ตัว" ขยับได้ด้วย joystick + มี score บนจอ ยังไม่ต้องสนุก
 * แค่ "วิ่งได้" เป็นฐานให้กลุ่มไปต่อ ก๊อปไฟล์นี้ตั้งชื่อใหม่แล้วเปลี่ยนสี/ความเร็ว/กติกา
 * เป็นของกลุ่มคุณ กฎทอง: เริ่มจากโครงนี้ก่อน รันให้เห็นกล่องขยับ แล้วค่อยเติมทีละ Issue
 *
 * The empty game skeleton for the MVP clinic: after the title screen a green
 * player box moves with the joystick and a score label sits top left. Back
 * quits and Start pauses (the game loop does both). The gameplay, polish and
 * leaderboard Issues are left as TODOs.
 *
 * Twin of mygame.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/mygame.py). Style: CLEAN_C.md.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

/* --- ตั้งค่าเกม (แก้ตรงนี้ก่อนเป็นอันดับแรก) --- */
enum {
    PLAYER_SPEED = 8,                /* ขยับทีละกี่พิกเซลต่อเฟรม */
    PLAYER_COLOR = BENTO_GAME_GREEN, /* ลองเปลี่ยนเป็น CYAN / PINK ดู */
    FPS = 30,                        /* ความถี่ของ loop */
    PLAYER_X = 380,
    PLAYER_Y = 350,
    PLAYER_PX = 30,
    LABEL_X = 10,
    LABEL_Y = 10
};

typedef struct {
    bento_game_t game;
    /* เริ่มจาก Box ก่อน คาบหน้าค่อยเปลี่ยนเป็น Sprite */
    bento_game_box_t player;
    bento_game_text_t label; /* ป้ายคะแนนมุมซ้ายบน */
    int32_t score;
} my_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on
 * every load, and set again at the start of main() (CLEAN_C.md C9,
 * BENTO-MISRA-E03/E04). */
extern my_game_t g_my_game;
my_game_t g_my_game;

/* 1) อ่าน input แล้วขยับผู้เล่น (box move คุมไม่ให้หลุดจอ) */
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

/* ถูกเรียกทุกเฟรม; *out_stop = true จบเกม */
static bento_status_t update(void *user, bool *out_stop)
{
    my_game_t *my_game = &g_my_game;
    bento_game_keys_t keys;
    bento_status_t status;

    /* the state is g_my_game at file scope (C9): no void * conversion */
    (void)user;
    bento_game_keys_init(&keys);
    status = bento_game_keys(&my_game->game, &keys);
    if (status == BENTO_OK) {
        status = move_player(my_game, &keys);
    }
    /* BACK = ออกเกม, START = หยุดชั่วคราว game loop จัดการให้อัตโนมัติ
     * update() จึงจบเกมเฉพาะตอน "ชนะ/แพ้จริง" เท่านั้น (ดู TODO #2)
     *
     * --- TODO (Issue #2) gameplay: สร้างศัตรู/ของ + bento_game_hit + คะแนน ---
     *     เช่น ถ้า bento_game_hit(&player, &food): score++, อัปเดตป้ายด้วย
     *     bento_game_text_set ("Score: <แต้ม>"), bento_game_sfx "point" แล้ว
     *     bento_game_box_move_to ย้าย food ไปที่ใหม่
     *
     * --- TODO (Issue #3) polish: เปลี่ยน Box เป็น bento_game_sprite_create
     *     (เช่น "ship" ที่ 380, 350 สร้างครั้งเดียวตอนเริ่ม) + bento_game_sfx
     *     "gameover" ตอนจบ
     *
     * --- TODO (เสริม) ส่งคะแนนขึ้น leaderboard ห้อง (สาย IoT): ต่อ WiFi + MQTT
     *     ครั้งเดียวตอนเริ่มเกม (bento_wifi / bento_mqtt) แล้ว publish คะแนน
     *     ตอน game over */
    *out_stop = false; /* เล่นต่อ */
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

static bento_status_t add_player_and_label(my_game_t *my_game)
{
    bento_status_t status;

    my_game->score = 0;
    bento_game_box_init(&my_game->player);
    status = bento_game_box_create(
        &my_game->game, &my_game->player, (bento_real_t)PLAYER_X,
        (bento_real_t)PLAYER_Y, (bento_real_t)PLAYER_PX,
        (bento_real_t)PLAYER_PX, PLAYER_COLOR);
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
        status = add_player_and_label(my_game);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        /* วน loop จนกว่าจะจบเกม */
        status = bento_game_run(&my_game->game, update, NULL, FPS,
                                &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
