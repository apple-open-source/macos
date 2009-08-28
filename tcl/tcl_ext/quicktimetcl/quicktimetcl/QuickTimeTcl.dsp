# Microsoft Developer Studio Project File - Name="tclqt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=tclqt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "QuickTimeTcl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "QuickTimeTcl.mak" CFG="tclqt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tclqt - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "tclqt - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "tclqt - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USE_TCL_STUBS" /D "USE_TK_STUBS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 qtmlclient.lib QTVR.lib tclstub84.lib tkstub84.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib msvcrt.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib /out:"Release/QuickTimeTcl3.1.dll"

!ELSEIF  "$(CFG)" == "tclqt - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "USE_TCL_STUBS" /D "USE_TK_STUBS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 tclstub84.lib tkstub84.lib qtmlclient.lib QTVR.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib msvcrt.lib /nologo /subsystem:windows /dll /incremental:no /debug /machine:I386 /nodefaultlib /out:"QuickTimeTcl3.1.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "tclqt - Win32 Release"
# Name "tclqt - Win32 Debug"
# Begin Source File

SOURCE=.\EffectCommand.c
# End Source File
# Begin Source File

SOURCE=.\ExportCommand.c
# End Source File
# Begin Source File

SOURCE=.\MacPorted.c
# End Source File
# Begin Source File

SOURCE=.\MatsUtils.c
# End Source File
# Begin Source File

SOURCE=.\MatsUtils.h
# End Source File
# Begin Source File

SOURCE=.\MovieCallBack.c
# End Source File
# Begin Source File

SOURCE=.\MoviePlayer.c
# End Source File
# Begin Source File

SOURCE=.\MovieQTVRUtils.c
# End Source File
# Begin Source File

SOURCE=.\MovieUtils.c
# End Source File
# Begin Source File

SOURCE=.\QuickTimeTcl.c
# End Source File
# Begin Source File

SOURCE=.\QuickTimeTcl.h
# End Source File
# Begin Source File

SOURCE=.\QuickTimeTclWin.h
# End Source File
# Begin Source File

SOURCE=.\SeqGrabber.c
# End Source File
# Begin Source File

SOURCE=.\Tfp_Arrays.c
# End Source File
# Begin Source File

SOURCE=.\Tfp_Arrays.h
# End Source File
# Begin Source File

SOURCE=.\TimeCode.c
# End Source File
# Begin Source File

SOURCE=.\TracksCommand.c
# End Source File
# Begin Source File

SOURCE=.\UserData.c
# End Source File
# Begin Source File

SOURCE=.\Utils.c
# End Source File
# End Target
# End Project
