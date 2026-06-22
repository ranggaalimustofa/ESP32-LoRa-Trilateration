#pragma once
#include <stdint.h>

#pragma pack(push, 1)

// ─── Paket yang dikirim Tag (broadcast) ──────────────────────────────────────
// Tag mengirim paket ini, semua anchor akan menerimanya
struct TagPacket {
    uint8_t  pkt_type;    // Selalu PKT_TYPE_TAG_BROADCAST (0x01)
    uint8_t  tag_id;      // ID unik tag (0–254)
    uint32_t timestamp;   // millis() saat paket dikirim (ms)
    uint16_t seq;         // Sequence number, bertambah tiap broadcast
    uint8_t  battery;     // Persentase baterai (0–100)
};

// ─── Paket yang dikirim Anchor ke Server (via Serial) ────────────────────────
// Anchor membungkus data TagPacket + RSSI + SNR lalu kirim ke server
struct AnchorReport {
    uint8_t  pkt_type;    // Selalu PKT_TYPE_ANCHOR_REPORT (0x02)
    uint8_t  anchor_id;   // ID anchor yang menerima (1, 2, atau 3)
    uint8_t  tag_id;      // ID tag yang mengirim broadcast
    uint16_t seq;         // Sequence number dari tag
    uint32_t tag_ts;      // Timestamp dari tag (millis tag)
    uint32_t anchor_ts;   // Timestamp saat anchor menerima (millis anchor)
    int8_t   rssi;        // RSSI dalam dBm (negatif, contoh: -72)
    int8_t   snr;         // SNR dalam dB (bisa negatif)
    uint8_t  battery;     // Persentase baterai dari tag
};

#pragma pack(pop)
