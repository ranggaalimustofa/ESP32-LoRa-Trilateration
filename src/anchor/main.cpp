#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>
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
void   openServerIPPortal();
void   initLoRa();
bool   parsePacket(AnchorReport &report);
void   sendReportToServer(const AnchorReport &report);
void   saveServerIP(const char *ip);
String loadServerIP();
bool   isValidIP(const String &ip);
void   reconnectMQTT();
float  rssiToDistance(int rssi);

// ─── Global ──────────────────────────────────────────────────────────────────
Preferences            prefs;
String                 serverIP = ""; // Berfungsi sebagai MQTT Broker Address
WiFiManagerParameter  *paramServerIP;
WiFiClient             espClient;
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

    // Tampilkan Server IP/Broker yang akan digunakan
    Serial.printf("[ANCHOR-%d] MQTT Broker IP: %s\n", ANCHOR_ID, serverIP.c_str());

    // Konfigurasi Server MQTT
    mqttClient.setServer(serverIP.c_str(), MQTT_PORT);

    // Init LoRa
    initLoRa();

    Serial.printf("[ANCHOR-%d] Ready | IP: %s | Broker: %s:%d\n",
                  ANCHOR_ID,
                  WiFi.localIP().toString().c_str(),
                  serverIP.c_str(),
                  MQTT_PORT);
}

void loop() {
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
//  Tahan 3 detik          → buka portal update Server IP (WiFi tetap)
//  Tahan 6 detik          → reset TOTAL (hapus WiFi + Server IP)
//
void checkResetButton() {
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    if (digitalRead(RESET_BUTTON_PIN) != LOW) return;  // Tidak ditekan

    Serial.printf("[ANCHOR-%d] Tombol BOOT ditekan...\n", ANCHOR_ID);
    Serial.printf("[ANCHOR-%d]  3 detik = Portal Server IP\n", ANCHOR_ID);
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

        // Tandai sudah lewat 3 detik
        if (!reached3s && held >= HOLD_PORTAL_IP_MS) {
            reached3s = true;
            Serial.printf("[ANCHOR-%d] >> Lepas sekarang untuk Portal Server IP\n", ANCHOR_ID);
            Serial.printf("[ANCHOR-%d] >> Tahan terus untuk Reset TOTAL\n", ANCHOR_ID);
        }

        // Sudah 6 detik → reset total
        if (held >= HOLD_RESET_TOTAL_MS) {
            Serial.printf("[ANCHOR-%d] RESET TOTAL — Menghapus WiFi + Server IP...\n", ANCHOR_ID);
            WiFiManager wm;
            wm.resetSettings();
            prefs.begin("anchor-cfg", false);
            prefs.clear();
            prefs.end();
            Serial.printf("[ANCHOR-%d] Selesai. Restart...\n", ANCHOR_ID);
            delay(300);
            ESP.restart();
        }
    }

    // Tombol dilepas antara 3–6 detik → portal Server IP saja
    unsigned long held = millis() - pressTime;
    if (held >= HOLD_PORTAL_IP_MS) {
        Serial.printf("[ANCHOR-%d] Membuka portal update MQTT Broker...\n", ANCHOR_ID);
        // WiFi sudah tersimpan, konek dulu lalu buka portal
        WiFi.begin();
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(300);
        openServerIPPortal();
        ESP.restart();
    }

    // Dilepas sebelum 3 detik → batal
    Serial.printf("[ANCHOR-%d] Tombol dilepas, boot normal.\n", ANCHOR_ID);
}

// ─── Portal khusus update MQTT Broker (tanpa reset WiFi) ───────────────────────
void openServerIPPortal() {
    char apName[20];
    snprintf(apName, sizeof(apName), "Anchor-%d", ANCHOR_ID);

    char savedIP[40];
    serverIP.toCharArray(savedIP, sizeof(savedIP));

    WiFiManagerParameter paramIP("server_ip", "MQTT Broker (contoh: broker.emqx.io)", savedIP, 39);

    WiFiManager wm;
    wm.addParameter(&paramIP);
    wm.setCustomHeadElement(
        "<style>body{font-family:sans-serif;} label{font-weight:bold;}</style>"
        "<h3 style='color:#1a73e8'>Update MQTT Broker</h3>"
        "<p style='color:#555'>Isi alamat IP / domain MQTT Broker Anda.</p>"
    );

    // startConfigPortal: buka portal tanpa reset WiFi
    wm.setConfigPortalTimeout(WIFI_TIMEOUT_SEC);

    Serial.printf("[ANCHOR-%d] Portal aktif: sambung WiFi 'Anchor-%d' → buka http://192.168.4.1\n",
                  ANCHOR_ID, ANCHOR_ID);

    wm.startConfigPortal(apName, WIFI_AP_PASSWORD);

    // Ambil nilai yang diisi user
    String newIP = String(paramIP.getValue());
    newIP.trim();

    if (newIP.length() > 0 && isValidIP(newIP)) {
        // User mengisi yang valid — simpan ke NVS
        saveServerIP(newIP.c_str());
        serverIP = newIP;
        Serial.printf("[ANCHOR-%d] MQTT Broker disimpan: %s\n", ANCHOR_ID, serverIP.c_str());
    } else if (newIP.length() > 0) {
        // User mengisi tapi format salah — tolak, pakai default
        Serial.printf("[ANCHOR-%d] ERROR: Format Broker tidak valid: '%s'\n", ANCHOR_ID, newIP.c_str());
        serverIP = MQTT_BROKER_DEFAULT;
        Serial.printf("[ANCHOR-%d] Menggunakan Broker default: %s\n", ANCHOR_ID, serverIP.c_str());
    } else {
        // User tidak mengisi — pakai default
        serverIP = MQTT_BROKER_DEFAULT;
        Serial.printf("[ANCHOR-%d] MQTT Broker tidak diisi, menggunakan default: %s\n",
                      ANCHOR_ID, serverIP.c_str());
    }
}

