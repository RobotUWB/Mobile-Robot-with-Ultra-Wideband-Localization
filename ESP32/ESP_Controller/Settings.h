#ifndef SETTINGS_H
#define SETTINGS_H

#include <IPAddress.h>

// --- WiFi Settings ---
#define WIFI_SSID "UWB_sorting"
#define WIFI_PASS "zzzzzzzz"

const IPAddress local_IP(10, 10, 10, 50);
const IPAddress gateway(10, 10, 10, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress primaryDNS(8, 8, 8, 8);

// --- Pin Definitions ---
#define WIFI_LED_PIN 2
#define RX_Control 16
#define TX_Control 17

#define RX_UWB 26
#define TX_UWB 27
#define UWB_BAUDRATE 115200

#define EMER_PIN 25

// --- System Config ---
#define OTA_HOSTNAME "Sorting"
#define WEBSOCKET_PORT 81
#define WIFI_CHECK_INTERVAL 5000 
#define MAX_SERIAL_BUFFER 100

// --- Heartbeat & Safety Config ---
#define WEB_TIMEOUT_MS 300 // ระยะเวลาสูงสุดที่ยอมให้หน้าเว็บขาดการติดต่อ (300ms)
#define STM32_HEARTBEAT_INTERVAL 100 // ความถี่ที่ ESP32 จะส่งสัญญาณชีพไปหา STM32 (100ms)

#endif