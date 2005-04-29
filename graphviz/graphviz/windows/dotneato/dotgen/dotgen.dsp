# Microsoft Developer Studio Project File - Name="dotgen" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=dotgen - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "dotgen.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dotgen.mak" CFG="dotgen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dotgen - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "dotgen - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "dotgen - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /I "..\gvrender" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /YX /FD /c
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
PostBuild_Cmds=copy .\Release\dotgen.lib ..\..\makearch\win32\static\Release
# End Special Build Tool

!ELSEIF  "$(CFG)" == "dotgen - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /I "..\gvrender" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
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
PostBuild_Cmds=copy .\Debug\dotgen.lib ..\..\makearch\win32\static\Debug
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "dotgen - Win32 Release"
# Name "dotgen - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\acyclic.c
# End Source File
# Begin Source File

SOURCE=.\class1.c
# End Source File
# Begin Source File

SOURCE=.\class2.c
# End Source File
# Begin Source File

SOURCE=.\cluster.c
# End Source File
# Begin Source File

SOURCE=.\compound.c
# End Source File
# Begin Source File

SOURCE=.\conc.c
# End Source File
# Begin Source File

SOURCE=.\decomp.c
# End Source File
# Begin Source File

SOURCE=.\dotinit.c
# End Source File
# Begin Source File

SOURCE=.\dotsplines.c
# End Source File
# Begin Source File

SOURCE=.\fastgr.c
# End Source File
# Begin Source File

SOURCE=.\flat.c
# End Source File
# Begin Source File

SOURCE=.\mincross.c
# End Source File
# Begin Source File

SOURCE=.\ns.c
# End Source File
# Begin Source File

SOURCE=.\position.c
# End Source File
# Begin Source File

SOURCE=.\rank.c
# End Source File
# Begin Source File

SOURCE=.\routespl.c
# End Source File
# Begin Source File

SOURCE=.\sameport.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\dot.h
# End Source File
# Begin Source File

SOURCE=.\dotprocs.h
# End Source File
# End Group
# End Target
# End Project
