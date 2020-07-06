;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; secondlife setup.nsi
;; Copyright 2004-2011, Linden Research, Inc.
;; Copyright 2013-2015 Alchemy Viewer Project
;;
;; This library is free software; you can redistribute it and/or
;; modify it under the terms of the GNU Lesser General Public
;; License as published by the Free Software Foundation;
;; version 2.1 of the License only.
;;
;; This library is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; Lesser General Public License for more details.
;;
;; You should have received a copy of the GNU Lesser General Public
;; License along with this library; if not, write to the Free Software
;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
;;
;; Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
;;
;; NSIS 3 or higher required for Unicode support
;;
;; Author: James Cook, Don Kjer, Callum Prentice, Drake Arconis
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;--------------------------------
;Unicode
  Unicode true

;--------------------------------
;Include Modern UI

  !include "LogicLib.nsh"
  !include "StdUtils.nsh"
  !include "FileFunc.nsh"
  !insertmacro GetParameters
  !insertmacro GetOptions
  !include "x64.nsh"
  !include "WinVer.nsh"
  !include "MUI2.nsh"

;-------------------------------
;Global Variables
  ; These will be replaced by manifest scripts
  %%INST_VARS%%
  %%WIN64_BIN_BUILD%%

  Var INSTEXE
  Var INSTPROG
  Var INSTSHORTCUT
  Var AUTOSTART
  Var COMMANDLINE         ; command line passed to this installer, set in .onInit
  Var SHORTCUT_LANG_PARAM ; "--set InstallLanguage de", passes language to viewer
  Var SKIP_DIALOGS        ; set from command line in  .onInit. autoinstall 
                          ; GUI and the defaults.
  Var SKIP_AUTORUN        ; skip automatic launch of viewer after install
  Var STARTMENUFOLDER

;--------------------------------
;Registry Keys
  !define ALCHEMY_KEY      "SOFTWARE\${VENDORSTR}"
  !define INSTNAME_KEY    "${ALCHEMY_KEY}\${APPNAMEONEWORD}"
  !define MSCURRVER_KEY   "SOFTWARE\Microsoft\Windows\CurrentVersion"
  !define MSNTCURRVER_KEY "SOFTWARE\Microsoft\Windows NT\CurrentVersion"
  !define MSUNINSTALL_KEY "${MSCURRVER_KEY}\Uninstall\${APPNAMEONEWORD}"

;--------------------------------
;General

  ;Name and file
  Name "${APPNAME}"
  OutFile "${INSTOUTFILE}"
  Caption "${CAPTIONSTR}"
  BrandingText "${VENDORSTR}"

  ;Default installation folder
!ifdef WIN64_BIN_BUILD
  InstallDir "$PROGRAMFILES64\${APPNAMEONEWORD}"
!else
  InstallDir "$PROGRAMFILES\${APPNAMEONEWORD}"
!endif

  ;Get installation folder from registry if available and 32bit otherwise do it in init
!ifndef WIN64_BIN_BUILD
  InstallDirRegKey HKLM "${INSTNAME_KEY}" ""
!endif

  ;Request application privileges for Windows UAC
  RequestExecutionLevel admin
  
  ;Compression
  SetCompress auto			; compress to saves space
  SetCompressor /solid /final lzma	; compress whole installer as one block
  
  ;File Handling
  SetOverwrite on

;--------------------------------
;Interface Settings

  ;Show Details
  ShowInstDetails hide
  ShowUninstDetails hide

  !define MUI_ICON "%%SOURCE%%\installers\windows\install_icon.ico"
  !define MUI_UNICON "%%SOURCE%%\installers\windows\uninstall_icon.ico"
  !define MUI_WELCOMEFINISHPAGE_BITMAP "%%SOURCE%%\installers\windows\install_welcome.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "%%SOURCE%%\installers\windows\uninstall_welcome.bmp"
  !define MUI_ABORTWARNING

