/*
 * s02_thai_menu.c -- เมนูเกมภาษาไทยบน rich UI widgets (ฉบับฝึก)
 *
 * หน้าเมนูของเกมจริงอยากได้ปุ่มกด dropdown เลือกระดับ และ checkbox ซึ่ง ui
 * render ภาษาไทยได้ทุก widget: ตั้งผืนจอเท่าจอเกม หัวเรื่องหลายขนาดฟอนต์
 * dropdown ระดับความยาก checkbox เปิดเสียง และปุ่ม "เริ่มเกม" แล้วรอการกด
 * มีช่อง "เติม:" 2 จุดให้เขียนเอง (checkbox "สั่นเมื่อชน" และปุ่ม "ออก")
 *
 * A Thai game-settings menu on the ui widgets: labels in several font sizes,
 * a difficulty dropdown, a sound checkbox and a Start button. The loop polls
 * the touch events every 50 ms; pressing Start changes the status line. The
 * two blanks (a second checkbox and an Exit button) are left as TODOs.
 *
 * Twin of s02_thai_menu.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/s02_thai_menu.py). Style: CLEAN_C.md.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bento_runtime.h"
#include "bento_time.h"

/* Layout on the 792 x 398 game screen (game.WIDTH x game.HEIGHT). */
enum {
    SCREEN_W = 792,
    SCREEN_H = 398,
    LEFT_X = 40,
    TITLE_Y = 24,
    SUBTITLE_Y = 70,
    LEVEL_Y = 130,
    DROPDOWN_X = 300,
    DROPDOWN_Y = 124,
    SOUND_Y = 190,
    START_X = 300,
    BUTTON_Y = 300,
    START_W = 200,
    BUTTON_H = 60,
    STATUS_Y = 370,
    TITLE_PX = 28, /* ตัวใหญ่สุด = หัวเรื่อง */
    LABEL_PX = 20,
    SMALL_PX = 16,
    LOOP_MS = 50 /* พักสั้น ๆ ไม่ให้ลูป busy เกินจำเป็น */
};

/* Colours, 0xRRGGBB. */
enum {
    COLOR_TEXT = 0xFFFFFF,
    COLOR_SUBTITLE = 0xAACCFF,
    COLOR_STATUS = 0x888888
};

typedef struct {
    bento_ui_t *ui;
    bento_ui_id_t start_button;
    bento_ui_id_t status_label;
} menu_t;

static bento_status_t add_label(bento_ui_t *ui, const char *text,
                                const int16_t place[2], uint32_t color,
                                int32_t font_px, bento_ui_id_t *out_id)
{
    bento_ui_widget_args_t args;

    bento_ui_widget_args_init(&args);
    args.text = text;
    args.text_len = (uint32_t)strlen(text);
    args.x = place[0];
    args.y = place[1];
    args.color = color;
    args.font_px = font_px; /* value= คุมขนาดฟอนต์ ยิ่งเลขมากตัวยิ่งใหญ่ */
    return bento_ui_label_create(ui, &args, out_id);
}

/* หัวเรื่อง คำโปรย และป้ายกำกับช่องเลือกระดับความยาก */
static bento_status_t add_headings(const menu_t *menu)
{
    static const int16_t title_place[2] = {LEFT_X, TITLE_Y};
    static const int16_t subtitle_place[2] = {LEFT_X, SUBTITLE_Y};
    static const int16_t level_place[2] = {LEFT_X, LEVEL_Y};
    bento_ui_id_t label;
    bento_status_t status = add_label(menu->ui, "ตั้งค่าเกมงู", title_place,
                                      COLOR_TEXT, TITLE_PX, &label);

    if (status == BENTO_OK) {
        status =
            add_label(menu->ui, "ปรับให้ถูกใจก่อนเริ่มเล่น", subtitle_place,
                      COLOR_SUBTITLE, SMALL_PX, &label);
    }
    if (status == BENTO_OK) {
        status = add_label(menu->ui, "ระดับความยาก", level_place,
                           COLOR_TEXT, LABEL_PX, &label);
    }
    return status;
}

/* Dropdown: ตัวเลือกคั่นด้วย \n สแต็กกันบนจอได้ถูกต้อง รวมคำที่มีสระซ้อน */
static bento_status_t add_dropdown(const menu_t *menu)
{
    static const char options[] = "ง่าย\nปานกลาง\nยาก";
    bento_ui_widget_args_t args;
    bento_ui_id_t dropdown;

    bento_ui_widget_args_init(&args);
    args.text = options;
    args.text_len = (uint32_t)strlen(options);
    args.x = DROPDOWN_X;
    args.y = DROPDOWN_Y;
    return bento_ui_dropdown_create(menu->ui, &args, &dropdown);
}

