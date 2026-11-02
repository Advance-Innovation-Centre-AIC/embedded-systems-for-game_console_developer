/*
 * s04_thai.c -- ภาษาไทยบนจอเกม (คาบ 4 ของแถม, ฉบับฝึก เติมช่องว่าง)
 *
 * Twin of s04_thai.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/s04_thai.py).
 *
 * โชว์ข้อความภาษาไทยบนจอจริง หน้าเริ่ม "เกมภาษาไทย" รอกด Start แล้ววางป้าย
 * game.Text ขนาดมาตรฐาน 3 ป้าย กับ ui.Label ที่เลือกขนาดตัวอักษรได้ (14, 20, 28)
 * และคำยากที่มีสระซ้อน/วรรณยุกต์อีก 2 คำ จากนั้นวิ่งทุกเฟรม กด A = บวกแต้ม
 * ช่องว่าง 2 จุด ("เติม:") ยังเป็น pass จึงยังไม่มีป้ายคำที่ 3 และป้ายคะแนนไม่เปลี่ยน
 * กด Back เพื่อออก
 *
 * Shows Thai text on the game screen: the title "เกมภาษาไทย" waits for
 * START, then three game.Text labels at the standard size, three
 * ui.Label labels at chosen font sizes (14, 20, 28) and two hard words
 * with stacked vowels and tone marks. Then a frame loop: A adds a
 * point. Both blanks are still `pass`, so there is no extra word and
 * the score label never changes. BACK quits.
 *
 * C twin of the Example Explorer program devi-s04-thai
 * (py/devi-s04-thai.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - ui.Label(value=size) is bento_ui_label_create with font_px; its
 *     failure raises in MicroPython, so here main returns its status;
 *   - the module's globals (the three Texts, points) and the label ids
 *     live in one file-scope g_thai, which update() reads (C9);
 *   - title()'s BACK (SystemExit) and Stop (KeyboardInterrupt) end
 *     main with 0;
 *   - main prints the joystick line that the kits' joystick.init()
 *     prints inside title() (the reTerminal prints nothing);
 *   - blanks 1 and 2 stay empty, as in the practise file.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_game.h"
#include "bento_gpio.h"
#include "bento_runtime.h"

enum { /* game.run(update): the default frame rate */
    FPS = 30
};
enum { /* game.Text places: the title, the score, the hint */
    TITLE_X = 250,
    TITLE_Y = 40,
    SCORE_X = 20,
    SCORE_Y = 20,
    HINT_X = 230,
    HINT_Y = 360
};
enum { /* ui.Label rows: the size ladder, then the hard words */
    LABEL_COUNT = 5
};

typedef struct {
    bento_game_t game;
    bento_game_text_t title;
    bento_game_text_t score;
    bento_game_text_t hint;
    bento_ui_id_t labels[LABEL_COUNT];
    int32_t points;
} thai_game_t;

typedef struct {
    const char *text;
    int16_t x;
    int16_t y;
    uint32_t color;
    int32_t font_px;
} label_spec_t;

/* Is this the reTerminal? Asked at run time, by the name the board
 * gives, so one Emulator build serves both of its board models. A
 * board that cannot say keeps the build target's answer. */
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

/* The module's globals and the screen's widgets: the frame callback
 * reads them, and bento_game_t alone is too big for the 8 KB program
 * stack. File scope, zeroed by the loader on every load, and set again
 * at the start of main() (CLEAN_C.md C9, BENTO-MISRA-E03/E04). */
extern thai_game_t g_thai;
thai_game_t g_thai;

static bento_status_t add_label(bento_ui_t *ui,
                                const label_spec_t *spec,
                                bento_ui_id_t *label)
{
    bento_ui_widget_args_t args;

    bento_ui_widget_args_init(&args);
    args.text = spec->text;
    args.text_len = (uint32_t)strlen(spec->text);
    args.x = spec->x;
    args.y = spec->y;
    args.color = spec->color;
    args.font_px = spec->font_px;
    return bento_ui_label_create(ui, &args, label);
}

/* three game.Text labels at the standard size */
static bento_status_t add_texts(thai_game_t *thai)
{
    bento_status_t status = BENTO_OK;

    status =
        bento_game_text_create(&thai->game, &thai->title, "เกมเก็บของ",
                               TITLE_X, TITLE_Y, BENTO_GAME_CYAN);
    if (status == BENTO_OK) {
        status = bento_game_text_create(&thai->game, &thai->score,
                                        "คะแนน: 0", SCORE_X, SCORE_Y,
                                        BENTO_GAME_WHITE);
    }
    if (status == BENTO_OK) {
        status = bento_game_text_create(
            &thai->game, &thai->hint, "กดจอยซ้าย-ขวา รับของให้ทัน", HINT_X,
            HINT_Y, BENTO_GAME_GB_LIGHTEST);
    }
    return status;
}

/* the font size ladder, then the words the firmware must stack */
static bento_status_t add_sized_labels(thai_game_t *thai)
{
    static const label_spec_t labels[LABEL_COUNT] = {
        {"เล็ก 14", 120, 140, BENTO_GAME_BLUE, 14},
        {"กลาง 20", 120, 180, BENTO_GAME_BLUE, 20},
        {"ใหญ่ 28", 120, 230, BENTO_GAME_YELLOW, 28},
        {"ปั๊มน้ำ", 460, 150, BENTO_GAME_PINK, 24},
        {"เกี๊ยวกุ้ง", 460, 200, BENTO_GAME_GREEN, 24}};
    bento_status_t status = BENTO_OK;

    for (uint32_t i = 0U;
         (i < (uint32_t)LABEL_COUNT) && (status == BENTO_OK); i++) {
        status = add_label(thai->game.ui, &labels[i], &thai->labels[i]);
    }
    /* blank 1: the student's own hard word at (460, 250), ORANGE */
    return status;
}

/* One frame: A adds a point (a test that the Thai label can change) */
static bento_status_t update(void *user, bool *out_stop)
{
    thai_game_t *thai = &g_thai;
    bento_game_keys_t pressed;

    (void)user;        /* the state is g_thai (file scope, C9) */
    *out_stop = false; /* the original never returns False */
    bento_game_keys_init(&pressed);
    /* keys() never fails for valid pointers */
    (void)bento_game_keys(&thai->game, &pressed);
    if (pressed.a) {
        thai->points += 1;
        /* blank 2: set the score label to "คะแนน: %d" */
    }
    return BENTO_OK;
}

int main(void)
{
    thai_game_t *thai = &g_thai;
    bool quit = false;
    bool finished = false;
    bento_status_t status = BENTO_OK;

    (void)memset(thai, 0, sizeof(*thai));
    bento_game_init(&thai->game, bento_runtime_get_ui(),
                    bento_runtime_is_running);
    bento_game_text_init(&thai->title);
    bento_game_text_init(&thai->score);
    bento_game_text_init(&thai->hint);
    status = bento_game_title(&thai->game, "เกมภาษาไทย", NULL, &quit);
    /* the kits' joystick.init() prints this line; the reTerminal's
     * joystick.py prints nothing */
    if ((thai->game.joystick_init == BENTO_OK) && !on_reterminal()) {
        (void)printf("joystick: USB Host HID initialized on CM55\n");
    }
    if ((status == BENTO_OK) && !quit) {
        status = add_texts(thai);
        if (status == BENTO_OK) {
            status = add_sized_labels(thai);
        }
        thai->points = 0;
        if (status == BENTO_OK) {
            status = bento_game_run(&thai->game, update, NULL, FPS,
                                    &finished);
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
