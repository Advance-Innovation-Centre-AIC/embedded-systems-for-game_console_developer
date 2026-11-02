/*
 * s12_net.c -- เน็ต/IoT: ส่งคะแนนขึ้น Leaderboard ผ่าน MQTT
 *
 * ต่อ WiFi ก่อน (ลองสูงสุด 6 ครั้ง) แล้วใช้ MQTT คุยกับ broker: publish = ส่งคะแนน
 * ของเราขึ้นกระดาน, subscribe + รับข้อความ = รับคะแนนของเพื่อนอีกบอร์ดมาดู 15 วินาที
 * ใช้เล่นแบบ "สองบอร์ดส่งคะแนนหากัน" ได้เลย แก้ค่าตั้งค่า 4 บรรทัดให้ตรงของจริงก่อนรัน
 * เติมช่องที่เว้นไว้ (มี 4 จุด) ให้ครบแล้วลองรันสองบอร์ดส่งคะแนนหากันดู
 *
 * The practise skeleton of the leaderboard: the settings, the messages, the
 * subscribe and the 15 s listening loop are given. The four blanks (join
 * Wi-Fi, show the IP, connect to the broker, publish the score) are TODOs;
 * as shipped the Wi-Fi blank leaves ok false, so the program prints that it
 * could not join and ends, as the Python skeleton does.
 *
 * Twin of s12_net.py (embedded-systems-for-game_console_developer/
 * developer-ii/practise_codes/s12_net.py). Style: CLEAN_C.md.
 * wifi.connect() and mqtt.connect() block in Python; in C each is a start,
 * then a poll until it answers (the C API never blocks): the blanks say
 * which calls. The 15 s window counts milliseconds, not whole seconds.
 * In the BENTO Emulator, MQTT from C is not available yet: the program says
 * so, then takes the .py's "broker unreachable" path (print and end). The
 * note drops ภาษา and บอร์ด from the standard text: as written it is 143
 * bytes, above the 126 a text may carry.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_mqtt.h"
#include "bento_runtime.h"
#include "bento_time.h"
#include "bento_wifi.h"

/* The settings, one table that every step reads. */
typedef struct {
    const char *ssid;
    const char *password;
    const char *broker;
    const char *topic;
    const char *player;
} settings_t;

/* --- ตั้งค่า: แก้ 4 บรรทัดนี้ให้ตรงกับของจริงก่อนรัน --- */
static const settings_t SETTINGS = {
    "YOUR_WIFI_SSID",            /* ชื่อ WiFi (2.4GHz) ของห้องเรา */
    "YOUR_WIFI_PASSWORD",        /* รหัส WiFi */
    "test.mosquitto.org",        /* MQTT broker สาธารณะ (ทดสอบได้เลย) */
    "tesaiot/class/leaderboard", /* ห้องเดียวกันทั้งคาบ ทุกคนใช้ topic นี้ */
    "board-01"                   /* ชื่อผู้เล่น/บอร์ดของเรา (แต่ละคนตั้งไม่ซ้ำ) */
};

enum {
    WIFI_TRIES = 6,  /* ครั้งแรกมักยังไม่ติด */
    RETRY_MS = 2000, /* พัก 2 วิ ให้ WCM ตั้งตัว */
    POLL_MS = 100,
    BROKER_PORT = 1883, /* พอร์ต MQTT ปกติ */
    MY_SCORE = 1250,    /* สมมุติว่าเพิ่งเล่นจบได้เท่านี้ */
    PAYLOAD_CHARS = 24, /* "board-01:" and any int32_t */
    LISTEN_MS = 15000,
    LISTEN_PAUSE_MS = 200 /* พักนิดให้ระบบหายใจ */
};

/* The Emulator note: one label, Thai at 20 px, wrapped inside the screen. */
enum {
    NOTE_X = 16,
    NOTE_Y = 160,
    NOTE_W = 760,
    NOTE_PX = 20,
    NOTE_COLOR = 0xFFFFFF
};

/* Stop arrives at a sleep, as Python's KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t ms)
{
    bento_status_t status = bento_time_sleep_ms(ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* --- 1) ต่อ WiFi --- */
static bento_status_t join_wifi(bool *ok)
{
    (void)printf("กำลังต่อ WiFi ...\n");
    /* เติมช่อง (1): ต่อ WiFi ด้วย bento_wifi_connect_start (ใส่ SETTINGS.ssid, .password
     *   ใน bento_wifi_connect_params_t) แล้ววน bento_wifi_connect_poll ทุก POLL_MS
     *   จนไม่ใช่ BENTO_E_IN_PROGRESS ได้ BENTO_OK เมื่อสำเร็จ ให้เก็บผลไว้ใน *ok */
    *ok = false;
    return BENTO_OK;
}

static void show_ip(void)
{
    /* เติมช่อง (2): โชว์ IP ที่ได้มาด้วย bento_wifi_ip(BENTO_WIFI_IF_ANY, ...)
     *   และ bento_wifi_ipv4_format (แทน "?" ด้านล่าง) */
    (void)printf("ต่อ WiFi แล้ว IP = %s\n", "?");
}

/* --- 2) ต่อ MQTT broker --- */
static bento_status_t connect_broker(uint32_t *client, bool *ok)
{
    (void)printf("กำลังต่อ MQTT broker ...\n");
    /* เติมช่อง (3): ต่อ broker ด้วย bento_mqtt_open_start (host SETTINGS.broker, port
     *   BROKER_PORT = 1883, client_id SETTINGS.player ใน bento_mqtt_config_t) แล้ววน
     *   bento_mqtt_open_poll(*client) จนไม่ใช่ BENTO_E_IN_PROGRESS
     *   ได้ BENTO_OK เมื่อสำเร็จ ให้เก็บไว้ใน *ok
     *   ถ้า bento_mqtt_open_start ตอบ BENTO_E_UNSUPPORTED (BENTO Emulator ยังไม่มี
     *   MQTT จากภาษา C) ให้คืนค่านั้นออกไปเลย use_broker() จะขึ้นข้อความบอกเอง */
    *client = 0U;
    *ok = false;
    return BENTO_OK;
}

