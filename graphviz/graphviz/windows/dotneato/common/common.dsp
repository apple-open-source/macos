# Microsoft Developer Studio Project File - Name="common" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=common - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "common.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "common.mak" CFG="common - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "common - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "common - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "common - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "NDEBUG" /D "HAVE_SETMODE" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy .\Release\common.lib ..\..\makearch\win32\static\Release
# End Special Build Tool

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "HAVE_SETMODE" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy .\Debug\common.lib ..\..\makearch\win32\static\Debug
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "common - Win32 Release"
# Name "common - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\arrows.c
# End Source File
# Begin Source File

SOURCE=.\colxlate.c
# End Source File
# Begin Source File

SOURCE=.\diagen.c
# End Source File
# Begin Source File

SOURCE=.\emit.c
# End Source File
# Begin Source File

SOURCE=.\figgen.c
# End Source File
# Begin Source File

SOURCE=.\fontmetrics.c
# End Source File
# Begin Source File

SOURCE=.\gdgen.c
# End Source File
# Begin Source File

SOURCE=.\globals.c
# End Source File
# Begin Source File

SOURCE=.\hpglgen.c
# End Source File
# Begin Source File

SOURCE=.\htmllex.c
# End Source File
# Begin Source File

SOURCE=.\htmlparse.c
# End Source File
# Begin Source File

SOURCE=.\htmltable.c
# End Source File
# Begin Source File

SOURCE=.\input.c

!IF  "$(CFG)" == "common - Win32 Release"

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

# ADD CPP /W4

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\mapgen.c
# End Source File
# Begin Source File

SOURCE=.\mifgen.c
# End Source File
# Begin Source File

SOURCE=.\mpgen.c
# End Source File
# Begin Source File

SOURCE=.\output.c
# End Source File
# Begin Source File

SOURCE=.\picgen.c
# End Source File
# Begin Source File

SOURCE=.\pointset.c
# End Source File
# Begin Source File

SOURCE=.\postproc.c
# End Source File
# Begin Source File

SOURCE=.\psgen.c
# End Source File
# Begin Source File

SOURCE=.\shapes.c
# End Source File
# Begin Source File

SOURCE=.\splines.c
# End Source File
# Begin Source File

SOURCE=.\strcasecmp.c
# End Source File
# Begin Source File

SOURCE=.\strncasecmp.c
# End Source File
# Begin Source File

SOURCE=.\svggen.c
# End Source File
# Begin Source File

SOURCE=.\timing.c
# End Source File
# Begin Source File

SOURCE=.\utils.c
# End Source File
# Begin Source File

SOURCE=.\vrmlgen.c
# End Source File
# Begin Source File

SOURCE=.\vtxgen.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\colortbl.h
# End Source File
# Begin Source File

SOURCE=.\const.h
# End Source File
# Begin Source File

SOURCE=.\globals.h
# End Source File
# Begin Source File

SOURCE=.\macros.h
# End Source File
# Begin Source File

SOURCE=.\ps.h
# End Source File
# Begin Source File

SOURCE=.\render.h
# End Source File
# Begin Source File

SOURCE=.\renderprocs.h
# End Source File
# Begin Source File

SOURCE=.\types.h
# End Source File
# Begin Source File

SOURCE=.\utils.h
# End Source File
# Begin Source File

SOURCE=.\xbuf.h
# End Source File
# End Group
# End Target
# End Project
