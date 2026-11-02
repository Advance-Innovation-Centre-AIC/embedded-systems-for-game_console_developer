/*
 * shooter_step4.c -- Shooter #3 (คาบ 12): เติมศัตรู + ชนแบบกล่อง + คะแนน + ชีวิต ฉบับฝึก
 *
 * กระสุนใช้ "ชุดหมุนเวียน" 6 นัดจากคาบ 11 (ให้มาแล้ว) ศัตรู 6 ตัว "วนกลับขึ้นบน" ทุกครั้งที่
 * หลุดพื้นหรือโดนยิง (แบบของตกใน Catch คาบ 4) ศัตรูใช้อยู่ตลอด ไม่มีช่วงว่าง
 * กติกา: ยิงโดน = Score +1 / หลุดพื้น = Lives -1 / Lives เหลือ 0 = GAME OVER
 * สิ่งที่เพิ่งได้ใช้แบบหลายคู่: bento_game_hit (ชนแบบกล่อง) และ bento_game_text_set
 * ช่องที่ต้องเติมยังว่าง ศัตรูจึงจอดรอเหนือจอ ยานขับได้และยิงได้
 * เฉลย: solution_codes/shooter_step4.py · ใบ้: สไลด์คาบ 12 + ใบงาน
 *
 * Shooter step 4, the practise skeleton: the title screen, then the ship that
 * steers (step 2), fires from the pool of six bullets with a cooldown (step 3), and
 * six placeholder enemies parked above the screen. The blanks (random enemies,
 * respawn, falling, hits, score, lives, game over) are still empty, so you can steer
 * and fire bullets that fly up and park again. The game loop handles Back (quit)
 * and Start (pause) by itself.
 *
 * Twin of shooter_step4.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/shooter_step4.py). Style: CLEAN_C.md.
 * What C does differently:
 * - random and ENEMY_COLORS are read only by the blanks, so they are named there;
 * - respawn_enemy() is pass and never called, and an uncalled static function fails
 *   the -Werror build, so blank 2 says how to write it;
 * - find_free_bullet() answers NULL for None; max(low, min(high, x)) is clamp();
 * - the module's globals are one shooter_t at file scope (512 bytes or more, C9);
 * - title()'s Back (SystemExit) and Stop (KeyboardInterrupt) both end with 0;
 * - joystick.init() prints its line on the kits, so the C prints it there too.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_runtime.h"

#include "bento_fmt.h"
#include "bento_game.h"
#include "bento_gpio.h"

/* The ship, the HUD and the frame rate. */
enum {
    FPS = 30,
    SHIP_X = 365,
    SHIP_Y = 352,
    SHIP_W = 62,
    SHIP_H = 24,
    HUD_X = 10,
    HUD_Y = 8,
    START_LIVES = 3
};

/* The bullet pool and the enemies. */
enum {
    MAX_BULLETS = 6, /* ชุดกระสุน 6 นัด */
    MAX_ENEMIES = 6, /* ศัตรู 6 ตัว */
    BULLET_W = 6,
    BULLET_H = 14,
    ENEMY_W = 30,
    ENEMY_H = 24,
    PARKED_Y = -50,    /* จอดรอเหนือจอ */
    FREE_LINE_Y = -20, /* ว่าง ⇔ y < -20 */
    BULLET_NOSE_Y = 340,
    BULLET_STEP_PX = 9,
    COOLDOWN_FRAMES = 8
};

typedef struct {
    bento_game_t game;
    bento_game_box_t ship;
    bento_game_text_t hud;
    bento_game_box_t bullets[MAX_BULLETS];
    bento_game_box_t enemies[MAX_ENEMIES];
    bento_real_t enemy_speed[MAX_ENEMIES];
    bento_real_t ship_x;
    bento_real_t ship_speed;
    int32_t score;
    int32_t lives;
    int32_t fire_cooldown;
} shooter_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every
 * load, and set again at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern shooter_t g_shooter;
