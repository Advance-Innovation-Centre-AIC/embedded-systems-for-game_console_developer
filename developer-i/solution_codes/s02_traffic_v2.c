/*
 * s02_traffic_v2.c -- ไฟจราจรแบบ "ติดทีละดวง" (อ่านเสริม ไม่คิดคะแนน)
 *
 * เวอร์ชันอ่านเสริมของ s02_button_fsm: แกน FSM (ปุ่ม -> debounce ->
 * เปลี่ยน state) เหมือนเดิม เปลี่ยนแค่การแสดงผลเป็น "ติดทีละดวง"
 * ดวงเดียวต่อสถานะ เหมือนไฟจราจรจริง กดปุ่ม USER BTN1 หนึ่งครั้ง
 * = เขียว -> เหลือง -> แดง -> วนกลับ ตรรกะ (state) แยกจากการแสดงผล
 *
 * A traffic light that lights one lamp per state. Each press of USER
 * BTN1 (a press is the edge from released to pressed) moves the state
 * machine GREEN -> YELLOW -> RED -> GREEN; the lamp of the state is on,
 * the others are off, and the state's name is printed. Only the display
 * changed from s02_button_fsm: the logic stays apart from the output.
 *
 * Twin of s02_traffic_v2.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s02_traffic_v2.py).
 * Style: CLEAN_C.md.
 * What C does differently:
 *   - the lamp and NAME dicts are tables indexed by the state, and the
 *     Button and LED objects are their numbers; each one is checked once
 *     with bento_gpio_*_name(), which fails where gpio.led(n) raises;
 *   - `while True` ends on Stop: MicroPython raises KeyboardInterrupt in
 *     the sleep, here the sleep is followed by the Stop check and the
 *     program returns 0;
 *   - a failed gpio or sleep call returns its status (MicroPython raises).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bento_gpio.h"
#include "bento_runtime.h"
#include "bento_time.h"

enum {
    BUTTON = 0U, /* ปุ่มดวงแรก (index 0 = USER BTN1 บนบอร์ด) */
    STATES = 3U,
    SETTLE_MS = 50U, /* หน่วงสั้น ๆ ให้สัญญาณปุ่มนิ่งก่อนอ่านต่อ */
    LOOP_MS = 10U
};

/* แต่ละสถานะ = LED หนึ่งดวง: GREEN -> LED2 (1), YELLOW -> LED1 (0),
 * RED -> RGB_RED (2) */
static const uint32_t LAMP[STATES] = {1U, 0U, 2U};

/* ติดดวงที่ตรงกับ state ปัจจุบัน ดับดวงที่เหลือให้หมด */
static bento_status_t show(uint32_t state)
{
    static const char *const state_names[STATES] = {"GREEN", "YELLOW",
                                                    "RED"};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    for (i = 0U; (status == BENTO_OK) && (i < STATES); i++) {
        status = (i == state) ? bento_gpio_led_on(LAMP[i])
                              : bento_gpio_led_off(LAMP[i]);
    }
    if (status == BENTO_OK) {
        (void)printf("STATE = %s\n", state_names[state]);
    }
    return status;
}

/* A sleep, then Stop becomes a status. */
static bento_status_t pause_ms(uint32_t ms)
{
    bento_status_t status = bento_time_sleep_ms(ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* The button and the three lamps exist on this board. */
static bento_status_t check_pins(void)
{
    const char *name = NULL;
    bento_status_t status = bento_gpio_button_name(BUTTON, &name);
    uint32_t i;

    for (i = 0U; (status == BENTO_OK) && (i < STATES); i++) {
        status = bento_gpio_led_name(LAMP[i], &name);
    }
    return status;
}

/* One pass of the loop: a press edge moves the state machine. */
static bento_status_t poll_button(uint32_t *state, bool *prev_pressed)
{
    bool pressed = false;
    bento_status_t status =
        bento_gpio_button_is_pressed(BUTTON, &pressed);

    /* นับเป็น "กด" เฉพาะตอนเพิ่งเปลี่ยนจากปล่อย -> กด */
    if ((status == BENTO_OK) && pressed && !*prev_pressed) {
        /* เขียว -> เหลือง -> แดง -> วนกลับ */
        *state = (*state + 1U) % STATES;
        status = show(*state);
        if (status == BENTO_OK) {
            status = pause_ms(SETTLE_MS);
        }
    }
    if (status == BENTO_OK) {
        *prev_pressed = pressed;
        status = pause_ms(LOOP_MS);
    }
    return status;
}

int main(void)
{
    uint32_t state = 0U;
    bool prev_pressed = false;
    bento_status_t status = check_pins();

    /* debounce: จำสถานะปุ่มรอบก่อน เพื่อจับ "ขอบขาลง" (กดลงครั้งเดียว) */
    if (status == BENTO_OK) {
        status = bento_gpio_button_is_pressed(BUTTON, &prev_pressed);
    }
    if (status == BENTO_OK) {
        status = show(state);
    }
    if (status == BENTO_OK) {
        (void)printf(
            "กดปุ่ม USER BTN1 เพื่อเปลี่ยนไฟจราจร (Ctrl+C เพื่อหยุด)\n");
    }
    while (status == BENTO_OK) {
        status = poll_button(&state, &prev_pressed);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
