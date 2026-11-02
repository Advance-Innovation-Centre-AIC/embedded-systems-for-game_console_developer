/*
 * s13_11_game_hud.c -- คาบ 13 (คาบพิเศษ): HUD ในเกม
 *
 * HUD ตอบ "ตอนนี้ฉันเป็นยังไง" ภายในครึ่งวินาที ตามกฎ 3 ข้อ:
 * ตัวเลขสำคัญอยู่มุมเดิมเสมอ (คะแนนซ้ายบน เวลากลางบน ชีวิตขวาบน)
 * อัปเดตเฉพาะตอนค่าเปลี่ยน และข่าวใหญ่อยู่กลางจอ (READY / GAME OVER)
 * กด Start รอป้าย READY หายไป แล้วจอยซ้าย/ขวาเลื่อนตะกร้ารับผลไม้
 * พลาด = ยานขวาบนหายหนึ่งลำ 5 วินาทีสุดท้ายมีเสียงนับ ชีวิตหรือเวลา
 * หมด = GAME OVER กลางจอ กด A เล่นใหม่
 *
 * A game HUD answers "how am I doing?" within half a second, by three
 * rules: the key numbers stay in the same corner (score top left,
 * time top centre, lives top right), a label is written only when its
 * value changes, and big news sits in the middle (READY, GAME OVER).
 * Press Start, wait for READY to go, then move the basket left and
 * right to catch the fruit. A miss takes one ship from the top right;
 * the last 5 seconds tick. No lives or no time left is GAME OVER; A
 * plays again. The brain is checked by a self test before the screen.
 *
 * Twin of s13_11_game_hud.py (embedded-systems-for-game_console_
 * developer/developer-i/solution_codes/s13_11_game_hud.py).
 * Style: CLEAN_C.md. What C does differently:
 * - the phase strings "ready", "play", "over" are phase_t values;
 * - the st and shown dicts are members of one struct at file scope;
 * - shown's None is -1 (a score and a second count are never below 0);
 * - a Sprite has no size in C: the fruit size is kept by the program
 *   and game.hit is bento_game_hit_rect on the basket Box's fields;
 * - random is bento_random.h, its state in the struct;
 * - the self test cases are one table of inputs and wanted values;
 * - raise SystemExit after a failed self test, title()'s Back and Stop
 *   end the program with 0.
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

/* The round (แก้ได้). */
enum {
    FPS = 30,
    GAME_SECONDS = 30, /* เล่นรอบละกี่วินาที */
    LIVES = 3,         /* ชีวิต = จำนวนยานมุมขวาบน */
    READY_FRAMES = 90, /* ป้าย READY ค้าง 90 เฟรม = 3 วินาที */
    FRUITS = 3,        /* ผลไม้ที่ตกพร้อมกัน (ชุดหมุนเวียน) */
    FALL_MIN = 18,     /* ความเร็วตก หน่วย 0.1 px ต่อเฟรม */
    FALL_MAX = 30,
    FALL_SCALE = 10,
    BASKET_W = 110,
    BASKET_SPEED = 9,
    GROUND_Y = 356, /* ผลไม้ตกถึงตรงนี้ = พลาด */
    SHIP_W = 62,    /* ขนาดภาพ ship กับ snake_food */
    FRUIT_PX = 16,
    TOTAL_FRAMES = GAME_SECONDS * FPS,
    COUNTDOWN_SECONDS = 5
};

/* The screen. */
enum {
    HUD_BAR_H = 40,
    GROUND_H = 8,
    BASKET_X = 340,
    BASKET_LIFT = 18,
    BASKET_H = 14,
    BASKET_RADIUS = 4,
    SHIP_MARGIN = 12,
    SHIP_GAP = 6,
    SHIP_Y = 6,
    HUD_X = 12,
    HUD_Y = 11,
    CHAR_HALF_PX = 5, /* ตัวอักษรกว้างราว 10 px */
    BANNER_X = 236,
    BANNER_Y = 150,
    BANNER_W = 320,
    BANNER_H = 90,
    BANNER_RADIUS = 8,
    BANNER_BORDER_W = 3,
    HEADLINE_Y = 172,
    SUBLINE_Y = 204,
    BANNER_LINES = 2,
    DROP_X_MIN = 20,
    DROP_X_RIGHT = 36,
    DROP_Y_MIN = 20,
    DROP_Y_MAX = 400,
    TEXT_CHARS = 64
};

enum {
    COLOR_HUD_BAR = 0x1E2A36,
    COLOR_BANNER = 0x10122C
};