shooter_t g_shooter;

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
    if ((bento_gpio_board_info(&info) == BENTO_OK) && (info.name != NULL)) {
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

static bento_status_t add_ship_and_hud(shooter_t *shooter)
{
    bento_status_t status = bento_game_box_create(
        &shooter->game, &shooter->ship, (bento_real_t)SHIP_X, (bento_real_t)SHIP_Y,
        (bento_real_t)SHIP_W, (bento_real_t)SHIP_H, BENTO_GAME_GREEN);

    shooter->ship_x = (bento_real_t)SHIP_X;
    shooter->ship_speed = (bento_real_t)0.0;
    shooter->score = 0;
    shooter->lives = START_LIVES;
    shooter->fire_cooldown = 0;
    if (status == BENTO_OK) {
        status = bento_game_text_create(&shooter->game, &shooter->hud, "Score: 0   Lives: 3",
                                        HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* The six bullets, made once, parked above the screen and hidden. */
static bento_status_t add_bullet_pool(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < MAX_BULLETS); i++) {
        status = bento_game_box_create(&shooter->game, &shooter->bullets[i], (bento_real_t)0,
                                       (bento_real_t)PARKED_Y, (bento_real_t)BULLET_W,
                                       (bento_real_t)BULLET_H, BENTO_GAME_CYAN);
    }
    for (i = 0; (status == BENTO_OK) && (i < MAX_BULLETS); i++) {
        status = bento_game_box_hide(&shooter->game, &shooter->bullets[i]);
    }
    return status;
}

/* ----- เติมส่วนนี้เอง (งานของคุณ) (1): สร้างศัตรู 6 ตัวครั้งเดียว -----
 *   ให้ตกจากเหนือจอด้วยความเร็วสุ่ม
 *   1) ตำแหน่ง x สุ่มให้อยู่ในจอ (bento_random, ระวังอย่าให้เลยขอบขวา game.width)
 *   2) ตำแหน่ง y ให้เป็นค่าติดลบ (เริ่มเหนือจอ) เพื่อให้ศัตรูทยอยร่วงลงมาไม่พร้อมกัน
 *   3) สีของแต่ละตัวสุ่มจาก ENEMY_COLORS ของ .py: BENTO_GAME_RED, BENTO_GAME_ORANGE,
 *      BENTO_GAME_PINK (ตาราง static const uint32_t ที่หัวไฟล์)
 *   4) ตั้ง enemy_speed[i] เป็นความเร็วสุ่ม ช้าพอให้ยิงทัน
 *   ตอนนี้ยังเป็น placeholder: กล่องแดงจอดที่ (0, -50) ความเร็ว 0.0 */
static bento_status_t add_enemies(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < MAX_ENEMIES); i++) {
        status = bento_game_box_create(&shooter->game, &shooter->enemies[i], (bento_real_t)0,
                                       (bento_real_t)PARKED_Y, (bento_real_t)ENEMY_W,
                                       (bento_real_t)ENEMY_H, BENTO_GAME_RED);
    }
    for (i = 0; i < MAX_ENEMIES; i++) {
        shooter->enemy_speed[i] = (bento_real_t)0.0;
    }
    return status;
}

/* The first parked bullet of the pool, or NULL when all six fly. */
static bento_game_box_t *find_free_bullet(shooter_t *shooter)
{
    bento_game_box_t *free_bullet = NULL;
    int32_t i;

    for (i = 0; (free_bullet == NULL) && (i < MAX_BULLETS); i++) {
        if (shooter->bullets[i].y < (bento_real_t)FREE_LINE_Y) {
            free_bullet = &shooter->bullets[i];
        }
    }
    return free_bullet;
}

/* ----- เติมส่วนนี้เอง (งานของคุณ) (2): ศัตรูวนกลับขึ้นบน (เริ่มรอบใหม่เหนือจอ) -----
 *   เขียน static bento_status_t respawn_enemy(shooter_t *shooter, int32_t index) ตรงนี้
 *   1) bento_game_box_move_to ย้าย enemies[index] ไป x สุ่มในจอ, y ติดลบ (เหนือจอ)
 *      (ช่วงค่าดูตาราง "ชิ้นส่วนสุ่ม" ในสไลด์คาบ 12)
 *   2) เปลี่ยนสีใหม่ด้วย bento_game_box_set_color สุ่มจาก ENEMY_COLORS */

/* value kept between low and high, as max(low, min(high, value)) */
static bento_real_t clamp(bento_real_t value, bento_real_t low, bento_real_t high)
{
    const bento_real_t below_high = (value < high) ? value : high;

    return (below_high > low) ? below_high : low;
}

/* ยาน (step2): เร่ง ไถล เพดานความเร็ว ขอบจอ แล้ววาด */
static bento_status_t steer_ship(shooter_t *shooter, const bento_game_keys_t *keys)
{
    const bento_real_t accel = (bento_real_t)1.4;
    const bento_real_t max_speed = (bento_real_t)13.0;
    const bento_real_t friction = (bento_real_t)0.80;
    const bento_real_t right_edge = (bento_real_t)(shooter->game.width - SHIP_W);

    if (keys->left) {
        shooter->ship_speed -= accel;
    } else if (keys->right) {
        shooter->ship_speed += accel;
    } else {
        shooter->ship_speed *= friction;
    }
    shooter->ship_speed = clamp(shooter->ship_speed, -max_speed, max_speed);
    shooter->ship_x = clamp(shooter->ship_x + shooter->ship_speed, (bento_real_t)0.0, right_edge);
    return bento_game_box_move_to(&shooter->game, &shooter->ship, shooter->ship_x,
                                  (bento_real_t)SHIP_Y);
}

