# Automated test of sebar-agent (mass installation of the AruniControl agent)
# on a fresh Windows (GitHub runner). The runner is both the admin computer and
# the client ("localhost"): sebar.ps1 installs, skips, reinstalls and removes
# the agent through WinRM and through PsExec, agent.cmd is tested on its own.
#
#   powershell -File ci/sebar-agent-test.ps1 -Setup AruniControl-Setup-x.y.z.exe
#
# Exits with 1 if any check failed.

param(
	[Parameter(Mandatory = $true)][string]$Setup
)

$ErrorActionPreference = "Continue"
$script:failures = @()

$dir = Resolve-Path (Join-Path $PSScriptRoot "..\sebar-agent")
$sebar = Join-Path $dir "sebar.ps1"
$cli = "$env:ProgramFiles\AruniControl\veyon-wcli.exe"
$data = "$env:ProgramData\AruniControl"

function Check([string]$name, [scriptblock]$test) {
	try {
		$result = & $test
		if ($result -eq $false -or $null -eq $result) { throw "check returned $result" }
		Write-Host "PASS  $name" -ForegroundColor Green
	} catch {
		Write-Host "FAIL  $name : $_" -ForegroundColor Red
		$script:failures += $name
	}
}

function Wait-Until([scriptblock]$condition, [int]$seconds = 60) {
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline) {
		if (& $condition) { return $true }
		Start-Sleep -Seconds 2
	}
	return $false
}

function Listening { [bool](Get-NetTCPConnection -LocalPort 11100 -State Listen -ErrorAction SilentlyContinue) }

function Set-List([string[]]$names) { Set-Content -Path (Join-Path $dir "komputer.txt") -Value $names -Encoding Ascii }

# runs sebar.ps1 in Windows PowerShell 5.1 like Sebar.cmd does, returns the exit code
function Sebar {
	Write-Host "  > sebar.ps1 $($args -join ' ')"
	& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $sebar -Ya -AkunSaya @args 2>&1 | ForEach-Object { Write-Host "    $_" }
	return $LASTEXITCODE
}

function Last-Result {
	$file = Get-ChildItem (Join-Path $dir "hasil\hasil-*.csv") | Sort-Object LastWriteTime | Select-Object -Last 1
	@(Import-Csv $file.FullName)
}

function No-Leftovers { -not (Get-ChildItem "$env:SystemRoot\Temp" -Filter "aruni-sebar-*" -ErrorAction SilentlyContinue) }

# ---------------------------------------------------------------- prepare
Write-Host "=== prepare (WinRM, admin computer with AruniControl)"
Enable-PSRemoting -Force -SkipNetworkProfileCheck | Out-Null
Set-Item WSMan:\localhost\Client\TrustedHosts -Value "*" -Force
Check "WinRM running" { Test-WSMan localhost -ErrorAction Stop }

# downloaded installer with its release name: sebar.ps1 has to pick it up
Copy-Item $Setup $dir
Start-Process -FilePath $Setup -ArgumentList "/S" -Wait
Check "admin computer installed" { Test-Path $cli }
& $cli authkeys create ci | Out-Null
Check "key created" { Test-Path "$data\keys\public\ci\key" }
Check "server listening" { Wait-Until { Listening } 120 }

# ---------------------------------------------------------------- list / test
Write-Host "=== list parsing and -Uji"
Set-List @("bukan_nama!")
Check "invalid list entry refused" { (Sebar) -eq 1 }

Set-List @("# komentar", "localhost  # di belakang komentar", "LOCALHOST", "")
Check "-Uji succeeds" { (Sebar -Uji) -eq 0 }
Check "-Uji: duplicates removed, ready" { $r = @(Last-Result); $r.Count -eq 1 -and $r[0].Status -eq "SIAP" }
Check "-Uji installs nothing" { -not (Test-Path (Join-Path $dir "AruniControl-Setup.exe")) }

Set-List @("10.255.255.1")
Check "unreachable computer fails" { (Sebar -Uji) -eq 1 }
Check "unreachable reported" { (Last-Result)[0].Status -eq "TIDAK TERJANGKAU" }
Check "retry list written" { Get-ChildItem (Join-Path $dir "hasil\gagal-*.txt") }

# ---------------------------------------------------------------- WinRM
Write-Host "=== install through WinRM"
Set-List @("localhost")
Check "running agent skipped" { (Sebar -Cara WinRM) -eq 0 }
Check "skipped reported" { (Last-Result)[0].Status -eq "DILEWATI" }
Check "setup code generated from this computer" { (Get-Content (Join-Path $dir "aruni-setup.txt") -Raw).StartsWith("ARUNISETUP1:") }
Check "installer taken over" { Test-Path (Join-Path $dir "AruniControl-Setup.exe") }

