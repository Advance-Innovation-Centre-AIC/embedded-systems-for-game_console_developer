/*
 * s13_net.c -- เน็ต/IoT: ส่งคะแนนขึ้น Leaderboard ผ่าน MQTT
 *
 * Twin of s13_net.py (embedded-systems-for-game_console_developer/developer-i/solution_codes/s13_net.py)
 *
 * ต่อ WiFi ก่อน (ลองได้หกครั้ง) แล้วต่อ MQTT broker สมัครรับ topic
 * กระดานคะแนนของห้อง ส่งคะแนนของเราขึ้นไปเป็น "ชื่อ:คะแนน" แล้วรอรับ
 * คะแนนของเพื่อนอีกบอร์ด 15 วินาที จบแล้วปิดการเชื่อมต่อ
 * แก้ชื่อ WiFi รหัส broker และชื่อผู้เล่นให้ตรงของจริงก่อนรัน
 *
 * Joins Wi-Fi first (up to six tries), then the MQTT broker,
 * subscribes to the class leaderboard topic, publishes our score as
 * "name:score" and listens 15 seconds for the scores of the other
 * boards, then closes the link. Set the Wi-Fi name, the broker and
 * the player name to the real ones before running.
 *
 * C twin of the Example Explorer program devi-s13-net
 * (py/devi-s13-net.py), with the blanks filled as this solution fills
 * them. Style: CLEAN_C.md. What C must do differently:
 *   - wifi.connect() and mqtt.connect() print "[WiFi] ..." and
 *     "[MQTT] ..." lines of their own; the C modules print none;
 *   - a refused join, a timeout or a broker that does not answer is
 *     MicroPython's False; any other failure ends the program, as the
 *     exception there;
 *   - the binding keeps one incoming message, a newer one replacing
 *     it; bento_mqtt keeps four, so each round reads them all and
 *     prints the newest, cut to 255 bytes as there;
 *   - `raise SystemExit` ends the program with 0;
 *   - the client is closed when Stop ends the program too, which
 *     MicroPython leaves to the firmware.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bento_mqtt.h"
#include "bento_runtime.h"
#include "bento_time.h"
#include "bento_wifi.h"

/* Set these four to the real ones before running: TOPIC and PLAYER
 * here, the WiFi ssid and password in join_wifi(), the broker in
 * connect_broker() (each sits in the one function that reads it). */
/* one room, one topic for the whole lesson */
static const char TOPIC[] = "tesaiot/class/leaderboard";
/* our player or board name; each one picks another */
static const char PLAYER[] = "board-01";

enum {
    WIFI_TRIES = 6,       /* the first try often fails */
    RETRY_MS = 2000,      /* let the WCM settle between tries */
    BROKER_PORT = 1883,   /* the usual MQTT port */
    MY_SCORE = 1250,      /* as if a round just ended with it */
    LISTEN_S = 15,        /* how long to listen for other scores */
    LISTEN_STEP_MS = 200, /* a short rest for the system each round */
    JOIN_POLL_MS = 50     /* how often a join is asked how it goes */
};

enum {
    PAYLOAD_BYTES = 40,  /* PLAYER, a colon, any int32_t and the NUL */
    MESSAGE_BYTES = 256, /* the binding keeps 255 bytes of a payload */
    TOPIC_BYTES = 129    /* bento_mqtt keeps 128 bytes of a topic */
};

/* The newest message of a round, as the binding's one slot keeps it. */
typedef struct {
    bool received;
    uint32_t length;
    uint8_t payload[MESSAGE_BYTES];
} message_t;

/* Stop arrives during a sleep, as KeyboardInterrupt does. */
static bento_status_t pause_ms(uint32_t duration_ms)
{
    bento_status_t status = bento_time_sleep_ms(duration_ms);

    if ((status == BENTO_OK) && !bento_runtime_is_running()) {
        status = BENTO_E_CANCELLED;
    }
    return status;
}

/* Waits for the join the radio is making; Stop cancels it. */
static bento_status_t wait_for_join(void)
{
    bento_status_t status = BENTO_E_IN_PROGRESS;

    while (status == BENTO_E_IN_PROGRESS) {
        status = bento_time_sleep_ms(JOIN_POLL_MS);
        if (status == BENTO_OK) {
            status = bento_wifi_connect_poll();
        }
    }
    if (status == BENTO_E_CANCELLED) {
        (void)bento_wifi_connect_cancel(); /* answers BENTO_OK only */
    }
    return status;
}

