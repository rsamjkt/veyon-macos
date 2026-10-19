<#
.SYNOPSIS
	AruniControl - pasang (atau hapus) agent ke banyak komputer Windows sekaligus
	dari satu komputer admin, tanpa membuka komputer client satu per satu.

.DESCRIPTION
	Untuk setiap komputer di komputer.txt: installer + kode pemasangan disalin
	ke C:\Windows\Temp di komputer itu, agent.cmd dijalankan di sana (lewat
	WinRM, atau lewat PsExec kalau WinRM tidak aktif), file sementaranya
	dihapus lagi, lalu dicek apakah port 11100 sudah terbuka.

	Hasil tiap komputer ditulis ke folder hasil\ (CSV + daftar yang gagal untuk
	diulang). Lihat README.md untuk persiapan dan cara membuat kode pemasangan.

.EXAMPLE
	.\sebar.ps1                         # pasang ke semua komputer di komputer.txt
	.\sebar.ps1 -Uji                    # hanya cek koneksi + akun, tidak memasang
	.\sebar.ps1 -Daftar lab2.txt        # daftar komputer lain
	.\sebar.ps1 -Daftar hasil\gagal-20261019-101500.txt   # ulangi yang gagal
	.\sebar.ps1 -Paksa                  # pasang ulang walaupun agent sudah jalan
	.\sebar.ps1 -Hapus                  # hapus agent dari komputer-komputer itu
	.\sebar.ps1 -Siapkan                # siapkan folder untuk USB / GPO saja
