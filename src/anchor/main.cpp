#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include "lora_config.h"
#include "packet.h"

// ─── Konfigurasi Anchor ───────────────────────────────────────────────────────
#ifndef ANCHOR_ID
  #define ANCHOR_ID  1
#endif
#ifndef ANCHOR_X
  #define ANCHOR_X   0.0
#endif
#ifndef ANCHOR_Y
  #define ANCHOR_Y   0.0
#endif

// ─── Prototypes ──────────────────────────────────────────────────────────────
void   checkResetButton();
void   initWiFi();
void   initLoRa();
bool   parsePacket(AnchorReport &report);
void   sendReportToServer(const AnchorReport &report);
void   reconnectMQTT();
float  rssiToDistance(int rssi);
void   initOTA();

// ─── Global ──────────────────────────────────────────────────────────────────
Preferences            prefs;
WiFiClientSecure       espClient;
PubSubClient           mqttClient(espClient);

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial);

    char apName[20];
    snprintf(apName, sizeof(apName), "Anchor-%d", ANCHOR_ID);
    Serial.printf("\n[ANCHOR-%d] Booting...\n", ANCHOR_ID);

    // Cek tombol dulu sebelum hal lain
    checkResetButton();

    // Init WiFi
    initWiFi();

    // Init OTA
    initOTA();

    // Tampilkan Server IP/Broker yang akan digunakan
    Serial.printf("[ANCHOR-%d] MQTT Broker IP: %s\n", ANCHOR_ID, MQTT_BROKER_DEFAULT);

    // Konfigurasi Server MQTT
    espClient.setInsecure();
    mqttClient.setServer(MQTT_BROKER_DEFAULT, MQTT_PORT);

    // Init LoRa
    initLoRa();

    Serial.printf("[ANCHOR-%d] Ready | IP: %s | Broker: %s:%d\n",
                  ANCHOR_ID,
                  WiFi.localIP().toString().c_str(),
                  MQTT_BROKER_DEFAULT,
                  MQTT_PORT);
}

void loop() {
    // Proses OTA handle
    ArduinoOTA.handle();

    // Reconnect WiFi jika putus
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[ANCHOR-%d] WiFi putus, reconnecting...\n", ANCHOR_ID);
        WiFi.reconnect();
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(500);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[ANCHOR-%d] Gagal reconnect, restart...\n", ANCHOR_ID);
            ESP.restart();
        }
    }

    
    // Pastikan koneksi MQTT aktif
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    // Kirim heartbeat berkala ke MQTT (setiap 5 detik) agar server tahu jangkar ini aktif
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 5000) {
        lastHeartbeat = millis();
        StaticJsonDocument<128> hbDoc;
        hbDoc["heartbeat"] = true;
        hbDoc["anchorId"] = ANCHOR_ID;
        hbDoc["ip"] = WiFi.localIP().toString();
        String hbPayload;
        serializeJson(hbDoc, hbPayload);
        mqttClient.publish(MQTT_TOPIC, hbPayload.c_str());
        Serial.printf("[ANCHOR-%d] Sent direct heartbeat to MQTT (IP: %s)\n", ANCHOR_ID, hbDoc["ip"].as<const char*>());
    }

    // Polling LoRa
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    AnchorReport report;
    if (parsePacket(report)) {
        sendReportToServer(report);
    }
}