enum {
    CASE_INPUTS = 4,  /* the most inputs a self test case passes */
    SECONDS_INPUT = 3 /* next_phase's secs, its fourth input */
};

typedef enum {
    PHASE_READY = 0,
    PHASE_PLAY,
    PHASE_OVER
} phase_t;

typedef enum {
    CHECK_SECONDS_LEFT = 0,
    CHECK_NEXT_PHASE
} check_t;

/* One line in the middle banner: its Text and what it shows. */
typedef struct {
    bento_game_text_t text;
    const char *message;
} banner_line_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_sprite_t fruits[FRUITS];
    bento_game_box_t basket;
    bento_game_sprite_t ships[LIVES];
    bento_game_text_t hud_score;
    bento_game_text_t hud_time;
    bento_game_box_t banner;
    banner_line_t ready_lines[BANNER_LINES];
    banner_line_t over_lines[BANNER_LINES];
    bento_real_t fall[FRUITS];
    phase_t phase;
    int32_t count;
    int32_t frame;
    int32_t score;
    int32_t lives;
    int32_t shown_score; /* -1: nothing shown yet */
    int32_t shown_time;
} hud_game_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the
 * loader on every load, and set again at the start of main()
 * (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern hud_game_t g_hud;
hud_game_t g_hud;

static int32_t floor_div(int32_t numerator, int32_t divisor)
{
    int32_t quotient = numerator / divisor;

    if (((numerator % divisor) != 0) &&
        ((numerator < 0) != (divisor < 0))) {
        quotient -= 1;
    }
    return quotient;
}

/* ปัดขึ้น: เฟรมแรกยังเห็น 30 และเห็น 0 ตอนหมดเวลาจริงเท่านั้น */
static int32_t seconds_left(int32_t frame, int32_t total, int32_t fps)
{
    const int32_t seconds = floor_div((total - frame) + fps - 1, fps);

    return (seconds > 0) ? seconds : 0;
}

/* เครื่องสถานะของรอบเกม  ready -> play -> over */
static phase_t next_phase(phase_t phase, int32_t ready_count,
                          int32_t lives, int32_t seconds)
{
    phase_t next = phase;

    if ((phase == PHASE_READY) && (ready_count <= 0)) {
        next = PHASE_PLAY;
    } else if ((phase == PHASE_PLAY) &&
               ((lives <= 0) || (seconds <= 0))) {
        next = PHASE_OVER;
    } else {
        /* stay */
    }
    return next;
}

/* กลางจอ = กึ่งกลางจอ - ครึ่งความยาวข้อความ */
static int32_t centre_x(const char *message)
{
    return (BENTO_GAME_WIDTH / 2) -
           ((int32_t)strlen(message) * CHAR_HALF_PX);
}

static const char *phase_name(int32_t phase)
{
    static const char *const names[] = {"ready", "play", "over"};

    return names[phase];
}

/* One self test case: what it calls, its four inputs, and the wanted
 * answer (a phase_t for next_phase). */
typedef struct {
    check_t check;
    int32_t inputs[CASE_INPUTS];
    int32_t want;
} test_case_t;

static int32_t run_case(const test_case_t *test)
{
    int32_t got;

    if (test->check == CHECK_SECONDS_LEFT) {
        got = seconds_left(test->inputs[0], test->inputs[1],
                           test->inputs[2]);
    } else {
        got = (int32_t)next_phase((phase_t)test->inputs[0],
                                  test->inputs[1], test->inputs[2],
                                  test->inputs[SECONDS_INPUT]);
    }
    return got;
}

static void print_failure(const test_case_t *test, int32_t got)
{
    if (test->check == CHECK_SECONDS_LEFT) {
        (void)printf("ยังไม่ผ่าน: seconds_left ได้ %" PRId32
                     " ควรได้ %" PRId32 "\n",
                     got, test->want);
    } else {
        (void)printf("ยังไม่ผ่าน: next_phase ได้ %s ควรได้ %s\n",
                     phase_name(got), phase_name(test->want));
    }
}

/* ตรวจสมองก่อนเปิดจอ */
static bool self_test(void)
{
    static const test_case_t cases[] = {
        {CHECK_SECONDS_LEFT, {0, 900, 30, 0}, 30},
        {CHECK_SECONDS_LEFT, {29, 900, 30, 0}, 30},
        {CHECK_SECONDS_LEFT, {899, 900, 30, 0}, 1},
        {CHECK_SECONDS_LEFT, {900, 900, 30, 0}, 0},
        {CHECK_NEXT_PHASE, {PHASE_READY, 0, 3, 30}, PHASE_PLAY},
        {CHECK_NEXT_PHASE, {PHASE_PLAY, 0, 0, 12}, PHASE_OVER},
        {CHECK_NEXT_PHASE, {PHASE_PLAY, 0, 2, 0}, PHASE_OVER},
        {CHECK_NEXT_PHASE, {PHASE_PLAY, 0, 2, 5}, PHASE_PLAY}};
    const uint32_t count = (uint32_t)(sizeof(cases) / sizeof(cases[0]));
    bool passed = true;
    uint32_t i;

    for (i = 0U; passed && (i < count); i++) {
        const int32_t got = run_case(&cases[i]);

        if (got != cases[i].want) {
            print_failure(&cases[i], got);
            passed = false;
        }
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของ HUD ถูกทั้ง %" PRIu32 " กรณี\n", count);
    }
    return passed;
}

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

/* title() calls start(), whose joystick.init() prints this line on
 * the kits and nothing on the reTerminal. */
static void report_joystick(const bento_game_t *game)
{
    if ((game->joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
}

/* Made empty: show_banner() writes the message when it shows. */
static bento_status_t add_line(hud_game_t *hud, banner_line_t *line,
                               const char *message, int32_t y,
                               uint32_t color)
{
    line->message = message;
    return bento_game_text_create(&hud->game, &line->text, "",
                                  centre_x(message), y, color);
}

static bento_status_t add_ships(hud_game_t *hud)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < LIVES); i++) {
        const int32_t x = BENTO_GAME_WIDTH - SHIP_MARGIN -
                          ((LIVES - i) * ((int32_t)SHIP_W + SHIP_GAP));

        status = bento_game_sprite_create(&hud->game, &hud->ships[i],
                                          "ship", (bento_real_t)x,
                                          (bento_real_t)SHIP_Y);
    }
    return status;
}

