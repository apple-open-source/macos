# Microsoft Developer Studio Project File - Name="agraph" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=agraph - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "agraph.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "agraph.mak" CFG="agraph - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "agraph - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "agraph - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "agraph - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "." /I ".." /I "../cdt" /D "NDEBUG" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /D YY_NEVER_INTERACTIVE=1 /YX /FD /c
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
PostBuild_Cmds=copy .\Release\agraph.lib ..\makearch\win32\static\Release
# End Special Build Tool

!ELSEIF  "$(CFG)" == "agraph - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".." /I "." /I "../cdt" /D "_DEBUG" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /D YY_NEVER_INTERACTIVE=1 /YX /FD /GZ /c
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
PostBuild_Cmds=copy .\Debug\agraph.lib ..\makearch\win32\static\Debug
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "agraph - Win32 Release"
# Name "agraph - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\agerror.c
# End Source File
# Begin Source File

SOURCE=.\apply.c
# End Source File
# Begin Source File

SOURCE=.\attr.c
# End Source File
# Begin Source File

SOURCE=.\edge.c
# End Source File
# Begin Source File

SOURCE=.\flatten.c
# End Source File
# Begin Source File

SOURCE=.\grammar.c
# End Source File
# Begin Source File

SOURCE=.\graph.c
# End Source File
# Begin Source File

SOURCE=.\id.c
# End Source File
# Begin Source File

SOURCE=.\imap.c
# End Source File
# Begin Source File

SOURCE=.\io.c
# End Source File
# Begin Source File

SOURCE=.\mem.c
# End Source File
# Begin Source File

SOURCE=.\node.c
# End Source File
# Begin Source File

SOURCE=.\obj.c
# End Source File
# Begin Source File

SOURCE=.\pend.c
# End Source File
# Begin Source File

SOURCE=.\rec.c
# End Source File
# Begin Source File

SOURCE=.\refstr.c
# End Source File
# Begin Source File

SOURCE=.\scan.c
# End Source File
# Begin Source File

SOURCE=.\subg.c
# End Source File
# Begin Source File

SOURCE=.\utils.c
# End Source File
# Begin Source File

SOURCE=.\write.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\aghdr.h
# End Source File
# Begin Source File

SOURCE=.\agraph.h
# End Source File
# Begin Source File

SOURCE=.\grammar.h
# End Source File
# Begin Source File

SOURCE=.\malloc.h
# End Source File
# Begin Source File

SOURCE=.\unistd.h
# End Source File
# Begin Source File

SOURCE=.\vmstub.h
# End Source File
# Begin Source File

SOURCE=.\yaccdefs.h
# End Source File
# End Group
# End Target
# End Project
