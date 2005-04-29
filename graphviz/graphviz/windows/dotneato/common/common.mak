# Microsoft Developer Studio Generated NMAKE File, Based on common.dsp
!IF "$(CFG)" == ""
CFG=common - Win32 Debug
!MESSAGE No configuration specified. Defaulting to common - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "common - Win32 Release" && "$(CFG)" != "common - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "common.mak" CFG="common - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "common - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "common - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "common - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\common.lib"

!ELSE 

ALL : "pathplan - Win32 Release" "graph - Win32 Release" "gd - Win32 Release" "$(OUTDIR)\common.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"gd - Win32 ReleaseCLEAN" "graph - Win32 ReleaseCLEAN" "pathplan - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\arrows.obj"
	-@erase "$(INTDIR)\colxlate.obj"
	-@erase "$(INTDIR)\diagen.obj"
	-@erase "$(INTDIR)\emit.obj"
	-@erase "$(INTDIR)\figgen.obj"
	-@erase "$(INTDIR)\fontmetrics.obj"
	-@erase "$(INTDIR)\gdgen.obj"
	-@erase "$(INTDIR)\globals.obj"
	-@erase "$(INTDIR)\hpglgen.obj"
	-@erase "$(INTDIR)\htmllex.obj"
	-@erase "$(INTDIR)\htmlparse.obj"
	-@erase "$(INTDIR)\htmltable.obj"
	-@erase "$(INTDIR)\input.obj"
	-@erase "$(INTDIR)\mapgen.obj"
	-@erase "$(INTDIR)\mifgen.obj"
	-@erase "$(INTDIR)\mpgen.obj"
	-@erase "$(INTDIR)\output.obj"
	-@erase "$(INTDIR)\picgen.obj"
	-@erase "$(INTDIR)\pointset.obj"
	-@erase "$(INTDIR)\postproc.obj"
	-@erase "$(INTDIR)\psgen.obj"
	-@erase "$(INTDIR)\shapes.obj"
	-@erase "$(INTDIR)\splines.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strncasecmp.obj"
	-@erase "$(INTDIR)\svggen.obj"
	-@erase "$(INTDIR)\timing.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vrmlgen.obj"
	-@erase "$(INTDIR)\vtxgen.obj"
	-@erase "$(OUTDIR)\common.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "NDEBUG" /D "HAVE_SETMODE" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\common.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\common.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\common.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arrows.obj" \
	"$(INTDIR)\colxlate.obj" \
	"$(INTDIR)\diagen.obj" \
	"$(INTDIR)\emit.obj" \
	"$(INTDIR)\figgen.obj" \
	"$(INTDIR)\fontmetrics.obj" \
	"$(INTDIR)\gdgen.obj" \
	"$(INTDIR)\globals.obj" \
	"$(INTDIR)\hpglgen.obj" \
	"$(INTDIR)\htmllex.obj" \
	"$(INTDIR)\htmlparse.obj" \
	"$(INTDIR)\htmltable.obj" \
	"$(INTDIR)\input.obj" \
	"$(INTDIR)\mapgen.obj" \
	"$(INTDIR)\mifgen.obj" \
	"$(INTDIR)\mpgen.obj" \
	"$(INTDIR)\output.obj" \
	"$(INTDIR)\picgen.obj" \
	"$(INTDIR)\pointset.obj" \
	"$(INTDIR)\postproc.obj" \
	"$(INTDIR)\psgen.obj" \
	"$(INTDIR)\shapes.obj" \
	"$(INTDIR)\splines.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strncasecmp.obj" \
	"$(INTDIR)\svggen.obj" \
	"$(INTDIR)\timing.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\vrmlgen.obj" \
	"$(INTDIR)\vtxgen.obj" \
	"..\..\gd\Release\gd.lib" \
	"..\..\graph\Release\graph.lib" \
	"..\..\pathplan\Release\pathplan.lib"

"$(OUTDIR)\common.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pathplan - Win32 Release" "graph - Win32 Release" "gd - Win32 Release" "$(OUTDIR)\common.lib"
   copy .\Release\common.lib ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\common.lib"

!ELSE 

