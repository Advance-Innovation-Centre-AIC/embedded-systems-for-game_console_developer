/*
 * s13_13_reskin_farm.c -- คาบ 13 (คาบพิเศษ): เอาภาพเดิมไปเล่าเรื่องใหม่ = โดรนพ่นยากันแมลงในแปลงผัก
 *
 * ภาพเดิม ความหมายใหม่: ship = โดรนพ่นยา, enemy / enemy2 / enemy3 = แมลงศัตรูพืช 3 แบบ,
 * snake_food = ผลผลิต, boom1-boom6 = แมลงโดนยา, เสียง flap = เสียงพ่นละออง
 * ตรรกะคือ Shooter เดิมทั้งหมด กด Start แล้วใช้จอย 4 ทิศบินโดรน กด A พ่นยา (ละอองตกลงตรง ๆ)
 * แมลงคลานไปตามแถวผัก เจอผลไหนจะหยุดกัดราว 3.5 วินาทีแล้วผลนั้นหายไป
 * พ่นโดนแมลง = ภาพระเบิดแวบหนึ่งแล้วหาย ตัวใหม่คลานเข้ามาแทน กำจัดครบ = FARM SAFE!
 *
 * The Shooter logic told as a new story: a crop-spraying drone over a vegetable row. Three
 * pests at a time crawl along the row (a pool of 15 in all), stop to chew a crop for 3.5 s
 * and eat it; a spray that hits a pest turns it into a two-frame explosion and the next one
 * crawls in. All crops lost = CROPS LOST, every pest gone = FARM SAFE!, then A plays again.
 * The pure logic (clamp, crop_under, verdict) is checked once before the screen opens.
 *
 * Twin of s13_13_reskin_farm.py (embedded-systems-for-game_console_developer/developer-i/
 * solution_codes/s13_13_reskin_farm.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - the st dict and the per-pest lists are one farm_t, at file scope because it is large;
 *   - a pest's state string ("walk", "chew", "boom", "gone") and verdict()'s answer are enums;
 *   - a sprite keeps no w and h in bento_game, so each pest keeps its own size for the hit test;
 *   - the drone's position is an int32_t, as in Python, where it only ever adds whole numbers;
 *   - raise SystemExit after a failed self test ends main() with 0.
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

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
enum {
    CROPS = 7,        /* ผลผลิตในแถว */
    PESTS_TOTAL = 15, /* แมลงทั้งรอบ */
    ON_SCREEN = 3,    /* แมลงบนจอพร้อมกันกี่ตัว (ขนาดชุดหมุนเวียน) */
    BOOM_FRAMES = 10, /* ภาพระเบิดค้างกี่เฟรม */
    BITE = 105,       /* แมลงกัดผลหนึ่งผลนานกี่เฟรม (105 / 30 = 3.5 วินาที) */
    SPRAYS = 3,       /* ละอองในชุดหมุนเวียน */
    SPRAY_COOLDOWN = 6, /* พ่นได้ทุก 6 เฟรม */
    SPRAY_SPEED = 7,    /* ละอองตกเฟรมละกี่ px */
    DRONE_SPEED = 6,
    LANE_Y = 290, /* แนวแมลงคลาน */
    CROP_Y = 304, /* ผัก */
    SOIL_Y = 300  /* ดิน */
};
/* ---- end of settings ---- */

/* The round. */
enum {
    KINDS = 3,
    FPS = 30
};

