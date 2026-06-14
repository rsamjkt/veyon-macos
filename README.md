<div align="center">

# AruniControl

### Pemantauan & Pengendalian Komputer untuk Kelas, Lab, dan Ujian

**Kendalikan banyak komputer dari satu layar — lintas macOS dan Windows.**

[![Lisensi](https://img.shields.io/badge/lisensi-GPLv2-green.svg)](COPYING)
[![Versi](https://img.shields.io/badge/versi-1.0.0_"Aurora"-F2812F.svg)](https://github.com/rsamjkt/veyon-macos/releases)
[![Platform](https://img.shields.io/badge/platform-macOS_•_Windows-blue.svg)](#-platform-yang-didukung)

</div>

---

## ✨ Apa itu AruniControl?

**AruniControl** adalah aplikasi untuk **memantau dan mengendalikan banyak komputer sekaligus** dari satu komputer pengajar/admin (disebut **Master**). Cocok untuk:

- 🏫 **Ruang kelas & laboratorium komputer** — pantau semua layar siswa dalam satu tampilan.
- 📝 **Ujian / CBT** — kunci layar, blokir internet, dan pantau aplikasi yang dibuka.
- 🖥️ **Remote support** — ambil alih layar untuk membantu pengguna dari jarak jauh.

AruniControl adalah bagian dari keluarga produk **Arunika** (AruniHealth, Arunika-Casemix, Arunika-ATTA).

---

## 🎯 Fitur

### Fitur inti
| Fitur | Keterangan |
|-------|------------|
| 👁️ **Overview** | Pantau semua komputer dalam satu grid secara real-time |
| 🖱️ **Remote Access** | Lihat & kendalikan layar komputer mana pun |
| 📡 **Demo** | Siarkan layar pengajar ke semua komputer (fullscreen/window) |
| 🔒 **Kunci Layar** | Bekukan layar agar perhatian fokus |
| 💬 **Pesan Teks** | Kirim pengumuman ke layar pengguna |
| ⚡ **Power Control** | Nyalakan / matikan / restart komputer dari jauh |
| 🚀 **Jalankan Program & Buka Website** | Eksekusi aplikasi / URL dari jarak jauh |

### Add-on khas AruniControl
| Add-on | Keterangan | Platform client |
|--------|------------|-----------------|
| 🎥 **Screen Recorder** | Rekam layar client ke video H.264 | dijalankan di Master |
| 🔎 **Network Discovery** | Deteksi otomatis client di jaringan lokal | macOS & Windows |
| 📊 **Application Monitoring** | Daftar aplikasi yang berjalan + aplikasi aktif tiap client (live) | macOS & Windows |
| 🔇 **AruniMedia** | Mute / unmute audio client | macOS & Windows |
| 🚫 **Internet Access Control** | Blokir / izinkan internet (mis. saat ujian) | macOS & Windows |
| 💬 **Chat** | Obrolan dua arah Master ↔ pengguna | macOS & Windows |

---

## 💻 Platform yang didukung

| Peran | macOS (Apple Silicon) | Windows (x64) |
|-------|:---------------------:|:-------------:|
| **Master** (pengajar/admin) | ✅ | ✅ |
| **Client** (yang dipantau) | ✅ | ✅ |

> Master di macOS bisa mengendalikan client Windows maupun macOS dalam satu jaringan.

---

## 📥 Download

Ambil rilis terbaru di **[halaman Releases](https://github.com/rsamjkt/veyon-macos/releases/tag/v1.0.0)** — versi **1.0.0 "Aurora"**:

| Paket | Untuk |
|-------|-------|
| `AruniControl-1.0.0-Aurora-macOS-arm64.zip` | Master & Configurator di **macOS** (Apple Silicon) |
| `AruniControl-Server-1.0.0-Aurora-Windows-x64.zip` | Client **Windows** lengkap (mandiri) |
| `AruniControl-AddonPlugins-1.0.0-Aurora-Windows-x64.zip` | Hanya 5 DLL add-on (drop-in ke instalasi Windows yang sudah ada) |

---

## 🛠️ Cara Instalasi (Lengkap)

Konsep dasarnya: **1 Master** mengendalikan **banyak Client**. Pasang aplikasi sesuai perannya.

> **Port jaringan** yang dipakai: `11100` (server), `11200` (VNC), `11300` (worker). Pastikan tidak diblokir firewall antar-komputer.

### 🍎 A. Master di macOS

1. **Unduh & ekstrak** `AruniControl-...-macOS-arm64.zip`, lalu pindahkan aplikasinya ke folder **Applications**.
2. **Buat kunci autentikasi** (wajib — tanpa ini Master menolak start). Buka **Terminal**:
   ```bash
   veyon-cli authkeys create master
   ```
   Kunci tersimpan di `~/Library/Application Support/AruniControl/keys/`.
3. **Buka AruniControl Master** dan **Configurator** dari Applications (atau lewat Terminal: `open -a "AruniControl Master"`).
4. Di Configurator, tambahkan komputer/lokasi yang akan dipantau (atau aktifkan **Network Discovery**, lihat bagian add-on).

### 🪟 B. Client di Windows

#### Cara termudah: installer sekali-klik (disarankan)
Unduh **`AruniControl-Setup-...-Windows-x64.exe`**, lalu **dobel-klik**. Installer otomatis: menyalin file, **mendaftarkan + menjalankan service**, menambah **aturan firewall** (port 11100), dan membuat shortcut Start Menu. Setelah itu cukup lakukan langkah **#2 (pasang kunci publik Master)** di bawah.
> Untuk deploy massal tanpa interaksi: `AruniControl-Setup-...exe /S` (mode senyap).

#### Cara manual (paket zip)
Atau pakai paket **`AruniControl-Server-...-Windows-x64.zip`** (mandiri — sudah termasuk semua dependency).

1. **Ekstrak** ke folder tetap, mis. `C:\Program Files\AruniControl\`.
2. **Pasang kunci publik Master** agar Master dipercaya client:
   - Di **Mac (Master)** — ekspor kunci publik:
     ```bash
     veyon-cli authkeys export master/public ~/master-public.key
     ```
   - Salin file `master-public.key` ke PC Windows, lalu **impor** (jalankan `cmd` di folder AruniControl):
     ```bat
     veyon-cli.exe authkeys import master/public master-public.key
     ```
3. **Daftarkan & jalankan service** (buka **cmd sebagai Administrator** di folder AruniControl):
   ```bat
   veyon-wcli.exe service register
   veyon-wcli.exe service start
   ```
   > Untuk tes cepat tanpa service, cukup jalankan `veyon-server.exe` langsung.
4. **Izinkan firewall** — saat pertama jalan, Windows biasanya minta izin; centang **Allow**. Bila perlu manual, izinkan **inbound TCP port 11100** untuk `veyon-server.exe`.
5. Selesai — dari **Master**, masukkan **IP** PC Windows ini. Semua fitur inti + add-on langsung aktif.

#### Opsi ringan: hanya pasang DLL add-on
Jika client Windows **sudah** menjalankan AruniControl edisi 4.10.4 (x64) yang sama, pakai paket **`AruniControl-AddonPlugins-...zip`** (373 KB):
1. Ekstrak → ada folder `plugins\` berisi 5 `.dll`.
2. Salin ke-5 DLL ke folder plugin instalasi, mis. `C:\Program Files\AruniControl\plugins\`.
3. Restart service: `veyon-wcli.exe service stop` lalu `service start` (atau reboot).

> ⚠️ Drop-in hanya cocok bila versi/ABI sama (DLL terhubung ke `veyon-core.dll`). Bila ragu, gunakan **paket Server lengkap**.

### 🍎 C. Client di macOS (komputer yang dipantau)

1. Pasang paket macOS dan jalankan **server**.
2. Beri **dua izin privasi** ke binary server di **System Settings → Privacy & Security**:
   - **Screen Recording** — untuk menangkap layar.
   - **Accessibility** — untuk menerima input keyboard/mouse jarak jauh.
3. **Restart server** setelah izin diberikan (izin baru berlaku setelah restart).

---

## 🔌 Detail Add-on

- **🎥 Screen Recorder** — klik *Record screen* di toolbar Master untuk merekam layar client terpilih; klik lagi untuk stop. Rekaman tersimpan di Master (H.264). Bekerja terhadap client Windows maupun macOS.
- **🔎 Network Discovery** — memindai subnet jaringan mencari komputer yang menjalankan AruniControl Server (port `11100`) dan menampilkannya otomatis. Aktifkan di Configurator pada setelan *network object directory*.
- **📊 Application Monitoring** — klik *Application monitoring* → jendela per-komputer menampilkan daftar aplikasi GUI yang berjalan + aplikasi yang sedang aktif (di-update tiap 3 detik). Berguna mendeteksi aplikasi terlarang saat ujian.
- **🔇 AruniMedia** — tombol *Mute audio* untuk membisukan/mengaktifkan audio client (tanpa hak admin).
- **🚫 Internet Access Control** — tombol *Block internet* untuk memblokir akses internet client (LAN tetap jalan agar Master tetap terhubung). Di Windows memakai Windows Firewall via service `LocalSystem`; di macOS memakai `pf` (butuh hak admin di client).
- **💬 Chat** — obrolan dua arah; pesan muncul di sesi pengguna dan mereka bisa membalas.

---

## 🧰 Build dari Source (opsional, untuk developer)

### macOS (Apple Silicon)
```bash
# 1. Toolchain (sekali saja)
brew install cmake pkg-config ninja qt qca openssl@3 jpeg-turbo lzo libpng libvncserver
# 2. Build
./build-macos.sh
# 3. Paket aplikasi
./package-macos.sh        # mandiri (untuk Mac lain)
./package-macos-local.sh  # thin app (untuk mesin dev ini)
```

### Windows (x64)
Build otomatis lewat **GitHub Actions** (`.github/workflows/windows-build.yml`) memakai toolchain **MSYS2 mingw-w64**, menghasilkan paket Server Windows. Jalankan manual via tab **Actions → Windows build → Run workflow**.

Detail lanjutan, catatan teknis, dan keterbatasan ada di **[`README.macos.md`](README.macos.md)**.

---

## 📜 Lisensi

AruniControl dirilis di bawah **GNU General Public License v2 (GPLv2)** — lihat [`COPYING`](COPYING).

Perangkat lunak ini dibangun di atas komponen sumber-terbuka pihak ketiga; kredit dan pemberitahuan hak cipta selengkapnya ada di [`ATTRIBUTION.md`](ATTRIBUTION.md). Sesuai GPLv2, kode sumber tersedia penuh di repositori ini dan seluruh pemberitahuan hak cipta asli dipertahankan di berkas sumber.

© Arunika. "AruniControl" dan "Arunika" adalah merek milik Arunika.
