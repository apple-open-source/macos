# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=tcl_odbc - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to tcl_odbc - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "tcl_odbc - Win32 Debug" && "$(CFG)" !=\
 "tcl_odbc - Win32 Rel76" && "$(CFG)" != "tcl_odbc - Win32 Rel80" && "$(CFG)" !=\
 "tcl_odbc - Win32 Rel81"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "tclodbc.mak" CFG="tcl_odbc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tcl_odbc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "tcl_odbc - Win32 Rel76" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "tcl_odbc - Win32 Rel80" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "tcl_odbc - Win32 Rel81" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "tcl_odbc - Win32 Rel76"
RSC=rc.exe
MTL=mktyplib.exe
CPP=cl.exe

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

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
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\tclodbc.dll" "$(OUTDIR)\tclodbc.bsc"

CLEAN : 
	-@erase "$(INTDIR)\database.obj"
	-@erase "$(INTDIR)\database.sbr"
	-@erase "$(INTDIR)\encoding.obj"
	-@erase "$(INTDIR)\encoding.sbr"
	-@erase "$(INTDIR)\statemnt.obj"
	-@erase "$(INTDIR)\statemnt.sbr"
	-@erase "$(INTDIR)\strings.obj"
	-@erase "$(INTDIR)\strings.sbr"
	-@erase "$(INTDIR)\tclobj.obj"
	-@erase "$(INTDIR)\tclobj.sbr"
	-@erase "$(INTDIR)\tclodbc.obj"
	-@erase "$(INTDIR)\tclodbc.sbr"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\tclodbc.bsc"
	-@erase "$(OUTDIR)\tclodbc.dll"
	-@erase "$(OUTDIR)\tclodbc.exp"
	-@erase "$(OUTDIR)\tclodbc.ilk"
	-@erase "$(OUTDIR)\tclodbc.lib"
	-@erase "$(OUTDIR)\tclodbc.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "e:\tcl\inc80" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "e:\tcl\inc80" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/tclodbc.pch" /YX\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/tclodbc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\database.sbr" \
	"$(INTDIR)\encoding.sbr" \
	"$(INTDIR)\statemnt.sbr" \
	"$(INTDIR)\strings.sbr" \
	"$(INTDIR)\tclobj.sbr" \
	"$(INTDIR)\tclodbc.sbr"

"$(OUTDIR)\tclodbc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/tclodbc.pdb" /debug /machine:I386 /out:"$(OUTDIR)/tclodbc.dll"\
 /implib:"$(OUTDIR)/tclodbc.lib" 
LINK32_OBJS= \
	"$(INTDIR)\database.obj" \
	"$(INTDIR)\encoding.obj" \
	"$(INTDIR)\statemnt.obj" \
	"$(INTDIR)\strings.obj" \
	"$(INTDIR)\tclobj.obj" \
	"$(INTDIR)\tclodbc.obj"

"$(OUTDIR)\tclodbc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tcl_odbc"
# PROP BASE Intermediate_Dir "tcl_odbc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Tcl7.6"
# PROP Intermediate_Dir "Tcl7.6"
# PROP Target_Dir ""
OUTDIR=.\Tcl7.6
INTDIR=.\Tcl7.6

ALL : "$(OUTDIR)\tclodbc.dll" "$(OUTDIR)\tclodbc.bsc"

CLEAN : 
	-@erase "$(INTDIR)\database.obj"
	-@erase "$(INTDIR)\database.sbr"
	-@erase "$(INTDIR)\encoding.obj"
	-@erase "$(INTDIR)\encoding.sbr"
	-@erase "$(INTDIR)\statemnt.obj"
	-@erase "$(INTDIR)\statemnt.sbr"
	-@erase "$(INTDIR)\strings.obj"
	-@erase "$(INTDIR)\strings.sbr"
	-@erase "$(INTDIR)\tclobj.obj"
	-@erase "$(INTDIR)\tclobj.sbr"
	-@erase "$(INTDIR)\tclodbc.obj"
	-@erase "$(INTDIR)\tclodbc.sbr"
	-@erase "$(OUTDIR)\tclodbc.bsc"
	-@erase "$(OUTDIR)\tclodbc.dll"
	-@erase "$(OUTDIR)\tclodbc.exp"
	-@erase "$(OUTDIR)\tclodbc.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "e:\tcl\inc76" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "e:\tcl\inc76" /D "WIN32" /D "NDEBUG" /D\
 "_WINDOWS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/tclodbc.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Tcl7.6/
CPP_SBRS=.\Tcl7.6/
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/tclodbc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\database.sbr" \
	"$(INTDIR)\encoding.sbr" \
	"$(INTDIR)\statemnt.sbr" \
	"$(INTDIR)\strings.sbr" \
	"$(INTDIR)\tclobj.sbr" \
	"$(INTDIR)\tclodbc.sbr"