#>
[CmdletBinding()]
param(
	# file daftar komputer: satu nama/IP per baris, boleh rentang 192.168.1.10-40
	[string]$Daftar = 'komputer.txt',
	# Otomatis = WinRM kalau port 5985 terbuka, selain itu PsExec (SMB 445)
	[ValidateSet('Otomatis', 'WinRM', 'PsExec')][string]$Cara = 'Otomatis',
	# berapa komputer dikerjakan bersamaan
	[ValidateRange(1, 32)][int]$Paralel = 8,
	# hanya cek: komputer bisa dihubungi dan akun punya hak admin; tidak memasang apa pun
	[switch]$Uji,
	# hapus agent, bukan memasang
	[switch]$Hapus,
	# hanya siapkan folder ini (installer terverifikasi + versi.txt + aruni-setup.txt)
	# untuk dibawa lewat USB / share GPO; tidak menghubungi komputer client
	[switch]$Siapkan,
	# pasang ulang walaupun agent sudah berjalan (port 11100 terbuka)
	[switch]$Paksa,
	# pakai akun Windows yang sedang login (akun domain admin), tanpa tanya user/sandi
	[switch]$AkunSaya,
	# akun admin di komputer client (kalau tidak diisi, ditanyakan)
	[System.Management.Automation.PSCredential]$Kredensial,
	# izinkan kode yang berisi kunci privat (JANGAN untuk komputer siswa/client biasa)
	[switch]$IzinkanKunciPrivat,
	# jangan tanya konfirmasi
	[switch]$Ya
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
trap {
	Write-Host "[AruniControl] GAGAL tak terduga: $($_.Exception.Message)" -ForegroundColor Red
	Write-Host ($_.InvocationInfo.PositionMessage) -ForegroundColor DarkGray
	Remove-Stage
	exit 1
}
$site = 'https://arunicontrol.arunihealth.id'
$folder = $PSScriptRoot
$setup = Join-Path $folder 'AruniControl-Setup.exe'
$versionFile = Join-Path $folder 'versi.txt'
$versionHashFile = Join-Path $folder 'versi.sha256'
$keyFile = Join-Path $folder 'aruni-setup.txt'
$agentCmd = Join-Path $folder 'agent.cmd'

function Say($text) { Write-Host "[AruniControl] $text" -ForegroundColor Cyan }
function Ok($text) { Write-Host "[AruniControl] $text" -ForegroundColor Green }
function Warn($text) { Write-Host "[AruniControl] $text" -ForegroundColor Yellow }
# folder sementara di komputer admin (berisi salinan kode pemasangan)
$stage = $null
function Remove-Stage { if ($script:stage) { Remove-Item -LiteralPath $script:stage -Recurse -Force -ErrorAction SilentlyContinue } }
function Fail($text) { Write-Host "[AruniControl] GAGAL: $text" -ForegroundColor Red; Remove-Stage; exit 1 }

function Confirm-Step([string]$question) {
	if ($Ya) { return $true }
	$answer = Read-Host "[AruniControl] $question [Y/n]"
	return ($answer -eq '' -or $answer -match '^(y|ya|yes)$')
}

# ------------------------------------------------------------ daftar komputer
function Read-Targets([string]$path) {
	if (-not (Test-Path -LiteralPath $path)) {
		Fail "File daftar komputer '$path' tidak ada. Salin komputer.contoh.txt menjadi komputer.txt lalu isi nama/IP komputer client."
	}
	$list = New-Object System.Collections.Generic.List[string]
	$seen = @{}
	$lineNo = 0
	foreach ($raw in @(Get-Content -LiteralPath $path)) {
		$lineNo++
		$line = ($raw -replace '#.*$', '').Trim()
		if (-not $line) { continue }
		foreach ($word in ($line -split '[\s,;]+')) {
			if (-not $word) { continue }
			$names = @()
			if ($word -match '^(\d{1,3}\.\d{1,3}\.\d{1,3}\.)(\d{1,3})-(\d{1,3})$') {
				$prefix = $Matches[1]
				$from = [int]$Matches[2]
				$to = [int]$Matches[3]
				if ($from -lt 1 -or $to -gt 254 -or $from -gt $to) { Fail "Baris ${lineNo}: rentang '$word' tidak valid (contoh benar: 192.168.1.10-40)." }
				$names = @($from..$to | ForEach-Object { "$prefix$_" })
			} elseif ($word -match '^[A-Za-z0-9]([A-Za-z0-9.\-]*[A-Za-z0-9])?$') {
				$names = @($word)
			} else {
				Fail "Baris ${lineNo}: '$word' bukan nama komputer atau alamat IP yang valid."
			}
			foreach ($name in $names) {
				if (-not $seen.ContainsKey($name)) { $seen[$name] = $true; $list.Add($name) }
			}
		}
	}
	return , $list.ToArray()
}

# ---------------------------------------------------------- kode pemasangan
function Read-SetupCode([string]$code) {
	# "ARUNISETUP1:" + base64url( qCompress( JSON ) ); qCompress = 4 byte
	# panjang + zlib (2 byte header + deflate)
	try {
		$b64 = $code.Substring(12).Replace('-', '+').Replace('_', '/')
		switch ($b64.Length % 4) { 1 { return $null } 2 { $b64 += '==' } 3 { $b64 += '=' } }
		$bytes = [Convert]::FromBase64String($b64)
		if ($bytes.Length -lt 7) { return $null }
		$stream = New-Object System.IO.MemoryStream -ArgumentList (, $bytes)
		$stream.Position = 6
		$deflate = New-Object System.IO.Compression.DeflateStream -ArgumentList $stream, ([System.IO.Compression.CompressionMode]::Decompress)
		$reader = New-Object System.IO.StreamReader -ArgumentList $deflate, ([System.Text.Encoding]::UTF8)
		try { $json = $reader.ReadToEnd() } finally { $reader.Dispose() }
		$content = $json | ConvertFrom-Json
		if ($content.v -ne 1) { return $null }
		return $content
	} catch {
		return $null
	}
}

function Get-SetupKey {
	$code = $null
	$source = $null
	$save = $false
	if ($env:ARUNI_KEY) {
		$code = $env:ARUNI_KEY
		$source = 'variabel ARUNI_KEY'
	} elseif (Test-Path -LiteralPath $keyFile) {
		$code = Get-Content -LiteralPath $keyFile -Raw
		$source = 'aruni-setup.txt'
	} else {
		# komputer admin dengan AruniControl: buat kodenya langsung (kunci publik + daftar komputer)
		$wcli = Join-Path $env:ProgramFiles 'AruniControl\veyon-wcli.exe'
		if (Test-Path -LiteralPath $wcli) {
			Say 'aruni-setup.txt belum ada - membuat kode pemasangan dari AruniControl di komputer ini (kunci publik + daftar komputer)...'
			$ErrorActionPreference = 'Continue'
			$output = & $wcli gateway setupcode computers 2>&1 | Out-String
			$ErrorActionPreference = 'Stop'
			$line = @($output -split "`r?`n" | Where-Object { $_ -match '^\s*ARUNISETUP1:' }) | Select-Object -Last 1
			if ($line) {
				$code = $line
				$source = 'kode yang dibuat dari komputer ini'
				$save = $true
			} else {
				Warn ('Kode tidak bisa dibuat: ' + ($output -replace '\s+', ' ').Trim())
			}
		}
	}
	if (-not $code) { return $null }

	$code = ($code -replace '["\s]', '')
	if ($code -notmatch '^ARUNISETUP1:[A-Za-z0-9_-]+$') {
		Fail "Isi $source bukan kode pemasangan yang valid (harus diawali ARUNISETUP1:). Salin lagi dari Configurator."
	}
	$content = Read-SetupCode $code
	if (-not $content) { Fail "Kode pemasangan dari $source rusak atau terpotong. Salin lagi dari Configurator." }

	$keys = @($content.keys | Where-Object { $_ })
	$private = @($keys | Where-Object { $_.PSObject.Properties['private'] -and $_.private })
	$computers = @($content.PSObject.Properties['computers'] | ForEach-Object { $_.Value } | Where-Object { $_ })
	$roaming = [bool]($content.PSObject.Properties['enroll'] -and $content.enroll)

	if ($keys.Count -eq 0) {
		Fail 'Kode pemasangan tidak berisi kunci autentikasi - Master tidak akan bisa terhubung. Buat ulang kode dengan mencentang kunci.'
	}
	if ($private.Count -gt 0 -and -not $IzinkanKunciPrivat) {
		Fail ('Kode pemasangan berisi KUNCI PRIVAT (' + (($private | ForEach-Object { $_.name }) -join ', ') + '). ' +
			'Siapa pun yang memegangnya bisa mengendalikan semua komputer, dan komputer client tidak membutuhkannya. ' +
			'Buat ulang kode TANPA centang "Sertakan kunci privat". (Khusus komputer guru/Master: tambahkan -IzinkanKunciPrivat.)')
	}
	if ($save) {
		Set-Content -LiteralPath $keyFile -Value $code -Encoding Ascii -NoNewline
		Ok 'Kode disimpan ke aruni-setup.txt (dipakai lagi lain kali).'
	}
	Ok ("Kode pemasangan ($source): kunci " + (($keys | ForEach-Object { $_.name }) -join ', ') +
		$(if ($computers.Count) { ", daftar ruangan/komputer ($($computers.Count) entri)" } else { '' }) +
		$(if ($roaming) { ', laptop jelajah' } else { '' }) +
		$(if ($private.Count) { ', BERISI KUNCI PRIVAT' } else { '' }))
	return $code
}

# ---------------------------------------------------------------- installer
function Get-Manifest {
	[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
	$script:manifestError = 'manifest tidak lengkap'
	for ($attempt = 1; $attempt -le 3; $attempt++) {
		try {
			$manifest = Invoke-RestMethod -UseBasicParsing -TimeoutSec 30 -Uri "$site/unduh/versi.json" -Headers @{ 'User-Agent' = 'AruniControl-Sebar (windows)' }
			$file = $manifest.files.windows
			if ($manifest.version -and $file -and $file.url -and $file.sha256) { return $manifest }
		} catch {
			$script:manifestError = $_.Exception.Message
		}
		if ($attempt -lt 3) { Start-Sleep -Seconds 3 }
	}
	return $null
}

# versi.txt berlaku untuk installer dengan SHA-256 di versi.sha256
function Save-Version([string]$version, [string]$hash) {
	Set-Content -LiteralPath $versionFile -Value $version -Encoding Ascii -NoNewline
	Set-Content -LiteralPath $versionHashFile -Value $hash -Encoding Ascii -NoNewline
}

function Prepare-Installer {
	if (-not (Test-Path -LiteralPath $setup)) {
		# installer yang diunduh lewat browser memakai nama lengkap rilisnya
		$other = @(Get-ChildItem -LiteralPath $folder -Filter 'AruniControl-Setup-*.exe' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime) | Select-Object -Last 1
		if ($other) {
			Say "Memakai $($other.Name) sebagai AruniControl-Setup.exe"
			Copy-Item -LiteralPath $other.FullName -Destination $setup
		}
	}
	$manifest = Get-Manifest

	if (-not (Test-Path -LiteralPath $setup)) {
		if (-not $manifest) { Fail "AruniControl-Setup.exe belum ada dan $site tidak bisa dihubungi. Unduh installer Windows dari situs itu lalu taruh di folder ini." }
		$file = $manifest.files.windows
		Say "Mengunduh AruniControl $($manifest.version) ($([math]::Round($file.size / 1MB, 1)) MB)..."
		$part = "$setup.part"
		Invoke-WebRequest -UseBasicParsing -Uri $file.url -OutFile $part -Headers @{ 'User-Agent' = 'AruniControl-Sebar (windows)' }
		$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $part).Hash.ToLower()
		if ($hash -ne $file.sha256.ToLower()) {
			Remove-Item -LiteralPath $part -Force
			Fail 'Checksum installer tidak cocok - unduhan rusak atau diubah. Dibatalkan.'
		}
		Move-Item -LiteralPath $part -Destination $setup -Force
		Save-Version $manifest.version $hash
		Ok "Installer $($manifest.version) terverifikasi (SHA-256 cocok)."
	} else {
		$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $setup).Hash.ToLower()
		$knownHash = $null
		if ((Test-Path -LiteralPath $versionFile) -and (Test-Path -LiteralPath $versionHashFile)) {
			$knownHash = (Get-Content -LiteralPath $versionHashFile -Raw).Trim().ToLower()
		}
		if ($manifest -and $hash -eq $manifest.files.windows.sha256.ToLower()) {
			Save-Version $manifest.version $hash
			Ok "Installer $($manifest.version) terverifikasi (SHA-256 cocok dengan rilis resmi)."
		} elseif (-not $manifest -and $knownHash -eq $hash) {
			Warn "Situs tidak bisa dihubungi ($script:manifestError). Installer sama dengan yang sudah diverifikasi sebelumnya (versi $((Get-Content -LiteralPath $versionFile -Raw).Trim()))."
		} else {
			# versi tidak diketahui: agent.cmd jangan sampai melewati pemasangan
			Remove-Item -LiteralPath $versionFile, $versionHashFile -Force -ErrorAction SilentlyContinue
			if ($manifest) {
				Warn "AruniControl-Setup.exe BUKAN rilis terbaru $($manifest.version) (SHA-256 $hash). Tetap dipakai; hapus file itu kalau mau mengunduh versi terbaru."
			} else {
				Warn "Situs tidak bisa dihubungi ($script:manifestError), installer tidak bisa dicek. Dipakai apa adanya (SHA-256 $hash)."
			}
		}
	}
	# tanpa tanda "diunduh dari internet" - jangan sampai muncul peringatan keamanan di komputer client
	Unblock-File -LiteralPath $setup -ErrorAction SilentlyContinue
}

