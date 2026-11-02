/*
 * s08_sound.c -- เสียงเกมบน PSoC Edge (คาบ 8)
 *
 * Twin of s08_sound.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s08_sound.py)
 *
 * สองเรื่อง: (1) เล่นเสียงสำเร็จรูป (sfx) สามเสียงตาม id ที่บอร์ดเตรียมไว้
 * (2) ต่อโน้ต MIDI ห้าตัว โด เร มี ฟา ซอล เป็นทำนองด้วย tone สองรอบ
 * รอบแรกคลื่นสี่เหลี่ยม (เสียงเกม 8-bit) รอบสองคลื่นไซน์ (เสียงนุ่ม)
 * ลองแก้ทำนอง รูปคลื่น หรือความยาวโน้ต แล้วฟังว่าเปลี่ยนอย่างไร
 *
 * Two things: (1) three ready-made sound effects played by the ids
 * the board defines, (2) five MIDI notes, C D E F G, strung into a
 * tune with tone, twice: first as a square wave (an 8-bit game sound),
 * then as a sine wave (softer). Change the melody, the wave or the
 * note length and listen to what changes.
 *
 * C twin of the Example Explorer program devi-s08-sound
 * (py/devi-s08-sound.py), with the blanks filled as this solution
 * fills them. Style: CLEAN_C.md. What C must do differently:
 *   - ui.sfx and ui.tone are fire-and-forget: their result is ignored,
 *     except on a build without sound, where MicroPython raises
 *     AttributeError and here the program returns BENTO_E_UNSUPPORTED;
 *   - Stop during a sleep ends the program, as KeyboardInterrupt;
 *   - the closing comment's list of the 21 sound names is not code
 *     and stays in the .py.
 */
#include <stdint.h>
#include <stdio.h>

#include "bento_runtime.h"
#include "bento_time.h"

/* The melody: MIDI note numbers, 60 = middle C (C4). */
enum {
    NOTE_C4 = 60,
    NOTE_D4 = 62,
    NOTE_E4 = 64,
    NOTE_F4 = 65,
    NOTE_G4 = 67,
    MELODY_NOTES = 5
};

/* How a note sounds: loudness 0..127 and length. */
enum {
    VELOCITY = 100,
    NOTE_MS = 200,
    NOTE_GAP_MS = 20 /* so two notes never overlap and the beat shows */
};

/* Pauses after each sound, in milliseconds. */
enum {
    START_SOUND_MS = 600,
    EAT_SOUND_MS = 400,
    GAME_OVER_SOUND_MS = 900,
    BETWEEN_TUNES_MS = 300
};

/* Stop arrives during a sleep, as KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t duration_ms)
{
    bento_status_t status = bento_time_sleep_ms(duration_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* A build without sound has no ui.sfx or ui.tone (AttributeError);
 * any other refusal was never seen by the program. */
static bento_status_t sound_status(bento_ui_status_t sent)
{
    return (sent == BENTO_UI_E_UNSUPPORTED) ? BENTO_E_UNSUPPORTED
                                            : BENTO_OK;
}

/* Prints what plays, plays a ready-made sound, then waits. */
static bento_status_t play_effect(bento_ui_t *ui, const char *line,
                                  int32_t sound, uint32_t wait_ms)
{
    bento_status_t status;

    (void)printf("%s\n", line);
    status = sound_status(bento_ui_sfx_play(ui, (uint8_t)sound));
    if (status == BENTO_OK) {
        status = pause_ms(wait_ms);
    }
    return status;
}

/* One note after another; the program waits, as tone() does not. */
static bento_status_t play_melody(bento_ui_t *ui, int32_t wave)
{
    static const int32_t melody[MELODY_NOTES] = {
        NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4};
    bento_status_t status = BENTO_OK;
    int32_t i;

    for (i = 0; (i < MELODY_NOTES) && (status == BENTO_OK); i++) {
        status = sound_status(
            bento_ui_sfx_tone(ui, (uint8_t)melody[i], (uint8_t)wave,
                              (uint8_t)VELOCITY, (uint16_t)NOTE_MS));
        if (status == BENTO_OK) {
            status = pause_ms(NOTE_MS + NOTE_GAP_MS);
        }
    }
    return status;
}

static bento_status_t play_effects(bento_ui_t *ui)
{
    bento_status_t status = play_effect(
        ui, "เล่น sfx: เริ่มเกม", BENTO_UI_SFX_UI_START, START_SOUND_MS);

    if (status == BENTO_OK) {
        status = play_effect(ui, "เล่น sfx: กินอาหาร (snake)",
                             BENTO_UI_SFX_SNAKE_EAT, EAT_SOUND_MS);
    }
    if (status == BENTO_OK) {
        status =
            play_effect(ui, "เล่น sfx: เกมจบ", BENTO_UI_SFX_GAME_OVER,
                        GAME_OVER_SOUND_MS);
    }
    return status;
}

static bento_status_t play_tunes(bento_ui_t *ui)
{
    bento_status_t status;

    (void)printf("เล่นทำนองด้วย WAVE_SQUARE (เสียงเกมคลาสสิก)\n");
    status = play_melody(ui, BENTO_UI_WAVE_SQUARE);
    if (status == BENTO_OK) {
        status = pause_ms(BETWEEN_TUNES_MS);
    }
    if (status == BENTO_OK) {
        (void)printf("เล่นทำนองเดิมด้วย WAVE_SINE (เสียงนุ่มขึ้น)\n");
        status = play_melody(ui, BENTO_UI_WAVE_SINE);
    }
    return status;
}

int main(void)
{
    bento_ui_t *ui = bento_runtime_get_ui();
    bento_status_t status = play_effects(ui);

    if (status == BENTO_OK) {
        status = play_tunes(ui);
    }
    if (status == BENTO_OK) {
        (void)printf("จบแล็บเสียง — ลองแก้ melody / wave / NOTE_MS "
                     "ดูว่าผลลัพธ์เปลี่ยนยังไง\n");
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
