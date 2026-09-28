# Automated test of an AruniControl Windows installer on a fresh Windows
# (GitHub runner). Installs it, then drives every feature through veyon-wcli
# against the local AruniControl server and checks the effect on the system.
#
#   pwsh ci/windows-smoke-test.ps1 -Setup AruniControl-Setup-x.y.z.exe
#
# Exits with 1 if any check failed.

param(
	[Parameter(Mandatory = $true)][string]$Setup
)

$ErrorActionPreference = "Continue"
$script:failures = @()
$script:warnings = @()

$cli = "$env:ProgramFiles\AruniControl\veyon-wcli.exe"
$data = "$env:ProgramData\AruniControl"

function Check([string]$name, [scriptblock]$test) {
	try {
		$result = & $test
		if ($result -eq $false) { throw "check returned false" }
		Write-Host "PASS  $name" -ForegroundColor Green
	} catch {
		Write-Host "FAIL  $name : $_" -ForegroundColor Red
		$script:failures += $name
	}
}

function Warn([string]$name, [scriptblock]$test) {
	try {
		$result = & $test
		if ($result -eq $false) { throw "check returned false" }
		Write-Host "PASS  $name" -ForegroundColor Green
	} catch {
		Write-Host "WARN  $name : $_" -ForegroundColor Yellow
		$script:warnings += $name
	}
}

function Aruni {
	$output = & $cli @args 2>&1 | Out-String
	Write-Host "  > veyon-wcli $($args -join ' ')"
	Write-Host ($output.Trim() -replace '(?m)^', '    ')
	return $output
}

function Wait-Until([scriptblock]$condition, [int]$seconds = 60) {
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline) {
		if (& $condition) { return $true }
		Start-Sleep -Seconds 2
	}
	return $false
}

function Wait-Server {
	Wait-Until { (Test-NetConnection 127.0.0.1 -Port 11100 -WarningAction SilentlyContinue).TcpTestSucceeded } 120
}

function Access-Log {
	Get-Content "$data\logs\access.jsonl" -ErrorAction SilentlyContinue | ForEach-Object { $_ | ConvertFrom-Json }
}

# ---------------------------------------------------------------- install
Write-Host "=== install"
Start-Process -FilePath $Setup -ArgumentList '/S' -Wait
Check "installed" { Test-Path $cli }
Check "server listening" { Wait-Server }

Aruni authkeys create ci | Out-Null
Check "key created" { Test-Path "$data\keys\private\ci\key" }

# installation code: remove the public key, install again with aruni-setup.txt
# next to the installer (also tests updating over an installed version)
$code = (Aruni gateway setupcode computers).Trim().Split("`n")[-1].Trim()
Check "setup code created" { $code.StartsWith("ARUNISETUP1:") }
$setupDir = Split-Path -Parent (Resolve-Path $Setup)
Set-Content -Path "$setupDir\aruni-setup.txt" -Value $code -Encoding ascii
Remove-Item -Recurse -Force "$data\keys\public\ci"
Start-Process -FilePath $Setup -ArgumentList '/S' -Wait
Remove-Item "$setupDir\aruni-setup.txt"
Check "setup code applied by the installer" { Test-Path "$data\keys\public\ci\key" }
Check "key authentication configured" { (Aruni config get Authentication/Method).Trim().EndsWith("1") }
Check "server listening after update" { Wait-Server }
$env:VEYON_AUTH_KEY_NAME = "ci"

$plugins = Aruni plugin list
foreach ($plugin in @("ExamMode", "Inventory", "SoftwareDeploy", "AdminRoles", "RemoteCommand", "DeviceControl",
					  "LabClean", "ApplicationMonitoring", "SiteFilter", "AccessLog", "AruniGateway")) {
	Check "plugin $plugin" { $plugins -match "(?m)^$plugin\s*$" }
}

# ---------------------------------------------------------------- remote command
Write-Host "=== remote command"
$out = Aruni remotecommand run 127.0.0.1 "Write-Output ('ps-ok-' + [Environment]::UserName)"
Check "powershell command" { $out -match "ps-ok-" }
$out = Aruni remotecommand run 127.0.0.1 "echo cmd-ok" cmd
Check "cmd command" { $out -match "cmd-ok" }
$out = Aruni remotecommand run 127.0.0.1 "exit 7"
Check "exit code reported" { $out -match "exit 7" }
Check "command in access log" { (Access-Log | Where-Object { $_.e -eq "remote_command" }).Count -ge 1 }