/* ตัวเลือกเปิด/ปิดเสียง: value=1 ให้ติ๊กไว้ตั้งแต่แรก */
static bento_status_t add_sound_checkbox(const menu_t *menu)
{
    static const char text[] = "เปิดเสียงในเกม";
    bento_ui_widget_args_t args;
    bento_ui_id_t checkbox;

    bento_ui_widget_args_init(&args);
    args.text = text;
    args.text_len = (uint32_t)strlen(text);
    args.x = LEFT_X;
    args.y = SOUND_Y;
    args.font_px = 1;
    return bento_ui_checkbox_create(menu->ui, &args, &checkbox);
}

/* ปุ่มเริ่มเกม (ข้อความไทยบนปุ่ม) */
static bento_status_t add_start_button(menu_t *menu)
{
    static const char text[] = "เริ่มเกม";
    bento_ui_widget_args_t args;

    bento_ui_widget_args_init(&args);
    args.text = text;
    args.text_len = (uint32_t)strlen(text);
    args.x = START_X;
    args.y = BUTTON_Y;
    args.w = START_W;
    args.h = BUTTON_H;
    return bento_ui_button_create(menu->ui, &args, &menu->start_button);
}

static bento_status_t build_menu(menu_t *menu)
{
    static const int16_t status_place[2] = {LEFT_X, STATUS_Y};
    bento_status_t status = add_headings(menu);

    if (status == BENTO_OK) {
        status = add_dropdown(menu);
    }
    if (status == BENTO_OK) {
        status = add_sound_checkbox(menu);
    }
    /* เติม: เพิ่ม checkbox อีกหนึ่งช่องชื่อ "สั่นเมื่อชน" ที่ x=40, y=235
     *       ใบ้: แบบเดียวกับ add_sound_checkbox แต่ไม่ต้องติ๊ก (font_px = 0) */
    if (status == BENTO_OK) {
        status = add_start_button(menu);
    }
    /* เติม: สร้างปุ่ม "ออก" ไว้มุมซ้ายล่างด้วย bento_ui_button_create
     *       ใบ้: x=40, y=300, w=120, h=60 แล้วเก็บ id ไว้ใน menu_t */
    if (status == BENTO_OK) {
        status =
            add_label(menu->ui, "เลือกระดับแล้วกดเริ่มเกม", status_place,
                      COLOR_STATUS, SMALL_PX, &menu->status_label);
    }
    return status;
}

/* The original reads only ev["handle"]: any event on the button counts. */
static void handle_events(const menu_t *menu)
{
    static const char started[] = "เริ่มเกม! (ต่อไปคือโหลดสนาม Snake)";
    bento_ui_event_t events[BENTO_UI_POLL_EVENTS_MAX];
    uint32_t count = 0U;
    uint32_t i;

    /* ui.poll() answers no events when it fails */
    if (bento_ui_poll(menu->ui, events, BENTO_UI_POLL_EVENTS_MAX,
                      &count, NULL) != BENTO_OK) {
        count = 0U;
    }
    for (i = 0U; i < count; i++) {
        if (events[i].id == menu->start_button) {
            /* a setter's refusal was ignored before too (modui.c) */
            (void)bento_ui_set_text(menu->ui, menu->status_label,
                                    started, (uint32_t)strlen(started));
        }
        /* ถ้าทำปุ่ม "ออก" เสร็จแล้ว ลองเทียบ events[i].id กับ id ของปุ่มออก
         * เพื่อจบลูป */
    }
}

/* ลูปเมนู: รอจนกว่าจะกด เริ่มเกม (หรือ ออก) */
static bento_status_t run_menu(const menu_t *menu)
{
    bento_status_t status = BENTO_OK;

    while (status == BENTO_OK) {
        handle_events(menu);
        status = bento_time_sleep_ms(LOOP_MS);
        if ((status == BENTO_OK) && !bento_runtime_is_running()) {
            status = BENTO_E_CANCELLED;
        }
    }
    return status;
}

int main(void)
{
    menu_t menu = {bento_runtime_get_ui(), 0U, 0U};
    /* ตั้งผืนจอให้เท่าจอเกม แล้วล้างของเก่าก่อนวางเมนูใหม่ */
    bento_status_t status =
        bento_ui_screen(menu.ui, SCREEN_W, SCREEN_H);

    if (status == BENTO_OK) {
        status = bento_ui_clear(menu.ui);
    }
    if (status == BENTO_OK) {
        status = build_menu(&menu);
    }
    if (status == BENTO_OK) {
        status = run_menu(&menu);
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
