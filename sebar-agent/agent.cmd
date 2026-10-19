@echo off
rem AruniControl - pasang / hapus agent di komputer ini, tanpa internet.
rem
rem   agent.cmd           pasang (klik kanan > Run as administrator), jeda di akhir
rem   agent.cmd pasang    pasang; dilewati kalau versi yang sama sudah terpasang
rem                       (aman untuk GPO startup script yang jalan tiap boot)
rem   agent.cmd paksa     pasang ulang walaupun versinya sama (dipakai sebar.ps1)
rem   agent.cmd hapus     hapus AruniControl dari komputer ini
rem
rem Butuh AruniControl-Setup.exe di folder ini. aruni-setup.txt (kode
rem pemasangan) di folder yang sama dibaca otomatis oleh installer.
rem versi.txt (ditulis sebar.ps1) dipakai untuk melewati versi yang sama.
rem
rem Kode keluar: 0 berhasil/dilewati, 2 installer tidak ada, 3 tidak terpasang
rem setelah pemasangan, 4 masih terpasang setelah dihapus, 5 bukan
rem administrator, 9 perintah tidak dikenal, lainnya = kode keluar installer.

setlocal EnableExtensions
set "HERE=%~dp0"
set "DIR=%ProgramFiles%\AruniControl"
if defined ProgramW6432 set "DIR=%ProgramW6432%\AruniControl"
set "ARP=HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\AruniControl"
set "MODE=%~1"
set "JEDA=0"
set "RC=0"
if "%MODE%"=="" set "JEDA=1"
if "%MODE%"=="" set "MODE=pasang"

fltmc >nul 2>&1
if not errorlevel 1 goto admin_ok
echo [AruniControl] GAGAL: jalankan sebagai Administrator (klik kanan, Run as administrator).
set "RC=5"
goto selesai
:admin_ok

if /i "%MODE%"=="pasang" goto pasang
if /i "%MODE%"=="paksa" goto pasang
if /i "%MODE%"=="hapus" goto hapus
echo [AruniControl] GAGAL: perintah tidak dikenal "%MODE%" (pakai: pasang, paksa, hapus).
set "RC=9"
goto selesai

rem ------------------------------------------------------------------ pasang
:pasang
if exist "%HERE%AruniControl-Setup.exe" goto ada_setup
echo [AruniControl] GAGAL: AruniControl-Setup.exe tidak ada di folder agent.cmd.
set "RC=2"
goto selesai
:ada_setup

if /i "%MODE%"=="paksa" goto jalankan_setup
if not exist "%DIR%\veyon-wcli.exe" goto jalankan_setup
if not exist "%HERE%versi.txt" goto jalankan_setup
set "BARU="
set /p BARU=<"%HERE%versi.txt"
set "LAMA="
for /f "tokens=2,*" %%a in ('reg query "%ARP%" /v DisplayVersion /reg:64 2^>nul') do if /i "%%a"=="REG_SZ" set "LAMA=%%b"
if not defined LAMA for /f "tokens=2,*" %%a in ('reg query "%ARP%" /v DisplayVersion /reg:32 2^>nul') do if /i "%%a"=="REG_SZ" set "LAMA=%%b"
if not defined BARU goto jalankan_setup
if not "%BARU%"=="%LAMA%" goto jalankan_setup
echo [AruniControl] DILEWATI: versi %LAMA% sudah terpasang di %COMPUTERNAME% (pakai "agent.cmd paksa" untuk pasang ulang).
goto selesai

:jalankan_setup
if exist "%HERE%aruni-setup.txt" goto ada_kode
echo [AruniControl] Peringatan: aruni-setup.txt tidak ada - agent dipasang tanpa kunci autentikasi.
:ada_kode
echo [AruniControl] Memasang agent di %COMPUTERNAME% (tanpa jendela)...
start "" /wait "%HERE%AruniControl-Setup.exe" /S /AGENT
set "RC=%ERRORLEVEL%"
if "%RC%"=="0" goto cek_pasang
echo [AruniControl] GAGAL: installer berhenti dengan kode %RC%.
goto selesai
:cek_pasang
if exist "%DIR%\veyon-wcli.exe" goto pasang_ok
echo [AruniControl] GAGAL: AruniControl tidak ditemukan setelah pemasangan.
set "RC=3"
goto selesai
:pasang_ok
echo [AruniControl] BERHASIL: agent terpasang di %COMPUTERNAME%.
goto selesai

rem ------------------------------------------------------------------- hapus
:hapus
if exist "%DIR%\uninstall.exe" goto ada_uninstall
echo [AruniControl] BERHASIL: AruniControl memang tidak terpasang di %COMPUTERNAME%.
goto selesai
:ada_uninstall
echo [AruniControl] Menghapus AruniControl dari %COMPUTERNAME%...
rem _?= menjalankan uninstaller di tempat dan menunggunya; harus terakhir, tanpa tanda kutip
start "" /wait "%DIR%\uninstall.exe" /S _?=%DIR%
rmdir /s /q "%DIR%" >nul 2>&1
if not exist "%DIR%\veyon-wcli.exe" goto hapus_ok
echo [AruniControl] GAGAL: AruniControl masih terpasang setelah dihapus.
set "RC=4"
goto selesai
:hapus_ok
echo [AruniControl] BERHASIL: AruniControl dihapus dari %COMPUTERNAME%.
goto selesai

:selesai
if "%JEDA%"=="1" pause
exit /b %RC%