# ------------------------------------------------------------------- PsExec
function Find-PsExec {
	foreach ($name in 'PsExec64.exe', 'PsExec.exe') {
		$path = Join-Path $folder $name
		if (Test-Path -LiteralPath $path) { return $path }
	}
	$command = @(Get-Command 'PsExec64.exe', 'PsExec.exe' -CommandType Application -ErrorAction SilentlyContinue) | Select-Object -First 1
	if ($command) { return $command.Path }
	return $null
}

function Get-PsExec([bool]$required) {
	$path = Find-PsExec
	if ($path) { return $path }
	if ($required) { Warn 'PsExec (Microsoft Sysinternals) belum ada di folder ini.' }
	else { Warn 'PsExec belum ada: komputer tanpa WinRM tidak bisa dipasang.' }
	if (-not (Confirm-Step 'Unduh PsExec64.exe resmi dari live.sysinternals.com sekarang?')) {
		if ($required) { Fail 'Butuh PsExec64.exe. Unduh dari https://learn.microsoft.com/sysinternals/downloads/psexec lalu taruh di folder ini.' }
		return $null
	}
	$path = Join-Path $folder 'PsExec64.exe'
	try {
		[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
		Invoke-WebRequest -UseBasicParsing -Uri 'https://live.sysinternals.com/PsExec64.exe' -OutFile $path
	} catch {
		Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
		if ($required) { Fail "PsExec tidak bisa diunduh: $($_.Exception.Message)" }
		Warn "PsExec tidak bisa diunduh: $($_.Exception.Message)"
		return $null
	}
	$signature = Get-AuthenticodeSignature -LiteralPath $path
	if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
		Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
		Fail 'PsExec64.exe yang diunduh tidak bertanda tangan Microsoft - dihapus.'
	}
	Ok 'PsExec64.exe terunduh (tanda tangan Microsoft valid).'
	return $path
}

