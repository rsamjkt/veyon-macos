@echo off
rem Jalankan sebar.ps1 tanpa perlu mengubah ExecutionPolicy.
rem   Sebar.cmd             pasang ke semua komputer di komputer.txt
rem   Sebar.cmd -Uji        hanya cek koneksi dan akun
rem   Sebar.cmd -Hapus      hapus agent
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0sebar.ps1" %*
set "RC=%ERRORLEVEL%"
if "%~1"=="" pause
exit /b %RC%