"$(OUTDIR)\tclodbc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl76.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib tcl76.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/tclodbc.pdb" /machine:I386 /out:"$(OUTDIR)/tclodbc.dll"\
 /implib:"$(OUTDIR)/tclodbc.lib" 
LINK32_OBJS= \
	"$(INTDIR)\database.obj" \
	"$(INTDIR)\encoding.obj" \
	"$(INTDIR)\statemnt.obj" \
	"$(INTDIR)\strings.obj" \
	"$(INTDIR)\tclobj.obj" \
	"$(INTDIR)\tclodbc.obj"

"$(OUTDIR)\tclodbc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tcl_odb0"
# PROP BASE Intermediate_Dir "tcl_odb0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Tcl8.0"
# PROP Intermediate_Dir "Tcl8.0"
# PROP Target_Dir ""
OUTDIR=.\Tcl8.0
INTDIR=.\Tcl8.0

ALL : "$(OUTDIR)\tclodbc.dll" "$(OUTDIR)\tclodbc.bsc"

CLEAN : 
	-@erase "$(INTDIR)\database.obj"
	-@erase "$(INTDIR)\database.sbr"
	-@erase "$(INTDIR)\encoding.obj"
	-@erase "$(INTDIR)\encoding.sbr"
	-@erase "$(INTDIR)\statemnt.obj"
	-@erase "$(INTDIR)\statemnt.sbr"
	-@erase "$(INTDIR)\strings.obj"
	-@erase "$(INTDIR)\strings.sbr"
	-@erase "$(INTDIR)\tclobj.obj"
	-@erase "$(INTDIR)\tclobj.sbr"
	-@erase "$(INTDIR)\tclodbc.obj"
	-@erase "$(INTDIR)\tclodbc.sbr"
	-@erase "$(OUTDIR)\tclodbc.bsc"
	-@erase "$(OUTDIR)\tclodbc.dll"
	-@erase "$(OUTDIR)\tclodbc.exp"
	-@erase "$(OUTDIR)\tclodbc.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "e:\tcl\inc80" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "e:\tcl\inc80" /D "WIN32" /D "NDEBUG" /D\
 "_WINDOWS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/tclodbc.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Tcl8.0/
CPP_SBRS=.\Tcl8.0/
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/tclodbc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\database.sbr" \
	"$(INTDIR)\encoding.sbr" \
	"$(INTDIR)\statemnt.sbr" \
	"$(INTDIR)\strings.sbr" \
	"$(INTDIR)\tclobj.sbr" \
	"$(INTDIR)\tclodbc.sbr"

"$(OUTDIR)\tclodbc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/tclodbc.pdb" /machine:I386 /out:"$(OUTDIR)/tclodbc.dll"\
 /implib:"$(OUTDIR)/tclodbc.lib" 
LINK32_OBJS= \
	"$(INTDIR)\database.obj" \
	"$(INTDIR)\encoding.obj" \
	"$(INTDIR)\statemnt.obj" \
	"$(INTDIR)\strings.obj" \
	"$(INTDIR)\tclobj.obj" \
	"$(INTDIR)\tclodbc.obj"

"$(OUTDIR)\tclodbc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tcl_odb1"
# PROP BASE Intermediate_Dir "tcl_odb1"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Tcl8.1"
# PROP Intermediate_Dir "Tcl8.1"
# PROP Target_Dir ""
OUTDIR=.\Tcl8.1
INTDIR=.\Tcl8.1

ALL : "$(OUTDIR)\tclodbc.dll" "$(OUTDIR)\tclodbc.bsc"

CLEAN : 
	-@erase "$(INTDIR)\database.obj"
	-@erase "$(INTDIR)\database.sbr"
	-@erase "$(INTDIR)\encoding.obj"
	-@erase "$(INTDIR)\encoding.sbr"
	-@erase "$(INTDIR)\statemnt.obj"
	-@erase "$(INTDIR)\statemnt.sbr"
	-@erase "$(INTDIR)\strings.obj"
	-@erase "$(INTDIR)\strings.sbr"
	-@erase "$(INTDIR)\tclobj.obj"
	-@erase "$(INTDIR)\tclobj.sbr"
	-@erase "$(INTDIR)\tclodbc.obj"
	-@erase "$(INTDIR)\tclodbc.sbr"
	-@erase "$(OUTDIR)\tclodbc.bsc"
	-@erase "$(OUTDIR)\tclodbc.dll"
	-@erase "$(OUTDIR)\tclodbc.exp"
	-@erase "$(OUTDIR)\tclodbc.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "e:\tcl\inc81" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "e:\tcl\inc81" /D "WIN32" /D "NDEBUG" /D\
 "_WINDOWS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/tclodbc.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Tcl8.1/
CPP_SBRS=.\Tcl8.1/
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/tclodbc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\database.sbr" \
	"$(INTDIR)\encoding.sbr" \
	"$(INTDIR)\statemnt.sbr" \
	"$(INTDIR)\strings.sbr" \
	"$(INTDIR)\tclobj.sbr" \
	"$(INTDIR)\tclodbc.sbr"

