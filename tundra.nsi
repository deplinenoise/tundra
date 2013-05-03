; Pull in Modern UI
!include "MUI2.nsh"

; The name of the installer
Name "Tundra 2.0"

SetCompressor /SOLID /FINAL lzma

; We need Vista or later -- not supported on NSIS 2.46
;TargetMinimalOS 6.0  

; The file to write
OutFile "${BUILDDIR}\Tundra-Setup.exe"

; The default installation directory
InstallDir "$PROGRAMFILES64\Tundra 2.0"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Andreas Fredriksson\Tundra 2.0" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

; MUI setup
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "doc\gpl3.rtf"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
;--------------------------------

; Pages

;--------------------------------
; The stuff to install
Section "Tundra 2.0 (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR\bin
  
  ; Put file there
  File "${BUILDDIR}\tundra2.exe"
  File "${BUILDDIR}\t2-inspect.exe"
  File "${BUILDDIR}\t2-lua.exe"

  SetOutPath $INSTDIR\doc
  File "${BUILDDIR}\tundra-manual.html"

  SetOutPath $INSTDIR\scripts
  File /r "scripts\*.lua"
  
  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\Andreas Fredriksson\Tundra 2.0" "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tundra 2.0" "DisplayName" "Tundra 2.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tundra 2.0" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tundra 2.0" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tundra 2.0" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Tundra 2.0"
  CreateShortCut "$SMPROGRAMS\Tundra 2.0\Manual.lnk" "$INSTDIR\doc\tundra-manual.html" "" "$INSTDIR\doc\tundra-manual.html" 0
  CreateShortCut "$SMPROGRAMS\Tundra 2.0\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
SectionEnd

; Optional section (can be disabled by the user)
Section "Examples"

  SetOutPath $INSTDIR\examples
  File /r "examples\*"
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tundra 2.0"
  DeleteRegKey HKLM "SOFTWARE\Andreas Fredriksson\Tundra 2.0"

  ; Remove files and uninstaller
  Delete $INSTDIR\uninstall.exe

  RMDir /r $INSTDIR\bin
  RMDir /r $INSTDIR\doc
  RMDir /r $INSTDIR\scripts
  RMDir /r $INSTDIR\examples

  RMDir $INSTDIR

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\Tundra 2.0\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\Tundra 2.0"

SectionEnd
