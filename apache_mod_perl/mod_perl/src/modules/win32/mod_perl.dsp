# Microsoft Developer Studio Project File - Name="mod_perl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=mod_perl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mod_perl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mod_perl.mak" CFG="mod_perl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mod_perl - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mod_perl - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mod_perl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "\Perl\lib\CORE" /D "WIN32" /D "NDEBUG" /D "_WINSOCK2API_" /D "_MSWSOCK_" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386 /out:"Release/mod_perl.so"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386 /out:"Release/mod_perl.so"

!ELSEIF  "$(CFG)" == "mod_perl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "\Perl\lib\CORE" /D "WIN32" /D "_DEBUG" /D "_WINSOCK2API_" /D "_MSWSOCK_" /D "_WINDOWS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug/mod_perl.so" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug/mod_perl.so" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mod_perl - Win32 Release"
# Name "mod_perl - Win32 Debug"
# Begin Source File

SOURCE=..\perl\Apache.c
# End Source File
# Begin Source File

SOURCE=..\perl\Connection.c
# End Source File
# Begin Source File

SOURCE=..\perl\Constants.c
# End Source File
# Begin Source File

SOURCE=..\perl\dirent.h
# End Source File
# Begin Source File

SOURCE=..\perl\File.c
# End Source File
# Begin Source File

SOURCE=..\perl\Log.c
# End Source File
# Begin Source File

SOURCE=..\perl\ModuleConfig.c
# End Source File
# Begin Source File

SOURCE=..\perl\mod_perl.c
# End Source File
# Begin Source File

SOURCE=.\mod_perl.def
# End Source File
# Begin Source File

SOURCE=..\perl\mod_perl.h
# End Source File
# Begin Source File

SOURCE=..\perl\mod_perl_opmask.c
# End Source File
# Begin Source File

SOURCE=..\perl\perl_config.c
# End Source File
# Begin Source File

SOURCE=..\perl\perl_util.c
# End Source File
# Begin Source File

SOURCE=..\perl\perlio.c
# End Source File
# Begin Source File

SOURCE=..\perl\perlxsi.c
# End Source File
# Begin Source File

SOURCE=..\perl\Server.c
# End Source File
# Begin Source File

SOURCE=..\perl\Table.c
# End Source File
# Begin Source File

SOURCE=..\perl\URI.c
# End Source File
# Begin Source File

SOURCE=..\perl\Util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Apache\ApacheCore.lib
# End Source File
# Begin Source File

SOURCE=\Perl\lib\CORE\perl56.lib
# End Source File
# End Target
# End Project
