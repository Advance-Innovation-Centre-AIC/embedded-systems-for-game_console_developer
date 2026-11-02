/*
 * s01_led_blink.c -- LED ดวงแรกของเรา (Blink Your First LED)
 *
 * Twin of s01_led_blink.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s01_led_blink.py)
 *
 * ถามบอร์ดว่ามี LED กี่ดวงแล้วพิมพ์ข้อมูลบอร์ด ล้างจอ Playground
 * ปักป้ายสามป้าย แล้วเล่นท่ากะพริบห้าท่าบน LED2 (เขียว) สองรอบ:
 * กะพริบช้า กะพริบเร็ว หัวใจเต้น รหัส SOS และไฟวิ่งข้ามทุกดวง
 * ป้ายบนจอบอกว่ากำลังเล่นท่าไหน จบแล้วดับไฟทุกดวง
 *
 * Asks the board how many LEDs it has and prints the board's info,
 * clears the Playground screen and puts up three labels, then plays
 * five blink figures on LED2 (green) twice: slow blink, fast blink,
 * heartbeat, SOS and a knight-rider sweep over every LED. The labels
 * say which figure runs; at the end every LED is off.
 *
 * C twin of the Example Explorer program devi-s01-led-blink
 * (py/devi-s01-led-blink.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - print(a, b, c) is printf with the spaces print() puts between;
 *   - print(gpio.board_info()) prints the dict in the PSoC Edge
 *     firmware's key order;
 *   - Stop during a sleep ends the program, as KeyboardInterrupt;
 *   - a call that raises returns its status and ends the program.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_time.h"

/* The LED, the rounds and how often each figure repeats. LED2 (green,
 * index 1): LED1 (red, index 0) is often the system's status light. */
enum {
    LED_GREEN = 1,
    ROUNDS = 2,
    SLOW_BLINKS = 10,
    FAST_TOGGLES = 25,
    HEARTBEATS = 8,
    BEATS = 2,
    SOS_REPEATS = 2,
    SWEEPS = 6
};

/* Timing, in milliseconds. */
enum {
    SLOW_MS = 500,
    FAST_MS = 100,
    BEAT_MS = 120,
    BEAT_REST_MS = 700,
    SOS_SHORT_MS = 150,
    SOS_LONG_MS = 500,
    SOS_GAP_MS = 200,
    SOS_REPEAT_GAP_MS = 800,
    SWEEP_STEP_MS = 120
};

/* Labels: position, font size in px, colour 0xRRGGBB. */
enum {
    LABEL_X = 190,
    HEADING_Y = 70,
    STATUS_Y = 150,
    HINT_Y = 210,
    HEADING_FONT_PX = 28,
    STATUS_FONT_PX = 24,
    HINT_FONT_PX = 20,
    COLOR_HEADING = 0xFFFFFF,
    COLOR_STATUS = 0x00FF88,
    COLOR_HINT = 0xAAAAAA
};

enum {
    STATUS_TEXT_BYTES =
        96 /* the figure-1 text with any int32_t round number */
};

/* The two labels the figures change, and the board's LED count. */
typedef struct {
    bento_ui_t *ui;
    bento_ui_id_t status;
    bento_ui_id_t hint;
    int32_t led_count;
} blink_show_t;

/* A label in the original's form: text, x, y, colour and font size. */
typedef struct {
    const char *text;
    int16_t y;
    uint32_t color;
    int32_t font_px;
} label_spec_t;

/* Stop arrives during a sleep, as KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t duration_ms)
{
    bento_status_t status = bento_time_sleep_ms(duration_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* One list of the board_info() dict: ['a', 'b']. */
static bento_status_t print_names(
    uint32_t count,
    bento_status_t (*name_of)(uint32_t index, const char **name))
{
    bento_status_t status = BENTO_OK;
    uint32_t i;

    (void)printf("[");
    for (i = 0U; (i < count) && (status == BENTO_OK); i++) {
        const char *name = NULL;

        status = name_of(i, &name);
        if (status == BENTO_OK) {
            (void)printf("%s'%s'", (i > 0U) ? ", " : "",
                         (name != NULL) ? name : "");
        }
    }
    (void)printf("]");
    return status;
}

/* The dict as MicroPython prints it on the PSoC Edge kits: modgpio.c's
 * table, rehashed under the firmware's 1-byte qstr hash, keeps this
 * key order. */
