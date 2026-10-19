# Sebar agent AruniControl

Folder ini memasang **agent** AruniControl ke banyak komputer Windows sekaligus
dari **satu komputer admin**. Mirip memasang agent Wazuh: di komputer client
tidak ada yang perlu diklik, dan pengguna tidak melihat aplikasi apa pun (mode
agent tidak membuat shortcut Master/Configurator).

| File | Fungsi |
|---|---|
| `Sebar.cmd` | Klik dua kali untuk menjalankan `sebar.ps1` (tanpa mengubah ExecutionPolicy) |
| `sebar.ps1` | Memasang/menghapus agent ke semua komputer di `komputer.txt` |
| `agent.cmd` | Pemasang untuk satu komputer: dipakai `sebar.ps1`, bisa juga lewat USB/GPO |
| `komputer.contoh.txt` | Contoh daftar komputer |

File yang dibuat sendiri per lokasi (`aruni-setup.txt`, `komputer.txt`,
installer, `PsExec64.exe`, `hasil\`) tidak ikut masuk git.

---

## 1. Kode pemasangan (key)

Kode pemasangan berbentuk satu baris teks yang diawali `ARUNISETUP1:...`.
Isinya:

- **kunci publik autentikasi**: wajib. Dengan kunci ini agent hanya mau
  dikendalikan Master yang memegang pasangan kunci privatnya;
- daftar ruangan dan komputer (opsional);
- pendaftaran laptop jelajah (opsional).

Kode ini dibuat di **komputer admin/guru** yang sudah memasang AruniControl
lengkap dan sudah punya kunci (Configurator → Kunci autentikasi). Ada tiga
cara, pilih salah satu:

**a. Lewat Configurator (paling mudah)**
1. Buka **Configurator → Aruni Gateway → tab Pemasangan**.
2. Centang kunci yang mau dipakai. Centang juga *daftar ruangan dan komputer*
   kalau perlu.
3. **Jangan** centang *Sertakan kunci privat*. Kunci privat hanya untuk
   komputer guru/Master. Kalau kode berisi kunci privat, `sebar.ps1`
   menolaknya.
4. Klik **Buat kode** → **Simpan sebagai aruni-setup.txt...**, lalu simpan ke
   folder ini.

**b. Otomatis**: kalau `sebar.ps1` dijalankan di komputer admin yang sudah
memasang AruniControl dan `aruni-setup.txt` belum ada, kodenya dibuat sendiri
(kunci publik + daftar komputer), lalu disimpan sebagai `aruni-setup.txt`.

**c. Lewat perintah**: di komputer admin,
`"C:\Program Files\AruniControl\veyon-wcli.exe" gateway setupcode computers`.
Salin baris `ARUNISETUP1:...` ke `aruni-setup.txt`.

> Kode tanpa kunci privat tidak bisa dipakai untuk mengendalikan komputer,
> tapi tetap simpan baik-baik karena isinya juga daftar komputer Anda. Buat
> kode baru setiap kali kunci diganti atau ada komputer baru di daftar.

---

## 2. Cara A: massal dari komputer admin (`Sebar.cmd`)

### Persiapan sekali saja

1. Salin folder `sebar-agent` ke komputer admin (Windows 10/11).
2. Simpan kode pemasangan sebagai `aruni-setup.txt` di folder ini
   (lihat bagian 1).
3. Salin `komputer.contoh.txt` menjadi `komputer.txt`, lalu isi daftar
   komputernya:
   ```
   192.168.10.11-40     # rentang IP: .11 sampai .40
   PC-GURU-01           # nama komputer
   ```
4. Installer tidak perlu diunduh manual. `sebar.ps1` mengambil versi terbaru
   dari arunicontrol.arunihealth.id dan mencocokkan SHA-256-nya. Kalau komputer
   admin tidak punya internet, taruh `AruniControl-Setup-x.y.z-...-Windows-x64.exe`
   di folder ini.

### Syarat di komputer client

Cukup salah satu dari dua jalur berikut:

| Jalur | Syarat di client | Keterangan |
|---|---|---|
| **WinRM** (dipilih lebih dulu) | WinRM aktif (`winrm quickconfig`, atau GPO *Allow remote server management through WinRM*) | Biasanya sudah aktif di jaringan domain |
| **PsExec** | *File and Printer Sharing* aktif (port 445), share `ADMIN$` | PsExec64.exe resmi Microsoft diunduh otomatis (setelah ditanya) dan dicek tanda tangannya |

Selain itu dibutuhkan **akun administrator** di komputer client:

- **Komputer domain**: pakai akun domain admin (`DOMAIN\admin`), atau jalankan
  dengan `-AkunSaya` kalau Anda login sebagai domain admin.
- **Komputer tanpa domain (workgroup)**: pakai akun **Administrator bawaan**
  yang sandinya sama di semua PC. Akun admin lokal lain diblokir Windows untuk
  akses jarak jauh (UAC remote restriction). Kalau terpaksa memakai akun admin
  lokal lain, jalankan sekali di tiap client:
  `reg add HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System /v LocalAccountTokenFilterPolicy /t REG_DWORD /d 1 /f`
- **WinRM ke alamat IP / komputer workgroup**: di **komputer admin**
  (PowerShell sebagai Administrator) jalankan sekali:
  `Set-Item WSMan:\localhost\Client\TrustedHosts -Value '*' -Force`
  (atau isi daftar IP-nya saja, bukan `*`).

### Menjalankan

1. **Uji dulu.** Tidak ada yang dipasang; yang dicek hanya apakah komputer
   bisa dihubungi dan akunnya punya hak admin:
   ```
   Sebar.cmd -Uji
   ```
2. **Pasang:** klik dua kali `Sebar.cmd`, atau jalankan `Sebar.cmd` di Command
   Prompt. Isi akun admin client saat ditanya, lalu jawab `Y`.

Untuk setiap komputer, skrip menyalin installer, kode, dan `agent.cmd` ke
`C:\Windows\Temp` di komputer itu, menjalankannya tanpa jendela, menghapus file
sementaranya, lalu mengecek dari komputer admin bahwa port 11100 sudah terbuka.
Delapan komputer dikerjakan bersamaan. Contoh tampilannya:

```
[1/30] 192.168.10.11     BERHASIL         Agent terpasang dan port 11100 terbuka.
[2/30] 192.168.10.12     DILEWATI         Agent sudah berjalan (port 11100 terbuka). ...
[3/30] 192.168.10.13     TIDAK TERJANGKAU Port WinRM (5985) dan SMB (445) tertutup: ...
```

Hasil lengkapnya ada di `hasil\hasil-*.csv`. Komputer yang gagal dicatat di
`hasil\gagal-*.txt`; setelah penyebabnya diperbaiki, ulangi hanya untuk yang
gagal:

```
Sebar.cmd -Daftar hasil\gagal-20261019-101500.txt
```

### Pilihan lain

| Perintah | Fungsi |
|---|---|
| `Sebar.cmd -Uji` | Cek saja, tidak memasang |
| `Sebar.cmd -Paksa` | Pasang ulang walaupun agent sudah berjalan (misalnya setelah kode/kunci diganti, atau untuk update) |
| `Sebar.cmd -Hapus` | Hapus agent dari komputer di daftar |
| `Sebar.cmd -Siapkan` | Hanya siapkan folder (installer + kode + versi.txt) untuk USB/GPO |
| `Sebar.cmd -Daftar lab2.txt` | Pakai daftar komputer lain |
| `Sebar.cmd -Cara WinRM` / `-Cara PsExec` | Paksa satu jalur (bawaan: Otomatis) |
| `Sebar.cmd -AkunSaya` | Pakai akun yang sedang login, tanpa tanya user/sandi |
| `Sebar.cmd -Paralel 16` | Jumlah komputer yang dikerjakan bersamaan (1-32, bawaan 8) |
| `Sebar.cmd -Ya` | Tanpa konfirmasi (untuk dijadwalkan) |

Komputer yang agent-nya sudah berjalan dilewati, jadi `Sebar.cmd` aman diulang
kapan saja. Misalnya sekali lagi besok untuk PC yang hari ini mati.

---

## 3. Cara B: per komputer tanpa jaringan admin (USB / share)

Untuk PC yang tidak bisa dijangkau WinRM/PsExec:

1. Isi folder (flashdisk atau share): `agent.cmd`, `AruniControl-Setup.exe`,
   `aruni-setup.txt` (+ `versi.txt`). Semuanya disiapkan oleh `Sebar.cmd -Siapkan`
   di komputer admin: installer terbaru diunduh dan dicek SHA-256-nya, tanpa
   menghubungi komputer client.
2. Di PC client: klik kanan `agent.cmd` → **Run as administrator**.

Hapus: `agent.cmd hapus` dari Command Prompt (Administrator).

---

## 4. Cara C: GPO di jaringan domain

1. Taruh `agent.cmd`, `AruniControl-Setup.exe`, `aruni-setup.txt`, dan
   `versi.txt` (disiapkan oleh `Sebar.cmd -Siapkan`) di share yang bisa dibaca
   **Domain Computers**, misalnya `\\server\aruni$\`.
2. GPO → Computer Configuration → Policies → Windows Settings → Scripts →
   **Startup** → tambahkan `\\server\aruni$\agent.cmd`, parameter `pasang`.

Setiap PC memasang agent saat dinyalakan. Kalau versi yang sama sudah
terpasang, pemasangan dilewati sehingga boot tidak melambat. Untuk update,
hapus `AruniControl-Setup.exe` di share lalu jalankan `Sebar.cmd -Siapkan` di
folder share. Versi terbaru diunduh dan `versi.txt` ikut diperbarui.

> Share GPO dibaca semua komputer domain. Jangan taruh kode yang berisi kunci
> privat di sana.

---

## 5. Kalau gagal

| Pesan | Penyebab / solusi |
|---|---|
| `TIDAK TERJANGKAU` | PC mati, nama/IP salah, atau firewall memblokir 5985 dan 445 |
| `TrustedHosts` | Jalankan `Set-Item WSMan:\localhost\Client\TrustedHosts -Value '*' -Force` di komputer admin |
| `Access is denied` / `bukan administrator` | Pakai Administrator bawaan atau domain admin (lihat UAC di atas) |
| `user name or password` / `1326` | User/sandi salah |
| `1219` | Ada koneksi lain ke PC itu dengan akun berbeda: `net use \\PC /delete` |
| `PERIKSA ... port 11100` | Agent terpasang tapi tidak bisa dihubungi dari komputer admin: cek firewall/jaringan antar VLAN |
| `BUKAN rilis terbaru` | Installer di folder ini versi lama/lain. Hapus filenya supaya versi terbaru diunduh |

Pengujian otomatis: `.github/workflows/sebar-agent-test.yml` menjalankan
`ci/sebar-agent-test.ps1` di Windows sungguhan setiap kali folder ini berubah.
Yang diuji: pasang/lewati/pasang ulang/hapus lewat WinRM dan PsExec,
`agent.cmd`, penolakan kode berkunci privat, dan daftar komputer yang tidak
valid.