// ─── Inisialisasi WiFi dengan WiFiManager ────────────────────────────────────
void initWiFi() {
    serverIP = loadServerIP();

    char savedIP[40];
    serverIP.toCharArray(savedIP, sizeof(savedIP));

    paramServerIP = new WiFiManagerParameter(
        "server_ip", "MQTT Broker (contoh: broker.emqx.io)", savedIP, 39
    );

    WiFiManager wm;
    wm.addParameter(paramServerIP);

    wm.setAPCallback([](WiFiManager *mgr) {
        Serial.printf("[ANCHOR-%d] Portal konfigurasi aktif!\n", ANCHOR_ID);
        Serial.printf("[ANCHOR-%d] Sambung ke WiFi 'Anchor-%d' (pass: %s)\n",
                      ANCHOR_ID, ANCHOR_ID, WIFI_AP_PASSWORD);
        Serial.printf("[ANCHOR-%d] Lalu buka browser: http://192.168.4.1\n", ANCHOR_ID);
        Serial.printf("[ANCHOR-%d] Isi SSID WiFi + MQTT Broker lalu klik Save.\n", ANCHOR_ID);
    });

    wm.setSaveConfigCallback([]() {
        String newIP = String(paramServerIP->getValue());
        newIP.trim();
        if (newIP.length() > 0 && isValidIP(newIP)) {
            // User mengisi yang valid — simpan ke NVS
            saveServerIP(newIP.c_str());
            serverIP = newIP;
            Serial.printf("[ANCHOR-%d] MQTT Broker disimpan: %s\n", ANCHOR_ID, serverIP.c_str());
        } else if (newIP.length() > 0) {
            // Format salah — tolak, pakai default
            Serial.printf("[ANCHOR-%d] ERROR: Format Broker tidak valid: '%s'\n", ANCHOR_ID, newIP.c_str());
            serverIP = MQTT_BROKER_DEFAULT;
            saveServerIP(MQTT_BROKER_DEFAULT);
            Serial.printf("[ANCHOR-%d] Menggunakan Broker default: %s\n", ANCHOR_ID, serverIP.c_str());
        } else {
            // Tidak diisi — pakai default
            serverIP = MQTT_BROKER_DEFAULT;
            saveServerIP(MQTT_BROKER_DEFAULT);
            Serial.printf("[ANCHOR-%d] MQTT Broker tidak diisi, menggunakan default: %s\n",
                          ANCHOR_ID, serverIP.c_str());
        }
    });

    if (WIFI_TIMEOUT_SEC > 0) wm.setConfigPortalTimeout(WIFI_TIMEOUT_SEC);

    char apName[20];
    snprintf(apName, sizeof(apName), "Anchor-%d", ANCHOR_ID);

    if (!wm.autoConnect(apName, WIFI_AP_PASSWORD)) {
        Serial.printf("[ANCHOR-%d] Timeout WiFi. Restart...\n", ANCHOR_ID);
        ESP.restart();
    }

    Serial.printf("[ANCHOR-%d] WiFi OK — SSID: %s | IP: %s\n",
                  ANCHOR_ID,
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
}

// ─── NVS: Simpan & Load Server/Broker IP ─────────────────────────────────────
// ─── Validasi format IP / Domain Hostname ────────────────────────────────────
bool isValidIP(const String &ip) {
    if (ip.length() == 0) return false;
    for (int i = 0; i < (int)ip.length(); i++) {
        char c = ip[i];
        if (!isalnum(c) && c != '.' && c != '-') return false;
    }
    return true;
}

void saveServerIP(const char *ip) {
    prefs.begin("anchor-cfg", false);
    prefs.putString("server_ip", ip);
    prefs.end();
}

String loadServerIP() {
    prefs.begin("anchor-cfg", true);
    // Jika NVS kosong, gunakan MQTT_BROKER_DEFAULT
    String ip = prefs.getString("server_ip", MQTT_BROKER_DEFAULT);
    prefs.end();
    return ip;
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
    doc["battery"] = 100;
    
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
                      ANCHOR_ID, serverIP.c_str(), MQTT_PORT);
        
        String clientId = "ESP32-Anchor-" + String(ANCHOR_ID) + "-" + String(random(1000, 9999));
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.printf("[ANCHOR-%d] MQTT Terhubung!\n", ANCHOR_ID);
        } else {
            Serial.printf("[ANCHOR-%d] Gagal terhubung, rc=%d. Coba lagi dalam 5 detik...\n", 
                          ANCHOR_ID, mqttClient.state());
            delay(5000);
        }
    }
}