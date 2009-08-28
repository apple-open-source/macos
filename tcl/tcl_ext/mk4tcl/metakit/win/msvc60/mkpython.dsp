# Microsoft Developer Studio Project File - Name="mkpython" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=mkpython - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mkpython.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mkpython.mak" CFG="mkpython - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mkpython - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mkpython - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mkpython - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\builds"
# PROP Intermediate_Dir "..\..\builds\msvc60\mkpython\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "mkpython_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "..\..\include" /I "..\..\python\scxx" /I "c:\python24\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKPYTHON_EXPORTS" /FD /c
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
# ADD LINK32 c:\Python24\libs\python24.lib /nologo /dll /machine:I386 /out:"..\..\builds\Mk4py.dll" /libpath:"c:\python23\libs"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mkpython - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds"
# PROP Intermediate_Dir "..\..\builds\msvc60\mkpython\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "mkpython_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\include" /I "..\..\python\scxx" /I "c:\python24\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MKPYTHON_EXPORTS" /FR /FD /GZ /c
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
# ADD LINK32 c:\Python24\libs\python24.lib /nologo /dll /incremental:no /machine:I386 /nodefaultlib:"python20_d.lib" /out:"..\..\builds\Mk4py_d.dll" /pdbtype:sept /libpath:"c:\python23\libs"
# SUBTRACT LINK32 /debug

!ENDIF 

# Begin Target

# Name "mkpython - Win32 Release"
# Name "mkpython - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\python\scxx\PWOImp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\python\PyProperty.cpp
# End Source File
# Begin Source File

SOURCE=..\..\python\PyRowRef.cpp
# End Source File
# Begin Source File

SOURCE=..\..\python\PyStorage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\python\PyView.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\python\scxx\PWOBase.h
# End Source File
# Begin Source File

SOURCE=..\..\python\scxx\PWOMapping.h
# End Source File
# Begin Source File

SOURCE=..\..\python\scxx\PWOMSequence.h
# End Source File
# Begin Source File

SOURCE=..\..\python\scxx\PWONumber.h
# End Source File
# Begin Source File

SOURCE=..\..\python\scxx\PWOSequence.h
# End Source File
# Begin Source File

SOURCE=..\..\python\PyHead.h
# End Source File
# Begin Source File

SOURCE=..\..\python\PyProperty.h
# End Source File
# Begin Source File

SOURCE=..\..\python\PyRowRef.h
# End Source File
# Begin Source File

SOURCE=..\..\python\PyStorage.h
# End Source File
# Begin Source File

SOURCE=..\..\python\PyView.h
# End Source File
# End Group
# End Target
# End Project