/* Where things are, in px. */
enum {
    SOIL_H = 34,
    CROP_X0 = 96,
    CROP_STEP = 100,
    CROP_HALF = 8,  /* the crop picture's half width: its centre */
    CROP_REACH = 1, /* a pest within 1 px of a crop's centre chews it */
    PEST_OFF =
        20, /* a pest enters 20 px off the edge and turns 20 px before it */
    CRAWL_FRAMES = 6, /* the crawl bobs 2 px every 6 frames */
    BOOM_SHIFT =
        6, /* the 29 x 24 explosion is bigger than a pest: centre it again */
    CHEW_STEP = 3,    /* a pest steps 3 px past the crop it ate */
    SPRAY_PARK = -20, /* a free spray waits off screen */
    SPRAY_W = 6,
    SPRAY_H = 12,
    SPRAY_RADIUS = 3,
    SPRAY_FROM_DRONE = 28, /* the nozzle under the drone */
    SPRAY_GONE_Y = SOIL_Y + 10,
    DRONE_X0 = 365,
    DRONE_Y0 = 120,
    DRONE_W = 62,
    DRONE_TOP = 64,
    DRONE_BOTTOM = 220,
    HUD_X = 12,
    HUD_Y = 8,
    HUD_PESTS_X = 640,
    LEGEND_X = 200,
    LEGEND_Y = 40,
    BANNER_X = 246,
    BANNER_Y = 150,
    BANNER_W = 300,
    BANNER_H = 84,
    BANNER_RADIUS = 8,
    BANNER_EDGE = 3,
    BIG_LEFT = 50,
    BIG_Y = 168,
    SMALL_LEFT = 70,
    SMALL_Y = 200,
    SPEED_MIN_TENTHS = 6, /* a pest crawls 0.6 .. 1.2 px a frame */
    SPEED_MAX_TENTHS = 12,
    TENTHS = 10,
    HUD_CHARS =
        32 /* "SAVED " and two int32_t of 11 characters, '/' and the NUL */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_FIELD = 0x16301C,
    COLOR_SOIL = 0x5A3A22,
    COLOR_LEGEND = 0x9FC9A0,
    COLOR_BANNER = 0x10122C
};

typedef enum {
    PEST_WALK = 0,
    PEST_CHEW,
    PEST_BOOM,
    PEST_GONE
} pest_state_t;

typedef enum {
    VERDICT_NONE = 0,
    VERDICT_LOSE,
    VERDICT_WIN
} verdict_t;

/* verdict()'s answer as Python prints it, by verdict_t. */

/* The explosion pictures of each pest kind: the small flash, then the big one. */
static const char *const BOOM_NAMES[KINDS][2] = {
    {"boom1", "boom2"}, {"boom3", "boom4"}, {"boom5", "boom6"}};

typedef struct {
    int32_t kind;
    pest_state_t state;
    int32_t timer;
    int32_t target;
    int32_t direction;
    bento_real_t speed;
    int32_t width_px;
    int32_t height_px;
} pest_t;

typedef struct {
    bento_game_t game;
    bento_random_t random;
    bento_game_sprite_t crops[CROPS];
    bento_game_sprite_t pest_sprites[ON_SCREEN];
    pest_t pests[ON_SCREEN];
    bento_game_box_t sprays[SPRAYS];
    bento_game_sprite_t drone;
    bento_game_text_t hud_saved;
    bento_game_text_t hud_pests;
    bento_game_box_t banner;
    bento_game_text_t big;
    bento_game_text_t small;
    bool alive[CROPS];
    int32_t drone_x;
    int32_t drone_y;
    int32_t saved;
    int32_t left;
    int32_t queue;
    int32_t cool;
    int32_t tick;
    bool over;
    int32_t shown_saved; /* -1 = nothing shown yet (Python's None) */
    int32_t shown_pests;
} farm_t;

/* Too big for the 8 KB program stack: file scope, zeroed by the loader on every load, and set
 * again at the start of main() (CLEAN_C.md C9, deviation BENTO-MISRA-E03/E04). */
extern farm_t g_farm;
farm_t g_farm;

/* ---- the brain: pure, no screen ---- */

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    const int32_t below = (value < high) ? value : high;

    return (below > low) ? below : low;
}

static int32_t crop_x(int32_t crop)
{
    return CROP_X0 + (crop * CROP_STEP);
}

/* The crop whose centre is within 1 px under the pest's centre and not yet eaten; -1 = none. */
static int32_t crop_under(bento_real_t center_x,
                          const bool alive[CROPS])
{
    int32_t found = -1;
    int32_t i;

    for (i = 0; (i < CROPS) && (found < 0); i++) {
        const bento_real_t distance =
            center_x - (bento_real_t)(crop_x(i) + CROP_HALF);
        const bento_real_t away =
            (distance < (bento_real_t)0) ? -distance : distance;

        if (alive[i] && (away <= (bento_real_t)CROP_REACH)) {
            found = i;
        }
    }
    return found;
}

static verdict_t verdict(int32_t saved, int32_t pests_left)
{
    verdict_t answer = VERDICT_NONE;

    if (saved == 0) {
        answer = VERDICT_LOSE;
    } else if (pests_left == 0) {
        answer = VERDICT_WIN;
    } else {
        /* the round goes on */
    }
    return answer;
}

