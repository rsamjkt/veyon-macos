; AruniControl one-click installer (NSIS)
; Builds a self-contained Windows installer from the packaged staging folder.
;
; Required defines (passed via makensis -D...):
;   SRC      - staging folder (Windows path) containing the AruniControl Server build
;   OUTFILE  - output installer path
;   ICON     - .ico used for the installer/uninstaller
; Optional:
;   VERSION  - product version string (default 1.4.0)
;
; Command line of the installer (besides /S for a silent install):
;   /ENROLL=<code> - make this laptop a roaming laptop of the office gateway
;                    with the given enrollment code (ARUNIL1:...)

Unicode true

!define PRODUCT "AruniControl"
!define PUBLISHER "Arunika"
!ifndef VERSION
  !define VERSION "1.4.0"
!endif
!ifndef ICON
  !define ICON "installer.ico"
!endif
!ifndef OUTFILE
  !define OUTFILE "AruniControl-Setup.exe"
!endif

!define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}"

Name "${PRODUCT} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${PRODUCT}"
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\veyon-configurator.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Buka AruniControl Configurator"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "AruniControl Server" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  ; updating an existing installation (e.g. the automatic update running
  ; "setup.exe /S /UPDATE"): stop the service and all AruniControl programs
  ; first, their files are in use otherwise
  ${If} ${FileExists} "$INSTDIR\veyon-wcli.exe"
    DetailPrint "Menghentikan AruniControl versi lama..."
    nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service stop'
    ; no /T - this installer itself may have been started by veyon-server
    nsExec::ExecToLog 'taskkill /F /IM veyon-service.exe'
    nsExec::ExecToLog 'taskkill /F /IM veyon-server.exe /IM veyon-worker.exe /IM veyon-master.exe /IM veyon-configurator.exe'
    Sleep 2000
  ${EndIf}

  ; payload (self-contained server + plugins + Qt/QCA/OpenSSL/Interception)
  File /r "${SRC}\*.*"

  ; register + start the background service (runs as LocalSystem)
  DetailPrint "Mendaftarkan service AruniControl..."
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service register'
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service start'

  ; allow the server through Windows Firewall (inbound): 11100 control port,
  ; 11601-11698 roaming laptops forwarded by the Aruni Gateway, 11699 its
  ; directory for Masters on the office network (both LAN only, see
  ; GatewayService::isLocalNetworkAddress)
  DetailPrint "Menambah aturan firewall..."
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="AruniControl Server"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="AruniControl Server" dir=in action=allow program="$INSTDIR\veyon-server.exe" protocol=TCP localport=11100,11601-11699 enable=yes'

  ; roaming laptop enrollment for mass deployment: setup.exe /S /ENROLL=ARUNIL1:...
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/ENROLL=" $1
  ${IfNot} ${Errors}
  ${AndIf} $1 != ""
    DetailPrint "Mendaftarkan laptop ke gateway kantor..."
    nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" gateway enroll "$1"'
  ${EndIf}

  ; Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\${PRODUCT}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\AruniControl Configurator.lnk" "$INSTDIR\veyon-configurator.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\AruniControl Master.lnk" "$INSTDIR\veyon-master.exe"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\Uninstall AruniControl.lnk" "$INSTDIR\uninstall.exe"

  ; uninstaller + Add/Remove Programs entry
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${ARP}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${ARP}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${ARP}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${ARP}" "DisplayIcon" "$INSTDIR\veyon-configurator.exe"
  WriteRegStr HKLM "${ARP}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKLM "${ARP}" "NoModify" 1
  WriteRegDWORD HKLM "${ARP}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; stop + remove the service
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service stop'
  nsExec::ExecToLog '"$INSTDIR\veyon-wcli.exe" service unregister'

  ; remove firewall rule
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="AruniControl Server"'

  RMDir /r "$SMPROGRAMS\${PRODUCT}"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${ARP}"
SectionEnd
