/*
 * s12_net.c -- เน็ต/IoT: ส่งคะแนนขึ้น Leaderboard ผ่าน MQTT
 *
 * ต่อ WiFi ก่อน (ลองสูงสุด 6 ครั้ง) แล้วใช้ MQTT คุยกับ broker: publish = ส่งคะแนน
 * ของเราขึ้นกระดาน, subscribe + รับข้อความ = รับคะแนนของเพื่อนอีกบอร์ดมาดู 15 วินาที
 * ใช้เล่นแบบ "สองบอร์ดส่งคะแนนหากัน" ได้เลย แก้ค่าตั้งค่า 4 บรรทัดให้ตรงของจริงก่อนรัน
 *
 * Joins Wi-Fi (up to 6 tries, 2 s apart), connects to an MQTT broker,
 * subscribes to the class topic, publishes "player:score", then prints every
 * score message that arrives for 15 seconds and disconnects. Edit SSID,
 * PASSWORD, BROKER and TOPIC at the top first; run it on the board.
 *
 * Twin of s12_net.py (embedded-systems-for-game_console_developer/
 * developer-ii/solution_codes/s12_net.py). Style: CLEAN_C.md.
 * wifi.connect() and mqtt.connect() block in Python; here each is start, then
 * a poll every POLL_MS until it answers (the C API never blocks). The 15 s
 * window counts milliseconds where Python's time.time() counts whole seconds.
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

/* ---- แก้ก่อนรัน: settings (MISRA deviation CT02) ---- */
/* แก้ 4 บรรทัดนี้ให้ตรงกับของจริงก่อนรัน */
static const char SSID[] = "YOUR_WIFI_SSID"; /* ชื่อ WiFi (2.4GHz) */
static const char PASSWORD[] = "YOUR_WIFI_PASSWORD"; /* รหัส WiFi */
static const char BROKER[] = "test.mosquitto.org"; /* broker สาธารณะ */
/* ห้องเดียวกันทั้งคาบ ทุกคนใช้ topic นี้ */
static const char TOPIC[] = "tesaiot/class/leaderboard";
/* ชื่อผู้เล่น/บอร์ดของเรา (แต่ละคนตั้งไม่ซ้ำ) */
static const char PLAYER[] = "board-01";
/* ---- end of settings ---- */

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

/* Polls a started connect until it stops answering "in progress". */
static bento_status_t wait_for(bento_status_t (*poll)(uint32_t),
                               uint32_t handle, bento_status_t *answer)
{
    bento_status_t status = BENTO_OK;

    *answer = BENTO_E_IN_PROGRESS;
    while ((status == BENTO_OK) && (*answer == BENTO_E_IN_PROGRESS)) {
        status = pause_ms(POLL_MS);
        if (status == BENTO_OK) {
            *answer = poll(handle);
        }
    }
    if ((status == BENTO_OK) && (*answer == BENTO_E_CANCELLED)) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

static bento_status_t poll_wifi(uint32_t unused)
{
    (void)unused; /* one Wi-Fi join at a time: no handle */
    return bento_wifi_connect_poll();
}

/* One wifi.connect(): *joined is its True or False; a refusal or a
 * timeout is False, not an error. */
static bento_status_t wifi_connect(bool *joined)
{
    bento_wifi_connect_params_t params;
    bento_status_t answer;
    bento_status_t status = BENTO_OK;

    bento_wifi_connect_params_init(&params);
    params.ssid = SSID;
    params.ssid_len = (uint32_t)strlen(SSID);
    params.pass = PASSWORD;
    params.pass_len = (uint32_t)strlen(PASSWORD);
    answer = bento_wifi_connect_start(&params);
    if (answer == BENTO_OK) {
        status = wait_for(poll_wifi, 0U, &answer);
    }
    *joined = (status == BENTO_OK) && (answer == BENTO_OK);
    return status;
}

/* --- 1) ต่อ WiFi --- */
static bento_status_t join_wifi(bool *ok)
{
    int32_t i;
    bento_status_t status = BENTO_OK;

    (void)printf("กำลังต่อ WiFi ...\n");
    *ok = false;
    for (i = 0; (status == BENTO_OK) && !*ok && (i < WIFI_TRIES); i++) {
        status = wifi_connect(ok);
        if ((status == BENTO_OK) && !*ok) {
            (void)printf("  ยังไม่ติด ลองใหม่ %d/6 ...\n", (int)(i + 1));
            status = pause_ms(RETRY_MS);
        }
    }
    return status;
}

/* โชว์ IP ที่ได้มา */
static bento_status_t show_ip(void)
{
    char text[BENTO_WIFI_IPV4_TEXT_MAX];
    uint32_t address = 0U;
    uint32_t length = 0U;
    bento_status_t status = bento_wifi_ip(BENTO_WIFI_IF_ANY, &address);

    if (status == BENTO_OK) {
        status = bento_wifi_ipv4_format(address, text, sizeof(text),
                                        &length);
    }
    if (status == BENTO_OK) {
        (void)printf("ต่อ WiFi แล้ว IP = %s\n", text);
    }
    return status;
}

/* --- 2) ต่อ MQTT broker: *ok is mqtt.connect()'s True or False --- */
static bento_status_t connect_broker(uint32_t *client, bool *ok)
{
    bento_mqtt_config_t config;
    bento_status_t answer;
    bento_status_t status = BENTO_OK;

    (void)printf("กำลังต่อ MQTT broker ...\n");
    bento_mqtt_config_init(&config);
    config.host = BROKER;
    config.host_len = (uint32_t)strlen(BROKER);
    config.port = (uint16_t)BROKER_PORT;
    config.client_id = PLAYER;
    config.client_id_len = (uint32_t)strlen(PLAYER);
    answer = bento_mqtt_open_start(&config, client);
    if (answer == BENTO_E_UNSUPPORTED) {
        /* the BENTO Emulator: use_broker() says so and ends */
        status = answer;
    } else if (answer == BENTO_OK) {
        status = wait_for(bento_mqtt_open_poll, *client, &answer);
    } else {
        /* a refusal at the start is mqtt.connect()'s False */
    }
    *ok = (status == BENTO_OK) && (answer == BENTO_OK);
    return status;
}

/* --- 3) ส่งคะแนนของเราขึ้นกระดาน (รูปแบบง่าย ๆ "ชื่อ:คะแนน") --- */
static bento_status_t publish_score(uint32_t client)
{
    char payload[PAYLOAD_CHARS];
    bento_status_t status;

    (void)snprintf(payload, sizeof(payload), "%s:%" PRId32, PLAYER,
                   (int32_t)MY_SCORE);
    status = bento_mqtt_publish(client, TOPIC, (uint32_t)strlen(TOPIC),
                                (const uint8_t *)payload,
                                (uint32_t)strlen(payload), 0U, 0U);
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
        client, TOPIC, (uint32_t)strlen(TOPIC), 0U, &granted);

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
        status = show_ip();
        if (status == BENTO_OK) {
            status = use_broker();
        }
    } else {
        /* the status ends the program */
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
