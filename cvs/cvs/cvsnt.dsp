# Microsoft Developer Studio Project File - Name="cvsnt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=cvsnt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cvsnt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cvsnt.mak" CFG="cvsnt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cvsnt - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "cvsnt - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cvsnt - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WinRel"
# PROP Intermediate_Dir "WinRel"
# PROP Ignore_Export_Lib 0
# ADD BASE CPP /nologo /W3 /GX /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /D "NDEBUG" /D "WANT_WIN_COMPILER_VERSION" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /YX /FD /c
# SUBTRACT BASE CPP /WX /Fr
# ADD CPP /nologo /W3 /GX /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /I ".\WinDebug" /D "NDEBUG" /D "WANT_WIN_COMPILER_VERSION" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /YX /FD /c
# SUBTRACT CPP /WX /Fr
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 diff\WinRel\libdiff.lib lib\WinRel\libcvs.lib zlib\WinRel\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386 /out:".\WinRel\cvs.exe"
# ADD LINK32 diff\WinRel\libdiff.lib lib\WinRel\libcvs.lib zlib\WinRel\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386 /out:".\WinRel\cvs.exe"

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WinDebug"
# PROP Intermediate_Dir "WinDebug"
# PROP Ignore_Export_Lib 0
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /D "_DEBUG" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /D "WANT_WIN_COMPILER_VERSION" /YX /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /W3 /Gm /GX /Zi /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /D "_DEBUG" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /D "WANT_WIN_COMPILER_VERSION" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 diff\WinDebug\libdiff.lib lib\WinDebug\libcvs.lib zlib\WinDebug\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /pdb:".\WinDebug\cvs.pdb" /debug /machine:I386 /out:".\WinDebug\cvs.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 diff\WinDebug\libdiff.lib lib\WinDebug\libcvs.lib zlib\WinDebug\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /pdb:".\WinDebug\cvs.pdb" /debug /machine:I386 /out:".\WinDebug\cvs.exe"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "cvsnt - Win32 Release"
# Name "cvsnt - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\src\add.c
# End Source File
# Begin Source File

SOURCE=.\src\admin.c
# End Source File
# Begin Source File

SOURCE=.\src\annotate.c
# End Source File
# Begin Source File

SOURCE=.\src\buffer.c
# End Source File
# Begin Source File

SOURCE=.\src\checkin.c
# End Source File
# Begin Source File

SOURCE=.\src\checkout.c
# End Source File
# Begin Source File

SOURCE=.\src\classify.c
# End Source File
# Begin Source File

SOURCE=.\src\client.c
# End Source File
# Begin Source File

SOURCE=.\src\commit.c
# End Source File
# Begin Source File

SOURCE=.\src\create_adm.c
# End Source File
# Begin Source File

SOURCE=.\src\cvsrc.c
# End Source File
# Begin Source File

SOURCE=.\src\diff.c
# End Source File
# Begin Source File

SOURCE=.\src\edit.c
# End Source File
# Begin Source File

SOURCE=.\src\entries.c
# End Source File
# Begin Source File

SOURCE=.\src\error.c
# End Source File
# Begin Source File

SOURCE=.\src\expand_path.c
# End Source File
# Begin Source File

SOURCE=.\src\fileattr.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\filesubr.c"
# End Source File
# Begin Source File

SOURCE=.\src\find_names.c
# End Source File
# Begin Source File

SOURCE=.\src\hash.c
# End Source File
# Begin Source File

SOURCE=.\src\history.c
# End Source File
# Begin Source File

SOURCE=.\src\ignore.c
# End Source File
# Begin Source File

SOURCE=.\src\import.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\JmgStat.c"
# End Source File
# Begin Source File

SOURCE=.\src\lock.c
# End Source File
# Begin Source File

SOURCE=.\src\log.c
# End Source File
# Begin Source File

SOURCE=.\src\login.c
# End Source File
# Begin Source File

SOURCE=.\src\logmsg.c
# End Source File
# Begin Source File

SOURCE=.\src\main.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\mkdir.c"
# End Source File
# Begin Source File

SOURCE=.\src\mkmodules.c
# End Source File
# Begin Source File

SOURCE=.\src\modules.c
# End Source File
# Begin Source File

SOURCE=.\src\myndbm.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\ndir.c"
# End Source File
# Begin Source File

SOURCE=.\src\no_diff.c
# End Source File
# Begin Source File