/* ---- check the brain before the screen opens ---- */

static bool check_number(int32_t got, int32_t want)
{
    if (got != want) {
        (void)printf("ยังไม่ผ่าน: ได้ %d ควรได้ %d\n", (int)got, (int)want);
    }
    return got == want;
}

static bool check_verdict(verdict_t got, verdict_t want)
{
    static const char *const verdict_text[] = {"", "lose", "win"};

    if (got != want) {
        (void)printf("ยังไม่ผ่าน: ได้ %s ควรได้ %s\n", verdict_text[got],
                     verdict_text[want]);
    }
    return got == want;
}

static bool test_crops(void)
{
    /* crop_under(center, alive): which crops are alive, the centre and the crop wanted */
    static const int32_t crop_cases[][2] = {
        {104, 0}, {104, -1}, {150, -1}};
    static const bool crop_alive[] = {true, false, true};
    const size_t case_count =
        sizeof(crop_alive) / sizeof(crop_alive[0]);
    bool alive[CROPS];
    bool passed = true;
    int32_t i;
    int32_t crop;

    for (i = 0; passed && (i < (int32_t)case_count); i++) {
        for (crop = 0; crop < CROPS; crop++) {
            alive[crop] = crop_alive[i];
        }
        passed = check_number(
            crop_under((bento_real_t)crop_cases[i][0], alive),
            crop_cases[i][1]);
    }
    return passed;
}

static bool self_test(void)
{
    /* clamp(800, 0, 730) = 730, then verdict(saved, pests_left) */
    static const int32_t clamp_case[] = {800, 0, 730};
    static const int32_t verdict_cases[][2] = {{0, 3}, {4, 0}, {4, 2}};
    static const verdict_t verdict_wanted[] = {
        VERDICT_LOSE, VERDICT_WIN, VERDICT_NONE};
    bool passed =
        check_number(clamp(clamp_case[0], clamp_case[1], clamp_case[2]),
                     clamp_case[2]);
    const size_t verdict_count =
        sizeof(verdict_wanted) / sizeof(verdict_wanted[0]);
    int32_t i;

    if (passed) {
        passed = test_crops();
    }
    for (i = 0; passed && (i < (int32_t)verdict_count); i++) {
        passed = check_verdict(
            verdict(verdict_cases[i][0], verdict_cases[i][1]),
            verdict_wanted[i]);
    }
    if (passed) {
        (void)printf("ผ่าน! สมองของแปลงผักถูกทุกกรณี\n");
    }
    return passed;
}

/* ---- the screen: made once (the field first, the drone last, so it flies over all) ---- */