;--------------------------------
;Language Selection Dialog Settings

  ;Show all languages, despite user's codepage
  !define MUI_LANGDLL_ALLLANGUAGES

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKLM" 
  !define MUI_LANGDLL_REGISTRY_KEY "${INSTNAME_KEY}" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
  
  ;Always show the dialog
  !define MUI_LANGDLL_ALWAYSSHOW

;--------------------------------
;Install Pages

  !define MUI_PAGE_CUSTOMFUNCTION_PRE check_skip
  !insertmacro MUI_PAGE_WELCOME
  
  ;License Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE check_skip
  !insertmacro MUI_PAGE_LICENSE "%%SOURCE%%\..\..\doc\GPL-license.txt"

  ;Directory Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE check_skip
  !insertmacro MUI_PAGE_DIRECTORY

  ;Start Menu Folder Page
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "${INSTNAME_KEY}" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !define MUI_PAGE_CUSTOMFUNCTION_PRE check_skip
!ifdef WIN64_BIN_BUILD
  !define MUI_STARTMENUPAGE_DEFAULTFOLDER "${APPNAME} (64 bit) Viewer"
!else
  !define MUI_STARTMENUPAGE_DEFAULTFOLDER "${APPNAME} Viewer"
!endif
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENUFOLDER

  ;Install Progress Page
  !define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckWindowsServPack
  !insertmacro MUI_PAGE_INSTFILES

  ; Finish Page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE check_skip_finish
  !define MUI_FINISHPAGE_RUN
  !define MUI_FINISHPAGE_RUN_FUNCTION launch_viewer
  !define MUI_FINISHPAGE_SHOWREADME
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop Shortcut"
  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
  !define MUI_FINISHPAGE_SHOWREADME_FUNCTION create_desktop_shortcut
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT
  !insertmacro MUI_PAGE_FINISH

;--------------------------------
;Uninstall Pages

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !include "%%SOURCE%%\installers\windows\lang_en-us.nsi"
  !include "%%SOURCE%%\installers\windows\lang_de.nsi"
  !include "%%SOURCE%%\installers\windows\lang_es.nsi"
  !include "%%SOURCE%%\installers\windows\lang_fr.nsi"
  !include "%%SOURCE%%\installers\windows\lang_ja.nsi"
  !include "%%SOURCE%%\installers\windows\lang_pl.nsi"
  !include "%%SOURCE%%\installers\windows\lang_it.nsi"
  !include "%%SOURCE%%\installers\windows\lang_pt-br.nsi"
  !include "%%SOURCE%%\installers\windows\lang_da.nsi"
  !include "%%SOURCE%%\installers\windows\lang_ru.nsi"
  !include "%%SOURCE%%\installers\windows\lang_tr.nsi"
  !include "%%SOURCE%%\installers\windows\lang_zh.nsi"

;--------------------------------
;Version Information

  VIProductVersion "${VERSION_LONG}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "Singularity Viewer Installer"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "A viewer for the meta-verse!"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${VENDORSTR}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright © 2010-2020, ${VENDORSTR}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${APPNAME} Installer"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION_LONG}"
  VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION_LONG}"

;--------------------------------
;Reserve Files
  
  ;If you are using solid compression, files that are required before
  ;the actual installation should be stored first in the data block,
  ;because this will make your installer start faster.
  
  !insertmacro MUI_RESERVEFILE_LANGDLL
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\INetC.dll"
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\nsDialogs.dll"
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\StartMenu.dll"
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\StdUtils.dll"
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\System.dll"
  ReserveFile "${NSISDIR}\Plugins\x86-unicode\UserInfo.dll"

;--------------------------------
; Local Functions

;Page pre-checks for skip conditions
Function check_skip
  StrCmp $SKIP_DIALOGS "true" 0 +2
  Abort
FunctionEnd

Function check_skip_finish
  StrCmp $SKIP_DIALOGS "true" 0 +4
  StrCmp $AUTOSTART "true" 0 +3
  Call launch_viewer
  Abort