/* wifi.connect(SSID, PASSWORD): *joined is its True or False. */
static bento_status_t join_wifi(bool *joined)
{
    static const char ssid[] =
        "YOUR_WIFI_SSID"; /* the room's 2.4 GHz WiFi */
    static const char password[] = "YOUR_WIFI_PASSWORD";
    bento_wifi_connect_params_t params;
    bento_status_t status;

    bento_wifi_connect_params_init(&params);
    params.ssid = ssid;
    params.ssid_len = (uint32_t)strlen(ssid);
    params.pass = password;
    params.pass_len = (uint32_t)strlen(password);
    params.timeout_ms = 0U; /* the binding's own window */
    params.flags = BENTO_WIFI_F_SAVE_CREDS; /* the binding saves too */
    status = bento_wifi_connect_start(&params);
    if (status == BENTO_OK) {
        status = wait_for_join();
    }
    *joined = (status == BENTO_OK);
    if ((status == BENTO_E_FAILED) || (status == BENTO_E_TIMEOUT) ||
        (status == BENTO_E_INVALID_ARGUMENT)) {
        status = BENTO_OK;
    }
    return status;
}

/* Up to six tries, two seconds apart. */
static bento_status_t connect_wifi(bool *joined)
{
    bento_status_t status = BENTO_OK;
    int32_t i;

    (void)printf("กำลังต่อ WiFi ...\n");
    *joined = false;
    for (i = 0; (i < WIFI_TRIES) && (status == BENTO_OK) && !*joined;
         i++) {
        status = join_wifi(joined);
        if ((status == BENTO_OK) && !*joined) {
            (void)printf("  ยังไม่ติด ลองใหม่ %d/6 ...\n", (int)(i + 1));
            status = pause_ms(RETRY_MS);
        }
    }
    return status;
}

/* wifi.ip(): "0.0.0.0" when there is no address. */
static void print_ip(void)
{
    char text[BENTO_WIFI_IPV4_TEXT_MAX];
    uint32_t address = 0U;
    uint32_t length = 0U;

    if (bento_wifi_ip(BENTO_WIFI_IF_ANY, &address) != BENTO_OK) {
        address = 0U;
    }
    if (bento_wifi_ipv4_format(address, text, sizeof(text), &length) !=
        BENTO_OK) {
        (void)snprintf(text, sizeof(text), "0.0.0.0");
    }
    (void)printf("ต่อ WiFi แล้ว IP = %s\n", text);
}

/* mqtt.connect(BROKER, 1883, client_id=PLAYER): *linked is its True
 * or False. It raises OSError only when Wi-Fi cannot start. */
static bento_status_t connect_broker(uint32_t *client, bool *linked)
{
    /* a public broker, ready to test at once */
    static const char broker[] = "test.mosquitto.org";
    bento_mqtt_config_t config;
    bento_status_t status;

    bento_mqtt_config_init(&config);
    config.host = broker;
    config.host_len = (uint32_t)strlen(broker);
    config.port = (uint16_t)BROKER_PORT;
    config.client_id = PLAYER;
    config.client_id_len = (uint32_t)strlen(PLAYER);
    status = bento_mqtt_open_start(&config, client);
    if (status == BENTO_OK) {
        status = bento_mqtt_open_poll(*client);
    }
    while (status == BENTO_E_IN_PROGRESS) {
        status = pause_ms(JOIN_POLL_MS);
        if (status == BENTO_OK) {
            status = bento_mqtt_open_poll(*client);
        }
    }
    *linked = (status == BENTO_OK);
    if ((status != BENTO_E_CANCELLED) &&
        (status != BENTO_E_NOT_READY) &&
        (status != BENTO_E_UNSUPPORTED)) {
        status = BENTO_OK;
    }
    return status;
}

/* mqtt.subscribe(TOPIC): a broker's refusal is a False nobody reads. */
static bento_status_t subscribe_topic(uint32_t client)
{
    uint8_t granted_qos = 0U;
    bento_status_t status = bento_mqtt_subscribe(
        client, TOPIC, (uint32_t)strlen(TOPIC), 0U, &granted_qos);

    return (status == BENTO_E_FAILED) ? BENTO_OK : status;
}