# ---------------------------------------------------------------- inventory
Write-Host "=== inventory"
$json = Aruni inventory show 127.0.0.1
$inventory = $null
try { $inventory = ($json.Substring($json.IndexOf("{"))) | ConvertFrom-Json } catch { }
Check "inventory answered" { $inventory -ne $null }
Check "inventory os" { $inventory.os -match "Windows" }
Check "inventory cpu" { $inventory.cpu.Length -gt 0 }
Check "inventory ram" { $inventory.ramMB -gt 1000 }
Check "inventory disks" { @($inventory.disks).Count -ge 1 }
Check "inventory software" { @($inventory.software).Count -ge 5 }
Start-Sleep -Seconds 15
$json = Aruni inventory show 127.0.0.1
Warn "inventory serial number" { (($json.Substring($json.IndexOf("{"))) | ConvertFrom-Json).serial.Length -gt 0 }

# ---------------------------------------------------------------- software deployment
Write-Host "=== software deployment"
# a program the runner image does not have
$putty = "$env:ProgramFiles\PuTTY\putty.exe"
Check "test program not installed yet" { -not (Test-Path $putty) }
Invoke-WebRequest "https://the.earth.li/~sgtatham/putty/0.83/w64/putty-64bit-0.83-installer.msi" -OutFile "$env:TEMP\putty.msi"
Aruni softwaredeploy install 127.0.0.1 "$env:TEMP\putty.msi" | Out-Null
Check "msi installed" { Test-Path $putty }
Check "program listed" { (Aruni softwaredeploy list 127.0.0.1) -match "PuTTY" }
Aruni softwaredeploy uninstall 127.0.0.1 "PuTTY" | Out-Null
Check "program removed" { Wait-Until { -not (Test-Path $putty) } 60 }
Check "no deploy leftovers" { @(Get-ChildItem "$data\deploy" -ErrorAction SilentlyContinue).Count -eq 0 }

# ---------------------------------------------------------------- USB and printing
Write-Host "=== USB and printing"
Aruni feature start 127.0.0.1 DeviceControl '{"usb":"block","printer":"block"}' | Out-Null
Start-Sleep -Seconds 5
Check "USB storage blocked" { (Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\USBSTOR).Start -eq 4 }
Check "removable storage policy" { (Get-ItemProperty HKLM:\SOFTWARE\Policies\Microsoft\Windows\RemovableStorageDevices -ErrorAction Stop).Deny_All -eq 1 }
Check "print spooler disabled" { (Get-Service Spooler).StartType -eq "Disabled" -and (Get-Service Spooler).Status -ne "Running" }
Aruni feature start 127.0.0.1 DeviceControl '{"usb":"allow","printer":"allow"}' | Out-Null
Start-Sleep -Seconds 8
Check "USB storage allowed" { (Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\USBSTOR).Start -eq 3 }
Check "removable storage policy removed" { (Get-ItemProperty HKLM:\SOFTWARE\Policies\Microsoft\Windows\RemovableStorageDevices -ErrorAction SilentlyContinue).Deny_All -eq $null }
Check "print spooler running" { (Get-Service Spooler).StartType -eq "Automatic" -and (Get-Service Spooler).Status -eq "Running" }

# ---------------------------------------------------------------- site blocking
Write-Host "=== site blocking"
$hosts = "$env:windir\System32\drivers\etc\hosts"
Aruni feature start 127.0.0.1 SiteFilter '{"sites":["example.com"]}' | Out-Null
Start-Sleep -Seconds 4
Check "site blocked in hosts" { (Get-Content $hosts -Raw) -match "0\.0\.0\.0 example\.com" }
Aruni feature stop 127.0.0.1 SiteFilter | Out-Null
Start-Sleep -Seconds 4
Check "site unblocked" { (Get-Content $hosts -Raw) -notmatch "example\.com" }