"$(OUTDIR)\tclodbc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tcl80vc.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib tclstub81.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:"libc.lib" /nodefaultlib:"msvcrt.lib" /nodefaultlib:"libcd.lib" /nodefaultlib:"libcmtd.lib" /nodefaultlib:"msvcrtd.lib"
# SUBTRACT LINK32 /nodefaultlib
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib tclstub81.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/tclodbc.pdb" /machine:I386 /nodefaultlib:"libc.lib"\
 /nodefaultlib:"msvcrt.lib" /nodefaultlib:"libcd.lib"\
 /nodefaultlib:"libcmtd.lib" /nodefaultlib:"msvcrtd.lib"\
 /out:"$(OUTDIR)/tclodbc.dll" /implib:"$(OUTDIR)/tclodbc.lib" 
LINK32_OBJS= \
	"$(INTDIR)\database.obj" \
	"$(INTDIR)\encoding.obj" \
	"$(INTDIR)\statemnt.obj" \
	"$(INTDIR)\strings.obj" \
	"$(INTDIR)\tclobj.obj" \
	"$(INTDIR)\tclodbc.obj"

"$(OUTDIR)\tclodbc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "tcl_odbc - Win32 Debug"
# Name "tcl_odbc - Win32 Rel76"
# Name "tcl_odbc - Win32 Rel80"
# Name "tcl_odbc - Win32 Rel81"

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\tclodbc.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_TCLOD=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\tclodbc.obj" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"

"$(INTDIR)\tclodbc.sbr" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_TCLOD=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\tclodbc.obj" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"

"$(INTDIR)\tclodbc.sbr" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_TCLOD=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\tclodbc.obj" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"

"$(INTDIR)\tclodbc.sbr" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_TCLOD=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\tclodbc.obj" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"

"$(INTDIR)\tclodbc.sbr" : $(SOURCE) $(DEP_CPP_TCLOD) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tclobj.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_TCLOB=\
	".\Tclobj.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\tclobj.obj" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"

"$(INTDIR)\tclobj.sbr" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_TCLOB=\
	".\Tclobj.hxx"\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\tclobj.obj" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"

"$(INTDIR)\tclobj.sbr" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_TCLOB=\
	".\Tclobj.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\tclobj.obj" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"

"$(INTDIR)\tclobj.sbr" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_TCLOB=\
	".\Tclobj.hxx"\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\tclobj.obj" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"

"$(INTDIR)\tclobj.sbr" : $(SOURCE) $(DEP_CPP_TCLOB) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\encoding.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_ENCOD=\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\encoding.obj" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"

"$(INTDIR)\encoding.sbr" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_ENCOD=\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\encoding.obj" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"

"$(INTDIR)\encoding.sbr" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_ENCOD=\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\encoding.obj" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"

"$(INTDIR)\encoding.sbr" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_ENCOD=\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\encoding.obj" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"

"$(INTDIR)\encoding.sbr" : $(SOURCE) $(DEP_CPP_ENCOD) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\statemnt.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_STATE=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\statemnt.obj" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"

"$(INTDIR)\statemnt.sbr" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_STATE=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\statemnt.obj" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"

"$(INTDIR)\statemnt.sbr" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_STATE=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\statemnt.obj" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"

"$(INTDIR)\statemnt.sbr" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_STATE=\
	".\tclodbc.hxx"\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\statemnt.obj" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"

"$(INTDIR)\statemnt.sbr" : $(SOURCE) $(DEP_CPP_STATE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\strings.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_STRIN=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\strings.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"

"$(INTDIR)\strings.sbr" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_STRIN=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\strings.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"

"$(INTDIR)\strings.sbr" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_STRIN=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\strings.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"

"$(INTDIR)\strings.sbr" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_STRIN=\
	".\tclodbc.hxx"\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\strings.obj" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"

"$(INTDIR)\strings.sbr" : $(SOURCE) $(DEP_CPP_STRIN) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\database.cxx

!IF  "$(CFG)" == "tcl_odbc - Win32 Debug"

DEP_CPP_DATAB=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\database.obj" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"

"$(INTDIR)\database.sbr" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel76"

DEP_CPP_DATAB=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc76\tcl.h"\
	

"$(INTDIR)\database.obj" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"

"$(INTDIR)\database.sbr" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel80"

DEP_CPP_DATAB=\
	".\Tclobj.hxx"\
	".\tclodbc.hxx"\
	"e:\tcl\inc80\tcl.h"\
	

"$(INTDIR)\database.obj" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"

"$(INTDIR)\database.sbr" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "tcl_odbc - Win32 Rel81"

DEP_CPP_DATAB=\
	".\tclodbc.hxx"\
	"e:\tcl\inc81\tcl.h"\
	"E:\Tcl\inc81\tclDecls.h"\
	

"$(INTDIR)\database.obj" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"

"$(INTDIR)\database.sbr" : $(SOURCE) $(DEP_CPP_DATAB) "$(INTDIR)"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