/* mqtt.publish(TOPIC, payload) raises OSError without a link, and
 * answers a False nobody reads when the library refuses. */
static bento_status_t publish_score(uint32_t client)
{
    char payload[PAYLOAD_BYTES];
    uint32_t state = BENTO_MQTT_STATE_CLOSED;
    bento_status_t status = bento_mqtt_state(client, &state);

    if ((status == BENTO_OK) && (state != BENTO_MQTT_STATE_CONNECTED)) {
        status = BENTO_E_SESSION;
    }
    if (status == BENTO_OK) {
        (void)snprintf(payload, sizeof(payload), "%s:%d", PLAYER,
                       MY_SCORE);
        /* the original ignores publish()'s False too */
        (void)bento_mqtt_publish(client, TOPIC, (uint32_t)strlen(TOPIC),
                                 (const uint8_t *)payload,
                                 (uint32_t)strlen(payload), 0U, 0U);
        (void)printf("ส่งคะแนนแล้ว -> %s\n", payload);
    }
    return status;
}

/* Every message waiting now; the last one read is the newest. An
 * empty ring is get_message()'s None. */
static void read_newest(uint32_t client, message_t *newest)
{
    char topic[TOPIC_BYTES];
    bento_mqtt_msg_t message;

    newest->received = false;
    bento_mqtt_msg_init(&message);
    while (bento_mqtt_poll(
               client, &message, topic, sizeof(topic), newest->payload,
               (uint32_t)sizeof(newest->payload) - 1U) == BENTO_OK) {
        newest->received = true;
        newest->length = message.payload_len;
        bento_mqtt_msg_init(&message);
    }
}

/* Prints the other boards' scores until the deadline. */
static bento_status_t listen_scores(uint32_t client)
{
    message_t newest;
    int64_t now_s = 0;
    bento_status_t status = bento_time_time(&now_s);
    const int64_t deadline_s = now_s + (int64_t)LISTEN_S;

    while ((status == BENTO_OK) && (now_s < deadline_s)) {
        read_newest(client, &newest);
        if (newest.received) {
            (void)printf("ได้คะแนนใหม่: %.*s\n", (int)newest.length,
                         (const char *)newest.payload);
        }
        status = pause_ms(LISTEN_STEP_MS);
        if (status == BENTO_OK) {
            status = bento_time_time(&now_s);
        }
    }
    return status;
}

/* Steps 2 to 4, once Wi-Fi is up: the broker, our score, theirs. */
static bento_status_t play_online(uint32_t *client)
{
    bool linked = false;
    bento_status_t status;

    (void)printf("กำลังต่อ MQTT broker ...\n");
    status = connect_broker(client, &linked);
    if ((status == BENTO_OK) && !linked) {
        (void)printf("ต่อ broker ไม่ได้\n");
        status = BENTO_E_CANCELLED; /* raise SystemExit: a normal end */
    }
    if (status == BENTO_OK) {
        (void)printf("ต่อ broker แล้ว\n");
        status = subscribe_topic(*client);
    }
    if (status == BENTO_OK) {
        status = publish_score(*client);
    }
    if (status == BENTO_OK) {
        status = listen_scores(*client);
    }
    return status;
}

int main(void)
{
    uint32_t client = 0U;
    bool joined = false;
    bento_status_t status = connect_wifi(&joined);

    if ((status == BENTO_OK) && !joined) {
        (void)printf("ต่อ WiFi ไม่ได้ ลองเช็ค SSID/รหัสอีกที\n");
        status = BENTO_E_CANCELLED; /* raise SystemExit: a normal end */
    }
    if (status == BENTO_OK) {
        print_ip();
        status = play_online(&client);
    }
    if (client != 0U) {
        const bento_status_t close_status = bento_mqtt_close(client);

        if (status == BENTO_OK) {
            status = close_status;
            (void)printf("จบคาบ ปิดการเชื่อมต่อเรียบร้อย\n");
        }
    }
    return (status == BENTO_E_CANCELLED) ? 0 : (int)status;
}
