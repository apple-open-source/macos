# Microsoft Developer Studio Project File - Name="mkdll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=mkdll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mkdll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mkdll.mak" CFG="mkdll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mkdll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mkdll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mkdll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\builds"
# PROP Intermediate_Dir "..\..\builds\msvc60\mkdll\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKDLL_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O1 /Ob2 /I "..\..\include" /I ".." /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKDLL_EXPORTS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /machine:I386 /out:"..\..\builds\mk4vc60.dll"

!ELSEIF  "$(CFG)" == "mkdll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds"
# PROP Intermediate_Dir "..\..\builds\msvc60\mkdll\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKDLL_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W4 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKDLL_EXPORTS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\builds\mk4vc60_d.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mkdll - Win32 Release"
# Name "mkdll - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\column.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\custom.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\derived.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\field.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\fileio.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\format.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\handler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\persist.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\remap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\std.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\store.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\string.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\table.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\univ.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\view.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\viewx.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\column.h
# End Source File
# Begin Source File

SOURCE=..\..\src\column.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\custom.h
# End Source File
# Begin Source File

SOURCE=..\..\src\derived.h
# End Source File
# Begin Source File

SOURCE=..\..\src\field.h
# End Source File
# Begin Source File

SOURCE=..\..\src\field.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\format.h
# End Source File
# Begin Source File

SOURCE=..\..\src\handler.h
# End Source File
# Begin Source File

SOURCE=..\..\src\handler.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\header.h
# End Source File
# Begin Source File

SOURCE=..\..\src\mfc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4dll.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4io.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4str.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mk4str.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\msvc.h
# End Source File
# Begin Source File

SOURCE=..\..\src\persist.h
# End Source File
# Begin Source File

SOURCE=..\..\src\std.h
# End Source File
# Begin Source File

SOURCE=..\..\src\store.h
# End Source File
# Begin Source File

SOURCE=..\..\src\store.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\univ.h
# End Source File
# Begin Source File

SOURCE=..\..\src\univ.inl
# End Source File
# Begin Source File

SOURCE=..\..\src\win.h
# End Source File
# End Group
# End Target
# End Project
