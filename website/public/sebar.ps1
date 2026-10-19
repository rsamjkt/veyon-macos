# AruniControl - sebar agent ke banyak komputer dengan satu baris, dari komputer admin.
#
# Di PowerShell (komputer admin):
#   $env:ARUNI_KEY='ARUNISETUP1:...'; $env:ARUNI_KOMPUTER='192.168.10.11-40,PC-GURU-01'; irm https://arunicontrol.arunihealth.id/sebar.ps1 | iex
#
# ARUNI_KEY        kode pemasangan (kunci publik) dari Configurator komputer admin
#                  (Aruni Gateway -> Pemasangan). Kode berisi kunci privat ditolak.
# ARUNI_KOMPUTER   komputer client: nama/IP, rentang 192.168.10.11-40, dipisah koma
# Opsional (isi '1'): ARUNI_UJI (cek saja), ARUNI_HAPUS (hapus agent),
#   ARUNI_PAKSA (pasang ulang), ARUNI_AKUNSAYA (akun yang sedang login),
#   ARUNI_YA (tanpa konfirmasi); ARUNI_CARA = WinRM atau PsExec.
#
# Sebar Agent diunduh dari situs resmi, diperiksa SHA-256-nya, lalu disimpan di
# %USERPROFILE%\AruniControl-Sebar (hasil per komputer ada di folder hasil).

& {
	$ErrorActionPreference = 'Stop'
	$ProgressPreference = 'SilentlyContinue'
	$site = 'https://arunicontrol.arunihealth.id'
	$dir = Join-Path $env:USERPROFILE 'AruniControl-Sebar'

	function Say($text) { Write-Host "[AruniControl] $text" -ForegroundColor Cyan }
	function Ok($text) { Write-Host "[AruniControl] $text" -ForegroundColor Green }
	function Fail($text) { Write-Host "[AruniControl] GAGAL: $text" -ForegroundColor Red; throw $text }

	$remove = $env:ARUNI_HAPUS -eq '1'
	$test = $env:ARUNI_UJI -eq '1'
	if (-not $env:ARUNI_KEY -and -not $remove -and -not $test -and -not (Test-Path (Join-Path $dir 'aruni-setup.txt'))) {
		Fail "Isi kode pemasangan dulu: `$env:ARUNI_KEY='ARUNISETUP1:...' (Configurator > Aruni Gateway > Pemasangan)."
	}
	if (-not $env:ARUNI_KOMPUTER -and -not (Test-Path (Join-Path $dir 'komputer.txt'))) {
		Fail "Isi daftar komputer: `$env:ARUNI_KOMPUTER='192.168.10.11-40,PC-GURU-01'"
	}

	[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
	$work = Join-Path $env:TEMP ('aruni-sebar-unduh-' + [guid]::NewGuid().ToString('N'))
	New-Item -ItemType Directory -Force $work | Out-Null
	try {
		# --- Sebar Agent (ARUNI_SEBAR_ZIP: file lokal, untuk pengujian)
		$zip = Join-Path $work 'sebar-agent.zip'
		if ($env:ARUNI_SEBAR_ZIP) {
			Copy-Item $env:ARUNI_SEBAR_ZIP $zip
			Say "Memakai Sebar Agent lokal $env:ARUNI_SEBAR_ZIP"
		} else {
			Say 'Mengambil informasi versi terbaru...'
			$manifest = Invoke-RestMethod -UseBasicParsing -Uri "$site/unduh/versi.json" -Headers @{ 'User-Agent' = 'AruniControl-Sebar (windows)' }
			$file = $manifest.files.'sebar-agent'
			if (-not $file -or -not $file.url -or -not $file.sha256) { Fail 'Sebar Agent tidak ada di manifest rilis.' }
			Say "Mengunduh Sebar Agent $($manifest.version)..."
			Invoke-WebRequest -UseBasicParsing -Uri $file.url -OutFile $zip -Headers @{ 'User-Agent' = 'AruniControl-Sebar (windows)' }
			$hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
			if ($hash -ne $file.sha256.ToLower()) { Fail 'Checksum Sebar Agent tidak cocok - unduhan rusak atau diubah. Dibatalkan.' }
			Ok 'Sebar Agent terverifikasi (SHA-256 cocok).'
		}

		$extract = Join-Path $work 'isi'
		Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force
		$source = Join-Path $extract 'sebar-agent'
		if (-not (Test-Path (Join-Path $source 'sebar.ps1'))) { $source = $extract }
		if (-not (Test-Path (Join-Path $source 'sebar.ps1'))) { Fail 'Isi Sebar Agent tidak lengkap.' }
		# skrip diperbarui; installer, PsExec, kode, dan hasil sebelumnya tetap
		New-Item -ItemType Directory -Force $dir | Out-Null
		Copy-Item -Path (Join-Path $source '*') -Destination $dir -Recurse -Force

		if ($env:ARUNI_KOMPUTER) {
			$names = @($env:ARUNI_KOMPUTER -split '[,;\s]+' | Where-Object { $_ })
			Set-Content -LiteralPath (Join-Path $dir 'komputer.txt') -Value $names -Encoding Ascii
		}
	} finally {
		Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
	}

	$arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $dir 'sebar.ps1'))
	if ($test) { $arguments += '-Uji' }
	if ($remove) { $arguments += '-Hapus' }
	if ($env:ARUNI_PAKSA -eq '1') { $arguments += '-Paksa' }
	if ($env:ARUNI_AKUNSAYA -eq '1') { $arguments += '-AkunSaya' }
	if ($env:ARUNI_YA -eq '1') { $arguments += '-Ya' }
	if ($env:ARUNI_CARA) { $arguments += @('-Cara', $env:ARUNI_CARA) }

	# sebar.ps1 membaca kode dari ARUNI_KEY; berjalan di Windows PowerShell 5.1
	& (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe') @arguments
	$code = $LASTEXITCODE
	Write-Host ''
	Say "Folder kerja: $dir (hasil per komputer di folder hasil)"
	if ($code -ne 0) { Write-Host '[AruniControl] Ada komputer yang belum berhasil - lihat daftar di atas.' -ForegroundColor Yellow }
	$global:LASTEXITCODE = $code
}
