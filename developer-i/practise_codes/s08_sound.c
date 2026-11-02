/*
 * s08_sound.c -- เสียงเกมบน PSoC Edge (คาบ 8) ฉบับฝึก
 *
 * เราจะทำ 2 เรื่อง: (1) เล่น sfx สำเร็จรูปตาม id, (2) ต่อโน้ตเป็นทำนองสั้น ๆ ด้วย tone
 * บอร์ดสร้างเสียงด้วย PWM แล้วเลือกรูปคลื่น: SQUARE = กระด้างแบบเกม 8-bit, SINE = นุ่ม
 * โน้ตใช้เลข MIDI: 60 = โดกลาง (C4), +1 = ครึ่งเสียง, +12 = สูงขึ้น 1 ออกเทฟ
 * velocity = ความดัง (0-127), ms = ความยาวเสียงเป็นมิลลิวินาที
 * วิธีรัน: เปิดไฟล์นี้ใน BENTO IDE แล้วกด Program to Device
 *
 * The sound lab of lesson 8, the practise version. It prints each step, plays the
 * ready-made game-over sound, then walks twice through a five-note melody, once
 * for the square wave and once for the sine wave. Blanks (1) to (4) are still
 * empty, so only the game-over sound plays; the melodies keep their timing
 * (220 ms a note) in silence until you fill them in.
 *
 * Twin of s08_sound.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/s08_sound.py). Style: CLEAN_C.md.
 * What C does differently:
 * - the board sends a sound fire-and-forget and MicroPython drops its answer, so C
 *   ignores the sound call's status too;
 * - Stop during a sleep (KeyboardInterrupt) ends the program with 0.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bento_runtime.h"
#include "bento_time.h"

/* The lab's timing. */
enum {
    START_PAUSE_MS = 600,
    EAT_PAUSE_MS = 400,
    GAME_OVER_PAUSE_MS = 900,
    NOTE_MS = 200,    /* โน้ตละ 200 ms */
    NOTE_GAP_MS = 20, /* +20 ms กันโน้ตเกยกัน ฟังเป็นจังหวะ */
    MELODY_PAUSE_MS = 300,
    NOTE_VELOCITY = 100
};

/* time.sleep_ms(); Stop arrives here as BENTO_E_CANCELLED. */
static bento_status_t pause_ms(uint32_t duration_ms)
{
    bento_status_t status = bento_time_sleep_ms(duration_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* ส่วนที่ 1: เสียงสำเร็จรูป (sfx) เรียกด้วย id ที่บอร์ดเตรียมไว้ให้ */
static bento_status_t play_ready_made(bento_ui_t *ui)
{
    bento_status_t status;

    (void)printf("เล่น sfx: เริ่มเกม\n");
    /* เติม (1): เล่นเสียงเริ่มเกม ด้วย bento_ui_sfx_play(ui, BENTO_UI_SFX_UI_START) */
    status = pause_ms(START_PAUSE_MS);
    if (status == BENTO_OK) {
        (void)printf("เล่น sfx: กินอาหาร (snake)\n");
        /* เติม (2): เล่นเสียงกินอาหารของ snake ด้วย BENTO_UI_SFX_SNAKE_EAT */
        status = pause_ms(EAT_PAUSE_MS);
    }
    if (status == BENTO_OK) {
        (void)printf("เล่น sfx: เกมจบ\n");
        /* ตัวอย่างที่ทำให้ดู: เสียงเกมจบ; fire-and-forget, as MicroPython sends it */
        (void)bento_ui_sfx_play(ui, (uint8_t)BENTO_UI_SFX_GAME_OVER);
        status = pause_ms(GAME_OVER_PAUSE_MS);
    }
    return status;
}

/* ส่วนที่ 2: เล่นทีละโน้ต เราหน่วงเวลาเองให้โน้ตไม่ทับกัน */
static bento_status_t play_melody(uint8_t wave)
{
    /* เลข MIDI ของแต่ละโน้ต (โด เร มี ฟา ซอล) */
    static const uint8_t melody[] = {60U, 62U, 64U, 65U, 67U};
    bento_status_t status = BENTO_OK;
    uint32_t i;

    (void)
        wave; /* read by the tone of blanks (3) and (4) once you fill them in */
    for (i = 0U; (status == BENTO_OK) &&
                 (i < (sizeof(melody) / sizeof(melody[0])));
         i++) {
        /* เติม (3) และ (4): เล่นโน้ตปัจจุบันด้วย bento_ui_sfx_tone(bento_runtime_get_ui(),
         *   melody[i], wave, NOTE_VELOCITY, NOTE_MS) */
        status = pause_ms((uint32_t)NOTE_MS + (uint32_t)NOTE_GAP_MS);
    }
    return status;
}

int main(void)
{
    bento_ui_t *ui = bento_runtime_get_ui();
    bento_status_t status = play_ready_made(ui);

    if (status == BENTO_OK) {
        (void)printf("เล่นทำนองด้วย WAVE_SQUARE (เสียงเกมคลาสสิก)\n");
        status = play_melody((uint8_t)BENTO_UI_WAVE_SQUARE);
    }
    if (status == BENTO_OK) {
        status = pause_ms(MELODY_PAUSE_MS);
    }
    if (status == BENTO_OK) {
        /* ลองเปลี่ยนรูปคลื่น แล้วฟังว่าน้ำเสียงต่างกันอย่างไร (โน้ตเดิมทั้งหมด) */
        (void)printf("เล่นทำนองเดิมด้วย WAVE_SINE (เสียงนุ่มขึ้น)\n");
        status = play_melody((uint8_t)BENTO_UI_WAVE_SINE);
    }
    if (status == BENTO_OK) {
        (void)printf("จบแล็บเสียง — ลองแก้ melody / wave / NOTE_MS "
                     "ดูว่าผลลัพธ์เปลี่ยนยังไง\n");
    }
    /* เมนูเสียงสำเร็จรูปทั้งหมด 21 เสียง: bento_game_sfx(game, "ชื่อ") ใน bento_game.h
     *   Snake : "eat" "turn" "die" · Flappy: "flap" "point" "fall"
     *   Pong  : "wall" "paddle" "pong_score" "win" "lose"
     *   Shoot : "fire" "hit" "explode" "lose_life"
     *   UI    : "start" "select" "back" "deny" "move" "gameover" */
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