static bento_status_t add_field(farm_t *farm)
{
    bento_game_t *game = &farm->game;
    bento_game_box_t field;
    bento_game_box_t soil;
    bento_status_t status;
    int32_t i;

    bento_game_box_init(&field);
    bento_game_box_init(&soil);
    status = bento_game_box_create(
        game, &field, (bento_real_t)0, (bento_real_t)0,
        (bento_real_t)game->width, (bento_real_t)game->height,
        COLOR_FIELD);
    if (status == BENTO_OK) {
        status = bento_game_box_create(
            game, &soil, (bento_real_t)0, (bento_real_t)SOIL_Y,
            (bento_real_t)game->width, (bento_real_t)SOIL_H,
            COLOR_SOIL);
    }
    for (i = 0; (status == BENTO_OK) && (i < CROPS); i++) {
        status = bento_game_sprite_create(
            game, &farm->crops[i], "snake_food",
            (bento_real_t)crop_x(i), (bento_real_t)CROP_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_pool(game, farm->pest_sprites,
                                 (uint32_t)ON_SCREEN, "enemy");
    }
    for (i = 0; (status == BENTO_OK) && (i < SPRAYS); i++) {
        bento_game_box_init(&farm->sprays[i]);
        status = bento_game_box_create_styled(
            game, &farm->sprays[i], (bento_real_t)SPRAY_PARK,
            (bento_real_t)SPRAY_PARK, (bento_real_t)SPRAY_W,
            (bento_real_t)SPRAY_H, BENTO_GAME_CYAN,
            BENTO_GAME_BORDER_NONE, SPRAY_RADIUS, 0);
    }
    return status;
}

static bento_status_t add_texts(farm_t *farm)
{
    bento_game_t *game = &farm->game;
    bento_game_text_t legend;
    bento_status_t status;

    bento_game_text_init(&farm->hud_saved);
    bento_game_text_init(&farm->hud_pests);
    bento_game_text_init(&legend);
    bento_game_text_init(&farm->big);
    bento_game_text_init(&farm->small);
    bento_game_box_init(&farm->banner);
    status = bento_game_text_create(game, &farm->hud_saved, "", HUD_X,
                                    HUD_Y, BENTO_GAME_WHITE);
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &farm->hud_pests, "",
                                        HUD_PESTS_X, HUD_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            game, &legend,
            "ship = โดรน   enemy = แมลง   snake_food = ผลผลิต", LEGEND_X,
            LEGEND_Y, COLOR_LEGEND);
    }
    if (status == BENTO_OK) {
        status = bento_game_box_create_styled(
            game, &farm->banner, (bento_real_t)BANNER_X,
            (bento_real_t)BANNER_Y, (bento_real_t)BANNER_W,
            (bento_real_t)BANNER_H, COLOR_BANNER,
            (int32_t)BENTO_GAME_YELLOW, BANNER_RADIUS, BANNER_EDGE);
    }
    /* the two messages are the same length, so one place serves both */
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &farm->big, "",
                                        (game->width / 2) - BIG_LEFT,
                                        BIG_Y, BENTO_GAME_YELLOW);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(game, &farm->small, "",
                                        (game->width / 2) - SMALL_LEFT,
                                        SMALL_Y, BENTO_GAME_WHITE);
    }
    return status;
}

static bento_status_t build_screen(farm_t *farm)
{
    bento_status_t status = add_field(farm);

    if (status == BENTO_OK) {
        farm->drone_x = DRONE_X0;
        farm->drone_y = DRONE_Y0;
        status = bento_game_sprite_create(
            &farm->game, &farm->drone, "ship", (bento_real_t)DRONE_X0,
            (bento_real_t)DRONE_Y0);
    }
    if (status == BENTO_OK) {
        status = add_texts(farm);
    }
    farm->shown_saved = -1;
    farm->shown_pests = -1;
    return status;
}

/* A new pest crawls in from the left or right edge, one of three looks at random. */
static bento_status_t spawn_pest(farm_t *farm, int32_t slot)
{
    static const char *const enemy_names[KINDS] = {"enemy", "enemy2",
                                                   "enemy3"};
    static const int32_t enemy_size[KINDS][2] = {
        {16, 14}, {21, 10}, {18, 14}};
    pest_t *pest = &farm->pests[slot];
    bento_game_sprite_t *sprite = &farm->pest_sprites[slot];
    uint32_t side = 0U;
    int32_t tenths = 0;
    bento_status_t status =
        bento_random_randint(&farm->random, 0, KINDS - 1, &pest->kind);

    if (status == BENTO_OK) {
        status = bento_random_choice(&farm->random, 2U, &side);
    }
    if (status == BENTO_OK) {
        status = bento_random_randint(&farm->random, SPEED_MIN_TENTHS,
                                      SPEED_MAX_TENTHS, &tenths);
    }
    if (status == BENTO_OK) {
        pest->direction = (side == 0U) ? 1 : -1;
        pest->speed = (bento_real_t)tenths / (bento_real_t)TENTHS;
        pest->state = PEST_WALK;
        status = bento_game_sprite_frame(&farm->game, sprite,
                                         enemy_names[pest->kind]);
    }
    if (status == BENTO_OK) {
        const int32_t start_x =
            (pest->direction > 0) ? -PEST_OFF : farm->game.width;

        pest->width_px = enemy_size[pest->kind][0];
        pest->height_px = enemy_size[pest->kind][1];
        status = bento_game_sprite_move_to(&farm->game, sprite,
                                           (bento_real_t)start_x,
                                           (bento_real_t)LANE_Y);
    }
    if (status == BENTO_OK) {
        status = bento_game_sprite_show(&farm->game, sprite);
    }
    return status;
}