# ---------------------------------------------------------------- exam mode
Write-Host "=== exam mode"
$env:ARUNI_TEST_ALLOW_LOCAL = "1"
Start-Process notepad
Start-Sleep -Seconds 2
Aruni feature start 127.0.0.1 ExamMode '{"sites":["example.com"],"apps":["notepad"],"lockKeys":true,"blockInternet":false,"url":"https://example.com","kiosk":true}' | Out-Null
Check "exam mode active" { Wait-Until { (Get-Content "$data\exammode.json" -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json).active -eq $true } 20 }
Check "forbidden application closed" { Wait-Until { -not (Get-Process notepad -ErrorAction SilentlyContinue) } 40 }
Start-Process notepad
Check "forbidden application closed again" { Wait-Until { -not (Get-Process notepad -ErrorAction SilentlyContinue) } 40 }
Check "task manager locked" { (Get-ItemProperty HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System).DisableTaskMgr -eq 1 }
Warn "kiosk browser runs as the user, not as SYSTEM" {
	Wait-Until { (Get-Process msedge -IncludeUserName -ErrorAction SilentlyContinue | Where-Object { $_.UserName -and $_.UserName -notmatch "SYSTEM" }).Count -gt 0 } 40
}
Check "kiosk browser never runs as SYSTEM" { -not (Get-Process msedge -IncludeUserName -ErrorAction SilentlyContinue | Where-Object { $_.UserName -match "SYSTEM" }) }
Aruni feature stop 127.0.0.1 ExamMode | Out-Null
Check "exam mode ended" { Wait-Until { (Get-Content "$data\exammode.json" -Raw | ConvertFrom-Json).active -eq $false } 20 }
Check "task manager unlocked" { (Get-ItemProperty HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System).DisableTaskMgr -eq $null }
Start-Process notepad
Start-Sleep -Seconds 25
Check "applications allowed after the exam" { Get-Process notepad -ErrorAction SilentlyContinue }
Stop-Process -Name notepad -ErrorAction SilentlyContinue
Warn "kiosk browser closed after the exam" { Wait-Until { -not (Get-Process msedge -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -match "aruni-exam-browser" }) } 20 }
Remove-Item Env:\ARUNI_TEST_ALLOW_LOCAL