# --------------------------------------------- pekerjaan untuk satu komputer
# (berjalan paralel di runspace terpisah - semua yang dibutuhkan ada di $o)
$worker = {
	param([string]$pc, [hashtable]$o)
	$ErrorActionPreference = 'Continue'
	$ProgressPreference = 'SilentlyContinue'
	$r = @{ Komputer = $pc; Status = 'GAGAL'; Cara = '-'; Pesan = '' }

	function Test-Port([string]$hostName, [int]$port, [int]$ms = 1500) {
		$client = New-Object System.Net.Sockets.TcpClient
		try {
			$async = $client.BeginConnect($hostName, $port, $null, $null)
			if (-not $async.AsyncWaitHandle.WaitOne($ms)) { return $false }
			$client.EndConnect($async)
			return $true
		} catch {
			return $false
		} finally {
			$client.Close()
		}
	}

	function Wait-Port([int]$port, [bool]$open, [int]$seconds) {
		$deadline = (Get-Date).AddSeconds($seconds)
		while ($true) {
			if ((Test-Port $pc $port) -eq $open) { return $true }
			if ((Get-Date) -gt $deadline) { return $false }
			Start-Sleep -Seconds 3
		}
	}

	function Get-Text($err) {
		$text = if ($err -is [System.Management.Automation.ErrorRecord]) { $err.Exception.Message } else { "$err" }
		$text = ($text -replace '\s+', ' ').Trim()
		if ($text.Length -gt 300) { $text = $text.Substring(0, 300) + '...' }
		return $text
	}

	function Add-Hint([string]$text) {
		if ($text -match 'TrustedHosts') {
			return "$text => WinRM ke IP/komputer non-domain: di komputer admin (PowerShell Admin) jalankan sekali: Set-Item WSMan:\localhost\Client\TrustedHosts -Value '*' -Force"
		}
		if ($text -match '1326|user name or password|nama pengguna atau kata sandi|logon failure') {
			return "$text => User atau sandi salah."
		}
		if ($text -match '1219|[Mm]ultiple connections') {
			return "$text => Sudah ada koneksi lain ke komputer ini dengan akun berbeda: jalankan 'net use \\$pc /delete' lalu ulangi."
		}
		if ($text -match 'Access is denied|Akses ditolak|0x80070005|bukan administrator') {
			return "$text => Akun bukan admin jarak jauh: pakai akun Administrator bawaan atau akun domain admin (lihat README, bagian UAC)."
		}
		if ($text -match 'network path was not found|jalur jaringan tidak ditemukan|error 53\b') {
			return "$text => Berbagi file (SMB) tidak bisa diakses: aktifkan File and Printer Sharing di komputer itu."
		}
		return $text
	}

	# antivirus / proses yang baru selesai bisa menahan file sebentar
	function Remove-Folder([string]$path) {
		for ($i = 0; $i -lt 20; $i++) {
			Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue
			if (-not (Test-Path -LiteralPath $path)) { return }
			Start-Sleep -Seconds 3
		}
	}

	function Get-Summary([string]$output, [int]$code) {
		$lines = @($output -split "`r?`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ })
		$own = @($lines | Where-Object { $_ -match '^\[AruniControl\]' }) | Select-Object -Last 1
		if ($own) { return ($own -replace '^\[AruniControl\]\s*((BERHASIL|DILEWATI|GAGAL):\s*)?', '') }
		$last = $lines | Select-Object -Last 1
		if ($last) { return "$last (kode $code)" }
		return "kode keluar $code"
	}

	function Invoke-WinRM([string[]]$files, [string]$mode) {
		$params = @{ ComputerName = $pc; ErrorAction = 'Stop' }
		if ($o.Cred) { $params.Credential = $o.Cred }
		$session = New-PSSession @params
		try {
			$isAdmin = Invoke-Command -Session $session -ErrorAction Stop -ScriptBlock {
				([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
			}
			if (-not $isAdmin) { throw 'Akun tersambung lewat WinRM tetapi bukan administrator di komputer itu.' }
			if ($mode -eq 'uji') { return [pscustomobject]@{ Code = 0; Out = '' } }

			$dir = Invoke-Command -Session $session -ErrorAction Stop -ScriptBlock {
				$d = Join-Path $env:SystemRoot ('Temp\aruni-sebar-' + [guid]::NewGuid().ToString('N'))
				New-Item -ItemType Directory -Path $d -Force | Out-Null
				$d
			}
			try {
				foreach ($file in $files) { Copy-Item -LiteralPath $file -Destination $dir -ToSession $session -ErrorAction Stop }
				$result = Invoke-Command -Session $session -ErrorAction Stop -ArgumentList $dir, $mode -ScriptBlock {
					param($d, $m)
					$text = & "$env:SystemRoot\System32\cmd.exe" /d /c "$d\agent.cmd" $m 2>&1 | Out-String
					[pscustomobject]@{ Code = $LASTEXITCODE; Out = $text }
				}
			} finally {
				Invoke-Command -Session $session -ErrorAction SilentlyContinue -ArgumentList $dir -ScriptBlock {
					param($d)
					for ($i = 0; $i -lt 20; $i++) {
						Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction SilentlyContinue
						if (-not (Test-Path -LiteralPath $d)) { break }
						Start-Sleep -Seconds 3
					}
				}
			}
			return [pscustomobject]@{ Code = [int]$result.Code; Out = [string]$result.Out }
		} finally {
			Remove-PSSession -Session $session -ErrorAction SilentlyContinue
		}
	}

	function Invoke-PsExec([string[]]$files, [string]$mode) {
		$drive = 'AruniSebar' + [guid]::NewGuid().ToString('N').Substring(0, 8)
		$params = @{ Name = $drive; PSProvider = 'FileSystem'; Root = "\\$pc\ADMIN`$"; ErrorAction = 'Stop' }
		if ($o.Cred) { $params.Credential = $o.Cred }
		New-PSDrive @params | Out-Null
		try {
			if (-not (Test-Path -LiteralPath "${drive}:\Temp")) { throw "\\$pc\ADMIN`$ terbuka tetapi C:\Windows\Temp tidak bisa diakses (bukan administrator?)." }
			if ($mode -eq 'uji') { return [pscustomobject]@{ Code = 0; Out = '' } }

			$sub = 'aruni-sebar-' + [guid]::NewGuid().ToString('N')
			$target = "${drive}:\Temp\$sub"
			New-Item -ItemType Directory -Path $target -Force -ErrorAction Stop | Out-Null
			try {
				foreach ($file in $files) { Copy-Item -LiteralPath $file -Destination $target -ErrorAction Stop }
				$arguments = @("\\$pc", '-accepteula', '-nobanner', '-s')
				if ($o.Cred) { $arguments += @('-u', $o.Cred.UserName, '-p', $o.Cred.GetNetworkCredential().Password) }
				# %SystemRoot% diuraikan oleh cmd.exe di komputer client (= folder ADMIN$);
				# jalan.cmd menulis keluaran agent.cmd ke agent.log dan kode keluarnya
				# ke selesai.txt - keluaran PsExec sendiri tidak selalu lengkap
				$arguments += @('cmd.exe', '/d', '/c', "%SystemRoot%\Temp\$sub\jalan.cmd", $mode)
				$psexecText = & $o.PsExec @arguments 2>&1 | Out-String
				$psexecCode = $LASTEXITCODE

				$log = "$target\agent.log"
				$done = "$target\selesai.txt"
				$deadline = (Get-Date).AddMinutes(15)
				$startDeadline = (Get-Date).AddSeconds(20)
				while (-not (Test-Path -LiteralPath $done)) {
					if (-not (Test-Path -LiteralPath $log) -and (Get-Date) -gt $startDeadline) {
						# agent.cmd tidak pernah mulai: PsExec gagal terhubung/menjalankan
						return [pscustomobject]@{ Code = $(if ($psexecCode) { [int]$psexecCode } else { 1 }); Out = [string]$psexecText }
					}
					if ((Get-Date) -gt $deadline) {
						return [pscustomobject]@{ Code = 1; Out = 'Waktu habis: agent.cmd belum selesai setelah 15 menit.' }
					}
					Start-Sleep -Seconds 2
				}
				Start-Sleep -Seconds 1
				$code = [int]((Get-Content -LiteralPath $done -Raw).Trim())
				$text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
			} finally {
				Remove-Folder $target
			}
			return [pscustomobject]@{ Code = $code; Out = [string]$text }
		} finally {
			Remove-PSDrive -Name $drive -Force -ErrorAction SilentlyContinue
		}
	}

	function Invoke-Target {
		$winrm = Test-Port $pc 5985
		$smb = Test-Port $pc 445
		$agent = Test-Port $pc 11100

		if ($o.Mode -eq 'paksa' -and $agent -and -not $o.Paksa) {
			$r.Status = 'DILEWATI'
			$r.Pesan = 'Agent sudah berjalan (port 11100 terbuka). Pakai -Paksa untuk memasang ulang.'
			return
		}

		$ways = @()
		if ($o.Cara -ne 'PsExec' -and $winrm) { $ways += 'WinRM' }
		if ($o.Cara -ne 'WinRM' -and $smb -and $o.PsExec) { $ways += 'PsExec' }
		if ($ways.Count -eq 0) {
			if (-not $winrm -and -not $smb) {
				$r.Status = 'TIDAK TERJANGKAU'
				$r.Pesan = 'Port WinRM (5985) dan SMB (445) tertutup: komputer mati, salah nama/IP, atau firewall.'
			} elseif ($o.Cara -eq 'WinRM') {
				$r.Pesan = 'WinRM tidak aktif (port 5985 tertutup). Aktifkan WinRM, atau pakai -Cara PsExec.'
			} elseif (-not $smb) {
				$r.Pesan = 'SMB (port 445) tertutup: aktifkan File and Printer Sharing, atau aktifkan WinRM.'
			} else {
				$r.Pesan = 'WinRM tidak aktif dan PsExec64.exe tidak ada di folder sebar-agent.'
			}
			return
		}

		$mode = $o.Mode
		$files = if ($mode -eq 'paksa') { $o.InstallFiles } else { $o.RemoveFiles }
		$errors = @()
		foreach ($way in $ways) {
			try {
				if ($way -eq 'WinRM') { $result = Invoke-WinRM $files $mode } else { $result = Invoke-PsExec $files $mode }
			} catch {
				$errors += "${way}: $(Add-Hint (Get-Text $_))"
				continue
			}
			$r.Cara = $way

			if ($mode -eq 'uji') {
				$r.Status = 'SIAP'
				$r.Pesan = "Bisa lewat $way dengan akun ini" + $(if ($agent) { '; agent sudah berjalan' } else { '; agent belum terpasang' })
				return
			}
			if ($result.Code -ne 0) {
				$r.Pesan = Add-Hint (Get-Summary $result.Out $result.Code)
				return
			}
			$summary = Get-Summary $result.Out $result.Code
			if ($mode -eq 'paksa') {
				if (Wait-Port 11100 $true 120) {
					$r.Status = 'BERHASIL'
					$r.Pesan = "Agent terpasang dan port 11100 terbuka."
				} else {
					$r.Status = 'PERIKSA'
					$r.Pesan = "$summary Tetapi port 11100 belum bisa dihubungi dari komputer admin (firewall/jaringan?)."
				}
			} else {
				if (Wait-Port 11100 $false 60) {
					$r.Status = 'BERHASIL'
					$r.Pesan = $summary
				} else {
					$r.Status = 'PERIKSA'
					$r.Pesan = "$summary Tetapi port 11100 masih terbuka."
				}
			}
			return
		}
		$r.Pesan = $errors -join ' | '
	}

	try {
		Invoke-Target
	} catch {
		$r.Status = 'GAGAL'
		$r.Pesan = Get-Text $_
	}
	[pscustomobject]$r
}

# ===================================================================== utama
$mode = if ($Uji) { 'uji' } elseif ($Hapus) { 'hapus' } else { 'paksa' }
$action = @{ uji = 'UJI (tidak memasang apa pun)'; hapus = 'HAPUS agent'; paksa = 'PASANG agent' }[$mode]
if ($Siapkan) { $action = 'SIAPKAN folder (tidak menghubungi komputer client)' }
Write-Host ''
Say "Sebar agent AruniControl - $action"

if (-not (Test-Path -LiteralPath $agentCmd)) { Fail 'agent.cmd tidak ada di folder ini - salin folder sebar-agent dengan lengkap.' }

if ($Siapkan) {
	$code = Get-SetupKey
	if (-not $code) { Fail 'Kode pemasangan belum ada. Simpan kode sebagai aruni-setup.txt di folder ini (lihat README).' }
	if (-not (Test-Path -LiteralPath $keyFile) -or (Get-Content -LiteralPath $keyFile -Raw).Trim() -ne $code) {
		Set-Content -LiteralPath $keyFile -Value $code -Encoding Ascii -NoNewline
	}
	Prepare-Installer
	Unblock-File -LiteralPath $agentCmd -ErrorAction SilentlyContinue
	Ok 'Folder siap. Untuk USB/share cukup: agent.cmd, AruniControl-Setup.exe, aruni-setup.txt, versi.txt'
	exit 0
}

$listPath = $Daftar
if (-not [IO.Path]::IsPathRooted($listPath) -and -not (Test-Path -LiteralPath $listPath)) { $listPath = Join-Path $folder $Daftar }
$targets = Read-Targets $listPath
if ($targets.Count -eq 0) { Fail "Daftar komputer '$listPath' masih kosong." }
Say "$($targets.Count) komputer dari $(Split-Path -Leaf $listPath)"

$stage = Join-Path ([IO.Path]::GetTempPath()) ('aruni-sebar-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$runner = Join-Path $stage 'jalan.cmd'
Set-Content -LiteralPath $runner -Encoding Ascii -Value (@(
	'@echo off',
	'rem dibuat oleh sebar.ps1: keluaran agent.cmd ke agent.log, kode keluar ke selesai.txt',
	'call "%~dp0agent.cmd" %1 > "%~dp0agent.log" 2>&1',
	'> "%~dp0selesai.txt" echo %ERRORLEVEL%'
) -join "`r`n")
$installFiles = @()
if ($mode -eq 'paksa') {
	$code = Get-SetupKey
	if (-not $code) {
		Fail 'Kode pemasangan belum ada. Di komputer admin: Configurator > Aruni Gateway > Pemasangan > Buat kode > "Simpan sebagai aruni-setup.txt..." ke folder ini (lihat README).'
	}
	Prepare-Installer
	$stagedKey = Join-Path $stage 'aruni-setup.txt'
	Set-Content -LiteralPath $stagedKey -Value $code -Encoding Ascii -NoNewline
	$installFiles = @($setup, $agentCmd, $runner, $stagedKey)
	if (Test-Path -LiteralPath $versionFile) { $installFiles += $versionFile }
} elseif ($mode -eq 'uji') {
	if ($env:ARUNI_KEY -or (Test-Path -LiteralPath $keyFile)) { [void](Get-SetupKey) }
	else { Warn 'aruni-setup.txt belum ada - perlu dibuat sebelum memasang (lihat README).' }
}
Unblock-File -LiteralPath $agentCmd -ErrorAction SilentlyContinue

$psexec = $null
if ($Cara -ne 'WinRM') { $psexec = Get-PsExec ($Cara -eq 'PsExec') }

$cred = $null
if ($Kredensial) {
	$cred = $Kredensial
} elseif (-not $AkunSaya) {
	Say 'Masukkan akun administrator di komputer client:'
	Say '  komputer tanpa domain: Administrator (atau akun admin lokal yang sama di semua PC)'
	Say '  komputer domain      : DOMAIN\admin'
	Say '  Cancel/kosong        : pakai akun Windows yang sedang login'
	try { $cred = Get-Credential -Message 'Akun administrator di komputer client AruniControl' } catch { $cred = $null }
}
if ($cred -and -not $cred.UserName) { $cred = $null }
if ($cred) {
	# ".\user" = akun lokal di komputer client, sama dengan nama tanpa domain
	$userName = $cred.UserName -replace '^\.?\\', ''
	if ($userName -ne $cred.UserName) { $cred = New-Object System.Management.Automation.PSCredential -ArgumentList $userName, $cred.Password }
}
$accountText = if ($cred) { $cred.UserName } else { "$env:USERDOMAIN\$env:USERNAME (akun yang sedang login)" }

Write-Host ''
Say "Aksi    : $action"
Say "Komputer: $($targets.Count) ($(@($targets | Select-Object -First 5) -join ', ')$(if ($targets.Count -gt 5) { ', ...' }))"
Say "Cara    : $Cara$(if ($Cara -ne 'WinRM') { if ($psexec) { ' (PsExec siap)' } else { ' (tanpa PsExec)' } })"
Say "Akun    : $accountText"
Say "Paralel : $Paralel"
if (-not (Confirm-Step 'Lanjut?')) { Say 'Dibatalkan.'; Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue; exit 1 }
Write-Host ''

$options = @{
	Mode = $mode
	Cara = $Cara
	Paksa = [bool]$Paksa
	Cred = $cred
	PsExec = $psexec
	InstallFiles = [string[]]$installFiles
	RemoveFiles = [string[]]@($agentCmd, $runner)
}

$colors = @{ BERHASIL = 'Green'; SIAP = 'Green'; DILEWATI = 'DarkGray'; PERIKSA = 'Yellow'; GAGAL = 'Red'; 'TIDAK TERJANGKAU' = 'Red' }
$results = New-Object System.Collections.Generic.List[object]
$pool = [runspacefactory]::CreateRunspacePool(1, $Paralel)
$pool.Open()
$jobs = New-Object System.Collections.Generic.List[object]
try {
	foreach ($pc in $targets) {
		$ps = [powershell]::Create()
		$ps.RunspacePool = $pool
		[void]$ps.AddScript($worker.ToString()).AddArgument($pc).AddArgument($options)
		$jobs.Add([pscustomobject]@{ Komputer = $pc; PowerShell = $ps; Handle = $ps.BeginInvoke() })
	}
	$started = Get-Date
	while ($jobs.Count -gt 0) {
		foreach ($job in $jobs.ToArray()) {
			if (-not $job.Handle.IsCompleted) { continue }
			$result = $null
			try {
				$result = @($job.PowerShell.EndInvoke($job.Handle) | Where-Object { $_ -and $_.PSObject.Properties['Status'] }) | Select-Object -Last 1
			} catch {
				$result = $null
				$failure = $_.Exception.Message
			}
			if (-not $result) {
				$result = [pscustomobject]@{ Komputer = $job.Komputer; Status = 'GAGAL'; Cara = '-'; Pesan = $(if ($failure) { $failure } else { 'Tidak ada hasil.' }) }
			}
			$failure = $null
			$job.PowerShell.Dispose()
			[void]$jobs.Remove($job)
			$results.Add($result)
			$color = $colors[[string]$result.Status]
			if (-not $color) { $color = 'White' }
			Write-Host ('[{0}/{1}] {2,-18} {3,-16} {4}' -f $results.Count, $targets.Count, $result.Komputer, $result.Status, $result.Pesan) -ForegroundColor $color
		}
		if ($jobs.Count -gt 0) { Start-Sleep -Milliseconds 300 }
	}
} finally {
	foreach ($job in $jobs) { try { $job.PowerShell.Stop(); $job.PowerShell.Dispose() } catch { } }
	$pool.Close()
	$pool.Dispose()
	if ($stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
}

# ------------------------------------------------------------------- hasil
$resultDir = Join-Path $folder 'hasil'
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$csv = Join-Path $resultDir "hasil-$mode-$stamp.csv"
$results | Select-Object Komputer, Status, Cara, Pesan | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
$failed = @($results | Where-Object { $_.Status -in @('GAGAL', 'TIDAK TERJANGKAU') })
$check = @($results | Where-Object { $_.Status -eq 'PERIKSA' })

Write-Host ''
$counts = $results | Group-Object Status | Sort-Object Name | ForEach-Object { "$($_.Name): $($_.Count)" }
Say ("Selesai dalam {0:mm\:ss} - {1}" -f ((Get-Date) - $started), ($counts -join ', '))
Say "Hasil lengkap: $csv"
if ($failed.Count -gt 0) {
	$retry = Join-Path $resultDir "gagal-$stamp.txt"
	Set-Content -LiteralPath $retry -Value @($failed | ForEach-Object { $_.Komputer }) -Encoding Ascii
	Warn "$($failed.Count) komputer gagal. Perbaiki penyebabnya lalu ulangi hanya yang gagal:"
	Warn "  .\sebar.ps1 -Daftar hasil\gagal-$stamp.txt$(if ($mode -eq 'hapus') { ' -Hapus' } elseif ($mode -eq 'uji') { ' -Uji' })"
	exit 1
}
if ($check.Count -gt 0) { Warn "$($check.Count) komputer perlu diperiksa (lihat kolom Pesan)." }
exit 0
