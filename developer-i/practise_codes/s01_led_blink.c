/*
 * s01_led_blink.c -- LED ดวงแรกของเรา (ฉบับฝึกเติมโค้ด)
 *
 * โปรแกรมพิมพ์จำนวน LED และข้อมูลบอร์ด ล้างหน้าจอ Playground แล้ว
 * ปักป้ายสามบรรทัด (หัวเรื่อง สถานะ และคำใบ้) จากนั้นเล่นท่ากะพริบ
 * ห้าท่าบน LED2 (index 1) สองรอบ: กะพริบช้า กะพริบเร็ว หัวใจเต้น
 * รหัส SOS และไฟวิ่ง knight-rider ข้ามทุกดวง แต่ละท่าเปลี่ยนป้าย
 * และพิมพ์หนึ่งบรรทัด จบด้วยการดับไฟทุกดวงและข้อความปิดท้าย ช่อง
 * "เติม:" ยังเป็น pass จึงทำตามไฟล์ฝึกเป๊ะ: ท่า 1 และ 2 แค่รอ
 * ท่า 3 ไม่มีช่วงพักยาว ท่า 4 ไม่ค้างไฟตาม length_ms ท่า 5 ขาไป
 * ไม่เปิดไฟ และขากลับไม่ดับไฟ
 *
 * The program prints the LED count and the board info, clears the
 * Playground screen and puts up three labels (a heading, the status
 * and a hint). Then it plays five blink figures on LED2 (index 1)
 * twice: a slow blink, a fast blink, a heartbeat, SOS in Morse code
 * and a knight-rider sweep over every LED. Each figure changes the
 * labels and prints one line, and the show ends with every LED off
 * and a closing text. The "fill in" blanks are still `pass`, so it
 * does exactly what the worksheet does: figures 1 and 2 only wait,
 * figure 3 has no long pause, figure 4 does not hold the LED on for
 * length_ms, and figure 5 lights no LED on the way out and turns
 * none off on the way back.
 *
 * Twin of s01_led_blink.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/s01_led_blink.py),
 * from the library twin devi-s01-led-blink (py/devi-s01-led-blink.py).
 * Style: CLEAN_C.md. What C must do differently:
 *   - print(a, b, c) is printf() with the single spaces print() puts
 *     between its arguments;
 *   - print(gpio.board_info()) prints the dict as MicroPython prints it
 *     on the PSoC Edge kits, in modgpio.c's key order: name, buttons,
 *     leds, btn_names, led_names;
 *   - ui.Label(..., value=N) is the font size N, args.font_px;
 *   - gpio.led(1) is the LED's index, checked once as gpio.led() checks
 *     it (a missing LED raises ValueError there, a status here);
 *   - a failed call returns its status where MicroPython raises; the
 *     label text setters are ignored, as modui.c ignores them;
 *   - the IDE's Stop at a sleep (KeyboardInterrupt) is
 *     BENTO_E_CANCELLED, and main() then ends with 0.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_time.h"

/* The three labels. */
enum {
    LABEL_X = 190,
    HEADING_Y = 70,
    STATUS_Y = 150,
    HINT_Y = 210,
    HEADING_FONT_PX = 28,
    STATUS_FONT_PX = 24,
    HINT_FONT_PX = 20,
    LINE_BYTES = 64 /* the round's status line, any uint32_t */
};

/* Label colours, 0xRRGGBB. */
enum {
    COLOR_HEADING = 0xFFFFFF,
    COLOR_STATUS = 0x00FF88,
    COLOR_HINT = 0xAAAAAA
};

/* The figures: counts and times in milliseconds. */
enum {
    /* LED2 (เขียว) เพราะ LED1 (แดง) มักถูกใช้เป็นไฟ status */
    BLINK_LED = 1,
    ROUNDS = 2, /* รอบสองจะได้ดูทัน ถ้ารอบแรกยังหาไฟไม่เจอ */
    SLOW_BLINKS = 10,
    SLOW_HALF_MS = 500,
    FAST_BLINKS = 25,
    FAST_MS = 100,
    HEARTBEATS = 8,
    BEATS = 2, /* ตุบสองครั้งติดกัน */
    BEAT_MS = 120,
    SOS_REPEATS = 2,
    SOS_SIGNS = 9,
    SOS_GAP_MS = 200,
    SOS_PAUSE_MS = 800, /* เว้นก่อนส่งซ้ำ */
    SWEEPS = 6,
    SWEEP_MS = 120
};