/* The fruit first, so it drops in from under the HUD bar. */
static bento_status_t add_field(hud_game_t *hud)
{
    bento_game_t *game = &hud->game;
    bento_game_box_t bar;
    bento_game_box_t ground;
    bento_status_t status =
        bento_game_pool(game, hud->fruits, FRUITS, "snake_food");

    bento_game_box_init(&bar);
    bento_game_box_init(&ground);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &bar, (bento_real_t)0, (bento_real_t)0,
            (bento_real_t)BENTO_GAME_WIDTH, (bento_real_t)HUD_BAR_H,
            COLOR_HUD_BAR);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &ground, (bento_real_t)0, (bento_real_t)GROUND_Y,
            (bento_real_t)BENTO_GAME_WIDTH, (bento_real_t)GROUND_H,
            BENTO_GAME_GB_DARK);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &hud->basket, (bento_real_t)BASKET_X,
            (bento_real_t)GROUND_Y - (bento_real_t)BASKET_LIFT,
            (bento_real_t)BASKET_W, (bento_real_t)BASKET_H,
            BENTO_GAME_ORANGE, (int32_t)BENTO_GAME_WHITE, BASKET_RADIUS,
            0);
    }
    if (status == BENTO_OK) {
        status = add_ships(hud);
    }
    return status;
}

static bento_status_t build_screen(hud_game_t *hud)
{
    bento_game_t *game = &hud->game;
    bento_status_t status = add_field(hud);

    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &hud->hud_score, "",
                                        HUD_X, HUD_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &hud->hud_time, "",
                                        centre_x("TIME 30"), HUD_Y,
                                        BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &hud->banner, (bento_real_t)BANNER_X,
            (bento_real_t)BANNER_Y, (bento_real_t)BANNER_W,
            (bento_real_t)BANNER_H, COLOR_BANNER,
            (int32_t)BENTO_GAME_YELLOW, BANNER_RADIUS, BANNER_BORDER_W);
    }
    if (status == BENTO_OK) {
        status = add_line(hud, &hud->ready_lines[0], "READY",
                          HEADLINE_Y, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = add_line(hud, &hud->ready_lines[1], "catch the fruit",
                          SUBLINE_Y, BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = add_line(hud, &hud->over_lines[0], "GAME OVER",
                          HEADLINE_Y, BENTO_GAME_RED);
    }
    if (status == BENTO_OK) {
        status = add_line(hud, &hud->over_lines[1], "A = play again",
                          SUBLINE_Y, BENTO_GAME_WHITE);
    }
    return status;
}

/* กฎ 3: ข่าวใหญ่ขึ้นกลางจอในกรอบเด่น แล้วหายไปเมื่อหมดข่าว */
static bento_status_t show_banner(hud_game_t *hud,
                                  const banner_line_t *lines, bool on)
{
    bento_status_t status =
        on ? bento_game_box_show(&hud->game, &hud->banner)
           : bento_game_box_hide(&hud->game, &hud->banner);
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < BANNER_LINES); i++) {
        status = bento_game_text_set(&hud->game, &lines[i].text,
                                     on ? lines[i].message : "");
    }
    return status;
}

