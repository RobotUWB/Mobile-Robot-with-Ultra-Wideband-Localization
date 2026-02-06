#ifndef SETTINGS_H
#define SETTINGS_H

#include <IPAddress.h>

// --- WiFi Settings ---
#define WIFI_SSID "GMR"
#define WIFI_PASS "12123121211212312121"

// --- Static IP Settings ---
const IPAddress local_IP(192, 168, 88, 115);
const IPAddress gateway(192, 168, 88, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress primaryDNS(8, 8, 8, 8);

// --- Pin Definitions ---
#define RX_PIN 16
#define TX_PIN 17

// --- System Settings ---
#define OTA_HOSTNAME "CyberBot-Clean"
#define RECONNECT_INTERVAL 30000 // 30 วินาที

#endif