/* Big news in the middle: a message shows the frame, an empty one hides it. */
static bento_status_t show_banner(farm_t *farm, const char *message,
                                  const char *sub)
{
    bento_status_t status =
        (message[0] != '\0')
            ? bento_game_box_show(&farm->game, &farm->banner)
            : bento_game_box_hide(&farm->game, &farm->banner);

    if (status == BENTO_OK) {
        status = bento_game_text_set(&farm->game, &farm->big, message);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_set(&farm->game, &farm->small, sub);
    }
    return status;
}

static bento_status_t new_round(farm_t *farm)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    farm->saved = CROPS;
    farm->left = PESTS_TOTAL;
    farm->queue = PESTS_TOTAL - ON_SCREEN;
    farm->cool = 0;
    farm->tick = 0;
    farm->over = false;
    for (i = 0; i < CROPS; i++) {
        farm->alive[i] = true;
    }
    for (i = 0; (status == BENTO_OK) && (i < CROPS); i++) {
        status = bento_game_sprite_show(&farm->game, &farm->crops[i]);
    }
    for (i = 0; (status == BENTO_OK) && (i < ON_SCREEN); i++) {
        status = spawn_pest(farm, i);
    }
    if (status == BENTO_OK) {
        status = show_banner(farm, "", "");
    }
    return status;
}

static bool pest_can_be_hit(const pest_t *pest)
{
    return (pest->state == PEST_WALK) || (pest->state == PEST_CHEW);
}

/* The spray hit this pest: it turns into an explosion. */
static bento_status_t burst_pest(farm_t *farm, int32_t slot)
{
    pest_t *pest = &farm->pests[slot];
    bento_game_sprite_t *sprite = &farm->pest_sprites[slot];
    bento_status_t status;

    pest->state = PEST_BOOM;
    pest->timer = BOOM_FRAMES;
    status = bento_game_sprite_frame(&farm->game, sprite,
                                     BOOM_NAMES[pest->kind][0]);
    if (status == BENTO_OK) {
        status = bento_game_sprite_move_to(
            &farm->game, sprite, sprite->x - (bento_real_t)BOOM_SHIFT,
            (bento_real_t)LANE_Y - (bento_real_t)BOOM_SHIFT);
    }
    farm->left -= 1;
    bento_game_sfx(&farm->game, "hit");
    return status;
}

/* One spray falls; a hit bursts the pest; a used spray goes back to the pool. */
static bento_status_t step_spray(farm_t *farm, bento_game_box_t *spray)
{
    bento_status_t status =
        bento_game_box_move_to(&farm->game, spray, spray->x,
                               spray->y + (bento_real_t)SPRAY_SPEED);
    bool used = spray->y > (bento_real_t)SPRAY_GONE_Y;
    int32_t i;

    for (i = 0; (status == BENTO_OK) && (i < ON_SCREEN); i++) {
        const pest_t *pest = &farm->pests[i];
        const bento_game_sprite_t *sprite = &farm->pest_sprites[i];

        if (!used && pest_can_be_hit(pest) &&
            bento_game_hit_rect(spray->x, spray->y, spray->w, spray->h,
                                sprite->x, sprite->y,
                                (bento_real_t)pest->width_px,
                                (bento_real_t)pest->height_px)) {
            status = burst_pest(farm, i);
            used = true;
        }
    }
    if ((status == BENTO_OK) && used) {
        status = bento_game_box_move_to(&farm->game, spray,
                                        (bento_real_t)SPRAY_PARK,
                                        (bento_real_t)SPRAY_PARK);
        if (status == BENTO_OK) {
            status = bento_game_box_hide(&farm->game, spray);
        }
    }
    return status;
}

static bento_status_t walk_pest(farm_t *farm, int32_t slot)
{
    pest_t *pest = &farm->pests[slot];
    bento_game_sprite_t *sprite = &farm->pest_sprites[slot];
    const bool past_left =
        (sprite->x < (bento_real_t)0) && (pest->direction < 0);
    const bool past_right =
        (sprite->x > (bento_real_t)(farm->game.width - PEST_OFF)) &&
        (pest->direction > 0);
    const int32_t bob = ((farm->tick / CRAWL_FRAMES) % 2) * 2;
    bento_real_t step;
    bento_status_t status;
    int32_t crop;

    if (past_left || past_right) {
        pest->direction =
            -pest->direction; /* the end of the row: turn round */
    }
    step = (bento_real_t)pest->direction * pest->speed;
    status =
        bento_game_sprite_move_to(&farm->game, sprite, sprite->x + step,
                                  (bento_real_t)(LANE_Y + bob));
    crop = crop_under(
        sprite->x + ((bento_real_t)pest->width_px / (bento_real_t)2),
        farm->alive);
    if (crop >= 0) {
        pest->state = PEST_CHEW;
        pest->timer = BITE;
        pest->target = crop;
    }
    return status;
}

