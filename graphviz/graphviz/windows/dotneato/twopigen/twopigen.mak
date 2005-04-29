# Microsoft Developer Studio Generated NMAKE File, Based on twopigen.dsp
!IF "$(CFG)" == ""
CFG=twopigen - Win32 Debug
!MESSAGE No configuration specified. Defaulting to twopigen - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "twopigen - Win32 Release" && "$(CFG)" != "twopigen - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "twopigen.mak" CFG="twopigen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "twopigen - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "twopigen - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "twopigen - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\twopigen.lib"

!ELSE 

ALL : "pack - Win32 Release" "$(OUTDIR)\twopigen.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"pack - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\circle.obj"
	-@erase "$(INTDIR)\twopiinit.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\twopigen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\neatogen" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /I "..\pack" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\twopigen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\twopigen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\twopigen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\circle.obj" \
	"$(INTDIR)\twopiinit.obj" \
	"..\pack\Release\pack.lib"

"$(OUTDIR)\twopigen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pack - Win32 Release" "$(OUTDIR)\twopigen.lib"
   copy .\Release\twopigen.lib ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "twopigen - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\twopigen.lib"

!ELSE 

ALL : "pack - Win32 Debug" "$(OUTDIR)\twopigen.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"pack - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\circle.obj"
	-@erase "$(INTDIR)\twopiinit.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\twopigen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\neatogen" /I "../gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /I "..\pack" /D "_DEBUG" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /Fp"$(INTDIR)\twopigen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\twopigen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\twopigen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\circle.obj" \
	"$(INTDIR)\twopiinit.obj" \
	"..\pack\Debug\pack.lib"

"$(OUTDIR)\twopigen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pack - Win32 Debug" "$(OUTDIR)\twopigen.lib"
   copy .\Debug\twopigen.lib ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("twopigen.dep")
!INCLUDE "twopigen.dep"
!ELSE 
!MESSAGE Warning: cannot find "twopigen.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "twopigen - Win32 Release" || "$(CFG)" == "twopigen - Win32 Debug"
SOURCE=.\circle.c

"$(INTDIR)\circle.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\twopiinit.c

"$(INTDIR)\twopiinit.obj" : $(SOURCE) "$(INTDIR)"


!IF  "$(CFG)" == "twopigen - Win32 Release"

"pack - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Release" 
   cd "..\twopigen"

"pack - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Release" RECURSE=1 CLEAN 
   cd "..\twopigen"

!ELSEIF  "$(CFG)" == "twopigen - Win32 Debug"

"pack - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Debug" 
   cd "..\twopigen"

"pack - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\twopigen"

!ENDIF 


!ENDIF 

