/*
 * s13_net.c -- เน็ต/IoT: ส่งคะแนนขึ้น Leaderboard ผ่าน MQTT (ฉบับฝึก)
 *
 * ไอเดียของคาบนี้: ต่อ WiFi ก่อน แล้วใช้ MQTT คุยกับ broker
 *   publish = ส่งคะแนนของเราขึ้นกระดาน, subscribe + รับข้อความ = ดูคะแนนของเพื่อนอีกบอร์ด
 * ในไฟล์ฝึกช่อง (1) ยังไม่ได้เติม ok จึงเป็น false: โปรแกรมพิมพ์ว่ากำลังต่อ WiFi
 * แล้วพิมพ์ว่าต่อไม่ได้ และจบตรงนั้น
 * วิธีรัน: เปิดไฟล์นี้ใน BENTO IDE แล้วกด "Program to Device"
 *
 * The network lab of lesson 13, the practise version: connect to WiFi, then send
 * this board's score to an MQTT leaderboard and read the other boards' scores for
 * 15 seconds. Blank (1), the WiFi connection, is still empty, so the program
 * prints that it connects, prints that it could not, and ends there.
 *
 * Twin of s13_net.py (embedded-systems-for-game_console_developer/developer-i/
 * practise_codes/s13_net.py). Style: CLEAN_C.md.
 * What C does differently:
 * - raise SystemExit ends the program normally: main returns 0;
 * - the imports of wifi, mqtt and time set nothing up, so C includes nothing for them;
 * - the rest is never reached and is not written as code, as each part below says.
 * Never reached while blank (1) keeps ok false, and how C would do it:
 * - the settings SSID, PASSWORD, BROKER "test.mosquitto.org", TOPIC
 *   "tesaiot/class/leaderboard" and PLAYER "board-01" (static const texts here);
 * - blank (2) prints the IP (bento_wifi_ip, bento_wifi_ipv4_format);
 * - blank (3) connects to the broker on port 1883 (bento_mqtt_open_start, _open_poll),
 *   or prints that it could not and ends;
 * - subscribe to TOPIC (bento_mqtt_subscribe); blank (4) publishes "board-01:1250"
 *   (bento_mqtt_publish) and prints it;
 * - for 15 seconds, every 0.2 s, print each new score (bento_mqtt_poll), then
 *   disconnect (bento_mqtt_close) and print that the lab is done.
 */
#include <stdbool.h>
#include <stdio.h>

#include "bento_runtime.h"

/* ต่อ WiFi: answers true when connected (blank (1) still empty: false). */
static bool connect_wifi(void)
{
    bool connected;

    (void)printf("กำลังต่อ WiFi ...\n");
    /* เติมช่อง (1): ต่อ WiFi ด้วย bento_wifi_connect_start แล้วรอ bento_wifi_connect_poll
     *   ได้ BENTO_OK เมื่อสำเร็จ ให้เก็บผลไว้ใน connected */
    connected = false;
    return connected;
}

int main(void)
{
    const bool connected = connect_wifi();

    if (!connected) {
        (void)printf("ต่อ WiFi ไม่ได้ ลองเช็ค SSID/รหัสอีกที\n");
    }
    return 0;
}