/* ยิง (step3): A or Up fires one free bullet from the ship's nose once the cooldown is 0. */
static bento_status_t fire_when_asked(shooter_t *shooter, const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;

    shooter->fire_cooldown = (shooter->fire_cooldown > 1) ? (shooter->fire_cooldown - 1) : 0;
    if ((keys->a || keys->up) && (shooter->fire_cooldown == 0)) {
        bento_game_box_t *bullet = find_free_bullet(shooter);

        if (bullet != NULL) {
            /* two steps, as the .py adds half the ship, then takes off half the bullet */
            const bento_real_t ship_middle = shooter->ship_x + (bento_real_t)(SHIP_W / 2);

            status = bento_game_box_show(&shooter->game, bullet);
            if (status == BENTO_OK) {
                status = bento_game_box_move_to(&shooter->game, bullet,
                                                ship_middle - (bento_real_t)(BULLET_W / 2),
                                                (bento_real_t)BULLET_NOSE_Y);
            }
            if (status == BENTO_OK) {
                bento_game_sfx(&shooter->game, "fire");
                shooter->fire_cooldown = COOLDOWN_FRAMES;
            }
        }
    }
    return status;
}

/* Each flying bullet goes up a step; one past the free line is hidden, back in the pool. */
static bento_status_t move_bullets(shooter_t *shooter)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < MAX_BULLETS); i++) {
        bento_game_box_t *bullet = &shooter->bullets[i];

        if (bullet->y >= (bento_real_t)FREE_LINE_Y) {
            status = bento_game_box_move_to(&shooter->game, bullet, bullet->x,
                                            bullet->y - (bento_real_t)BULLET_STEP_PX);
            if ((status == BENTO_OK) && (bullet->y < (bento_real_t)FREE_LINE_Y)) {
                status = bento_game_box_hide(&shooter->game, bullet);
            }
        }
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    shooter_t *shooter = &g_shooter;
    bento_game_keys_t keys;
    bento_status_t status = bento_game_keys(&shooter->game, &keys);

    (void)user;     /* the state is g_shooter */
    (void)out_stop; /* only blank (3+4) ends the game, at GAME OVER */
    /* (Back = ออก / Start = พักเกม: bento_game_run จัดการให้) */
    if (status == BENTO_OK) {
        status = steer_ship(shooter, &keys);
    }
    if (status == BENTO_OK) {
        status = fire_when_asked(shooter, &keys);
    }
    if (status == BENTO_OK) {
        status = move_bullets(shooter);
    }
    /* ----- เติมส่วนนี้เอง (งานของคุณ) (3+4) -----
     *   ก ตก / ข หลุดพื้น / ค ชีวิตหมด / ง วนกระสุน / จ โดน
     *   วนศัตรูทุกตัว index 0 .. MAX_ENEMIES - 1 (enemy = &shooter->enemies[index])
     *   ก) ให้ศัตรูตกลงทีละเฟรม: bento_game_box_move_to ลงตาม enemy_speed[index] ของมัน
     *   ข) หลุดพื้น? เทียบ enemy->y กับ game.height ถ้าหลุด: ลด lives, อัปเดต HUD ด้วย
     *      bento_game_text_set, เรียก respawn_enemy แล้ว continue ไปตัวถัดไป
     *   ค) ถ้า lives หมด (<= 0): bento_game_sfx "gameover", bento_game_text_create "GAME OVER"
     *      แล้วตั้ง *out_stop = true เพื่อจบเกม
     *   ง) ถ้ายังไม่หลุด: วนกระสุนเฉพาะนัดที่ใช้อยู่ (y >= FREE_LINE_Y)
     *      แล้วเช็ค bento_game_hit(bullet, enemy)
     *   จ) ถ้าโดน: score += 1, bento_game_sfx "hit", อัปเดต HUD (อย่าลืมตรงนี้!),
     *      คืนกระสุน (bento_game_box_move_to ไป (0, PARKED_Y) + bento_game_box_hide),
     *      respawn_enemy แล้ว break ออกจาก loop กระสุน */
    return status;
}

int main(void)
{
    shooter_t *shooter = &g_shooter;
    bool quit = false;
    bento_status_t status;

    (void)memset(shooter, 0, sizeof(*shooter));
    bento_game_init(&shooter->game, bento_runtime_get_ui(), bento_runtime_is_running);
    /* หน้าเริ่ม: Start=เล่น Back=ออก (ทำ start ให้ในตัว) */
    status = bento_game_title(&shooter->game, "SHOOTER", NULL, &quit);
    report_joystick(&shooter->game);
    if ((status == BENTO_OK) && !quit) {
        status = add_ship_and_hud(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_bullet_pool(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_enemies(shooter);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status = bento_game_run(&shooter->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