FunctionEnd

Function launch_viewer
  ${StdUtils.ExecShellAsUser} $0 "$INSTDIR\$INSTEXE" "open" "$SHORTCUT_LANG_PARAM"
FunctionEnd

Function create_desktop_shortcut
!ifdef WIN64_BIN_BUILD
  CreateShortCut "$DESKTOP\$INSTSHORTCUT x64.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM" "$INSTDIR\$INSTEXE"
!else
  CreateShortCut "$DESKTOP\$INSTSHORTCUT.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM" "$INSTDIR\$INSTEXE"
!endif
FunctionEnd

;Check version compatibility
Function CheckWindowsVersion
!ifdef WIN64_BIN_BUILD
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This version requires a 64 bit operating system."
    Quit
  ${EndIf}
!endif

  ${If} ${AtMostWinVista}
    MessageBox MB_OK $(CheckWindowsVersionMB)
    Quit
  ${EndIf}
FunctionEnd

;Check service pack compatibility and suggest upgrade
Function CheckWindowsServPack
  ${If} ${IsWin7}
  ${AndIfNot} ${IsServicePack} 1
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}

  ${If} ${IsWin2008R2}
  ${AndIfNot} ${IsServicePack} 1
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}
FunctionEnd

;Make sure the user can install/uninstall
Function CheckIfAdministrator
  DetailPrint $(CheckAdministratorInstDP)
  UserInfo::GetAccountType
  Pop $R0
  StrCmp $R0 "Admin" lbl_is_admin
    MessageBox MB_OK $(CheckAdministratorInstMB)
    Quit
lbl_is_admin:
  Return
FunctionEnd

Function un.CheckIfAdministrator
  DetailPrint $(CheckAdministratorUnInstDP)
  UserInfo::GetAccountType
  Pop $R0
  StrCmp $R0 "Admin" lbl_is_admin
    MessageBox MB_OK $(CheckAdministratorUnInstMB)
    Quit
lbl_is_admin:
  Return
FunctionEnd

;Checks for CPU compatibility
Function CheckCPUFlags
  Push $1
  System::Call 'kernel32::IsProcessorFeaturePresent(i) i(10) .r1'
  IntCmp $1 1 OK_SSE2
  MessageBox MB_OKCANCEL $(MissingSSE2) /SD IDOK IDOK OK_SSE2
  Quit

  OK_SSE2:
  Pop $1
  Return
FunctionEnd

;Checks if installed version is same as installer and offers to cancel
Function CheckIfAlreadyCurrent
!ifdef WIN64_BIN_BUILD
  SetRegView 64
!endif
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\${VENDORSTR}\$INSTPROG" "Version"
  StrCmp $0 ${VERSION_LONG} 0 continue_install
  StrCmp $SKIP_DIALOGS "true" continue_install
  MessageBox MB_OKCANCEL $(CheckIfCurrentMB) /SD IDOK IDOK continue_install
  Quit
continue_install:
  Pop $0
  Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of install process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CloseSecondLife
  Push $0
  FindWindow $0 "Second Life" ""
  IntCmp $0 0 DONE
  
  StrCmp $SKIP_DIALOGS "true" CLOSE
    MessageBox MB_YESNOCANCEL $(CloseSecondLifeInstMB) IDYES CLOSE IDNO DONE

;  CANCEL_INSTALL:
  Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeInstDP)
    SendMessage $0 16 0 0

  LOOP:
	FindWindow $0 "Second Life" ""
	IntCmp $0 0 DONE
	Sleep 500
	Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of uninstall process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CloseSecondLife
  Push $0
  FindWindow $0 "Second Life" ""
  IntCmp $0 0 DONE
  MessageBox MB_OKCANCEL $(CloseSecondLifeUnInstMB) IDOK CLOSE IDCANCEL CANCEL_UNINSTALL

  CANCEL_UNINSTALL:
  Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeUnInstDP)
    SendMessage $0 16 0 0

  LOOP:
    FindWindow $0 "Second Life" ""
    IntCmp $0 0 DONE
    Sleep 500
    Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd



;--------------------------------
;Installer Sections

Section "Viewer"
  SectionIn RO
  SetShellVarContext all
!ifdef WIN64_BIN_BUILD
  SetRegView 64
!endif
  ;Start with some default values.
  StrCpy $INSTPROG "${APPNAMEONEWORD}"
  StrCpy $INSTEXE "${INSTEXE}"
  StrCpy $INSTSHORTCUT "${APPNAME}"

  Call CheckIfAlreadyCurrent
  Call CloseSecondLife			    ; Make sure we're not running

  SetOutPath "$INSTDIR"  
  ;Remove old shader files first so fallbacks will work.
  RMDir /r "$INSTDIR\app_settings\shaders\*"
  
  ;This placeholder is replaced by the complete list of all the files in the installer, by viewer_manifest.py
  %%INSTALL_FILES%%

  ;Create temp dir and set out dir to it
  CreateDirectory "$TEMP\AlchemyInst"
  SetOutPath "$TEMP\AlchemyInst"

  ;Download LibVLC
!ifdef WIN64_BIN_BUILD
  inetc::get /RESUME "Failed to download VLC media package. Retry?" "https://videolan.mirrors.hivelocity.net/vlc/3.0.8/win64/vlc-3.0.8-win64.7z" "$TEMP\AlchemyInst\libvlc.7z" /END
!else
  inetc::get /RESUME "Failed to download VLC media package. Retry?" "https://videolan.mirrors.hivelocity.net/vlc/3.0.8/win32/vlc-3.0.8-win32.7z" "$TEMP\AlchemyInst\libvlc.7z" /END
!endif
  Nsis7z::ExtractWithDetails "$TEMP\AlchemyInst\libvlc.7z" "Unpacking media plugins %s..."
  Rename "$TEMP\AlchemyInst\vlc-3.0.8\libvlc.dll" "$INSTDIR\llplugin\libvlc.dll"
  Rename "$TEMP\AlchemyInst\vlc-3.0.8\libvlccore.dll" "$INSTDIR\llplugin\libvlccore.dll"
  Rename "$TEMP\AlchemyInst\vlc-3.0.8\plugins" "$INSTDIR\llplugin\plugins"

  ;Download and install VC redist
!ifdef WIN64_BIN_BUILD
  inetc::get /RESUME "Failed to download VS2019 redistributable package. Retry?" "https://aka.ms/vs/16/release/vc_redist.x64.exe" "$TEMP\AlchemyInst\vc_redist_16.x64.exe" /END
  ExecWait "$TEMP\AlchemyInst\vc_redist_16.x64.exe /install /passive /norestart"

  inetc::get /RESUME "Failed to download VS2013 redistributable package. Retry?" "https://aka.ms/highdpimfc2013x64enu" "$TEMP\AlchemyInst\vc_redist_12.x64.exe" /END
  ExecWait "$TEMP\AlchemyInst\vc_redist_12.x64.exe /install /passive /norestart"
!else
  inetc::get /RESUME "Failed to download VS2019 redistributable package. Retry?" "https://aka.ms/vs/16/release/vc_redist.x86.exe" "$TEMP\AlchemyInst\vc_redist_16.x86.exe" /END
  ExecWait "$TEMP\AlchemyInst\vc_redist_16.x86.exe /install /passive /norestart"

  inetc::get /RESUME "Failed to download VS2013 redistributable package. Retry?" "https://aka.ms/highdpimfc2013x86enu" "$TEMP\AlchemyInst\vc_redist_12.x86.exe" /END
  ExecWait "$TEMP\AlchemyInst\vc_redist_12.x86.exe /install /passive /norestart"