/* --- 3) ส่งคะแนนของเราขึ้นกระดาน (รูปแบบง่าย ๆ "ชื่อ:คะแนน") --- */
static bento_status_t publish_score(uint32_t client)
{
    char payload[PAYLOAD_CHARS];
    bento_status_t status;

    (void)snprintf(payload, sizeof(payload), "%s:%" PRId32,
                   SETTINGS.player, (int32_t)MY_SCORE);
    /* เติมช่อง (4): ส่งคะแนนขึ้นกระดานด้วย bento_mqtt_publish(client, SETTINGS.topic,
     *   ความยาว topic, payload, ความยาว payload, qos 0, flags 0) */
    (void)client;
    status = BENTO_OK;
    if (status == BENTO_OK) {
        (void)printf("ส่งคะแนนแล้ว -> %s\n", payload);
    }
    return status;
}

/* One get_message(): print a message when one is waiting. */
static bento_status_t print_message(uint32_t client)
{
    char topic[BENTO_MQTT_TOPIC_MAX + 1U];
    uint8_t data[BENTO_MQTT_PAYLOAD_MAX + 1U];
    bento_mqtt_msg_t message;
    bento_status_t status;

    bento_mqtt_msg_init(&message);
    status = bento_mqtt_poll(client, &message, topic, sizeof(topic),
                             data, BENTO_MQTT_PAYLOAD_MAX);
    if (status == BENTO_OK) {
        data[message.payload_len] = 0U; /* the decoded text ends here */
        (void)printf("ได้คะแนนใหม่: %s\n", (const char *)data);
    }
    return (status == BENTO_E_NOT_FOUND) ? BENTO_OK : status;
}

/* --- 4) วนรับคะแนนของเพื่อน 15 วินาที --- */
static bento_status_t listen(uint32_t client)
{
    const uint32_t start_ms = bento_time_ticks_ms();
    bento_status_t status = BENTO_OK;

    while ((status == BENTO_OK) &&
           (bento_time_ticks_diff(bento_time_ticks_ms(), start_ms) <
            LISTEN_MS)) {
        status = print_message(client);
        if (status == BENTO_OK) {
            status = pause_ms(LISTEN_PAUSE_MS);
        }
    }
    return status;
}

/* Subscribe, publish, listen, disconnect. */
static bento_status_t share_scores(uint32_t client)
{
    uint8_t granted = 0U;
    /* สมัครรับข้อความใน topic เดียวกัน เพื่อเห็นคะแนนของทุกบอร์ด */
    bento_status_t status = bento_mqtt_subscribe(
        client, SETTINGS.topic, (uint32_t)strlen(SETTINGS.topic), 0U,
        &granted);

    if (status == BENTO_OK) {
        status = publish_score(client);
    }
    if (status == BENTO_OK) {
        status = listen(client);
    }
    if (status == BENTO_OK) {
        status = bento_mqtt_close(client);
    }
    if (status == BENTO_OK) {
        (void)printf("จบคาบ ปิดการเชื่อมต่อเรียบร้อย\n");
    }
    return status;
}

/* The BENTO Emulator's C runtime has no MQTT yet (bento_mqtt answers
 * BENTO_E_UNSUPPORTED there): say so on screen and on the console. */
static bento_status_t show_emulator_note(void)
{
    static const char note[] = "MQTT จาก C ยังไม่มีใน BENTO Emulator "
                               "ให้รันบน Dev Kit หรือ Eva Kit";
    bento_ui_widget_args_t args;
    bento_ui_id_t label;
    bento_status_t status;

    bento_ui_widget_args_init(&args);
    args.text = note;
    args.text_len = (uint32_t)strlen(note);
    args.x = NOTE_X;
    args.y = NOTE_Y;
    args.w = NOTE_W;
    args.color = NOTE_COLOR;
    args.font_px = NOTE_PX;
    status =
        bento_ui_label_create(bento_runtime_get_ui(), &args, &label);
    (void)printf(
        "%s / MQTT from C is not in the BENTO Emulator yet: run it "
        "on a Dev Kit or Eva Kit.\n",
        note);
    return status;
}

/* The broker part: connect, then share the scores, or say why not. */
static bento_status_t use_broker(void)
{
    uint32_t client = 0U;
    bool ok = false;
    bento_status_t status = connect_broker(&client, &ok);

    if (status == BENTO_E_UNSUPPORTED) {
        /* then on as the .py goes when the broker cannot be reached */
        status = show_emulator_note();
        ok = false;
    }
    if ((status == BENTO_OK) && !ok) {
        (void)printf("ต่อ broker ไม่ได้\n");
    } else if (status == BENTO_OK) {
        (void)printf("ต่อ broker แล้ว\n");
        status = share_scores(client);
    } else {
        /* the status ends the program */
    }
    return status;
}

int main(void)
{
    bool ok = false;
    bento_status_t status = join_wifi(&ok);

    if ((status == BENTO_OK) && !ok) {
        (void)printf("ต่อ WiFi ไม่ได้ ลองเช็ค SSID/รหัสอีกที\n");
    } else if (status == BENTO_OK) {
        show_ip();
        status = use_broker();
    } else {
        /* the status ends the program */
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
