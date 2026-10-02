# AruniControl - pasang agent (client) dengan satu baris, seperti agent Wazuh.
#
# Di PowerShell sebagai Administrator:
#   $env:ARUNI_KEY='ARUNISETUP1:...'; irm https://arunicontrol.arunihealth.id/pasang.ps1 | iex
#
# ARUNI_KEY  kode pemasangan dari Configurator komputer admin
#            (Aruni Gateway -> Pemasangan). Berisi kunci publik, opsional daftar
#            komputer dan laptop jelajah. Tidak dikirim ke mana pun - hanya
#            dipakai di komputer ini.
# ARUNI_UNINSTALL=1   hapus AruniControl dari komputer ini
#
# Installer diunduh dari situs resmi dan diperiksa SHA-256-nya terhadap
# manifest rilis sebelum dijalankan.

& {
	$ErrorActionPreference = 'Stop'
	$ProgressPreference = 'SilentlyContinue'
	$site = 'https://arunicontrol.arunihealth.id'
	$installDir = Join-Path $env:ProgramFiles 'AruniControl'
	$cli = Join-Path $installDir 'veyon-wcli.exe'

	function Say($text) { Write-Host "[AruniControl] $text" -ForegroundColor Cyan }
	function Ok($text) { Write-Host "[AruniControl] $text" -ForegroundColor Green }
	function Fail($text) { Write-Host "[AruniControl] GAGAL: $text" -ForegroundColor Red; throw $text }

	$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
	if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
		Fail 'Jalankan PowerShell sebagai Administrator (klik kanan > Run as administrator).'
	}

	# --- uninstall
	if ($env:ARUNI_UNINSTALL -eq '1') {
		$uninstaller = Join-Path $installDir 'uninstall.exe'
		if (-not (Test-Path $uninstaller)) { Fail 'AruniControl tidak terpasang di komputer ini.' }
		Say 'Menghapus AruniControl...'
		Start-Process -FilePath $uninstaller -ArgumentList '/S', "_?=$installDir" -Wait
		Remove-Item -Recurse -Force $installDir -ErrorAction SilentlyContinue
		Ok 'AruniControl sudah dihapus.'
		return
	}

	# --- installation code
	$key = "$env:ARUNI_KEY".Trim()
	if ($key -and -not ($key -match '^ARUNISETUP1:[A-Za-z0-9_-]+$')) {
		Fail 'ARUNI_KEY bukan kode pemasangan yang valid (harus diawali ARUNISETUP1:). Salin lagi dari Configurator.'
	}
	if (-not $key) {
		Say 'Tanpa ARUNI_KEY: agent dipasang, kunci autentikasi perlu diimpor terpisah.'
	}

	[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
	$work = Join-Path $env:TEMP ('aruni-agent-' + [guid]::NewGuid().ToString('N'))
	New-Item -ItemType Directory -Force $work | Out-Null

	try {
		# --- installer (ARUNI_INSTALLER: local file, for testing)
		$setup = Join-Path $work 'AruniControl-Setup.exe'
		if ($env:ARUNI_INSTALLER) {
			Copy-Item $env:ARUNI_INSTALLER $setup
			Say "Memakai installer lokal $env:ARUNI_INSTALLER"
		} else {
			Say 'Mengambil informasi versi terbaru...'
			$manifest = Invoke-RestMethod -UseBasicParsing -Uri "$site/unduh/versi.json" -Headers @{ 'User-Agent' = 'AruniControl-Agent (windows)' }
			$file = $manifest.files.windows
			if (-not $file -or -not $file.url -or -not $file.sha256) { Fail 'Manifest rilis tidak lengkap.' }
			Say "Mengunduh AruniControl $($manifest.version) ($([math]::Round($file.size / 1MB, 1)) MB)..."
			Invoke-WebRequest -UseBasicParsing -Uri $file.url -OutFile $setup -Headers @{ 'User-Agent' = 'AruniControl-Agent (windows)' }
			$hash = (Get-FileHash -Algorithm SHA256 $setup).Hash.ToLower()
			if ($hash -ne $file.sha256.ToLower()) { Fail 'Checksum installer tidak cocok - unduhan rusak atau diubah. Dibatalkan.' }
			Ok 'Installer terverifikasi (SHA-256 cocok).'
		}

		# the installer reads the installation code from this file (no command line length limit)
		if ($key) {
			Set-Content -Path (Join-Path $work 'aruni-setup.txt') -Value $key -Encoding ascii -NoNewline
		}

		Say 'Memasang agent (tanpa jendela)...'
		$process = Start-Process -FilePath $setup -ArgumentList '/S', '/AGENT' -Wait -PassThru
		if ($process.ExitCode -ne 0) { Fail "Installer berhenti dengan kode $($process.ExitCode)." }
		if (-not (Test-Path $cli)) { Fail 'Instalasi tidak ditemukan setelah pemasangan.' }

		# --- check
		$listening = $false
		for ($i = 0; $i -lt 30 -and -not $listening; $i++) {
			Start-Sleep -Seconds 2
			$listening = [bool](Get-NetTCPConnection -LocalPort 11100 -State Listen -ErrorAction SilentlyContinue)
		}
		$keys = & $cli authkeys list 2>$null | Where-Object { $_ -match '/public$' }

		Write-Host ''
		Ok "AruniControl agent terpasang di $env:COMPUTERNAME"
		if ($listening) { Ok 'Layanan berjalan (port 11100).' } else { Write-Host '[AruniControl] Layanan belum mendengarkan di port 11100 - cek lagi beberapa saat lagi.' -ForegroundColor Yellow }
		if ($keys) { Ok ('Kunci terpasang: ' + (($keys | ForEach-Object { $_.Split('/')[0] }) -join ', ')) }
		elseif ($key) { Write-Host '[AruniControl] Kunci dari kode pemasangan tidak ditemukan - cek kodenya.' -ForegroundColor Yellow }
		$addresses = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' }).IPAddress -join ', '
		if ($addresses) { Ok "Alamat komputer ini: $addresses" }
	} finally {
		Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
	}
}