SOURCE=.\src\parseinfo.c
# End Source File
# Begin Source File

SOURCE=.\src\patch.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\pwd.c"
# End Source File
# Begin Source File

SOURCE=".\windows-NT\rcmd.c"
# End Source File
# Begin Source File

SOURCE=.\src\rcs.c
# End Source File
# Begin Source File

SOURCE=.\src\rcscmds.c
# End Source File
# Begin Source File

SOURCE=.\src\recurse.c
# End Source File
# Begin Source File

SOURCE=.\src\release.c
# End Source File
# Begin Source File

SOURCE=.\src\remove.c
# End Source File
# Begin Source File

SOURCE=.\src\repos.c
# End Source File
# Begin Source File

SOURCE=.\src\root.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\run.c"
# End Source File
# Begin Source File

SOURCE=.\src\scramble.c
# End Source File
# Begin Source File

SOURCE=.\src\server.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\sockerror.c"
# End Source File
# Begin Source File

SOURCE=.\src\stack.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\startserver.c"
# End Source File
# Begin Source File

SOURCE=.\src\status.c
# End Source File
# Begin Source File

SOURCE=.\src\subr.c
# End Source File
# Begin Source File

SOURCE=.\src\tag.c
# End Source File
# Begin Source File

SOURCE=.\src\update.c
# End Source File
# Begin Source File

SOURCE=.\src\vers_ts.c
# End Source File
# Begin Source File

SOURCE=.\src\version.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\waitpid.c"
# End Source File
# Begin Source File

SOURCE=.\src\watch.c
# End Source File
# Begin Source File

SOURCE=".\windows-NT\woe32.c"
# End Source File
# Begin Source File

SOURCE=.\src\wrapper.c
# End Source File
# Begin Source File

SOURCE=.\src\zlib.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\src\buffer.h
# End Source File
# Begin Source File

SOURCE=.\src\client.h
# End Source File
# Begin Source File

SOURCE=".\windows-NT\config.h"
# End Source File
# Begin Source File

SOURCE=.\src\cvs.h
# End Source File
# Begin Source File

SOURCE=.\zlib\deflate.h
# End Source File
# Begin Source File

SOURCE=.\diff\diffrun.h
# End Source File
# Begin Source File

SOURCE=.\src\edit.h
# End Source File
# Begin Source File

SOURCE=.\src\fileattr.h
# End Source File
# Begin Source File

SOURCE=.\lib\fnmatch.h
# End Source File
# Begin Source File

SOURCE=.\lib\getline.h
# End Source File
# Begin Source File

SOURCE=.\lib\getopt.h
# End Source File
# Begin Source File

SOURCE=.\src\hardlink.h
# End Source File
# Begin Source File

SOURCE=.\src\hash.h
# End Source File
# Begin Source File

SOURCE=.\src\history.h
# End Source File
# Begin Source File

SOURCE=.\zlib\infblock.h
# End Source File
# Begin Source File

SOURCE=.\zlib\infcodes.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\zlib\infutil.h
# End Source File
# Begin Source File

SOURCE=".\windows-NT\JmgStat.h"
# End Source File
# Begin Source File

SOURCE=.\lib\md5.h
# End Source File
# Begin Source File

SOURCE=.\src\myndbm.h
# End Source File
# Begin Source File

SOURCE=".\windows-NT\ndir.h"
# End Source File
# Begin Source File

SOURCE=".\windows-NT\pwd.h"
# End Source File
# Begin Source File

SOURCE=".\windows-NT\rcmd.h"
# End Source File
# Begin Source File

SOURCE=.\src\rcs.h
# End Source File
# Begin Source File

SOURCE=.\lib\regex.h
# End Source File
# Begin Source File

SOURCE=.\src\root.h
# End Source File
# Begin Source File

SOURCE=.\lib\savecwd.h
# End Source File
# Begin Source File

SOURCE=.\src\server.h
# End Source File
# Begin Source File

SOURCE=.\src\stack.h
# End Source File
# Begin Source File

SOURCE=.\lib\system.h
# End Source File
# Begin Source File

SOURCE=.\src\update.h
# End Source File
# Begin Source File

SOURCE=.\lib\wait.h
# End Source File
# Begin Source File

SOURCE=.\src\watch.h
# End Source File
# Begin Source File

SOURCE=.\lib\xsize.h
# End Source File
# Begin Source File

SOURCE=.\lib\xtime.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.h
# End Source File
# End Group
# End Target
# End Project