# remove the public key: the installation code has to bring it back
Remove-Item -Recurse -Force "$data\keys\public\ci"
Check "reinstall with -Paksa" { (Sebar -Cara WinRM -Paksa) -eq 0 }
Check "WinRM installation reported" { $r = (Last-Result)[0]; $r.Status -eq "BERHASIL" -and $r.Cara -eq "WinRM" }
Check "key applied from the code" { Test-Path "$data\keys\public\ci\key" }
Check "agent mode (no Master shortcut)" { -not (Test-Path "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\AruniControl\AruniControl Master.lnk") }
Check "listening after WinRM installation" { Wait-Until { Listening } 60 }
Check "no files left on the client (WinRM)" { No-Leftovers }

# ---------------------------------------------------------------- PsExec
Write-Host "=== remove and install through PsExec"
Check "remove through PsExec" { (Sebar -Cara PsExec -Hapus) -eq 0 }
Check "PsExec downloaded and signed" { (Get-AuthenticodeSignature (Join-Path $dir "PsExec64.exe")).Status -eq "Valid" }
Check "PsExec removal reported" { $r = (Last-Result)[0]; $r.Status -eq "BERHASIL" -and $r.Cara -eq "PsExec" }
Check "program removed" { -not (Test-Path $cli) }
Check "port closed" { Wait-Until { -not (Listening) } 30 }

Check "install through PsExec" { (Sebar -Cara PsExec) -eq 0 }
Check "PsExec installation reported" { $r = (Last-Result)[0]; $r.Status -eq "BERHASIL" -and $r.Cara -eq "PsExec" }
Check "installed through PsExec" { Test-Path $cli }
Check "key present after PsExec installation" { Test-Path "$data\keys\public\ci\key" }
Check "no files left on the client (PsExec)" { No-Leftovers }

# ---------------------------------------------------------------- agent.cmd
Write-Host "=== agent.cmd on its own"
$agent = Join-Path $dir "agent.cmd"
Remove-Item (Join-Path $dir "versi.txt") -ErrorAction SilentlyContinue
Check "-Siapkan succeeds" { (Sebar -Siapkan) -eq 0 }
$installed = @("HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\AruniControl", "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\AruniControl" |
	ForEach-Object { (Get-ItemProperty $_ -ErrorAction SilentlyContinue).DisplayVersion } | Where-Object { $_ })[0]
Check "installed version known" { $installed }
Check "-Siapkan wrote the release version" { (Get-Content (Join-Path $dir "versi.txt") -Raw).Trim() -eq $installed }
$output = & cmd.exe /d /c "`"$agent`" pasang" 2>&1 | Out-String
Write-Host $output
Check "same version skipped" { $LASTEXITCODE -eq 0 -and $output -match "DILEWATI" }
$output = & cmd.exe /d /c "`"$agent`" salah" 2>&1 | Out-String
Check "unknown command refused" { $LASTEXITCODE -eq 9 }

# ---------------------------------------------------------------- safety
Write-Host "=== private keys refused"
$private = (& $cli gateway setupcode private 2>&1 | Out-String) -split "`r?`n" | Where-Object { $_ -match "^ARUNISETUP1:" } | Select-Object -Last 1
Check "private setup code created" { $private }
$normal = Get-Content (Join-Path $dir "aruni-setup.txt") -Raw
Set-Content -Path (Join-Path $dir "aruni-setup.txt") -Value $private -Encoding Ascii -NoNewline
$before = @(Get-ChildItem (Join-Path $dir "hasil\hasil-*.csv")).Count
Check "code with private keys refused" { (Sebar -Paksa) -eq 1 }
Check "nothing done with a private code" { @(Get-ChildItem (Join-Path $dir "hasil\hasil-*.csv")).Count -eq $before }
Set-Content -Path (Join-Path $dir "aruni-setup.txt") -Value $normal -Encoding Ascii -NoNewline

Write-Host "=== remove through WinRM"
Check "remove through WinRM" { (Sebar -Cara WinRM -Hapus) -eq 0 }
Check "program removed (WinRM)" { -not (Test-Path $cli) }
Check "service removed" { -not (Get-Service | Where-Object { $_.Name -match "veyon|aruni" }) }

Write-Host ""
if ($script:failures.Count -gt 0) {
	Write-Host "$($script:failures.Count) check(s) failed:" -ForegroundColor Red
	$script:failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
	exit 1
}
Write-Host "All checks passed." -ForegroundColor Green
exit 0
