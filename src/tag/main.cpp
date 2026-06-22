#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "lora_config.h"
#include "packet.h"

// ─── AVR Compatibility ───────────────────────────────────────────────────────
#if defined(ARDUINO_ARCH_AVR)
#include <stdarg.h>
void serial_printf(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
}
#else
#define serial_printf Serial.printf
#endif

// ─── Konfigurasi Tag ──────────────────────────────────────────────────────────
// Injeksi via build_flags: -D TAG_ID=1
#ifndef TAG_ID
  #define TAG_ID  1
#endif

// Interval broadcast dalam milidetik
#define BROADCAST_INTERVAL_MS  1000   // Kirim setiap 1 detik

// ─── Prototypes ──────────────────────────────────────────────────────────────
void initLoRa();
void broadcastTag();
uint8_t readBatteryPercentage();

// ─── State ───────────────────────────────────────────────────────────────────
static uint16_t seqNumber    = 0;
static uint32_t lastBroadcast = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial);

    pinMode(BATTERY_PIN, INPUT);

    serial_printf("[TAG-%d] Booting...\n", TAG_ID);
    initLoRa();

    serial_printf("[TAG-%d] Ready. Broadcasting every %d ms\n",
                  TAG_ID, BROADCAST_INTERVAL_MS);
}

void loop() {
    uint32_t now = millis();

    if (now - lastBroadcast >= BROADCAST_INTERVAL_MS) {
        lastBroadcast = now;
        broadcastTag();
    }
}

// ─── Inisialisasi LoRa ───────────────────────────────────────────────────────
void initLoRa() {
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQ)) {
        serial_printf("[TAG-%d] ERROR: LoRa init failed! Halting.\n", TAG_ID);
        while (true);
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();

    serial_printf("[TAG-%d] LoRa OK — SF%d, BW%.0fkHz, SyncWord=0x%02X\n",
                  TAG_ID, LORA_SF, LORA_BW / 1000.0f, LORA_SYNC_WORD);
}

// ─── Broadcast paket tag ke semua anchor ─────────────────────────────────────
void broadcastTag() {
    TagPacket pkt;
    pkt.pkt_type  = PKT_TYPE_TAG_BROADCAST;
    pkt.tag_id    = TAG_ID;
    pkt.timestamp = millis();
    pkt.seq       = seqNumber++;
    pkt.battery   = readBatteryPercentage();

    // Kirim paket sebagai raw bytes
    LoRa.beginPacket();
    LoRa.write((uint8_t *)&pkt, sizeof(TagPacket));
    LoRa.endPacket();   // Blocking — tunggu sampai TX selesai

    serial_printf("[TAG-%d] Broadcast — Seq=%d | TS=%lu ms | Bat=%d%%\n",
                  TAG_ID, pkt.seq, pkt.timestamp, pkt.battery);
}

// ─── Membaca persentase sisa baterai ─────────────────────────────────────────
uint8_t readBatteryPercentage() {
    float voltage = 0.0f;
    const float vRef = 3.3f;
    
    #if defined(ARDUINO_ARCH_AVR)
        // Arduino Pro Mini (10-bit ADC)
        int raw = analogRead(BATTERY_PIN);
        // Asumsi pembagi tegangan 2x (misal resistor 100k/100k)
        voltage = (raw * vRef / 1023.0f) * 2.0f;
    #else
        // ESP32 (12-bit ADC)
        int raw = analogRead(BATTERY_PIN);
        // Asumsi pembagi tegangan 2x (misal resistor 100k/100k)
        voltage = (raw * 3.3f / 4095.0f) * 2.0f;
    #endif

    // Konversi voltase ke persentase (3.2V = 0%, 4.2V = 100%)
    int pct = (int)((voltage - 3.2f) / (4.2f - 3.2f) * 100.0f);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    
    return (uint8_t)pct;
}
