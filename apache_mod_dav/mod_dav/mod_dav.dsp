# Microsoft Developer Studio Project File - Name="mod_dav" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=mod_dav - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mod_dav.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mod_dav.mak" CFG="mod_dav - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mod_dav - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mod_dav - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mod_dav - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "\apache-1.3\src\include" /I "\apache-1.3\src\lib\expat-lite" /I "\apache-1.3\src\os\win32" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "SHARED_MODULE" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib \apache-1.3\src\release\apachecore.lib sdbm\release\sdbm.lib \apache-1.3\src\lib\expat-lite\Release\xmlparse.lib \apache-1.3\src\lib\expat-lite\Release\xmltok.lib /nologo /subsystem:windows /dll /machine:I386

!ELSEIF  "$(CFG)" == "mod_dav - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mod_dav_"
# PROP BASE Intermediate_Dir "mod_dav_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "\apache-1.3\src\include" /I "\apache-1.3\src\os\win32" /I "\apache-1.3\src\lib\expat-lite" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SHARED_MODULE" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib \apache-1.3\src\debug\apachecore.lib sdbm\debug\sdbm.lib \apache-1.3\src\lib\expat-lite\Debug\xmlparse.lib \apache-1.3\src\lib\expat-lite\Debug\xmltok.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "mod_dav - Win32 Release"
# Name "mod_dav - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\dav_dyn.c
# End Source File
# Begin Source File

SOURCE=.\dav_fs_dbm.c
# End Source File
# Begin Source File

SOURCE=.\dav_fs_lock.c
# End Source File
# Begin Source File

SOURCE=.\dav_fs_repos.c
# End Source File
# Begin Source File

SOURCE=.\dav_lock.c
# End Source File
# Begin Source File

SOURCE=.\dav_opaquelock.c
# End Source File
# Begin Source File

SOURCE=.\dav_props.c
# End Source File
# Begin Source File

SOURCE=.\dav_util.c
# End Source File
# Begin Source File

SOURCE=.\dav_xmlparse.c
# End Source File
# Begin Source File

SOURCE=.\mod_dav.c
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\dav_opaquelock.h
# End Source File
# Begin Source File

SOURCE=.\mod_dav.h
# End Source File
# Begin Source File

SOURCE=.\xmlparse.h
# End Source File
# End Group
# End Target
# End Project