!endif

  ;Remove temp dir and reset out to inst dir
  RMDir /r "$TEMP\AlchemyInst\"
  SetOutPath "$INSTDIR"

  ;Pass the installer's language to the client to use as a default
  StrCpy $SHORTCUT_LANG_PARAM "--set InstallLanguage $(LanguageCode)"
  
  ;Create startmenu shortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    CreateDirectory "$SMPROGRAMS\$STARTMENUFOLDER"
!ifdef WIN64_BIN_BUILD
    CreateShortCut	"$SMPROGRAMS\$STARTMENUFOLDER\$INSTSHORTCUT (64 bit) Viewer.lnk" "$\"$INSTDIR\$INSTEXE$\"" "$SHORTCUT_LANG_PARAM"
    CreateShortCut	"$SMPROGRAMS\$STARTMENUFOLDER\Uninstall $INSTSHORTCUT (64 bit) Viewer.lnk" "$\"$INSTDIR\uninst.exe$\"" ""
!else
    CreateShortCut	"$SMPROGRAMS\$STARTMENUFOLDER\$INSTSHORTCUT.lnk" "$\"$INSTDIR\$INSTEXE$\"" "$SHORTCUT_LANG_PARAM"
    CreateShortCut	"$SMPROGRAMS\$STARTMENUFOLDER\Uninstall $INSTSHORTCUT.lnk" "$\"$INSTDIR\uninst.exe$\"" ""
!endif
    WriteINIStr		"$SMPROGRAMS\$STARTMENUFOLDER\SL Create Account.url" "InternetShortcut" "URL" "http://join.secondlife.com/"
    WriteINIStr		"$SMPROGRAMS\$STARTMENUFOLDER\SL Your Account.url"	"InternetShortcut" "URL" "http://www.secondlife.com/account/"
    WriteINIStr		"$SMPROGRAMS\$STARTMENUFOLDER\SL Scripting Language Help.url" "InternetShortcut" "URL" "http://wiki.secondlife.com/wiki/LSL_Portal"

  !insertmacro MUI_STARTMENU_WRITE_END

  ;Other shortcuts
  SetOutPath "$INSTDIR"
!ifdef WIN64_BIN_BUILD
  ;CreateShortCut "$DESKTOP\$INSTSHORTCUT.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
  CreateShortCut "$INSTDIR\$INSTSHORTCUT (64 bit) Viewer.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
  CreateShortCut "$INSTDIR\$INSTSHORTCUT (64 bit) Viewer Portable.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM --portable"
  CreateShortCut "$INSTDIR\Uninstall $INSTSHORTCUT (64 bit) Viewer.lnk" "$INSTDIR\uninst.exe" ""
!else
  ;CreateShortCut "$DESKTOP\$INSTSHORTCUT.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
  CreateShortCut "$INSTDIR\$INSTSHORTCUT Viewer.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
  CreateShortCut "$INSTDIR\$INSTSHORTCUT Viewer Portable.lnk" "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM --portable"
  CreateShortCut "$INSTDIR\Uninstall $INSTSHORTCUT Viewer.lnk" "$INSTDIR\uninst.exe" ""
!endif
    
  ;Write registry
  WriteRegStr HKLM "${INSTNAME_KEY}" "" "$INSTDIR"
  WriteRegStr HKLM "${INSTNAME_KEY}" "Version" "${VERSION_LONG}"
  WriteRegStr HKLM "${INSTNAME_KEY}" "Shortcut" "$INSTSHORTCUT"
  WriteRegStr HKLM "${INSTNAME_KEY}" "Exe" "$INSTEXE"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "Comments" "A viewer for the meta-verse!"