/* The labels the figures change, and the board's LED count. */
typedef struct {
    bento_ui_t *ui;
    bento_ui_id_t status;
    bento_ui_id_t hint;
    uint32_t led_count;
} blink_screen_t;

/* A sleep that turns the IDE's Stop into BENTO_E_CANCELLED. */
static bento_status_t pause_ms(uint32_t ms)
{
    bento_status_t status = bento_time_sleep_ms(ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* One name of a list, after a comma unless it is the first. */
static void print_name(uint32_t index, const char *name)
{
    (void)printf("%s'%s'", (index > 0U) ? ", " : "",
                 (name != NULL) ? name : "");
}

/* The board info dict, as MicroPython prints it. */
static bento_status_t print_board_info(void)
{
    bento_gpio_board_info_t info;
    const char *name = NULL;
    uint32_t i;
    bento_status_t status;

    bento_gpio_board_info_init(&info);
    status = bento_gpio_board_info(&info);
    if (status == BENTO_OK) {
        (void)printf("ข้อมูลบอร์ด: {'name': '%s', 'buttons': %" PRIu32
                     ", 'leds': %" PRIu32 ", 'btn_names': [",
                     (info.name != NULL) ? info.name : "", info.buttons,
                     info.leds);
        for (i = 0U; i < info.buttons; i++) {
            /* a name the board cannot give prints as '' */
            (void)bento_gpio_button_name(i, &name);
            print_name(i, name);
        }
        (void)printf("], 'led_names': [");
        for (i = 0U; i < info.leds; i++) {
            (void)bento_gpio_led_name(i, &name);
            print_name(i, name);
        }
        (void)printf("]}\n");
    }
    return status;
}

/* One label at LABEL_X. */
static bento_status_t add_label(bento_ui_t *ui, const char *text,
                                int16_t y, uint32_t color,
                                int32_t font_px, bento_ui_id_t *out)
{
    bento_ui_widget_args_t args;

    bento_ui_widget_args_init(&args);
    args.text = text;
    args.text_len = (uint32_t)strlen(text);
    args.x = LABEL_X;
    args.y = y;
    args.color = color;
    args.font_px = font_px;
    return bento_ui_label_create(ui, &args, out);
}

/* A label's new text; modui.c ignores a setter's refusal, so does C. */
static void set_text(const blink_screen_t *screen, bento_ui_id_t label,
                     const char *text)
{
    (void)bento_ui_set_text(screen->ui, label, text,
                            (uint32_t)strlen(text));
}

/* What the screen says during one figure. */
static void show_figure(const blink_screen_t *screen,
                        const char *status_text, const char *hint_text)
{
    set_text(screen, screen->status, status_text);
    set_text(screen, screen->hint, hint_text);
}

/* The Playground: cleared, then the heading, the status and the hint.
 */
static bento_status_t build_screen(blink_screen_t *screen)
{
    bento_ui_id_t heading = 0U;
    bento_status_t status = bento_ui_clear(screen->ui);

    if (status == BENTO_OK) {
        status =
            add_label(screen->ui, "มองไฟ LED เล็ก ๆ ใต้จอนี้", HEADING_Y,
                      COLOR_HEADING, HEADING_FONT_PX, &heading);
    }
    if (status == BENTO_OK) {
        status =
            add_label(screen->ui, "กำลังเริ่ม...", STATUS_Y, COLOR_STATUS,
                      STATUS_FONT_PX, &screen->status);
    }
    if (status == BENTO_OK) {
        status = add_label(screen->ui, "", HINT_Y, COLOR_HINT,
                           HINT_FONT_PX, &screen->hint);
    }
    return status;
}

/* ท่าที่ 1: กะพริบช้า วินาทีละครั้ง; the on and the off are blanks. */
static bento_status_t blink_slow(const blink_screen_t *screen,
                                 uint32_t round_no)
{
    char line[LINE_BYTES];
    bento_status_t status = BENTO_OK;
    uint32_t i;

    (void)snprintf(line, sizeof(line),
                   "ท่า 1/5: กะพริบช้า (รอบ %" PRIu32 "/2)", round_no);
    show_figure(screen, line, "ติดครึ่งวินาที ดับครึ่งวินาที ลองนับตามดู");
    (void)printf("ท่า 1: กะพริบช้า 1 Hz\n");
    for (i = 0U; (i < SLOW_BLINKS) && (status == BENTO_OK); i++) {
        status = pause_ms(SLOW_HALF_MS);
        if (status == BENTO_OK) {
            status = pause_ms(SLOW_HALF_MS);
        }
    }
    return status;
}

/* ท่าที่ 2: กะพริบเร็ว 5 เท่า; the toggle is a blank. */
static bento_status_t blink_fast(const blink_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    uint32_t i;

    show_figure(screen, "ท่า 2/5: กะพริบเร็ว",
                "โค้ดเดิม แค่ลด sleep_ms จาก 500 เหลือ 100");
    (void)printf("ท่า 2: กะพริบเร็ว 5 Hz\n");
    for (i = 0U; (i < FAST_BLINKS) && (status == BENTO_OK); i++) {
        status = pause_ms(FAST_MS);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_off(BLINK_LED);
    }
    return status;
}

/* One beat: on, a wait, off, a wait. */
static bento_status_t beat_once(void)
{
    bento_status_t status = bento_gpio_led_on(BLINK_LED);

    if (status == BENTO_OK) {
        status = pause_ms(BEAT_MS);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_off(BLINK_LED);
    }
    if (status == BENTO_OK) {
        status = pause_ms(BEAT_MS);
    }
    return status;
}

/* ท่าที่ 3: หัวใจเต้น ตุบ-ตุบ; the long pause is a blank. */
static bento_status_t heartbeat(const blink_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    uint32_t i;
    uint32_t beat;

    show_figure(screen, "ท่า 3/5: หัวใจเต้น ตุบ-ตุบ",
                "สองจังหวะติดกัน แล้วพักยาว เหมือนชีพจร");
    (void)printf("ท่า 3: heartbeat\n");
    for (i = 0U; (i < HEARTBEATS) && (status == BENTO_OK); i++) {
        for (beat = 0U; (beat < BEATS) && (status == BENTO_OK);
             beat++) {
            status = beat_once();
        }
    }
    return status;
}

/* One SOS: nine signs, each the LED on and off, then a gap. */
static bento_status_t sos_once(void)
{
    /* สั้นสามครั้ง = S, ยาวสามครั้ง = O, สั้นสามครั้ง = S */
    static const uint32_t sign_ms[SOS_SIGNS] = {
        150U, 150U, 150U, 500U, 500U, 500U, 150U, 150U, 150U};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    for (i = 0U; (i < SOS_SIGNS) && (status == BENTO_OK); i++) {
        const uint32_t length_ms = sign_ms[i];

        (void)length_ms; /* the blank would hold the LED this long */
        status = bento_gpio_led_on(BLINK_LED);
        if (status == BENTO_OK) {
            status = bento_gpio_led_off(BLINK_LED);
        }
        if (status == BENTO_OK) {
            status = pause_ms(SOS_GAP_MS);
        }
    }
    return status;
}

/* ท่าที่ 4: รหัส SOS แบบมอร์ส, twice. */
static bento_status_t send_sos(const blink_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    uint32_t i;

    show_figure(screen, "ท่า 4/5: รหัส SOS",
                "สั้น 3 ยาว 3 สั้น 3 = ... --- ...");
    (void)printf("ท่า 4: SOS\n");
    for (i = 0U; (i < SOS_REPEATS) && (status == BENTO_OK); i++) {
        status = sos_once();
        if (status == BENTO_OK) {
            status = pause_ms(SOS_PAUSE_MS);
        }
    }
    return status;
}

/* One sweep: forward over every LED (lighting it is a blank), then
 * back from the second last to the second (turning it off is a
 * blank), so the ends do not repeat. */
static bento_status_t sweep_once(uint32_t led_count)
{
    bento_status_t status = BENTO_OK;
    uint32_t i;
    int32_t back;

    for (i = 0U; (i < led_count) && (status == BENTO_OK); i++) {
        status = pause_ms(SWEEP_MS);
        if (status == BENTO_OK) {
            status = bento_gpio_led_off(i);
        }
    }
    for (back = (int32_t)led_count - 2;
         (back > 0) && (status == BENTO_OK); back--) {
        status = bento_gpio_led_on((uint32_t)back);
        if (status == BENTO_OK) {
            status = pause_ms(SWEEP_MS);
        }
    }
    return status;
}

/* ท่าที่ 5: ไฟวิ่ง knight-rider ข้าม LED ทุกดวง */
static bento_status_t knight_rider(const blink_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    uint32_t sweep;

    show_figure(screen, "ท่า 5/5: ไฟวิ่งข้ามทุกดวง",
                "คราวนี้มองให้ครบทุกดวง ไฟจะวิ่งไป-กลับ");
    (void)printf("ท่า 5: knight-rider\n");
    for (sweep = 0U; (sweep < SWEEPS) && (status == BENTO_OK);
         sweep++) {
        status = sweep_once(screen->led_count);
    }
    return status;
}

/* One round of the five figures. */
static bento_status_t play_round(const blink_screen_t *screen,
                                 uint32_t round_no)
{
    bento_status_t status;

    (void)printf("=== รอบที่ %" PRIu32 " / 2 ===\n", round_no);
    status = blink_slow(screen, round_no);
    if (status == BENTO_OK) {
        status = blink_fast(screen);
    }
    if (status == BENTO_OK) {
        status = heartbeat(screen);
    }
    if (status == BENTO_OK) {
        status = send_sos(screen);
    }
    if (status == BENTO_OK) {
        status = knight_rider(screen);
    }
    return status;
}

/* จบการแสดง: every LED off, then the closing texts. */
static bento_status_t finish_show(const blink_screen_t *screen)
{
    bento_status_t status = BENTO_OK;
    uint32_t i;

    for (i = 0U; (i < screen->led_count) && (status == BENTO_OK); i++) {
        status = bento_gpio_led_off(i);
    }
    if (status == BENTO_OK) {
        show_figure(screen, "จบแล้ว เก่งมาก",
                    "ลองแก้ตัวเลข sleep_ms ในโค้ด แล้วกด Program "
                    "ดูจังหวะใหม่");
        (void)printf("จบแล้ว เก่งมาก ลองปรับเวลา sleep_ms "
                     "ดูว่าจังหวะเปลี่ยนยังไง\n");
    }
    return status;
}

/* The board's LEDs, the screen, the LED the figures use, the rounds. */
static bento_status_t run_show(blink_screen_t *screen)
{
    const char *led_name = NULL;
    uint32_t round_no;
    bento_status_t status;

    /* ถามบอร์ดว่ามี LED กี่ดวง จะได้วนลูปไม่เกินจำนวนจริง */
    screen->led_count = bento_gpio_num_leds();
    (void)printf("บอร์ดนี้มี LED ทั้งหมด %" PRIu32 " ดวง\n",
                 screen->led_count);
    status = print_board_info();
    if (status == BENTO_OK) {
        status = build_screen(screen);
    }
    if (status == BENTO_OK) {
        /* gpio.led(1) refuses an LED the board does not have */
        status = bento_gpio_led_name(BLINK_LED, &led_name);
    }
    for (round_no = 1U; (round_no <= ROUNDS) && (status == BENTO_OK);
         round_no++) {
        status = play_round(screen, round_no);
    }
    if (status == BENTO_OK) {
        status = finish_show(screen);
    }
    return status;
}

int main(void)
{
    blink_screen_t screen;
    bento_status_t status;

    (void)memset(&screen, 0, sizeof(screen));
    screen.ui = bento_runtime_get_ui();
    status = run_show(&screen);
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
