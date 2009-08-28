# Microsoft Developer Studio Project File - Name="mktest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mktest - Win32 Debug STD
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mktest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mktest.mak" CFG="mktest - Win32 Debug STD"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mktest - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mktest - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mktest - Win32 Debug MFC" (based on "Win32 (x86) Console Application")
!MESSAGE "mktest - Win32 Debug STD" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mktest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\builds\msvc60\mktest\Release"
# PROP Intermediate_Dir "..\..\builds\msvc60\mktest\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "..\..\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 /nologo /subsystem:console /machine:I386 /out:"..\..\builds\mktest.exe"

!ELSEIF  "$(CFG)" == "mktest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mktest\Debug"
# PROP Intermediate_Dir "..\..\builds\msvc60\mktest\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\builds\mktest_d.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "mktest - Win32 Debug MFC"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mktest___Win32_Debug_MFC"
# PROP BASE Intermediate_Dir "mktest___Win32_Debug_MFC"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mktest\Debug_MFC"
# PROP Intermediate_Dir "..\..\builds\msvc60\mktest\Debug_MFC"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "_CONSOLE" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_MFC" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\builds\mktest_d.exe" /pdbtype:sept
# ADD LINK32 /nologo /subsystem:console /incremental:no /debug /debugtype:both /machine:I386 /out:"..\..\builds\mktest_mfc_d.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "mktest - Win32 Debug STD"

# PROP BASE Use_MFC 1
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mktest___Win32_Debug_STD"
# PROP BASE Intermediate_Dir "mktest___Win32_Debug_STD"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\builds\msvc60\mktest\Debug_STD"
# PROP Intermediate_Dir "..\..\builds\msvc60\mktest\Debug_STD"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "_CONSOLE" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_MFC" /FR /FD /GZ /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "_CONSOLE" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "q4_STD" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\builds\mktest_mfc_d.exe" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\builds\mktest_std_d.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mktest - Win32 Release"
# Name "mktest - Win32 Debug"
# Name "mktest - Win32 Debug MFC"
# Name "mktest - Win32 Debug STD"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\tests\regress.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tbasic1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tbasic2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tcusto1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tcusto2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tdiffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\textend.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tformat.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tlimits.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tmapped.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tnotify.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tresize.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tstore1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tstore2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tstore3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tstore4.cpp
# End Source File
# Begin Source File

SOURCE=..\..\tests\tstore5.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\tests\regress.h
# End Source File
# End Group
# End Target
# End Project
