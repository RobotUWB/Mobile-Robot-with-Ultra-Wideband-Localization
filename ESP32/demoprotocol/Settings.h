#ifndef SETTINGS_H
#define SETTINGS_H

#include <IPAddress.h>

// --- WiFi Settings ---
#define WIFI_SSID "GMR"
#define WIFI_PASS "12123121211212312121"

<<<<<<< HEAD
// --- Static IP Settings ---
=======
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
const IPAddress local_IP(192, 168, 88, 115);
const IPAddress gateway(192, 168, 88, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress primaryDNS(8, 8, 8, 8);

// --- Pin Definitions ---
#define RX_PIN 16
#define TX_PIN 17

<<<<<<< HEAD
// --- System Settings ---
#define OTA_HOSTNAME "Sorting"
#define RECONNECT_INTERVAL 30000 // 30 วินาที
=======
// --- System Config ---
#define OTA_HOSTNAME "Sorting"
#define WEBSOCKET_PORT 81
#define WIFI_CHECK_INTERVAL 5000 
#define MAX_SERIAL_BUFFER 100

// --- Heartbeat & Safety Config ---
#define WEB_TIMEOUT_MS 300 // ระยะเวลาสูงสุดที่ยอมให้หน้าเว็บขาดการติดต่อ (300ms)
#define STM32_HEARTBEAT_INTERVAL 100 // ความถี่ที่ ESP32 จะส่งสัญญาณชีพไปหา STM32 (100ms)
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")

#endif