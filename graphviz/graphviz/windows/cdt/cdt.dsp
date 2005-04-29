# Microsoft Developer Studio Project File - Name="cdt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=cdt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cdt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cdt.mak" CFG="cdt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cdt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "cdt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cdt - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
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
PostBuild_Cmds=copy .\Release\cdt.lib ..\makearch\win32\static\Release
# End Special Build Tool

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
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
PostBuild_Cmds=copy .\Debug\cdt.lib ..\makearch\win32\static\Debug
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "cdt - Win32 Release"
# Name "cdt - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\dtclose.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtdisc.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtextract.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtflatten.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dthash.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtlist.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtmethod.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtopen.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtrenew.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtrestore.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtsize.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtstat.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtstrhash.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dttree.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtview.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dtwalk.c

!IF  "$(CFG)" == "cdt - Win32 Release"

# ADD CPP /D "MSWIN32"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

# ADD CPP /I "." /D "MSWIN32"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\cdt.h
# End Source File
# Begin Source File

SOURCE=.\dthdr.h
# End Source File
# End Group
# End Target
# End Project