!ifdef WIN64_BIN_BUILD
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "DisplayName" "$INSTSHORTCUT (64 bit) Viewer"
!else
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "DisplayName" "$INSTSHORTCUT Viewer"
!endif
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\$INSTEXE"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "DisplayVersion" "${VERSION_LONG}"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "InstallSource" "$EXEDIR\"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "HelpLink" "http://www.singularityviewer.org"
  WriteRegDWORD HKLM "${MSUNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${MSUNINSTALL_KEY}" "NoRepair" 1
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "Publisher" "${VENDORSTR}"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "URLInfoAbout" "http://www.singularityviewer.org"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "URLUpdateInfo" "http://www.singularityviewer.org/downloads"
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "UninstallString" "$\"$INSTDIR\uninst.exe$\""
  WriteRegStr HKLM "${MSUNINSTALL_KEY}" "QuietUninstallString" "$\"$INSTDIR\uninst.exe$\" /S"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "EstimatedSize" "$0"


  ;Write URL registry info
  DeleteRegKey HKEY_CLASSES_ROOT "${URLNAME}"
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "" "URL:Second Life"
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "URL Protocol" ""
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\DefaultIcon" "" "$INSTDIR\$INSTEXE"
  ;; URL param must be last item passed to viewer, it ignores subsequent params
  ;; to avoid parameter injection attacks.
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\shell" "" "open"
!ifdef WIN64_BIN_BUILD
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\shell\open" "FriendlyAppName" "$INSTSHORTCUT (64 bit) Viewer"
!else
  WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\shell\open" "FriendlyAppName" "$INSTSHORTCUT Viewer"
!endif
  WriteRegExpandStr HKEY_CLASSES_ROOT "${URLNAME}\shell\open\command" "" "$\"$INSTDIR\$INSTEXE$\" -url $\"%1$\""

  DeleteRegKey HKEY_CLASSES_ROOT "x-grid-info"
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info" "" "URL:Hypergrid"
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info" "URL Protocol" ""
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info\DefaultIcon" "" "$INSTDIR\$INSTEXE"
  ;; URL param must be last item passed to viewer, it ignores subsequent params
  ;; to avoid parameter injection attacks.
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info\shell" "" "open"
!ifdef WIN64_BIN_BUILD
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info\shell\open" "FriendlyAppName" "$INSTSHORTCUT (64 bit) Viewer"
!else
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-info\shell\open" "FriendlyAppName" "$INSTSHORTCUT Viewer"
!endif
  WriteRegExpandStr HKEY_CLASSES_ROOT "x-grid-info\shell\open\command" "" "$\"$INSTDIR\$INSTEXE$\" -url $\"%1$\""

  DeleteRegKey HKEY_CLASSES_ROOT "x-grid-location-info"
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info" "" "URL:Hypergrid legacy"
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info" "URL Protocol" ""
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\DefaultIcon" "" "$\"$INSTDIR\$INSTEXE$\""

  ;; URL param must be last item passed to viewer, it ignores subsequent params
  ;; to avoid parameter injection attacks.
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\shell" "" "open"
!ifdef WIN64_BIN_BUILD
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\shell\open" "FriendlyAppName" "$INSTSHORTCUT (64 bit) Viewer"
!else
  WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\shell\open" "FriendlyAppName" "$INSTSHORTCUT Viewer"
!endif
  WriteRegExpandStr HKEY_CLASSES_ROOT "x-grid-location-info\shell\open\command" "" "$\"$INSTDIR\$INSTEXE$\" -url $\"%1$\""
  
  ;Create uninstaller
  SetOutPath "$INSTDIR"  
  WriteUninstaller "$INSTDIR\uninst.exe"
  
SectionEnd

;--------------------------------
;Installer Functions
Function .onInit
!ifdef WIN64_BIN_BUILD
  SetRegView 64
!endif
  ;Don't install on unsupported operating systems
  Call CheckWindowsVersion
  ;Don't install if not administator
  Call CheckIfAdministrator
  ;Don't install if we lack required cpu support
  Call CheckCPUFlags

  Push $0

  ;Get installation folder from registry if available for 64bit
