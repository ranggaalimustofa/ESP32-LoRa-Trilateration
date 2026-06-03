#pragma once

// ─── Pin LoRa ────────────────────────────────────────────────────────────────
#if defined(ARDUINO_ARCH_AVR)
  #define LORA_SS    10   // NSS untuk Arduino Pro Mini (RFM95)
  #define LORA_RST   9    // RST untuk Arduino Pro Mini
  #define LORA_DIO0  2    // DIO0 untuk Arduino Pro Mini (D2 mendukung external interrupt)
#else
  #define LORA_SS    5    // NSS untuk ESP32
  #define LORA_RST   14   // RST untuk ESP32
  #define LORA_DIO0  2    // DIO0 untuk ESP32
#endif

// ─── Parameter LoRa ──────────────────────────────────────────────────────────
#define LORA_FREQ        433E6   // Frekuensi: 433E6 / 868E6 / 915E6
#define LORA_SF          7       // Spreading Factor (6–12)
#define LORA_BW          125E3   // Bandwidth (Hz)
#define LORA_TX_POWER    17      // Daya transmisi (dBm)
#define LORA_SYNC_WORD   0x12    // Sync word — harus sama di semua perangkat

// ─── Protokol Paket ──────────────────────────────────────────────────────────
#define PKT_TYPE_TAG_BROADCAST  0x01
#define PKT_TYPE_ANCHOR_REPORT  0x02

// ─── Serial ──────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ─── WiFiManager ─────────────────────────────────────────────────────────────
#define WIFI_AP_PASSWORD  "anchor1234"  // Password portal konfigurasi WiFi
#define WIFI_TIMEOUT_SEC  180           // Timeout portal (detik), 0 = tunggu selamanya

// ─── Tombol BOOT (GPIO0) — satu tombol dua fungsi ────────────────────────────
#define RESET_BUTTON_PIN     0
#define HOLD_PORTAL_IP_MS    3000   // 3 detik → portal Server IP
#define HOLD_RESET_TOTAL_MS  6000   // 6 detik → reset total

// ─── LED Indikator (GPIO16) ──────────────────────────────────────────────────
#define INDICATOR_LED_PIN    16 // WiFi indicator

// ─── MQTT Configurations ─────────────────────────────────────────────────────
#if __has_include("credentials.h")
  #include "credentials.h"
#else
  #define MQTT_BROKER_DEFAULT  "broker.emqx.io"
  #define MQTT_PORT            1883
  #define MQTT_USER            ""
  #define MQTT_PASS            ""
#endif
#define MQTT_TOPIC           "room/positioning/updates"

// ─── Path Loss Model (RSSI -> Jarak) ─────────────────────────────────────────
#define PATH_LOSS_EXP        2.7f
#define RSSI_1M              -40