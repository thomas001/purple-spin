!include "MUI.nsh"
!include "pidgin-plugin.nsh"
!include "LogicLib.nsh"

Name "Purple Spin"

OutFile "Purple-Spin-installer.exe"

RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE "../COPYING"

!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"

Section "Dummy" SecDummy

  SetOutPath $INSTDIR\pixmaps\pidgin
  File /r ..\pixmaps\pidgin\*.*

  SetOutPath $INSTDIR\plugins
  File libspin.dll

  SetOutPath $INSTDIR
  File libjson-glib-1.0.dll

  WriteUninstaller "$INSTDIR\Purple-Spin-uninstaller.exe"
SectionEnd

Section "Uninstall"
SectionEnd

Function .onInit
  Push "2.7.0"
  Call CheckPidginVersion
  Pop $R0
  ${IfNot} $R0 = PIDGIN_VERSION_OK
    Call GetPidginVersion
    Pop $R0
    MessageBox MB_OK|MB_ICONSTOP "Incompatible Pidgin version found: $R0"
    Quit
  ${EndIf}

  Call GetPidginDirectory
  Pop $R0
  ${If} $R0 == ""
    MessageBox MB_OK|MB_ICONSTOP "Empty Pidgin directory"
    Quit
  ${EndIf}

  StrCpy $INSTDIR $R0
FunctionEnd