/* The x, then the height above the screen, then the speed: three
 * draws in Python's order. */
static bento_status_t drop(hud_game_t *hud, int32_t i)
{
    int32_t x = 0;
    int32_t height = 0;
    int32_t fall = 0;
    bento_status_t status = bento_random_randint(
        &hud->random, DROP_X_MIN, BENTO_GAME_WIDTH - DROP_X_RIGHT, &x);

    if (status == BENTO_OK) {
        status = bento_random_randint(&hud->random, DROP_Y_MIN,
                                      DROP_Y_MAX, &height);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(&hud->game, &hud->fruits[i],
                                           (bento_real_t)x,
                                           (bento_real_t)-height);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(&hud->random, FALL_MIN, FALL_MAX,
                                      &fall);
    }
    if (status == BENTO_OK) {
        hud->fall[i] = (bento_real_t)fall / (bento_real_t)FALL_SCALE;
    }
    return status;
}

static bento_status_t new_round(hud_game_t *hud)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    hud->phase = PHASE_READY;
    hud->count = READY_FRAMES;
    hud->frame = 0;
    hud->score = 0;
    hud->lives = LIVES;
    for (i = 0; (status == BENTO_OK) && (i < LIVES); i++) {
        status = bento_game_sprite_show(&hud->game, &hud->ships[i]);
    }
    for (i = 0; (status == BENTO_OK) && (i < FRUITS); i++) {
        status = drop(hud, i);
        if (status == BENTO_OK) {
            status =
                bento_game_sprite_show(&hud->game, &hud->fruits[i]);
        }
    }
    if (status == BENTO_OK) {
        status = show_banner(hud, hud->over_lines, false);
    }
    if (status == BENTO_OK) {
        status = show_banner(hud, hud->ready_lines, true);
    }
    return status;
}

static bool fruit_in_basket(const hud_game_t *hud, int32_t i)
{
    const bento_game_sprite_t *fruit = &hud->fruits[i];
    const bento_game_box_t *basket = &hud->basket;

    return bento_game_hit_rect(fruit->x, fruit->y,
                               (bento_real_t)FRUIT_PX,
                               (bento_real_t)FRUIT_PX, basket->x,
                               basket->y, basket->w, basket->h);
}

/* ตกถึงพื้น = ชีวิตหายหนึ่ง ที่มุมเดิม แล้วปล่อยผลไม้ใหม่ */
static bento_status_t lose_life(hud_game_t *hud, int32_t i)
{
    bento_status_t status = BENTO_OK;

    if (hud->lives > 0) {
        hud->lives -= 1;
        status =
            bento_game_sprite_hide(&hud->game, &hud->ships[hud->lives]);
        bento_game_sfx(&hud->game, "lose_life");
    }
    if (status == BENTO_OK) {
        status = drop(hud, i);
    }
    return status;
}

/* รับได้ = คะแนน · ตกถึงพื้น = ชีวิตหายหนึ่ง ที่มุมเดิม */
static bento_status_t move_fruit(hud_game_t *hud, int32_t i)
{
    bento_game_sprite_t *fruit = &hud->fruits[i];
    bento_status_t status = bento_game_sprite_move_to(
        &hud->game, fruit, fruit->x, fruit->y + hud->fall[i]);

    if (status == BENTO_OK) {
        if (fruit_in_basket(hud, i)) {
            hud->score += 1;
            bento_game_sfx(&hud->game, "eat");
            status = drop(hud, i);
        } else if (fruit->y >
                   ((bento_real_t)GROUND_Y - (bento_real_t)FRUIT_PX)) {
            status = lose_life(hud, i);
        } else {
            /* still falling */
        }
    }
    return status;
}

static bento_status_t play_frame(hud_game_t *hud,
                                 const bento_game_keys_t *keys)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    if (keys->left) {
        status = bento_game_box_move(&hud->game, &hud->basket,
                                     (bento_real_t)-BASKET_SPEED,
                                     (bento_real_t)0);
    }
    if ((status == BENTO_OK) && keys->right) {
        status = bento_game_box_move(&hud->game, &hud->basket,
                                     (bento_real_t)BASKET_SPEED,
                                     (bento_real_t)0);
    }
    if (hud->phase == PHASE_READY) {
        hud->count -= 1;
    } else {
        hud->frame += 1;
        for (i = 0; (status == BENTO_OK) && (i < FRUITS); i++) {
            status = move_fruit(hud, i);
        }
    }
    return status;
}