// ─── Cek Tombol BOOT saat startup ────────────────────────────────────────────
//
//  Tidak ditekan          → boot normal
//  Tahan 3 detik          → buka portal konfigurasi WiFi (WiFi tetap)
//  Tahan 6 detik          → reset TOTAL (hapus WiFi)
//
void checkResetButton() {
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(INDICATOR_LED_PIN, OUTPUT);
    digitalWrite(INDICATOR_LED_PIN, LOW); // LED Mati secara default

    if (digitalRead(RESET_BUTTON_PIN) != LOW) return;  // Tidak ditekan

    Serial.printf("[ANCHOR-%d] Tombol BOOT ditekan...\n", ANCHOR_ID);
    Serial.printf("[ANCHOR-%d]  3 detik = Portal Konfigurasi WiFi\n", ANCHOR_ID);
    Serial.printf("[ANCHOR-%d]  6 detik = Reset TOTAL\n", ANCHOR_ID);

    unsigned long pressTime = millis();
    bool          reached3s = false;

    while (digitalRead(RESET_BUTTON_PIN) == LOW) {
        unsigned long held = millis() - pressTime;

        // Feedback setiap 1 detik
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint >= 1000) {
            lastPrint = millis();
            Serial.printf("[ANCHOR-%d] Ditahan: %lu detik...\n", ANCHOR_ID, held / 1000 + 1);
        }

        // Tandai sudah lewat 3 detik -> LED Menyala Solid
        if (!reached3s && held >= HOLD_PORTAL_IP_MS && held < HOLD_RESET_TOTAL_MS) {
            reached3s = true;
            digitalWrite(INDICATOR_LED_PIN, HIGH); // LED Menyala Solid
            Serial.printf("[ANCHOR-%d] >> Lepas sekarang untuk Portal Konfigurasi WiFi\n", ANCHOR_ID);
            Serial.printf("[ANCHOR-%d] >> Tahan terus untuk Reset TOTAL\n", ANCHOR_ID);
        }

        // Sudah 6 detik → LED Berkedip Cepat (Kedip)
        if (held >= HOLD_RESET_TOTAL_MS) {
            digitalWrite(INDICATOR_LED_PIN, (millis() / 200) % 2 == 0 ? HIGH : LOW);
        }

        // Sudah 6 detik → reset total
        if (held >= HOLD_RESET_TOTAL_MS) {
            Serial.printf("[ANCHOR-%d] RESET TOTAL — Menghapus WiFi...\n", ANCHOR_ID);
            
            // Efek kedip sangat cepat selama 1.5 detik sebagai indikator konfirmasi reset total sebelum reboot
            for (int i = 0; i < 30; i++) {
                digitalWrite(INDICATOR_LED_PIN, !digitalRead(INDICATOR_LED_PIN));
                delay(50);
            }
            digitalWrite(INDICATOR_LED_PIN, LOW);

            WiFiManager wm;
            wm.resetSettings();
            prefs.begin("anchor-cfg", false);
            prefs.clear();
            prefs.end();
            Serial.printf("[ANCHOR-%d] Selesai. Restart...\n", ANCHOR_ID);
            delay(300);
            ESP.restart();
        }
        delay(10); // Ringankan beban CPU
    }

    // Tombol dilepas antara 3–6 detik → portal konfigurasi WiFi saja
    unsigned long held = millis() - pressTime;
    if (held >= HOLD_PORTAL_IP_MS && held < HOLD_RESET_TOTAL_MS) {
        digitalWrite(INDICATOR_LED_PIN, HIGH); // Pastikan LED menyala solid saat masuk portal
        Serial.printf("[ANCHOR-%d] Membuka portal konfigurasi WiFi...\n", ANCHOR_ID);
        
        WiFiManager wm;
        wm.setAPCallback([](WiFiManager *mgr) {
            digitalWrite(INDICATOR_LED_PIN, HIGH);
            Serial.printf("[ANCHOR-%d] Portal konfigurasi WiFi manual aktif!\n", ANCHOR_ID);
            Serial.printf("[ANCHOR-%d] Sambung ke WiFi 'Anchor-%d' (pass: %s)\n",
                          ANCHOR_ID, ANCHOR_ID, WIFI_AP_PASSWORD);
            Serial.printf("[ANCHOR-%d] Lalu buka browser: http://192.168.4.1\n", ANCHOR_ID);
        });
        wm.setConfigPortalTimeout(WIFI_TIMEOUT_SEC);
        
        char apName[20];
        snprintf(apName, sizeof(apName), "Anchor-%d", ANCHOR_ID);
        wm.startConfigPortal(apName, WIFI_AP_PASSWORD);
        
        digitalWrite(INDICATOR_LED_PIN, LOW); // Matikan LED setelah selesai
        ESP.restart();
    }

    // Dilepas sebelum 3 detik → batal
    digitalWrite(INDICATOR_LED_PIN, LOW); // Pastikan mati
    Serial.printf("[ANCHOR-%d] Tombol dilepas, boot normal.\n", ANCHOR_ID);
}

