/*
 * s01_interrupt.c -- Polling vs Interrupt: ปุ่มแบบ "ขัดจังหวะ" + กระพริบไฟแบบไม่บล็อก
 *
 * ใน Dev I เราอ่านปุ่มแบบ polling คือวนถามซ้ำ ๆ แล็บนี้ให้ฮาร์ดแวร์เป็นฝ่าย "สะกิด"
 * เราเองตอนปุ่มถูกกด (interrupt ที่ขอบขาลงของปุ่ม) และกระพริบไฟ LED 0 ทุก 500 ms
 * ด้วย software timer (เทียบเวลาเองในลูป ไม่ sleep ยาว) ไฟกระพริบสม่ำเสมอ
 * พร้อมกับการนับปุ่ม โดยไม่ขวางกัน กด Stop เพื่อหยุด (โครงนี้มีช่องให้เติมเอง)
 *
 * The practise skeleton: LED 0 and the button pin are set up, the loop runs
 * every 5 ms and prints the press count when it changes. The blanks are left
 * as TODOs: counting in the handler, attaching it to the falling edge, and
 * the blink test; until then the count stays 0 and the LED stays off.
 *
 * Twin of s01_interrupt.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/s01_interrupt.py). Style: CLEAN_C.md.
 * C has no interrupt callback into the program (the C API's rule): once the
 * edge is attached it goes into the library's event ring, which the loop
 * reads with bento_machine_pin_poll() and counts one press per falling edge.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_gpio.h"
#include "bento_machine.h"
#include "bento_runtime.h"
#include "bento_time.h"

enum {
    BLINK_MS = 500, /* อยากให้ไฟสลับสถานะทุก ๆ กี่มิลลิวินาที */
    LOOP_MS = 5     /* พักสั้น ๆ พอ ไม่ให้ลูป busy เกินจำเป็น */
};

typedef struct {
    uint32_t led;    /* ไฟดวงแรกไว้กระพริบเป็นจังหวะ */
    uint32_t button; /* ปุ่ม USER BTN1 = P7_0 บนบอร์ด */
    int32_t press_count;
    bool led_on;
    uint32_t last_blink;
    int32_t last_reported; /* กันพิมพ์ค่าซ้ำ ๆ เมื่อจำนวนกดไม่เปลี่ยน */
} lab_t;

/* gpio.led(0) raises ValueError on a board without that LED. */
static bento_status_t take_led(lab_t *lab)
{
    lab->led = 0U;
    return (lab->led < bento_gpio_num_leds())
               ? BENTO_OK
               : BENTO_E_INVALID_ARGUMENT;
}

/* ปุ่ม ตั้งเป็นขาเข้า (no pull: the binding's latch is 0) */
static bento_status_t open_button(lab_t *lab)
{
    static const char button_name[] = "USER_BUTTON";
    bento_machine_pin_cfg_t config;
    bento_status_t status = bento_machine_pin_open(
        button_name, (uint32_t)strlen(button_name), &lab->button);

    if (status == BENTO_OK) {
        bento_machine_pin_cfg_init(&config);
        config.mode = BENTO_MACHINE_PIN_IN;
        config.value = 0;
        status = bento_machine_pin_config(lab->button, &config);
    }
    return status;
}

/* ซอฟต์แวร์ timer: ครบช่วง BLINK_MS แล้วค่อยสลับไฟ ไม่ใช้ sleep ยาว */
static bool blink_due(const lab_t *lab, uint32_t now)
{
    (void)lab;
    (void)now;
    /* เติม: เช็กว่าครบช่วงเวลาหรือยัง ใช้
     * bento_time_ticks_diff(now, lab->last_blink) >= BLINK_MS */
    return false;
}

static bento_status_t blink_step(lab_t *lab, uint32_t now)
{
    bento_status_t status = BENTO_OK;

    if (blink_due(lab, now)) {
        lab->led_on = !lab->led_on;
        status = lab->led_on ? bento_gpio_led_on(lab->led)
                             : bento_gpio_led_off(lab->led);
        lab->last_blink = now;
    }
    return status;
}

/* ปุ่มถูกนับให้แล้วโดย interrupt ลูปแค่คอยรายงานเมื่อค่าเปลี่ยน */
static void report_presses(lab_t *lab)
{
    if (lab->press_count != lab->last_reported) {
        (void)printf("ปุ่มถูกกดแล้ว %d ครั้ง\n", (int)lab->press_count);
        lab->last_reported = lab->press_count;
    }
}

static bento_status_t run_loop(lab_t *lab)
{
    bento_status_t status = BENTO_OK;

    while (status == BENTO_OK) {
        const uint32_t now = bento_time_ticks_ms();

        status = blink_step(lab, now);
        if (status == BENTO_OK) {
            report_presses(lab);
            status = bento_time_sleep_ms(LOOP_MS);
        }
        if ((status == BENTO_OK) && !bento_runtime_is_running()) {
            status = BENTO_E_CANCELLED;
        }
    }
    return status;
}

int main(void)
{
    lab_t lab;
    bento_status_t status;

    (void)memset(&lab, 0, sizeof(lab));
    status = take_led(&lab);
    if (status == BENTO_OK) {
        status = open_button(&lab);
    }
    lab.press_count = 0;
    /* "interrupt handler" ทำให้สั้นและเร็วที่สุด ห้ามงานหนัก/หน่วงเวลานาน
     * เติม: เขียนฟังก์ชัน on_button(lab) ที่นับเพิ่มทีละ 1 (lab->press_count += 1)
     * เติม: ผูกปุ่มที่ขอบขาลง ใช้ bento_machine_pin_irq_enable(lab.button,
     *       BENTO_MACHINE_PIN_IRQ_FALLING) แล้วในลูปอ่านขอบที่ค้างด้วย
     *       bento_machine_pin_poll และเรียก on_button ทีละขอบขาลงของปุ่ม */
    if (status == BENTO_OK) {
        (void)printf("กดปุ่ม USER BTN1 ได้เลย — ไฟจะกระพริบไปด้วยโดยไม่สะดุด "
                     "(Ctrl+C เพื่อหยุด)\n");
        lab.led_on = false;
        lab.last_blink = bento_time_ticks_ms();
        lab.last_reported = -1;
        status = run_loop(&lab);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