static bento_status_t chew_pest(farm_t *farm, int32_t slot)
{
    pest_t *pest = &farm->pests[slot];
    bento_game_sprite_t *sprite = &farm->pest_sprites[slot];
    bento_status_t status = BENTO_OK;

    pest->timer -= 1;
    if (pest->timer == 0) {
        pest->state = PEST_WALK;
        if (farm->alive[pest->target]) {
            farm->alive[pest->target] = false;
            status = bento_game_sprite_hide(&farm->game,
                                            &farm->crops[pest->target]);
            farm->saved -= 1;
            bento_game_sfx(&farm->game, "lose_life");
        }
        if (status == BENTO_OK) {
            /* step past the crop it ate */
            status = bento_game_sprite_move_to(
                &farm->game, sprite,
                sprite->x + (bento_real_t)(pest->direction * CHEW_STEP),
                (bento_real_t)LANE_Y);
        }
    }
    return status;
}

static bento_status_t explode_pest(farm_t *farm, int32_t slot)
{
    pest_t *pest = &farm->pests[slot];
    bento_game_sprite_t *sprite = &farm->pest_sprites[slot];
    bento_status_t status = BENTO_OK;

    pest->timer -= 1;
    if (pest->timer == (BOOM_FRAMES / 2)) {
        /* the second half shows the big ring */
        status = bento_game_sprite_frame(&farm->game, sprite,
                                         BOOM_NAMES[pest->kind][1]);
    }
    if ((status == BENTO_OK) && (pest->timer == 0)) {
        if (farm->queue > 0) {
            /* a pest still waits: it crawls in */
            farm->queue -= 1;
            status = spawn_pest(farm, slot);
        } else {
            /* the queue is empty: this slot stays empty */
            pest->state = PEST_GONE;
            status = bento_game_sprite_hide(&farm->game, sprite);
        }
    }
    return status;
}