static bento_status_t print_board_info(void)
{
    bento_gpio_board_info_t info;
    bento_status_t status;

    bento_gpio_board_info_init(&info);
    status = bento_gpio_board_info(&info);
    if (status == BENTO_OK) {
        (void)printf("ข้อมูลบอร์ด: {'name': '%s', 'buttons': %" PRIu32
                     ", 'leds': %" PRIu32 ", 'btn_names': ",
                     (info.name != NULL) ? info.name : "", info.buttons,
                     info.leds);
        status = print_names(info.buttons, bento_gpio_button_name);
    }
    if (status == BENTO_OK) {
        (void)printf(", 'led_names': ");
        status = print_names(info.leds, bento_gpio_led_name);
    }
    if (status == BENTO_OK) {
        (void)printf("}\n");
    }
    return status;
}

static bento_status_t add_label(bento_ui_t *ui,
                                const label_spec_t *spec,
                                bento_ui_id_t *out_id)
{
    bento_ui_widget_args_t args;

    bento_ui_widget_args_init(&args);
    args.text = spec->text;
    args.text_len = (uint32_t)strlen(spec->text);
    args.x = LABEL_X;
    args.y = spec->y;
    args.color = spec->color;
    args.font_px = spec->font_px;
    return bento_ui_label_create(ui, &args, out_id);
}

static bento_status_t build_screen(blink_show_t *show)
{
    static const label_spec_t heading = {"มองไฟ LED เล็ก ๆ ใต้จอนี้",
                                         HEADING_Y, COLOR_HEADING,
                                         HEADING_FONT_PX};
    static const label_spec_t status_label = {
        "กำลังเริ่ม...", STATUS_Y, COLOR_STATUS, STATUS_FONT_PX};
    static const label_spec_t hint_label = {"", HINT_Y, COLOR_HINT,
                                            HINT_FONT_PX};
    bento_ui_id_t heading_id = 0U;
    bento_status_t status = bento_ui_clear(show->ui);

    if (status == BENTO_OK) {
        status = add_label(show->ui, &heading, &heading_id);
    }
    if (status == BENTO_OK) {
        status = add_label(show->ui, &status_label, &show->status);
    }
    if (status == BENTO_OK) {
        status = add_label(show->ui, &hint_label, &show->hint);
    }
    return status;
}

/* The two labels, then the console line, as each figure starts. */
static void announce(const blink_show_t *show, const char *status_text,
                     const char *hint_text, const char *console_line)
{
    /* a setter's refusal was ignored before too (modui.c) */
    (void)bento_ui_set_text(show->ui, show->status, status_text,
                            (uint32_t)strlen(status_text));
    (void)bento_ui_set_text(show->ui, show->hint, hint_text,
                            (uint32_t)strlen(hint_text));
    (void)printf("%s\n", console_line);
}

/* On for on_ms, then off for off_ms. */
static bento_status_t blink_once(uint32_t led, uint32_t on_ms,
                                 uint32_t off_ms)
{
    bento_status_t status = bento_gpio_led_on(led);

    if (status == BENTO_OK) {
        status = pause_ms(on_ms);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_off(led);
    }
    if (status == BENTO_OK) {
        status = pause_ms(off_ms);
    }
    return status;
}

static bento_status_t blink_slow(const blink_show_t *show,
                                 int32_t round_no)
{
    char status_text[STATUS_TEXT_BYTES];
    bento_status_t status = BENTO_OK;
    int32_t i;

    (void)snprintf(status_text, sizeof(status_text),
                   "ท่า 1/5: กะพริบช้า (รอบ %" PRId32 "/2)", round_no);
    announce(show, status_text, "ติดครึ่งวินาที ดับครึ่งวินาที ลองนับตามดู",
             "ท่า 1: กะพริบช้า 1 Hz");
    for (i = 0; (i < SLOW_BLINKS) && (status == BENTO_OK); i++) {
        status = blink_once(LED_GREEN, SLOW_MS, SLOW_MS);
    }
    return status;
}

static bento_status_t blink_fast(const blink_show_t *show)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    announce(show, "ท่า 2/5: กะพริบเร็ว",
             "โค้ดเดิม แค่ลด sleep_ms จาก 500 เหลือ 100",
             "ท่า 2: กะพริบเร็ว 5 Hz");
    for (i = 0; (i < FAST_TOGGLES) && (status == BENTO_OK); i++) {
        status = bento_gpio_led_toggle(LED_GREEN);
        if (status == BENTO_OK) {
            status = pause_ms(FAST_MS);
        }
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_off(LED_GREEN);
    }
    return status;
}

static bento_status_t heartbeat(const blink_show_t *show)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    announce(show, "ท่า 3/5: หัวใจเต้น ตุบ-ตุบ",
             "สองจังหวะติดกัน แล้วพักยาว เหมือนชีพจร", "ท่า 3: heartbeat");
    for (i = 0; (i < HEARTBEATS) && (status == BENTO_OK); i++) {
        int32_t beat;

        for (beat = 0; (beat < BEATS) && (status == BENTO_OK); beat++) {
            status = blink_once(LED_GREEN, BEAT_MS, BEAT_MS);
        }
        if (status == BENTO_OK) {
            status = pause_ms(BEAT_REST_MS);
        }
    }
    return status;
}

