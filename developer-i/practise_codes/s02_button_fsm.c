/*
 * s02_button_fsm.c -- ปุ่ม + debounce + FSM ไฟจราจร (ฉบับฝึก เติมช่องว่าง)
 *
 * Twin of s02_button_fsm.py
 * (embedded-systems-for-game_console_developer/developer-i/practise_codes/s02_button_fsm.py).
 *
 * แล็บฝึกแกนของ game loop: อ่านปุ่ม USER BTN1 -> กรองสัญญาณ (จับขอบขาลง) -> เปลี่ยน
 * state ของ FSM -> แสดงผลบน LED2 กับ RGB_RED กดหนึ่งครั้ง = ไฟจราจรเลื่อนหนึ่งขั้น
 * เขียว -> เหลือง -> แดง -> วนกลับ ไฟล์ฝึกนี้มีช่องว่าง 4 จุด ("เติม:") ที่ยังเป็น pass
 * จึงพิมพ์ "STATE = GREEN" ทุกครั้งที่กด แต่ยังไม่เปิดไฟและ state ยังไม่เลื่อน
 *
 * The lab practises the core of a game loop: read the button, filter
 * it (a press edge), move a traffic-light state machine one step, and
 * show it on two LEDs. Its four blanks are still `pass`, so each press
 * prints "STATE = GREEN" again: no LED is driven and the state never
 * advances, exactly as the shipped practise file behaves.
 *
 * C twin of the Example Explorer program devi-s02-button-fsm
 * (py/devi-s02-button-fsm.py). Style: CLEAN_C.md.
 * What C does differently:
 *   - gpio.button(0) and gpio.led(n) are indexes, checked once with
 *     the name query, which fails where MicroPython raises;
 *   - the LED_PATTERN and NAME dicts are tables indexed by the state;
 *   - Stop ends the endless loop at a sleep (MicroPython's
 *     KeyboardInterrupt) and main returns 0;
 *   - the four blanks stay empty: show() reads the pattern and drives
 *     nothing, and the state never moves.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_time.h"

enum { /* the board: the first user button, LED2 (green) and RGB_RED */
    BUTTON = 0,
    LED_A = 1,
    LED_B = 2
};
enum { /* the traffic-light states */
    STATE_GREEN = 0,
    STATE_YELLOW = 1,
    STATE_RED = 2,
    STATE_COUNT = 3
};
enum { /* loop timing, ms */
    SETTLE_MS = 50,
    LOOP_MS = 10
};

typedef struct {
    bool a_on;
    bool b_on;
} led_pattern_t;

/* the LED pair of a state; the blanks have not wired it to the LEDs yet */
static void show(int32_t state)
{
    static const led_pattern_t led_pattern[STATE_COUNT] = {
        {true, false}, {true, true}, {false, true}};
    static const char *const names[STATE_COUNT] = {"GREEN", "YELLOW",
                                                   "RED"};
    const led_pattern_t pattern = led_pattern[state];

    /* blanks 1 and 2: drive led_a from a_on and led_b from b_on */
    (void)pattern.a_on;
    (void)pattern.b_on;
    (void)printf("STATE = %s\n", names[state]);
}

/* gpio.button() and gpio.led() raise for a part the board does not have */
static bento_status_t check_parts(void)
{
    const char *name = NULL;
    bento_status_t status =
        bento_gpio_button_name((uint32_t)BUTTON, &name);

    if (status == BENTO_OK) {
        status = bento_gpio_led_name((uint32_t)LED_A, &name);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_name((uint32_t)LED_B, &name);
    }
    return status;
}

/* a sleep that turns Stop into BENTO_E_CANCELLED */
static bento_status_t pause_ms(int32_t delay_ms)
{
    bento_status_t status = bento_time_sleep_ms((uint32_t)delay_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* read the button every LOOP_MS and react to each press edge */
static bento_status_t run_loop(int32_t state)
{
    bento_status_t status = BENTO_OK;
    /* blank 3: prev_pressed is not read from the button yet */
    bool prev_pressed = false;

    while (status == BENTO_OK) {
        bool pressed = false;

        status =
            bento_gpio_button_is_pressed((uint32_t)BUTTON, &pressed);
        if ((status == BENTO_OK) && pressed && !prev_pressed) {
            /* blank 4: the state does not advance yet */
            show(state);
            status = pause_ms(SETTLE_MS);
        }
        prev_pressed = pressed;
        if (status == BENTO_OK) {
            status = pause_ms(LOOP_MS);
        }
    }
    return status;
}

int main(void)
{
    const int32_t state = STATE_GREEN;
    bento_status_t status = BENTO_OK;

    /* no screen: the lab uses only the button and the LEDs */
    (void)bento_runtime_get_ui();
    status = check_parts();
    if (status == BENTO_OK) {
        show(state);
        (void)printf(
            "กดปุ่ม USER BTN1 เพื่อเปลี่ยนไฟจราจร (Ctrl+C เพื่อหยุด)\n");
        status = run_loop(state);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
