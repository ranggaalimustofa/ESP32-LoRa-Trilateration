# 📡 Trilaterasi ESP32 + LoRa

<div align="center">

![ESP32](https://img.shields.io/badge/ESP32-Microcontroller-blue?style=for-the-badge&logo=espressif)
![LoRa](https://img.shields.io/badge/LoRa-Communication-green?style=for-the-badge)
![PlatformIO](https://img.shields.io/badge/PlatformIO-IDE-orange?style=for-the-badge&logo=platformio)
![License](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Active-brightgreen?style=for-the-badge)

**Sistem penentuan posisi berbasis RSSI menggunakan protokol LoRa dan mikrokontroler ESP32**

[Fitur](#-fitur) · [Arsitektur](#-arsitektur-sistem) · [Hardware](#-kebutuhan-hardware) · [Instalasi](#-instalasi) · [Penggunaan](#-penggunaan) · [Kontribusi](#-kontribusi)

</div>

---

## 📖 Deskripsi

Proyek ini mengimplementasikan sistem **trilaterasi** untuk menentukan posisi suatu titik (node) di dalam ruang menggunakan **minimal 3 anchor node** yang berkomunikasi melalui protokol **LoRa (Long Range)**. Estimasi posisi dihitung berdasarkan nilai **RSSI (Received Signal Strength Indicator)** yang dikonversi menjadi jarak, kemudian diproses menggunakan algoritma trilaterasi geometris.

> 💡 **Trilaterasi** adalah metode penentuan posisi dengan menggunakan jarak dari tiga atau lebih titik referensi yang diketahui koordinatnya.

---

## ✨ Fitur

- 📶 Komunikasi jarak jauh menggunakan modul LoRa (SX1276/SX1278)
- 📍 Estimasi posisi 2D/3D menggunakan algoritma trilaterasi
- 🔁 Update posisi secara real-time
- 📊 Logging data RSSI dan koordinat via Serial Monitor
- 🌐 Visualisasi posisi opsional melalui Web Dashboard (ESP32 WebServer)
- ⚙️ Konfigurasi parameter LoRa yang fleksibel (frekuensi, SF, BW)
- 🔋 Mode hemat daya (Deep Sleep) untuk node baterai

---

## 🏗️ Arsitektur Sistem

```
                        ┌─────────────┐
                        │  Anchor A   │
                        │  ESP32+LoRa │
                        │  (x1, y1)   │
                        └──────┬──────┘
                               │ RSSI → d1
              ┌────────────────┼────────────────┐
              │                │                │
     ┌────────┴──────┐  ┌──────▼──────┐  ┌─────┴────────┐
     │   Anchor B    │  │   NODE      │  │   Anchor C   │
     │  ESP32+LoRa   │  │  ESP32+LoRa │  │  ESP32+LoRa  │
     │   (x2, y2)    │  │  (x?, y?)   │  │   (x3, y3)   │
     └───────────────┘  └─────────────┘  └──────────────┘
     RSSI → d2                                RSSI → d3
```

### Alur Kerja

```
Node Broadcast ──► Anchor menerima sinyal
                        │
                        ▼
               Hitung jarak dari RSSI
               d = 10^((TxPower - RSSI) / (10 * n))
                        │
                        ▼
               Kirim (AnchorID, RSSI, d) ke server
                        │
                        ▼
               Trilaterasi → (x̂, ŷ) estimasi posisi
                        │
                        ▼
               Tampilkan / Simpan hasil
```

---

## 🔧 Kebutuhan Hardware

| Komponen | Jumlah | Keterangan |
|----------|--------|------------|
| ESP32 Dev Board | 4+ | 3 anchor + 1 node mobile |
| Modul LoRa SX1276/SX1278 | 4+ | Frekuensi 433/868/915 MHz |
| Antena LoRa | 4+ | Sesuaikan dengan frekuensi |
| Kabel jumper | Secukupnya | Female-to-male |
| Breadboard / PCB | Sesuai kebutuhan | |
| Power Supply / Baterai | 4+ | 3.3V atau 5V |

### Wiring ESP32 ↔ LoRa (SX1278)

| LoRa Pin | ESP32 Pin |
|----------|-----------|
| VCC      | 3.3V      |
| GND      | GND       |
| SCK      | GPIO 18   |
| MISO     | GPIO 19   |
| MOSI     | GPIO 23   |
| NSS/CS   | GPIO 5    |
| RST      | GPIO 14   |
| DIO0     | GPIO 2    |

> ⚠️ **Catatan**: Sesuaikan pin mapping di file `config.h` jika menggunakan pin berbeda.

---

## 📦 Kebutuhan Software & Library

### Tools

| Software | Keterangan |
|----------|------------|
| [Visual Studio Code](https://code.visualstudio.com/) | Code editor utama |
| [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode) | Extension VS Code untuk embedded development |
| [Git](https://git-scm.com/) | Version control |
| Driver USB (CH340 / CP2102) | Sesuaikan dengan board ESP32 yang digunakan |

### Library yang Dibutuhkan

Library di bawah ini sudah terdaftar di `platformio.ini` dan akan **diunduh otomatis** saat build pertama kali.

| Library | Registry ID | Keterangan |
|---------|-------------|------------|
| [LoRa by Sandeep Mistry](https://github.com/sandeepmistry/arduino-LoRa) | `sandeepmistry/LoRa @ ^0.8.0` | Driver modul LoRa SX127x |
| [ArduinoJson](https://arduinojson.org/) | `bblanchon/ArduinoJson @ ^6.21.0` | Parsing & serialisasi JSON |
| [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) | `me-no-dev/AsyncTCP @ ^1.1.1` | Async TCP (opsional, untuk WebServer) |
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) | `me-no-dev/ESP Async WebServer @ ^1.2.3` | Web dashboard (opsional) |

---

## 🚀 Instalasi

### 1. Clone Repository

```bash
git clone https://github.com/username/trilaterasi-esp32-lora.git
cd trilaterasi-esp32-lora
```

### 2. Struktur Direktori

Proyek menggunakan struktur **PlatformIO multi-environment** — setiap perangkat (anchor/node) adalah environment terpisah dalam satu repository.

```
trilaterasi-esp32-lora/
├── platformio.ini              # Konfigurasi utama PlatformIO (semua environment)
│
├── anchor/
│   ├── src/
│   │   └── main.cpp            # Kode utama anchor
│   └── include/
│       └── config.h            # Konfigurasi anchor (ID, koordinat, pin LoRa)
│
├── node/
│   ├── src/
│   │   └── main.cpp            # Kode utama node mobile
│   └── include/
│       └── config.h            # Konfigurasi node
│
├── server/                     # (Opsional) ESP32 sebagai server trilaterasi
│   ├── src/
│   │   └── main.cpp
│   └── include/
│       ├── config.h
│       └── trilateration.h     # Algoritma trilaterasi
│
├── lib/                        # Library lokal/custom (jika ada)
├── docs/
│   ├── wiring_diagram.png
│   └── system_overview.pdf
├── test/                       # Unit test (PlatformIO test runner)
│   └── test_trilateration/
│       └── test_main.cpp
├── .gitignore
├── LICENSE
└── README.md
```

### 3. Konfigurasi `platformio.ini`

File ini mendefinisikan semua environment (anchor, node, server) dalam satu proyek:

```ini
[platformio]
default_envs = anchor_1

; ─── Konfigurasi bersama ───────────────────────────────────────────────────
[env:base]
platform  = espressif32
board     = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    sandeepmistry/LoRa @ ^0.8.0
    bblanchon/ArduinoJson @ ^6.21.0
    ; Hapus komentar jika menggunakan Web Dashboard:
    ; me-no-dev/AsyncTCP @ ^1.1.1
    ; me-no-dev/ESP Async WebServer @ ^1.2.3

; ─── Anchor 1 ──────────────────────────────────────────────────────────────
[env:anchor_1]
extends     = env:base
src_dir     = anchor/src
build_flags =
    -I anchor/include
    -D ANCHOR_ID=1
    -D ANCHOR_X=0.0
    -D ANCHOR_Y=0.0

; ─── Anchor 2 ──────────────────────────────────────────────────────────────
[env:anchor_2]
extends     = env:base
src_dir     = anchor/src
build_flags =
    -I anchor/include
    -D ANCHOR_ID=2
    -D ANCHOR_X=5.0
    -D ANCHOR_Y=0.0

; ─── Anchor 3 ──────────────────────────────────────────────────────────────
[env:anchor_3]
extends     = env:base
src_dir     = anchor/src
build_flags =
    -I anchor/include
    -D ANCHOR_ID=3
    -D ANCHOR_X=2.5
    -D ANCHOR_Y=5.0

; ─── Node Mobile ───────────────────────────────────────────────────────────
[env:node]
extends     = env:base
src_dir     = node/src
build_flags =
    -I node/include
    -D NODE_ID=10

; ─── Server (Opsional) ─────────────────────────────────────────────────────
[env:server]
extends     = env:base
src_dir     = server/src
build_flags =
    -I server/include
lib_deps =
    ${env:base.lib_deps}
    me-no-dev/AsyncTCP @ ^1.1.1
    me-no-dev/ESP Async WebServer @ ^1.2.3
```

> 💡 **Keunggulan multi-environment**: Satu codebase untuk semua perangkat. Koordinat dan ID anchor dikonfigurasi langsung melalui `build_flags`, tidak perlu edit source code.

### 4. Konfigurasi per Perangkat (`config.h`)

Edit file `anchor/include/config.h`:

```cpp
// Konfigurasi LoRa
#define LORA_FREQ       433E6       // Frekuensi: 433E6 / 868E6 / 915E6
#define LORA_SF         7           // Spreading Factor: 6-12
#define LORA_BW         125E3       // Bandwidth: 7.8E3 - 500E3
#define LORA_TX_POWER   17          // Daya transmisi (dBm)

// Pin ESP32
#define LORA_SS         5
#define LORA_RST        14
#define LORA_DIO0       2

// Catatan: ANCHOR_ID, ANCHOR_X, ANCHOR_Y diinjeksi via build_flags di platformio.ini
```

Edit file `node/include/config.h`:

```cpp
#define BROADCAST_INTERVAL  1000    // Interval broadcast (ms)

// Konfigurasi LoRa (harus sama persis dengan anchor)
#define LORA_FREQ       433E6
#define LORA_SF         7
#define LORA_BW         125E3

// Catatan: NODE_ID diinjeksi via build_flags di platformio.ini
```

### 5. Build & Upload

**Menggunakan PlatformIO CLI:**

```bash
# Build semua environment
pio run

# Build environment tertentu
pio run --environment anchor_1

# Upload ke perangkat (pastikan sudah terhubung via USB)
pio run --target upload --environment anchor_1
pio run --target upload --environment anchor_2
pio run --target upload --environment anchor_3
pio run --target upload --environment node

# Upload + langsung buka Serial Monitor
pio run --target upload --environment anchor_1 && pio device monitor --environment anchor_1
```

**Menggunakan PlatformIO di VS Code:**

1. Buka folder proyek di VS Code
2. Klik ikon PlatformIO di sidebar kiri
3. Di panel **Project Tasks**, pilih environment yang diinginkan
4. Klik **Upload** atau gunakan shortcut `Ctrl+Alt+U`

**Serial Monitor:**

```bash
# Buka serial monitor untuk environment tertentu
pio device monitor --environment anchor_1 --baud 115200

# Atau via VS Code: klik ikon 🔌 di status bar bawah
```

---

## 📐 Penggunaan

### Penempatan Anchor

Letakkan **minimal 3 anchor** dengan koordinat yang sudah diketahui:

```
Anchor A: (0, 0)   ← pojok kiri bawah
Anchor B: (5, 0)   ← pojok kanan bawah  (5 meter dari A)
Anchor C: (2.5, 5) ← tengah atas        (5 meter dari baseline)
```

> 📐 **Tips**: Semakin besar sudut antar anchor, semakin akurat hasil trilaterasi. Hindari penempatan yang colinear (satu garis lurus).

### Kalibrasi Model Path Loss

Sebelum digunakan, lakukan kalibrasi untuk mendapatkan nilai **Path Loss Exponent (n)** yang sesuai lingkungan:

```cpp
// Model path loss jarak-RSSI
// RSSI = TxPower - 10 * n * log10(d)
// Nilai n: 2.0 (ruang terbuka), 2.7-3.5 (dalam gedung)

#define PATH_LOSS_EXP   2.7         // Sesuaikan dengan environment
#define TX_POWER_DBM    17          // Daya transmisi anchor (dBm)
#define RSSI_1M         -60         // RSSI pada jarak 1 meter (kalibrasi)
```

### Monitor Output

```
Serial Monitor (115200 baud):

[ANCHOR-1] Node-10 RSSI: -72 dBm | Jarak: 3.24 m
[ANCHOR-2] Node-10 RSSI: -68 dBm | Jarak: 2.51 m  
[ANCHOR-3] Node-10 RSSI: -79 dBm | Jarak: 4.87 m

[TRILATERASI] Estimasi posisi Node-10:
  X: 2.43 m
  Y: 1.87 m
  Error estimasi: ±0.3 m
```

---

## 🧮 Algoritma Trilaterasi

Sistem menggunakan pendekatan **Least Squares** untuk menyelesaikan persamaan lingkaran dari 3+ anchor:

```
(x - x₁)² + (y - y₁)² = d₁²   ...(1)
(x - x₂)² + (y - y₂)² = d₂²   ...(2)
(x - x₃)² + (y - y₃)² = d₃²   ...(3)
```

Dikurangi persamaan (1) dari (2) dan (3) untuk mendapatkan sistem linear **Ax = b**, kemudian diselesaikan dengan metode pseudoinverse:

```
x = (Aᵀ A)⁻¹ Aᵀ b
```

---

## 📊 Performa & Akurasi

| Kondisi | Akurasi Estimasi |
|---------|-----------------|
| Line-of-Sight (LOS), ruang terbuka | ±0.5 – 1.5 m |
| Non-LOS, dalam gedung | ±1.5 – 3.0 m |
| Dengan obstacle padat | ±3.0 – 5.0 m |

> 📈 Akurasi dapat ditingkatkan dengan **filtering (Kalman Filter)** atau menambah jumlah anchor.

---

## 🐛 Troubleshooting

| Masalah | Kemungkinan Penyebab | Solusi |
|---------|---------------------|--------|
| LoRa tidak terdeteksi | Wiring salah / modul rusak | Cek koneksi SPI dan pin |
| RSSI tidak stabil | Interferensi / antena buruk | Ganti antena, pindah lokasi |
| Posisi tidak akurat | Nilai `n` tidak terkalibrasi | Lakukan kalibrasi path loss |
| Node tidak menerima data | Frekuensi/SF tidak sama | Samakan konfigurasi LoRa |
| Upload gagal | Driver CH340/CP2102 belum ada | Install driver yang sesuai |
| `Library not found` saat build | Cache PlatformIO korup | Jalankan `pio lib update` atau hapus folder `.pio/` |
| Port COM tidak terdeteksi | Izin port di Linux/macOS | Jalankan `sudo usermod -aG dialout $USER` lalu logout |
| Build error: `multiple definition` | Conflict environment | Pastikan `src_dir` tiap environment berbeda di `platformio.ini` |
| ESP32 terus restart (boot loop) | Stack overflow / heap penuh | Kurangi ukuran buffer JSON atau aktifkan PSRAM |

---

## 🤝 Kontribusi

Kontribusi sangat disambut! Silakan ikuti langkah berikut:

1. **Fork** repository ini
2. Buat branch fitur baru: `git checkout -b fitur/nama-fitur`
3. Commit perubahan: `git commit -m 'Tambah fitur: nama-fitur'`
4. Push ke branch: `git push origin fitur/nama-fitur`
5. Buat **Pull Request**

Untuk perubahan besar, buka **Issue** terlebih dahulu untuk mendiskusikan ide.

---

## 📚 Referensi

- [LoRa Alliance - LoRaWAN Specification](https://lora-alliance.org/)
- [Sandeep Mistry - Arduino LoRa Library](https://github.com/sandeepmistry/arduino-LoRa)
- Zafari, F. et al. (2019). *A Survey of Indoor Localization Systems and Technologies*. IEEE Communications Surveys & Tutorials.
- Goldsmith, A. (2005). *Wireless Communications*. Cambridge University Press.

---

## 📄 Lisensi

Proyek ini dilisensikan di bawah **MIT License** — lihat file [LICENSE](LICENSE) untuk detail.

---

## 👤 Kontak

**Nama Anda** — [@username](https://github.com/username) — email@example.com

🔗 Link Proyek: [https://github.com/username/trilaterasi-esp32-lora](https://github.com/username/trilaterasi-esp32-lora)

---

<div align="center">

Dibuat dengan ❤️ menggunakan ESP32 + LoRa

⭐ Beri bintang jika proyek ini bermanfaat!

</div>