static bento_status_t send_sos(const blink_show_t *show)
{
    /* three short, three long, three short: the LED's time on */
    static const uint32_t length_ms[] = {
        SOS_SHORT_MS, SOS_SHORT_MS, SOS_SHORT_MS,
        SOS_LONG_MS,  SOS_LONG_MS,  SOS_LONG_MS,
        SOS_SHORT_MS, SOS_SHORT_MS, SOS_SHORT_MS};
    bento_status_t status = BENTO_OK;
    int32_t i;

    announce(show, "ท่า 4/5: รหัส SOS", "สั้น 3 ยาว 3 สั้น 3 = ... --- ...",
             "ท่า 4: SOS");
    for (i = 0; (i < SOS_REPEATS) && (status == BENTO_OK); i++) {
        size_t k;

        for (k = 0U; (k < (sizeof(length_ms) / sizeof(length_ms[0]))) &&
                     (status == BENTO_OK);
             k++) {
            status = blink_once(LED_GREEN, length_ms[k], SOS_GAP_MS);
        }
        if (status == BENTO_OK) {
            status = pause_ms(SOS_REPEAT_GAP_MS);
        }
    }
    return status;
}

/* LED `index` on for one step of the sweep, then off. */
static bento_status_t sweep_step(int32_t index)
{
    const uint32_t led = (uint32_t)index;
    bento_status_t status = bento_gpio_led_on(led);

    if (status == BENTO_OK) {
        status = pause_ms(SWEEP_STEP_MS);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_off(led);
    }
    return status;
}

/* Out over every LED, then back without the two ends, so no LED
 * lights twice in a row. */
static bento_status_t knight_rider(const blink_show_t *show)
{
    bento_status_t status = BENTO_OK;
    int32_t sweep;

    announce(show, "ท่า 5/5: ไฟวิ่งข้ามทุกดวง",
             "คราวนี้มองให้ครบทุกดวง ไฟจะวิ่งไป-กลับ", "ท่า 5: knight-rider");
    for (sweep = 0; (sweep < SWEEPS) && (status == BENTO_OK); sweep++) {
        int32_t i;

        for (i = 0; (i < show->led_count) && (status == BENTO_OK);
             i++) {
            status = sweep_step(i);
        }
        for (i = show->led_count - 2; (i > 0) && (status == BENTO_OK);
             i--) {
            status = sweep_step(i);
        }
    }
    return status;
}

static bento_status_t play_round(const blink_show_t *show,
                                 int32_t round_no)
{
    bento_status_t status;

    (void)printf("=== รอบที่ %" PRId32 " / 2 ===\n", round_no);
    status = blink_slow(show, round_no);
    if (status == BENTO_OK) {
        status = blink_fast(show);
    }
    if (status == BENTO_OK) {
        status = heartbeat(show);
    }
    if (status == BENTO_OK) {
        status = send_sos(show);
    }
    if (status == BENTO_OK) {
        status = knight_rider(show);
    }
    return status;
}

static bento_status_t leds_off(int32_t led_count)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < led_count) && (status == BENTO_OK); i++) {
        status = bento_gpio_led_off((uint32_t)i);
    }
    return status;
}

int main(void)
{
    blink_show_t show = {bento_runtime_get_ui(), 0U, 0U,
                         (int32_t)bento_gpio_num_leds()};
    bento_status_t status;
    int32_t round_no;

    (void)printf("บอร์ดนี้มี LED ทั้งหมด %" PRId32 " ดวง\n", show.led_count);
    status = print_board_info();
    if (status == BENTO_OK) {
        status = build_screen(&show);
    }
    if (status == BENTO_OK) {
        /* gpio.led(1) refuses an LED the board does not have */
        status = (LED_GREEN < show.led_count)
                     ? BENTO_OK
                     : BENTO_E_INVALID_ARGUMENT;
    }
    for (round_no = 1; (round_no <= ROUNDS) && (status == BENTO_OK);
         round_no++) {
        status = play_round(&show, round_no);
    }
    if (status == BENTO_OK) {
        status = leds_off(show.led_count);
    }
    if (status == BENTO_OK) {
        announce(&show, "จบแล้ว เก่งมาก",
                 "ลองแก้ตัวเลข sleep_ms ในโค้ด แล้วกด Program ดูจังหวะใหม่",
                 "จบแล้ว เก่งมาก ลองปรับเวลา sleep_ms ดูว่าจังหวะเปลี่ยนยังไง");
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