// ─── Inisialisasi WiFi dengan WiFiManager ────────────────────────────────────
void initWiFi() {
    WiFiManager wm;

    wm.setAPCallback([](WiFiManager *mgr) {
        digitalWrite(INDICATOR_LED_PIN, HIGH); // LED Menyala Solid saat portal aktif
        Serial.printf("[ANCHOR-%d] Portal konfigurasi aktif!\n", ANCHOR_ID);
        Serial.printf("[ANCHOR-%d] Sambung ke WiFi 'Anchor-%d' (pass: %s)\n",
                      ANCHOR_ID, ANCHOR_ID, WIFI_AP_PASSWORD);
        Serial.printf("[ANCHOR-%d] Lalu buka browser: http://192.168.4.1\n", ANCHOR_ID);
    });

    if (WIFI_TIMEOUT_SEC > 0) wm.setConfigPortalTimeout(WIFI_TIMEOUT_SEC);

    char apName[20];
    snprintf(apName, sizeof(apName), "Anchor-%d", ANCHOR_ID);

    if (!wm.autoConnect(apName, WIFI_AP_PASSWORD)) {
        digitalWrite(INDICATOR_LED_PIN, LOW); // Matikan LED jika gagal/timeout
        Serial.printf("[ANCHOR-%d] Timeout WiFi. Restart...\n", ANCHOR_ID);
        ESP.restart();
    }

    digitalWrite(INDICATOR_LED_PIN, LOW); // Matikan LED jika koneksi WiFi sukses

    Serial.printf("[ANCHOR-%d] WiFi OK — SSID: %s | IP: %s\n",
                  ANCHOR_ID,
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
}

// ─── Inisialisasi LoRa ───────────────────────────────────────────────────────
void initLoRa() {
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQ)) {
        Serial.printf("[ANCHOR-%d] ERROR: LoRa init failed!\n", ANCHOR_ID);
        while (true);
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();

    Serial.printf("[ANCHOR-%d] LoRa OK — SF%d, BW%.0fkHz\n",
                  ANCHOR_ID, LORA_SF, LORA_BW / 1000.0f);
}

// ─── Parse paket LoRa ────────────────────────────────────────────────────────
bool parsePacket(AnchorReport &report) {
    int available = LoRa.available();
    if (available != sizeof(TagPacket)) {
        while (LoRa.available()) LoRa.read();
        Serial.printf("[ANCHOR-%d] WARN: Ukuran paket tidak valid (%d byte).\n",
                      ANCHOR_ID, available);
        return false;
    }

    TagPacket pkt;
    uint8_t  *buf = (uint8_t *)&pkt;
    for (size_t i = 0; i < sizeof(TagPacket); i++) {
        buf[i] = (uint8_t)LoRa.read();
    }

    if (pkt.pkt_type != PKT_TYPE_TAG_BROADCAST) {
        Serial.printf("[ANCHOR-%d] WARN: Tipe paket tidak dikenal (0x%02X).\n",
                      ANCHOR_ID, pkt.pkt_type);
        return false;
    }

    report.pkt_type  = PKT_TYPE_ANCHOR_REPORT;
    report.anchor_id = ANCHOR_ID;
    report.tag_id    = pkt.tag_id;
    report.seq       = pkt.seq;
    report.tag_ts    = pkt.timestamp;
    report.anchor_ts = (uint32_t)millis();
    report.rssi      = (int8_t)LoRa.packetRssi();
    report.snr       = (int8_t)LoRa.packetSnr();
    report.battery   = pkt.battery;

    return true;
}

// ─── Kalibrasi RSSI -> Jarak (dalam meter) ───────────────────────────────────
float rssiToDistance(int rssi) {
    if (rssi >= RSSI_1M) return 0.1f;
    float power = (float)(RSSI_1M - rssi) / (10.0f * PATH_LOSS_EXP);
    return pow(10, power);
}

