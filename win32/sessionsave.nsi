; sessionsave.nsi
;
; This script will generate an installer that installs a pidgin plug-in.

;--------------------------------

; Uncomment the next line to enable auto Pidgin download
; !define PIDGIN_AUTOINSTALL

; The name of the installer
Name "SessionSave Pidgin Plugin"

; The file to write
OutFile "sessionsave.exe"

; The default installation directory
InstallDir $PROGRAMFILES\pidgin\

; detect pidgin path from uninstall string if available
InstallDirRegKey HKLM \
	"Software\Microsoft\Windows\CurrentVersion\Uninstall\Pidgin" \
	"UninstallString"

; automatically close the installer when done.
AutoCloseWindow true

; hide the "show details" box
ShowInstDetails nevershow

;--------------------------------

; The stuff to install

Section ""

!ifdef PIDGIN_AUTOINSTALL
  Call MakeSureIGotPidgin
!endif

  SetOutPath $INSTDIR\Plugins

  ; File to extract
  File "sessionsave.dll"

  ; prompt user, and if they select yes, go to YesPidginInstall
  MessageBox MB_OK|MB_ICONINFORMATION \
             "SessionSave Pidgin plugin installed in $INSTDIR\plugins. Go to Tools->Preferences->Plugins from Pidgin to configure it."
SectionEnd

;--------------------------------

Function .onVerifyInstDir

!ifndef PIDGIN_AUTOINSTALL

  ;Check for Pidgin installation

  IfFileExists $INSTDIR\pidgin.exe Good
    Abort
  Good:

!endif ; PIDGIN_AUTOINSTALL

FunctionEnd

!ifdef PIDGIN_AUTOINSTALL

Function GetPidginInstPath

  Push $0
  Push $1
  Push $2
  ReadRegStr $0 HKLM \
	"Software\Microsoft\Windows\CurrentVersion\Uninstall\Pidgin" \
	"UninstallString"
  StrCmp $0 "" fin

    StrCpy $1 $0 1 0 ; get firstchar
    StrCmp $1 '"' "" getparent
      ; if first char is ", let's remove "'s first.
      StrCpy $0 $0 "" 1
      StrCpy $1 0
      rqloop:
        StrCpy $2 $0 1 $1
        StrCmp $2 '"' rqdone
        StrCmp $2 "" rqdone
        IntOp $1 $1 + 1
        Goto rqloop
      rqdone:
      StrCpy $0 $0 $1
    getparent:
    ; the uninstall string goes to an EXE, let's get the directory.
    StrCpy $1 -1
    gploop:
      StrCpy $2 $0 1 $1
      StrCmp $2 "" gpexit
      StrCmp $2 "\" gpexit
      IntOp $1 $1 - 1
      Goto gploop
    gpexit:
    StrCpy $0 $0 $1

    StrCmp $0 "" fin
    IfFileExists $0\pidgin.exe fin
      StrCpy $0 ""
  fin:
  Pop $2
  Pop $1
  Exch $0

FunctionEnd


Function MakeSureIGotPidgin

  Call GetPidginInstPath

  Pop $0
  StrCmp $0 "" getpidgin
    Return

  getpidgin:

  ; prompt user, and if they select yes, go to YesPidginInstall
  MessageBox MB_YESNO|MB_ICONQUESTION \
             "It appears you don't have pidgin installed. Would you like me to download and install it?" \
             IDYES YesPidginInstall
	Abort
  YesPidginInstall:

  Call ConnectInternet ;Make an internet connection (if no connection available)

  StrCpy $2 "$TEMP\pidgin-installer.exe"
  NSISdl::download http://easynews.dl.sourceforge.net/sourceforge/pidgin/pidgin-0.65.exe $2
  Pop $0
  StrCmp $0 success success
    SetDetailsView show
    DetailPrint "download failed: $0"
    Abort
  success:
    ExecWait '"$2" /S'
    Delete $2
    Call GetPidginInstPath
    Pop $0
    StrCmp $0 "" skip
    StrCpy $INSTDIR $0
  skip:

FunctionEnd

Function ConnectInternet

  ClearErrors
  Dialer::AttemptConnect
  IfErrors noie3

  Pop $R0
  StrCmp $R0 "online" connected
    MessageBox MB_OK|MB_ICONSTOP "Cannot connect to the internet."
    Quit

  noie3:

  ; IE3 not installed
  MessageBox MB_OK|MB_ICONINFORMATION "Please connect to the internet now."

  connected:

FunctionEnd

!endif ; PIDGIN_AUTOINSTALL