!ifdef WIN64_BIN_BUILD
  ReadRegStr $0 HKLM "SOFTWARE\${VENDORSTR}\${APPNAMEONEWORD}" ""
  IfErrors +2 0 ; If error jump past setting SKIP_AUTORUN
  StrCpy $INSTDIR $0
!endif

  ${GetParameters} $COMMANDLINE              ; get our command line

  ${GetOptions} $COMMANDLINE "/SKIP_DIALOGS" $0   
  IfErrors +2 0 ; If error jump past setting SKIP_DIALOGS
  StrCpy $SKIP_DIALOGS "true"

  ${GetOptions} $COMMANDLINE "/SKIP_AUTORUN" $0
  IfErrors +2 0 ; If error jump past setting SKIP_AUTORUN
  StrCpy $SKIP_AUTORUN "true"
  
  ${GetOptions} $COMMANDLINE "/AUTOSTART" $0
  IfErrors +2 0 ; If error jump past setting AUTOSTART
  StrCpy $AUTOSTART "true"


  ${GetOptions} $COMMANDLINE "/LANGID=" $0   ; /LANGID=1033 implies US English
  ; If no language (error), then proceed
  IfErrors lbl_configure_default_lang
  ; No error means we got a language, so use it
  StrCpy $LANGUAGE $0
  Goto lbl_return
  
lbl_configure_default_lang:
  ;For silent installs, no language prompt, use default
  IfSilent lbl_return
  StrCmp $SKIP_DIALOGS "true" lbl_return
 
  !insertmacro MUI_LANGDLL_DISPLAY

lbl_return:
  Pop $0
  Return
FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  SectionIn RO
  SetShellVarContext all
!ifdef WIN64_BIN_BUILD
  SetRegView 64
!endif
  
  StrCpy $INSTPROG "${APPNAMEONEWORD}"
  StrCpy $INSTSHORTCUT "${APPNAME}"

  Call un.CloseSecondLife
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $STARTMENUFOLDER
  RMDir /r "$SMPROGRAMS\$STARTMENUFOLDER"
  
  ;This placeholder is replaced by the complete list of all the files in the installer, by viewer_manifest.py
  %%DELETE_FILES%%

  ;Optional/obsolete files.  Delete won't fail if they don't exist.
  Delete "$INSTDIR\message_template.msg"
  Delete "$INSTDIR\VivoxVoiceService-*.log"

  ;Shortcuts in install directory
!ifdef WIN64_BIN_BUILD
  Delete "$INSTDIR\$INSTSHORTCUT (64 bit) Viewer.lnk"
  Delete "$INSTDIR\$INSTSHORTCUT (64 bit) Viewer Portable.lnk"
  Delete "$INSTDIR\Uninstall $INSTSHORTCUT (64 bit) Viewer.lnk"
!else
  Delete "$INSTDIR\$INSTSHORTCUT Viewer.lnk"
  Delete "$INSTDIR\$INSTSHORTCUT Viewer Portable.lnk"
  Delete "$INSTDIR\Uninstall $INSTSHORTCUT Viewer.lnk"
!endif
  
  Delete "$INSTDIR\uninst.exe"
  RMDir "$INSTDIR"
  
  IfFileExists "$INSTDIR" FOLDERFOUND NOFOLDER

FOLDERFOUND:
  ;Silent uninstall always removes all files (/SD IDYES)
  MessageBox MB_YESNO $(DeleteProgramFilesMB) /SD IDYES IDNO NOFOLDER
  RMDir /r "$INSTDIR"

NOFOLDER:
  DeleteRegKey HKLM "SOFTWARE\${VENDORSTR}\$INSTPROG"
  DeleteRegKey /ifempty HKLM "SOFTWARE\${VENDORSTR}"
  DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG"
 
SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit
!ifdef WIN64_BIN_BUILD
  SetRegView 64
!endif
  Call un.CheckIfAdministrator

  !insertmacro MUI_UNGETLANGUAGE
  
FunctionEnd
