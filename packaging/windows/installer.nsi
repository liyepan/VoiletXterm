; VoiletXterm NSIS installer
; Called with: /DAPP_VERSION=... /DDEPLOY_DIR=... /DICON_FILE=... /DOUTPUT_FILE=...
Unicode true
!include "MUI2.nsh"

!ifndef APP_VERSION
!define APP_VERSION "0.0.0"
!endif
!ifndef DEPLOY_DIR
!error "DEPLOY_DIR is required"
!endif
!ifndef OUTPUT_FILE
!error "OUTPUT_FILE is required"
!endif

Name "VoiletXterm"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\VoiletXterm"
InstallDirRegKey HKLM "Software\VoiletXterm" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "VoiletXterm ${APP_VERSION}"

!ifdef ICON_FILE
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!endif
!define MUI_FINISHPAGE_RUN "$INSTDIR\voiletxterm.exe"
!define MUI_FINISHPAGE_RUN_TEXT "启动 VoiletXterm"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VoiletXterm"

Section "VoiletXterm" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "${DEPLOY_DIR}\*.*"

    WriteUninstaller "$INSTDIR\uninstall.exe"
    CreateShortcut "$SMPROGRAMS\VoiletXterm.lnk" "$INSTDIR\voiletxterm.exe"
    CreateShortcut "$DESKTOP\VoiletXterm.lnk" "$INSTDIR\voiletxterm.exe"

    WriteRegStr HKLM "Software\VoiletXterm" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "VoiletXterm"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "liyepan"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\voiletxterm.exe"
    WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\VoiletXterm.lnk"
    Delete "$DESKTOP\VoiletXterm.lnk"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "${UNINST_KEY}"
    DeleteRegKey HKLM "Software\VoiletXterm"
SectionEnd