static bento_status_t step_pest(farm_t *farm, int32_t slot)
{
    bento_status_t status = BENTO_OK;

    switch (farm->pests[slot].state) {
    case PEST_WALK:
        status = walk_pest(farm, slot);
        break;
    case PEST_CHEW:
        status = chew_pest(farm, slot);
        break;
    case PEST_BOOM:
        status = explode_pest(farm, slot);
        break;
    default:
        /* a gone pest does nothing */
        break;
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

static bento_status_t fly_drone(farm_t *farm,
                                const bento_game_keys_t *keys)
{
    const int32_t dx = key_axis(keys->left, keys->right) * DRONE_SPEED;
    const int32_t dy = key_axis(keys->up, keys->down) * DRONE_SPEED;
    bento_status_t status = BENTO_OK;

    if ((dx != 0) || (dy != 0)) {
        farm->drone_x =
            clamp(farm->drone_x + dx, 0, farm->game.width - DRONE_W);
        farm->drone_y =
            clamp(farm->drone_y + dy, DRONE_TOP, DRONE_BOTTOM);
        status = bento_game_sprite_move_to(&farm->game, &farm->drone,
                                           (bento_real_t)farm->drone_x,
                                           (bento_real_t)farm->drone_y);
    }
    return status;
}

/* A free spray from the pool, if any, leaves the nozzle. */
static bento_status_t fire_spray(farm_t *farm)
{
    bento_status_t status = BENTO_OK;
    int32_t free_slot = -1;
    int32_t i;

    for (i = SPRAYS - 1; i >= 0; i--) {
        if (farm->sprays[i].y < (bento_real_t)0) {
            free_slot =
                i; /* counting down leaves the first free one, as Python's break */
        }
    }
    if (free_slot >= 0) {
        bento_game_box_t *spray = &farm->sprays[free_slot];

        status = bento_game_box_move_to(
            &farm->game, spray,
            (bento_real_t)(farm->drone_x + SPRAY_FROM_DRONE),
            (bento_real_t)(farm->drone_y + SPRAY_FROM_DRONE));
        if (status == BENTO_OK) {
            status = bento_game_box_show(&farm->game, spray);
        }
        bento_game_sfx(&farm->game, "flap");
        farm->cool = SPRAY_COOLDOWN;
    }
    return status;
}

/* The HUD is written only when its value changes. */
static bento_status_t show_hud(farm_t *farm)
{
    char text[HUD_CHARS];
    bento_status_t status = BENTO_OK;

    if (farm->saved != farm->shown_saved) {
        (void)snprintf(text, sizeof(text), "SAVED %d/%d",
                       (int)farm->saved, CROPS);
        status =
            bento_game_text_set(&farm->game, &farm->hud_saved, text);
        farm->shown_saved = farm->saved;
    }
    if ((status == BENTO_OK) && (farm->left != farm->shown_pests)) {
        (void)snprintf(text, sizeof(text), "PESTS %d", (int)farm->left);
        status =
            bento_game_text_set(&farm->game, &farm->hud_pests, text);
        farm->shown_pests = farm->left;
    }
    return status;
}

static bento_status_t end_if_decided(farm_t *farm)
{
    const verdict_t answer = verdict(farm->saved, farm->left);
    bento_status_t status = BENTO_OK;

    if (answer != VERDICT_NONE) {
        const bool won = (answer == VERDICT_WIN);

        farm->over = true;
        status = show_banner(farm, won ? "FARM SAFE!" : "CROPS LOST",
                             "A = play again");
        bento_game_sfx(&farm->game, won ? "win" : "gameover");
    }
    return status;
}

static bento_status_t play_frame(farm_t *farm,
                                 const bento_game_keys_t *keys)
{
    bento_status_t status;
    int32_t i;

    farm->tick += 1;
    status = fly_drone(farm, keys);
    farm->cool = (farm->cool > 0) ? (farm->cool - 1) : 0;
    if ((status == BENTO_OK) && keys->a && (farm->cool == 0)) {
        status = fire_spray(farm);
    }
    for (i = 0; (status == BENTO_OK) && (i < SPRAYS); i++) {
        if (farm->sprays[i].y >= (bento_real_t)0) {
            status = step_spray(farm, &farm->sprays[i]);
        }
    }
    for (i = 0; (status == BENTO_OK) && (i < ON_SCREEN); i++) {
        status = step_pest(farm, i);
    }
    if (status == BENTO_OK) {
        status = show_hud(farm);
    }
    if (status == BENTO_OK) {
        status = end_if_decided(farm);
    }
    return status;
}

/* One frame. on_frame always returns True, so *out_stop stays false. */
static bento_status_t update(void *user, bool *out_stop)
{
    /* the state lives at file scope (CLEAN_C.md C9), so user is not needed */
    farm_t *farm = &g_farm;
    bento_game_keys_t keys;
    bool again;
    bento_status_t status;

    (void)user;
    *out_stop =
        false; /* on_frame always returns True: the game goes on */
    bento_game_keys_init(&keys);
    (void)bento_game_keys(
        &farm->game,
        &keys); /* never fails: a silent pad is all keys up */
    /* read every frame: A held down at the end of a round is not a new press */
    again =
        bento_game_pressed_once(&farm->game, BENTO_GAME_KEY_A, &keys);
    if (farm->over) {
        status = again ? new_round(farm) : BENTO_OK;
    } else {
        status = play_frame(farm, &keys);
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
    farm_t *farm = &g_farm;
    bool quit = false;
    bento_status_t status = BENTO_OK;

    (void)memset(farm, 0, sizeof(*farm));
    if (self_test()) {
        status = bento_random_init(&farm->random);
        if (status == BENTO_OK) {
            bento_game_init(&farm->game, bento_runtime_get_ui(),
                            bento_runtime_is_running);
            status = bento_game_title(&farm->game, "FARM DRONE", NULL,
                                      &quit);
            report_joystick(&farm->game);
        }
        if ((status == BENTO_OK) && !quit) {
            status = build_screen(farm);
        }
        if ((status == BENTO_OK) && !quit) {
            status = new_round(farm);
        }
        if ((status == BENTO_OK) && !quit) {
            bool finished = false;

            status = bento_game_run(&farm->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