/* กฎ 2: เปลี่ยนเมื่อไรค่อยเขียน (เวลาเปลี่ยนวินาทีละครั้ง) */
static bento_status_t show_hud(hud_game_t *hud, int32_t seconds)
{
    char text[TEXT_CHARS];
    bento_status_t status = BENTO_OK;

    if (hud->score != hud->shown_score) {
        (void)snprintf(text, sizeof(text), "SCORE %" PRId32,
                       hud->score);
        status = bento_game_text_set(&hud->game, &hud->hud_score, text);
        hud->shown_score = hud->score;
    }
    if ((status == BENTO_OK) && (seconds != hud->shown_time)) {
        (void)snprintf(text, sizeof(text), "TIME %" PRId32, seconds);
        status = bento_game_text_set(&hud->game, &hud->hud_time, text);
        hud->shown_time = seconds;
        if ((hud->phase == PHASE_PLAY) && (seconds > 0) &&
            (seconds <= COUNTDOWN_SECONDS)) {
            bento_game_sfx(&hud->game, "move");
        }
    }
    return status;
}

static bento_status_t change_phase(hud_game_t *hud, int32_t seconds)
{
    const phase_t phase =
        next_phase(hud->phase, hud->count, hud->lives, seconds);
    bento_status_t status = BENTO_OK;

    if (phase != hud->phase) {
        hud->phase = phase;
        if (phase == PHASE_PLAY) {
            status = show_banner(hud, hud->ready_lines, false);
            bento_game_sfx(&hud->game, "select");
        } else {
            status = show_banner(hud, hud->over_lines, true);
            bento_game_sfx(&hud->game, "gameover");
        }
    }
    return status;
}

static bento_status_t on_frame(void *user, bool *out_stop)
{
    /* the state is the file-scope g_hud (no void * conversion) */
    hud_game_t *hud = &g_hud;
    bento_game_keys_t keys;
    bento_status_t status;

    (void)user;
    *out_stop = false; /* never ends the game loop by itself */
    bento_game_keys_init(&keys);
    status = bento_game_keys(&hud->game, &keys);
    if (status != BENTO_OK) {
        /* keys() never fails; nothing to do */
    } else if (hud->phase == PHASE_OVER) {
        /* จบแล้ว รอ A อย่างเดียว */
        if (bento_game_pressed_once(&hud->game, BENTO_GAME_KEY_A,
                                    &keys)) {
            status = new_round(hud);
        }
    } else {
        int32_t seconds;

        status = play_frame(hud, &keys);
        seconds = seconds_left(hud->frame, TOTAL_FRAMES, FPS);
        if (status == BENTO_OK) {
            status = show_hud(hud, seconds);
        }
        if (status == BENTO_OK) {
            status = change_phase(hud, seconds);
        }
    }
    return status;
}

static void reset_state(hud_game_t *hud)
{
    int32_t i;

    (void)memset(hud, 0, sizeof(*hud));
    bento_game_init(&hud->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    for (i = 0; i < FRUITS; i++) {
        bento_game_sprite_init(&hud->fruits[i]);
    }
    for (i = 0; i < LIVES; i++) {
        bento_game_sprite_init(&hud->ships[i]);
    }
    for (i = 0; i < BANNER_LINES; i++) {
        bento_game_text_init(&hud->ready_lines[i].text);
        bento_game_text_init(&hud->over_lines[i].text);
    }
    bento_game_box_init(&hud->basket);
    bento_game_box_init(&hud->banner);
    bento_game_text_init(&hud->hud_score);
    bento_game_text_init(&hud->hud_time);
    hud->phase = PHASE_READY;
    hud->shown_score = -1;
    hud->shown_time = -1;
}

int main(void)
{
    hud_game_t *hud = &g_hud;
    bool quit = false;
    bento_status_t status;

    reset_state(hud);
    status = bento_random_init(&hud->random);
    /* a failed self test is SystemExit: the program ends with 0 */
    if ((status == BENTO_OK) && !self_test()) {
        quit = true;
    }
    if ((status == BENTO_OK) && !quit) {
        status = bento_game_title(&hud->game, "GAME HUD", NULL, &quit);
        report_joystick(&hud->game);
    }
    if ((status == BENTO_OK) && !quit) {
        status = build_screen(hud);
    }
    if ((status == BENTO_OK) && !quit) {
        status = new_round(hud);
    }
    if ((status == BENTO_OK) && !quit) {
        bool finished = false;

        status =
            bento_game_run(&hud->game, on_frame, NULL, FPS, &finished);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
