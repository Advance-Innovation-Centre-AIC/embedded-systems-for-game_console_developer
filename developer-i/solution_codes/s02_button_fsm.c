/*
 * s02_button_fsm.c -- ปุ่ม + debounce + FSM (ไฟจราจร)
 *
 * Twin of s02_button_fsm.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s02_button_fsm.py)
 *
 * แกนของ game loop: อ่านปุ่ม -> กรองการเด้ง (จับขอบตอนเพิ่งกด) ->
 * เปลี่ยน state ของ FSM -> แสดงผลบน LED สองดวง กดปุ่ม USER BTN1
 * หนึ่งครั้ง ไฟจราจรเลื่อนไปหนึ่งขั้น: เขียว -> เหลือง -> แดง -> วนกลับ
 * กด Stop เพื่อหยุด
 *
 * The core of a game loop: read the button, filter its bounce (count
 * only the edge where it was just pressed), step the state machine,
 * show the state on two LEDs. One press of USER BTN1 moves the traffic
 * light one step: green, yellow, red, and round again. Press Stop to
 * end.
 *
 * C twin of the Example Explorer program devi-s02-button-fsm
 * (py/devi-s02-button-fsm.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - the LED_PATTERN and NAME dicts are tables indexed by the state;
 *   - gpio.button(0) and gpio.led(n) are checked by their name calls,
 *     which fail where MicroPython raises ValueError;
 *   - `while True` ends on Stop, as KeyboardInterrupt ends it;
 *   - a call that raises returns its status and ends the program.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_time.h"

/* The button and the two LEDs: LED2 (green) and RGB_RED, so LED1 (red,
 * index 0), often the system's status light, stays free. */
enum {
    BUTTON = 0,
    LED_A = 1,
    LED_B = 2
};

/* The traffic light's states, in the order a press steps through. */
enum {
    GREEN = 0,
    YELLOW = 1,
    RED = 2,
    STATE_COUNT = 3
};

/* Timing, in milliseconds. */
enum {
    SETTLE_MS = 50, /* let the button's signal settle after a press */
    LOOP_MS = 10    /* about 100 rounds a second, never busier */
};

/* Which LEDs light in a state: green = A, yellow = both, red = B. */
typedef struct {
    bool a_on;
    bool b_on;
} led_pattern_t;

static bento_status_t set_led(uint32_t led, bool on)
{
    return on ? bento_gpio_led_on(led) : bento_gpio_led_off(led);
}

/* The state on the LEDs, then its name on the console. */
static bento_status_t show(int32_t state)
{
    static const led_pattern_t led_pattern[STATE_COUNT] = {
        {true, false}, {true, true}, {false, true}};
    static const char *const names[STATE_COUNT] = {"GREEN", "YELLOW",
                                                   "RED"};
    bento_status_t status = set_led(LED_A, led_pattern[state].a_on);

    if (status == BENTO_OK) {
        status = set_led(LED_B, led_pattern[state].b_on);
    }
    if (status == BENTO_OK) {
        (void)printf("STATE = %s\n", names[state]);
    }
    return status;
}

/* Stop arrives during a sleep, as KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t duration_ms)
{
    bento_status_t status = bento_time_sleep_ms(duration_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* gpio.button(0), gpio.led(1) and gpio.led(2) refuse what the board
 * lacks; the name calls refuse the same, in the same order. */
static bento_status_t open_pins(void)
{
    const char *name = NULL;
    bento_status_t status = bento_gpio_button_name(BUTTON, &name);

    if (status == BENTO_OK) {
        status = bento_gpio_led_name(LED_A, &name);
    }
    if (status == BENTO_OK) {
        status = bento_gpio_led_name(LED_B, &name);
    }
    return status;
}

/* One press: the FSM steps to the next state, round again after red. */
static bento_status_t step_light(int32_t *state)
{
    bento_status_t status;

    *state = (*state + 1) % STATE_COUNT;
    status = show(*state);
    if (status == BENTO_OK) {
        status = pause_ms(SETTLE_MS);
    }
    return status;
}

/* Counts a press only on the edge from released to pressed, so one
 * long press that bounces steps the light once. */
static bento_status_t run_loop(int32_t state, bool prev_pressed)
{
    int32_t current = state;
    bool previous = prev_pressed;
    bento_status_t status = BENTO_OK;

    while (status == BENTO_OK) {
        bool pressed = false;

        status = bento_gpio_button_is_pressed(BUTTON, &pressed);
        if ((status == BENTO_OK) && pressed && !previous) {
            status = step_light(&current);
        }
        previous = pressed;
        if (status == BENTO_OK) {
            status = pause_ms(LOOP_MS);
        }
    }
    return status;
}

int main(void)
{
    bool prev_pressed = false;
    bento_status_t status = open_pins();

    if (status == BENTO_OK) {
        status = bento_gpio_button_is_pressed(BUTTON, &prev_pressed);
    }
    if (status == BENTO_OK) {
        status = show(GREEN);
    }
    if (status == BENTO_OK) {
        (void)printf("กดปุ่ม USER BTN1 เพื่อเปลี่ยนไฟจราจร (Ctrl+C "
                     "เพื่อหยุด)\n");
        status = run_loop(GREEN, prev_pressed);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