# ---------------------------------------------------------------- lab clean mode
Write-Host "=== lab clean mode"
net user siswa "Aruni-Test-2026!" /add | Out-Null
New-Item -ItemType Directory -Force "C:\Users\siswa\Desktop" | Out-Null
Set-Content "C:\Users\siswa\Desktop\tugas.txt" "file of a student"
New-Item -ItemType Directory -Force "$env:USERPROFILE\Desktop" | Out-Null
Set-Content "$env:USERPROFILE\Desktop\admin-keep.txt" "file of an administrator"
$users = '["siswa","' + $env:USERNAME + '"]'
Aruni feature start 127.0.0.1 LabClean ('{"enabled":true,"users":' + $users + ',"folders":["Desktop"],"keepDays":7}') | Out-Null
Start-Sleep -Seconds 3
Check "lab clean mode on" { (Get-Content "$data\labclean.json" -Raw | ConvertFrom-Json).settings.enabled -eq $true }
Check "fast startup off" { (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power").HiberbootEnabled -eq 0 }
Aruni feature start 127.0.0.1 LabClean '{"cleanNow":true}' | Out-Null
Start-Sleep -Seconds 4
Check "student file moved away" { -not (Test-Path "C:\Users\siswa\Desktop\tugas.txt") }
Check "student file kept in quarantine" { @(Get-ChildItem "$data\labclean" -Recurse -Filter tugas.txt).Count -eq 1 }
Check "administrator files untouched" { Test-Path "$env:USERPROFILE\Desktop\admin-keep.txt" }
# the cleaning at start of the service (as at boot): pretend a new boot
Set-Content "C:\Users\siswa\Desktop\tugas2.txt" "another file of a student"
$state = Get-Content "$data\labclean.json" -Raw | ConvertFrom-Json
$state.boot = "test"
$state | ConvertTo-Json -Depth 5 | Set-Content "$data\labclean.json"
Aruni service restart | Out-Null
Check "cleaned at start of the service" { Wait-Until { -not (Test-Path "C:\Users\siswa\Desktop\tugas2.txt") } 60 }
Check "server listening after service restart" { Wait-Server }
Aruni feature stop 127.0.0.1 LabClean | Out-Null
Start-Sleep -Seconds 3
Check "lab clean mode off" { (Get-Content "$data\labclean.json" -Raw | ConvertFrom-Json).settings.enabled -eq $false }

# ---------------------------------------------------------------- admin roles
Write-Host "=== admin roles"
Aruni authkeys create guru | Out-Null
$pem = Get-Content "$data\keys\public\guru\key" -Raw
function Send-Roles($roles) {
	$policy = @{ v = 1; roles = $roles } | ConvertTo-Json -Depth 5 -Compress
	$arguments = @{ policy = $policy } | ConvertTo-Json -Compress
	$env:VEYON_AUTH_KEY_NAME = "ci"
	Aruni feature start 127.0.0.1 AdminRoles $arguments | Out-Null
	Start-Sleep -Seconds 3
}
Send-Roles @(@{ key = "guru"; name = "Guru"; public = $pem; rooms = @(); hosts = @(); allowed = @("lock") })
Check "roles received" { (Get-Content "$data\roles.json" -Raw | ConvertFrom-Json).roles.Count -eq 1 }
$env:VEYON_AUTH_KEY_NAME = "guru"
Aruni feature start 127.0.0.1 DeviceControl '{"usb":"block"}' | Out-Null
Start-Sleep -Seconds 4
Check "function outside the role refused" { (Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\USBSTOR).Start -eq 3 }
Check "refusal logged with the key" { (Access-Log | Where-Object { $_.e -eq "feature_denied" -and $_.key -eq "guru" }).Count -ge 1 }
Aruni feature start 127.0.0.1 AdminRoles '{"policy":"{\"roles\":[]}"}' | Out-Null
Start-Sleep -Seconds 3
Check "role cannot change the roles" { (Get-Content "$data\roles.json" -Raw | ConvertFrom-Json).roles.Count -eq 1 }
Send-Roles @(@{ key = "guru"; name = "Guru"; public = $pem; rooms = @("Lain"); hosts = @("10.9.9.9"); allowed = @("lock") })
$env:VEYON_AUTH_KEY_NAME = "guru"
Aruni feature stop 127.0.0.1 ScreenLock | Out-Null
Start-Sleep -Seconds 2
Check "computer outside the rooms refused" { (Access-Log | Where-Object { $_.e -eq "access_denied" -and $_.key -eq "guru" }).Count -ge 1 }
Send-Roles @()
Check "roles removed" { (Get-Content "$data\roles.json" -Raw | ConvertFrom-Json).roles.Count -eq 0 }
Check "key of the removed role deleted" { -not (Test-Path "$data\keys\public\guru\key") }
$env:VEYON_AUTH_KEY_NAME = "ci"

# ---------------------------------------------------------------- schedules
Write-Host "=== schedules"
$gatewayDir = "$env:TEMP\aruni-gateway-test"
New-Item -ItemType Directory -Force $gatewayDir | Out-Null
$time = (Get-Date).AddMinutes(-1).ToString("HH:mm")
@{ rules = @(@{ id = "t1"; name = "Tes USB"; enabled = $true; days = 127; time = $time; action = "blockUsb"; room = ""; sites = @(); text = "" }) } |
	ConvertTo-Json -Depth 4 | Set-Content "$gatewayDir\schedules.json"
$env:ARUNI_GATEWAY_DIR = $gatewayDir
$env:ARUNI_SCHEDULE_TARGETS = "127.0.0.1"
$scheduler = Start-Process -FilePath $cli -ArgumentList "gateway runschedules" -PassThru -NoNewWindow
Check "schedule ran" { Wait-Until { (Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Services\USBSTOR).Start -eq 4 } 60 }
Check "schedule result recorded" { Wait-Until { (Get-Content "$gatewayDir\schedules-status.json" -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json).t1.result -match "1" } 30 }
Stop-Process -Id $scheduler.Id -Force -ErrorAction SilentlyContinue
Remove-Item Env:\ARUNI_GATEWAY_DIR, Env:\ARUNI_SCHEDULE_TARGETS
Aruni feature start 127.0.0.1 DeviceControl '{"usb":"allow"}' | Out-Null

# ---------------------------------------------------------------- attendance and usage
Write-Host "=== attendance"
Warn "logon recorded" { (Access-Log | Where-Object { $_.e -eq "user_login" }).Count -ge 1 }

# ---------------------------------------------------------------- result
Write-Host ""
Write-Host "server log (last lines):"
Get-ChildItem "$env:windir\Temp\AruniControl*.log", "$env:windir\Temp\Veyon*.log" -ErrorAction SilentlyContinue |
	ForEach-Object { Write-Host "--- $($_.Name)"; Get-Content $_ -Tail 40 }

Write-Host ""
if ($script:warnings.Count -gt 0) { Write-Host "Warnings: $($script:warnings -join ', ')" -ForegroundColor Yellow }
if ($script:failures.Count -gt 0) {
	Write-Host "FAILED: $($script:failures -join ', ')" -ForegroundColor Red
	exit 1
}
Write-Host "All checks passed" -ForegroundColor Green
exit 0