ALL : "pathplan - Win32 Debug" "graph - Win32 Debug" "gd - Win32 Debug" "$(OUTDIR)\common.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"gd - Win32 DebugCLEAN" "graph - Win32 DebugCLEAN" "pathplan - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\arrows.obj"
	-@erase "$(INTDIR)\colxlate.obj"
	-@erase "$(INTDIR)\diagen.obj"
	-@erase "$(INTDIR)\emit.obj"
	-@erase "$(INTDIR)\figgen.obj"
	-@erase "$(INTDIR)\fontmetrics.obj"
	-@erase "$(INTDIR)\gdgen.obj"
	-@erase "$(INTDIR)\globals.obj"
	-@erase "$(INTDIR)\hpglgen.obj"
	-@erase "$(INTDIR)\htmllex.obj"
	-@erase "$(INTDIR)\htmlparse.obj"
	-@erase "$(INTDIR)\htmltable.obj"
	-@erase "$(INTDIR)\input.obj"
	-@erase "$(INTDIR)\mapgen.obj"
	-@erase "$(INTDIR)\mifgen.obj"
	-@erase "$(INTDIR)\mpgen.obj"
	-@erase "$(INTDIR)\output.obj"
	-@erase "$(INTDIR)\picgen.obj"
	-@erase "$(INTDIR)\pointset.obj"
	-@erase "$(INTDIR)\postproc.obj"
	-@erase "$(INTDIR)\psgen.obj"
	-@erase "$(INTDIR)\shapes.obj"
	-@erase "$(INTDIR)\splines.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strncasecmp.obj"
	-@erase "$(INTDIR)\svggen.obj"
	-@erase "$(INTDIR)\timing.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vrmlgen.obj"
	-@erase "$(INTDIR)\vtxgen.obj"
	-@erase "$(OUTDIR)\common.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "HAVE_SETMODE" /Fp"$(INTDIR)\common.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\common.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\common.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arrows.obj" \
	"$(INTDIR)\colxlate.obj" \
	"$(INTDIR)\diagen.obj" \
	"$(INTDIR)\emit.obj" \
	"$(INTDIR)\figgen.obj" \
	"$(INTDIR)\fontmetrics.obj" \
	"$(INTDIR)\gdgen.obj" \
	"$(INTDIR)\globals.obj" \
	"$(INTDIR)\hpglgen.obj" \
	"$(INTDIR)\htmllex.obj" \
	"$(INTDIR)\htmlparse.obj" \
	"$(INTDIR)\htmltable.obj" \
	"$(INTDIR)\input.obj" \
	"$(INTDIR)\mapgen.obj" \
	"$(INTDIR)\mifgen.obj" \
	"$(INTDIR)\mpgen.obj" \
	"$(INTDIR)\output.obj" \
	"$(INTDIR)\picgen.obj" \
	"$(INTDIR)\pointset.obj" \
	"$(INTDIR)\postproc.obj" \
	"$(INTDIR)\psgen.obj" \
	"$(INTDIR)\shapes.obj" \
	"$(INTDIR)\splines.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strncasecmp.obj" \
	"$(INTDIR)\svggen.obj" \
	"$(INTDIR)\timing.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\vrmlgen.obj" \
	"$(INTDIR)\vtxgen.obj" \
	"..\..\gd\Debug\gd.lib" \
	"..\..\graph\Debug\graph.lib" \
	"..\..\pathplan\Debug\pathplan.lib"

"$(OUTDIR)\common.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pathplan - Win32 Debug" "graph - Win32 Debug" "gd - Win32 Debug" "$(OUTDIR)\common.lib"
   copy .\Debug\common.lib ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("common.dep")
!INCLUDE "common.dep"
!ELSE 
!MESSAGE Warning: cannot find "common.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "common - Win32 Release" || "$(CFG)" == "common - Win32 Debug"
SOURCE=.\arrows.c

"$(INTDIR)\arrows.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\colxlate.c

"$(INTDIR)\colxlate.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\diagen.c

"$(INTDIR)\diagen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\emit.c

"$(INTDIR)\emit.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\figgen.c

"$(INTDIR)\figgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\fontmetrics.c

"$(INTDIR)\fontmetrics.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\gdgen.c

"$(INTDIR)\gdgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\globals.c

"$(INTDIR)\globals.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\hpglgen.c

"$(INTDIR)\hpglgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\htmllex.c

"$(INTDIR)\htmllex.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\htmlparse.c

"$(INTDIR)\htmlparse.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\htmltable.c

"$(INTDIR)\htmltable.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\input.c

!IF  "$(CFG)" == "common - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "NDEBUG" /D "HAVE_SETMODE" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\common.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\input.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "common - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W4 /Gm /Gi /GX /ZI /Od /I "." /I ".." /I "..\.." /I "..\..\cdt" /I "..\..\pathplan" /I "..\..\gd" /I "..\..\graph" /I "..\gvrender" /I "..\..\third-party\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "HAVE_SETMODE" /Fp"$(INTDIR)\common.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\input.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\mapgen.c

"$(INTDIR)\mapgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mifgen.c

"$(INTDIR)\mifgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mpgen.c

"$(INTDIR)\mpgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\output.c

"$(INTDIR)\output.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\picgen.c

"$(INTDIR)\picgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\pointset.c

"$(INTDIR)\pointset.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\postproc.c

"$(INTDIR)\postproc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\psgen.c

"$(INTDIR)\psgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\shapes.c

"$(INTDIR)\shapes.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\splines.c

"$(INTDIR)\splines.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\strcasecmp.c

"$(INTDIR)\strcasecmp.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\strncasecmp.c

"$(INTDIR)\strncasecmp.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\svggen.c

"$(INTDIR)\svggen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\timing.c

"$(INTDIR)\timing.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\utils.c

"$(INTDIR)\utils.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\vrmlgen.c

"$(INTDIR)\vrmlgen.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\vtxgen.c

"$(INTDIR)\vtxgen.obj" : $(SOURCE) "$(INTDIR)"


!IF  "$(CFG)" == "common - Win32 Release"

"gd - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Release" 
   cd "..\dotneato\common"

"gd - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

"gd - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Debug" 
   cd "..\dotneato\common"

"gd - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ENDIF 

!IF  "$(CFG)" == "common - Win32 Release"

"graph - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Release" 
   cd "..\dotneato\common"

"graph - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

"graph - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Debug" 
   cd "..\dotneato\common"

"graph - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ENDIF 

!IF  "$(CFG)" == "common - Win32 Release"

"pathplan - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Release" 
   cd "..\dotneato\common"

"pathplan - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

"pathplan - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Debug" 
   cd "..\dotneato\common"

"pathplan - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\common"

!ENDIF 


!ENDIF 