// ─── Kirim laporan ke server via MQTT ────────────────────────────────────────
void sendReportToServer(const AnchorReport &report) {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }

    float distance = rssiToDistance(report.rssi);

    StaticJsonDocument<256> doc;
    doc["tagId"] = "tag-" + String(report.tag_id);
    doc["battery"] = report.battery;
    
    JsonArray anchors = doc.createNestedArray("anchors");
    JsonObject anchorObj = anchors.createNestedObject();
    anchorObj["anchorId"] = report.anchor_id;
    anchorObj["distance"] = distance;
    anchorObj["rssi"] = report.rssi;
    anchorObj["snr"] = report.snr;
    anchorObj["x"] = ANCHOR_X;
    anchorObj["y"] = ANCHOR_Y;

    String payload;
    serializeJson(doc, payload);

    if (mqttClient.publish(MQTT_TOPIC, payload.c_str())) {
        Serial.printf("[ANCHOR-%d] MQTT OK → Topic: %s | Tag=%d | Seq=%d | RSSI=%d dBm | Dist=%.2fm\n",
                      ANCHOR_ID, MQTT_TOPIC, report.tag_id,
                      report.seq, report.rssi, distance);
    } else {
        Serial.printf("[ANCHOR-%d] MQTT PUBLISH FAILED! Topic: %s\n", ANCHOR_ID, MQTT_TOPIC);
    }
}

// ─── Koneksi Ulang MQTT ──────────────────────────────────────────────────────
void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.printf("[ANCHOR-%d] Menghubungkan ke MQTT Broker di %s:%d...\n", 
                      ANCHOR_ID, MQTT_BROKER_DEFAULT, MQTT_PORT);
        
        // Nyalakan LED saat mulai menghubungkan
        digitalWrite(INDICATOR_LED_PIN, HIGH);
        
        String clientId = "ESP32-Anchor-" + String(ANCHOR_ID) + "-" + String(random(1000, 9999));
        
        if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.printf("[ANCHOR-%d] MQTT Terhubung!\n", ANCHOR_ID);
            digitalWrite(INDICATOR_LED_PIN, LOW); // Matikan LED ketika berhasil terhubung
        } else {
            Serial.printf("[ANCHOR-%d] Gagal terhubung, rc=%d. Coba lagi dalam 5 detik...\n", 
                          ANCHOR_ID, mqttClient.state());
            
            // Berkedip selama 5 detik penantian sebelum mencoba lagi
            unsigned long startDelay = millis();
            while (millis() - startDelay < 5000) {
                digitalWrite(INDICATOR_LED_PIN, (millis() / 250) % 2 == 0 ? HIGH : LOW);
                delay(50);
            }
        }
    }
}

// ─── Inisialisasi OTA (Over The Air) Updates ─────────────────────────────────
void initOTA() {
    char hostName[30];
    snprintf(hostName, sizeof(hostName), "Anchor-Node-%d", ANCHOR_ID);
    ArduinoOTA.setHostname(hostName);
    
    // Opsional: berikan password keamanan untuk upload OTA
    // ArduinoOTA.setPassword("anchorota123");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("[OTA] Mulai update " + type);
        digitalWrite(INDICATOR_LED_PIN, HIGH); // Nyalakan LED solid saat mulai
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update Berhasil Selesai!");
        // Berkedip cepat 10 kali sebagai selebrasi keberhasilan ota sebelum restart otomatis
        for (int i = 0; i < 10; i++) {
            digitalWrite(INDICATOR_LED_PIN, HIGH);
            delay(50);
            digitalWrite(INDICATOR_LED_PIN, LOW);
            delay(50);
        }
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
        // Berkedip dinamis saat kemajuan transfer data berjalan
        digitalWrite(INDICATOR_LED_PIN, !digitalRead(INDICATOR_LED_PIN));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
        
        digitalWrite(INDICATOR_LED_PIN, LOW); // Matikan LED jika error
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Layanan OTA Aktif | Hostname: %s\n", hostName);
}
