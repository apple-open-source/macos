# Microsoft Developer Studio Generated NMAKE File, Based on mod_dav.dsp

!IF "$(APACHE)" == ""
APACHE=\apache-1.3
!MESSAGE No Apache directory specified, defaulting to $(APACHE)
!MESSAGE
!ENDIF

!IF "$(CFG)" == ""
CFG=release
!MESSAGE No configuration specified. Defaulting to release build.
!MESSAGE
!ENDIF 

!IF "$(CFG)" != "release" && "$(CFG)" != "debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mod_dav.mak" CFG="debug" APACHE="\apache-1.3"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "debug"   (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!MESSAGE Specify the location of the Apache source tree with APACHE;
!MESSAGE the appropriate (debug or release) build of Apache should already exist.
!MESSAGE
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\mod_dav.dll"

!ELSE 

ALL : "sdbm - Win32 Release" "$(OUTDIR)\mod_dav.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"sdbm - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dav_dyn.obj"
	-@erase "$(INTDIR)\dav_fs_dbm.obj"
	-@erase "$(INTDIR)\dav_fs_lock.obj"
	-@erase "$(INTDIR)\dav_fs_repos.obj"
	-@erase "$(INTDIR)\dav_lock.obj"
	-@erase "$(INTDIR)\dav_opaquelock.obj"
	-@erase "$(INTDIR)\dav_props.obj"
	-@erase "$(INTDIR)\dav_util.obj"
	-@erase "$(INTDIR)\dav_xmlparse.obj"
	-@erase "$(INTDIR)\mod_dav.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\mod_dav.dll"
	-@erase "$(OUTDIR)\mod_dav.exp"
	-@erase "$(OUTDIR)\mod_dav.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "$(APACHE)\src\include" /I "$(APACHE)\src\lib\expat-lite" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "SHARED_MODULE" /Fp"$(INTDIR)\mod_dav.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_dav.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib $(APACHE)\src\corer\apachecore.lib sdbm\release\sdbm.lib $(APACHE)\src\lib\expat-lite\Release\xmlparse.lib $(APACHE)\src\lib\expat-lite\Release\xmltok.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\mod_dav.pdb" /machine:I386 /out:"$(OUTDIR)\mod_dav.dll" /implib:"$(OUTDIR)\mod_dav.lib" 
LINK32_OBJS= \
	"$(INTDIR)\dav_fs_dbm.obj" \
	"$(INTDIR)\dav_fs_lock.obj" \
	"$(INTDIR)\dav_fs_repos.obj" \
	"$(INTDIR)\dav_lock.obj" \
	"$(INTDIR)\dav_opaquelock.obj" \
	"$(INTDIR)\dav_props.obj" \
	"$(INTDIR)\dav_util.obj" \
	"$(INTDIR)\dav_xmlparse.obj" \
	"$(INTDIR)\mod_dav.obj" \
	"$(INTDIR)\dav_dyn.obj" \
	".\sdbm\Release\sdbm.lib"

"$(OUTDIR)\mod_dav.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\mod_dav.dll"

!ELSE 

ALL : "sdbm - Win32 Debug" "$(OUTDIR)\mod_dav.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"sdbm - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dav_dyn.obj"
	-@erase "$(INTDIR)\dav_fs_dbm.obj"
	-@erase "$(INTDIR)\dav_fs_lock.obj"
	-@erase "$(INTDIR)\dav_fs_repos.obj"
	-@erase "$(INTDIR)\dav_lock.obj"
	-@erase "$(INTDIR)\dav_opaquelock.obj"
	-@erase "$(INTDIR)\dav_props.obj"
	-@erase "$(INTDIR)\dav_util.obj"
	-@erase "$(INTDIR)\dav_xmlparse.obj"
	-@erase "$(INTDIR)\mod_dav.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\mod_dav.dll"
	-@erase "$(OUTDIR)\mod_dav.exp"
	-@erase "$(OUTDIR)\mod_dav.ilk"
	-@erase "$(OUTDIR)\mod_dav.lib"
	-@erase "$(OUTDIR)\mod_dav.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "$(APACHE)\src\include" /I "$(APACHE)\src\lib\expat-lite" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SHARED_MODULE" /Fp"$(INTDIR)\mod_dav.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\mod_dav.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib $(APACHE)\src\cored\apachecore.lib sdbm\debug\sdbm.lib $(APACHE)\src\lib\expat-lite\Debug\xmlparse.lib $(APACHE)\src\lib\expat-lite\Debug\xmltok.lib /nologo /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\mod_dav.pdb" /debug /machine:I386 /out:"$(OUTDIR)\mod_dav.dll" /implib:"$(OUTDIR)\mod_dav.lib" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\dav_fs_dbm.obj" \
	"$(INTDIR)\dav_fs_lock.obj" \
	"$(INTDIR)\dav_fs_repos.obj" \
	"$(INTDIR)\dav_lock.obj" \
	"$(INTDIR)\dav_opaquelock.obj" \
	"$(INTDIR)\dav_props.obj" \
	"$(INTDIR)\dav_util.obj" \
	"$(INTDIR)\dav_xmlparse.obj" \
	"$(INTDIR)\mod_dav.obj" \
	"$(INTDIR)\dav_dyn.obj" \
	".\sdbm\Debug\sdbm.lib"

"$(OUTDIR)\mod_dav.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "release" || "$(CFG)" == "debug"
SOURCE=.\dav_dyn.c

"$(INTDIR)\dav_dyn.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_fs_dbm.c

"$(INTDIR)\dav_fs_dbm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_fs_lock.c

"$(INTDIR)\dav_fs_lock.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_fs_repos.c

"$(INTDIR)\dav_fs_repos.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_lock.c

"$(INTDIR)\dav_lock.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_opaquelock.c

"$(INTDIR)\dav_opaquelock.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_props.c

"$(INTDIR)\dav_props.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_util.c

"$(INTDIR)\dav_util.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dav_xmlparse.c

"$(INTDIR)\dav_xmlparse.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mod_dav.c

"$(INTDIR)\mod_dav.obj" : $(SOURCE) "$(INTDIR)"


!IF  "$(CFG)" == "release"

"sdbm - Win32 Release" : 
   cd ".\sdbm"
   $(MAKE) /$(MAKEFLAGS) /F .\sdbm.mak CFG="sdbm - Win32 Release" 
   cd ".."

"sdbm - Win32 ReleaseCLEAN" : 
   cd ".\sdbm"
   $(MAKE) /$(MAKEFLAGS) /F .\sdbm.mak CFG="sdbm - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "debug"

"sdbm - Win32 Debug" : 
   cd ".\sdbm"
   $(MAKE) /$(MAKEFLAGS) /F .\sdbm.mak CFG="sdbm - Win32 Debug" 
   cd ".."

"sdbm - Win32 DebugCLEAN" : 
   cd ".\sdbm"
   $(MAKE) /$(MAKEFLAGS) /F .\sdbm.mak CFG="sdbm - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 


!ENDIF 

