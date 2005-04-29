# Microsoft Developer Studio Project File - Name="gd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=gd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gd.mak" CFG="gd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gd - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "gd - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gd - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "." /I ".." /I "..\third-party\include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /YX /FD /c
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
PostBuild_Cmds=copy .\Release\gd.lib ..\makearch\win32\static\Release
# End Special Build Tool

!ELSEIF  "$(CFG)" == "gd - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "." /I ".." /I "..\third-party\include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /FR /YX /FD /GZ /c
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
PostBuild_Cmds=copy .\Debug\gd.lib ..\makearch\win32\static\Debug
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "gd - Win32 Release"
# Name "gd - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\gd.c
# End Source File
# Begin Source File

SOURCE=.\gd_gd.c
# End Source File
# Begin Source File

SOURCE=.\gd_gd2.c
# End Source File
# Begin Source File

SOURCE=.\gd_gif.c
# End Source File
# Begin Source File

SOURCE=.\gd_io.c
# End Source File
# Begin Source File

SOURCE=.\gd_io_dp.c
# End Source File
# Begin Source File

SOURCE=.\gd_io_file.c
# End Source File
# Begin Source File

SOURCE=.\gd_io_ss.c
# End Source File
# Begin Source File

SOURCE=.\gd_jpeg.c
# End Source File
# Begin Source File

SOURCE=.\gd_png.c
# End Source File
# Begin Source File

SOURCE=.\gd_ss.c
# End Source File
# Begin Source File

SOURCE=.\gd_wbmp.c
# End Source File
# Begin Source File

SOURCE=.\gdcache.c
# End Source File
# Begin Source File

SOURCE=.\gdfontg.c
# End Source File
# Begin Source File

SOURCE=.\gdfontl.c
# End Source File
# Begin Source File

SOURCE=.\gdfontmb.c
# End Source File
# Begin Source File

SOURCE=.\gdfonts.c
# End Source File
# Begin Source File

SOURCE=.\gdfontt.c
# End Source File
# Begin Source File

SOURCE=.\gdft.c
# End Source File
# Begin Source File

SOURCE=.\gdhelpers.c
# End Source File
# Begin Source File

SOURCE=.\gdkanji.c
# End Source File
# Begin Source File

SOURCE=.\gdtables.c
# End Source File
# Begin Source File

SOURCE=.\gdxpm.c
# End Source File
# Begin Source File

SOURCE=.\strtok_r.c
# End Source File
# Begin Source File

SOURCE=.\wbmp.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\gd.h
# End Source File
# Begin Source File

SOURCE=.\gd_io.h
# End Source File
# Begin Source File

SOURCE=.\gdcache.h
# End Source File
# Begin Source File

SOURCE=.\gdfontg.h
# End Source File
# Begin Source File

SOURCE=.\gdfontl.h
# End Source File
# Begin Source File

SOURCE=.\gdfontmb.h
# End Source File
# Begin Source File

SOURCE=.\gdfonts.h
# End Source File
# Begin Source File

SOURCE=.\gdfontt.h
# End Source File
# Begin Source File

SOURCE=.\jisx0208.h
# End Source File
# Begin Source File

SOURCE=.\wbmp.h
# End Source File
# End Group
# End Target
# End Project
