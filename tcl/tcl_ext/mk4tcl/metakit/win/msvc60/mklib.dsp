# Microsoft Developer Studio Project File - Name="mklib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mklib - Win32 Debug STD
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mklib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mklib.mak" CFG="mklib - Win32 Debug STD"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mklib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mklib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "mklib - Win32 Debug MFC" (based on "Win32 (x86) Static Library")
!MESSAGE "mklib - Win32 Debug STD" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mklib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\builds\msvc60\mklib\Release"
# PROP Intermediate_Dir "..\..\builds\msvc60\mklib\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W4 /GX /O2 /Ob2 /I "..\..\include" /I ".." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "q4_INLINE" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\builds\mk4vc60s.lib"

!ELSEIF  "$(CFG)" == "mklib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mklib___Win32_Debug"
# PROP BASE Intermediate_Dir "mklib___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mklib\Debug"
# PROP Intermediate_Dir "..\..\builds\msvc60\mklib\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W4 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\builds\mk4vc60s_d.lib"

!ELSEIF  "$(CFG)" == "mklib - Win32 Debug MFC"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mklib___Win32_Debug_MFC"
# PROP BASE Intermediate_Dir "mklib___Win32_Debug_MFC"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mklib\Debug_MFC"
# PROP Intermediate_Dir "..\..\builds\msvc60\mklib\Debug_MFC"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "_LIB" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_MFC" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\builds\mk4vc60s_d.lib"
# ADD LIB32 /nologo /out:"..\..\builds\mk4vc60s_mfc_d.lib"

!ELSEIF  "$(CFG)" == "mklib - Win32 Debug STD"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mklib___Win32_Debug_STD"
# PROP BASE Intermediate_Dir "mklib___Win32_Debug_STD"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mklib\Debug_STD"
# PROP Intermediate_Dir "..\..\builds\msvc60\mklib\Debug_STD"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "_LIB" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_MFC" /FR /FD /GZ /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\include" /I ".." /D "_LIB" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_STD" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\builds\mk4vc60s_mfc_d.lib"
# ADD LIB32 /nologo /out:"..\..\builds\mk4vc60s_std_d.lib"

!ENDIF 

# Begin Target

# Name "mklib - Win32 Release"
# Name "mklib - Win32 Debug"
# Name "mklib - Win32 Debug MFC"
# Name "mklib - Win32 Debug STD"
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

SOURCE=..\..\src\remap.h
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
