# Website unduhan AruniControl

Halaman resmi untuk mengunduh AruniControl (bahasa Indonesia), berjalan di
Cloudflare Workers dengan static assets. Installer disimpan di R2.

- `index.src.html` - sumber halaman; `python3 build.py` menyisipkan ikon
  (dari `mobile/icons`) dan menulis `public/index.html`
- `public/` - aset statis (CSS, font, logo, screenshot di `img/`)
- `src/worker.js` - melayani `/unduh/*` dari bucket R2 `arunicontrol-unduhan`

## Deploy

```bash
python3 build.py
npx wrangler deploy
```

## Rilis baru

1. Unggah file ke R2 dengan awalan tag:
   `npx wrangler r2 object put arunicontrol-unduhan/v1.3.0/<file> --file <file> --remote`
   (termasuk `SHA256SUMS.txt` dari `shasum -a 256 *`)
2. Ubah `RELEASE` di `src/worker.js`, serta versi & ukuran file di `index.src.html`.
3. `python3 build.py && npx wrangler deploy`

## Screenshot

- Android: emulator + `adb exec-out screencap -p`
- Configurator (macOS): `ARUNI_SCREENSHOT=out.png ARUNI_SCREENSHOT_PAGE="Aruni Gateway" veyon-configurator`
  merender jendelanya sendiri, jadi tidak perlu izin Screen Recording
- Windows: workflow `.github/workflows/windows-screenshots.yml` (hasilnya di
  draft release "CI screenshots", hapus setelah diunduh)

Isi layar pribadi (thumbnail, ID gateway) disamarkan sebelum dipakai.
