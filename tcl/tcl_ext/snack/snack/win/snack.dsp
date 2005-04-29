# Microsoft Developer Studio Project File - Name="snack" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=snack - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "snack.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "snack.mak" CFG="snack - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "snack - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "snack - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "snack - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SNACK_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "C:\Tcl\include" /I "../generic" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SNACK_EXPORTS" /D "WIN" /D "USE_TCL_STUBS" /D "USE_TK_STUBS" /D "TCL_81_API" /D "BUILD_snack" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 tclstub84.lib tkstub84.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"C:\Tcl\lib\snack2.2\libsnack.dll" /libpath:"C:\Tcl\lib"

!ELSEIF  "$(CFG)" == "snack - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SNACK_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "C:\Tcl\include" /I "../generic" /D "_DEBUG" /D "WIN" /D "USE_TCL_STUBS" /D "USE_TK_STUBS" /D "TCL_81_API" /D "BUILD_snack" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SNACK_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib tclstub84.lib tkstub84.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /out:"C:\Tcl\lib\snack2.2\libsnack.dll" /pdbtype:sept /libpath:"C:\Tcl\lib"

!ENDIF 

# Begin Target

# Name "snack - Win32 Release"
# Name "snack - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\generic\ffa.c
# End Source File
# Begin Source File

SOURCE=..\generic\g711.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkAudio.c
# End Source File
# Begin Source File

SOURCE=.\jkAudIO_win.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkCanvSect.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkCanvSpeg.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkCanvWave.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkFilter.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkFilterIIR.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkFormatMP3.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkMixer.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkPitchCmd.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSound.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSoundEdit.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSoundEngine.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSoundFile.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSoundProc.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkSynthesis.c
# End Source File
# Begin Source File

SOURCE=..\generic\shape.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkGetF0.c
# End Source File
# Begin Source File

SOURCE=..\generic\jkFormant.c
# End Source File
# Begin Source File

SOURCE=..\generic\sigproc.c
# End Source File
# Begin Source File

SOURCE=..\generic\sigproc2.c
# End Source File
# Begin Source File

SOURCE=..\generic\snack.c
# End Source File
# Begin Source File

SOURCE=.\snack.def
# End Source File
# Begin Source File

SOURCE=..\generic\snackStubInit.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\generic\jkAudIO.h
# End Source File
# Begin Source File

SOURCE=..\generic\jkCanvItems.h
# End Source File
# Begin Source File

SOURCE=..\generic\jkFormatMP3.h
# End Source File
# Begin Source File

SOURCE=..\generic\jkSound.h
# End Source File
# Begin Source File

SOURCE=..\generic\snack.h
# End Source File
# Begin Source File

SOURCE=..\generic\snackDecls.h
# End Source File
# Begin Source File

SOURCE=..\generic\jkFormant.h
# End Source File
# Begin Source File

SOURCE=..\generic\jkGetF0.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
