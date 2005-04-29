# Microsoft Developer Studio Generated NMAKE File, Based on neatogen.dsp
!IF "$(CFG)" == ""
CFG=neatogen - Win32 Debug
!MESSAGE No configuration specified. Defaulting to neatogen - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "neatogen - Win32 Release" && "$(CFG)" != "neatogen - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "neatogen.mak" CFG="neatogen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "neatogen - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "neatogen - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "neatogen - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\neatogen.lib"

!ELSE 

ALL : "pathplan - Win32 Release" "graph - Win32 Release" "gd - Win32 Release" "$(OUTDIR)\neatogen.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"gd - Win32 ReleaseCLEAN" "graph - Win32 ReleaseCLEAN" "pathplan - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\adjust.obj"
	-@erase "$(INTDIR)\circuit.obj"
	-@erase "$(INTDIR)\edges.obj"
	-@erase "$(INTDIR)\find_ints.obj"
	-@erase "$(INTDIR)\geometry.obj"
	-@erase "$(INTDIR)\heap.obj"
	-@erase "$(INTDIR)\hedges.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\intersect.obj"
	-@erase "$(INTDIR)\legal.obj"
	-@erase "$(INTDIR)\lu.obj"
	-@erase "$(INTDIR)\matinv.obj"
	-@erase "$(INTDIR)\memory.obj"
	-@erase "$(INTDIR)\neatoinit.obj"
	-@erase "$(INTDIR)\neatosplines.obj"
	-@erase "$(INTDIR)\poly.obj"
	-@erase "$(INTDIR)\printvis.obj"
	-@erase "$(INTDIR)\site.obj"
	-@erase "$(INTDIR)\solve.obj"
	-@erase "$(INTDIR)\stuff.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\voronoi.obj"
	-@erase "$(OUTDIR)\neatogen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neatogen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\neatogen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\adjust.obj" \
	"$(INTDIR)\circuit.obj" \
	"$(INTDIR)\edges.obj" \
	"$(INTDIR)\find_ints.obj" \
	"$(INTDIR)\geometry.obj" \
	"$(INTDIR)\heap.obj" \
	"$(INTDIR)\hedges.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\intersect.obj" \
	"$(INTDIR)\legal.obj" \
	"$(INTDIR)\lu.obj" \
	"$(INTDIR)\matinv.obj" \
	"$(INTDIR)\memory.obj" \
	"$(INTDIR)\neatoinit.obj" \
	"$(INTDIR)\neatosplines.obj" \
	"$(INTDIR)\poly.obj" \
	"$(INTDIR)\printvis.obj" \
	"$(INTDIR)\site.obj" \
	"$(INTDIR)\solve.obj" \
	"$(INTDIR)\stuff.obj" \
	"$(INTDIR)\voronoi.obj" \
	"..\..\gd\Release\gd.lib" \
	"..\..\graph\Release\graph.lib" \
	"..\..\pathplan\Release\pathplan.lib"

"$(OUTDIR)\neatogen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pathplan - Win32 Release" "graph - Win32 Release" "gd - Win32 Release" "$(OUTDIR)\neatogen.lib"
   copy .\Release\neatogen.lib ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\neatogen.lib"

!ELSE 

ALL : "pathplan - Win32 Debug" "graph - Win32 Debug" "gd - Win32 Debug" "$(OUTDIR)\neatogen.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"gd - Win32 DebugCLEAN" "graph - Win32 DebugCLEAN" "pathplan - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\adjust.obj"
	-@erase "$(INTDIR)\circuit.obj"
	-@erase "$(INTDIR)\edges.obj"
	-@erase "$(INTDIR)\find_ints.obj"
	-@erase "$(INTDIR)\geometry.obj"
	-@erase "$(INTDIR)\heap.obj"
	-@erase "$(INTDIR)\hedges.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\intersect.obj"
	-@erase "$(INTDIR)\legal.obj"
	-@erase "$(INTDIR)\lu.obj"
	-@erase "$(INTDIR)\matinv.obj"
	-@erase "$(INTDIR)\memory.obj"
	-@erase "$(INTDIR)\neatoinit.obj"
	-@erase "$(INTDIR)\neatosplines.obj"
	-@erase "$(INTDIR)\poly.obj"
	-@erase "$(INTDIR)\printvis.obj"
	-@erase "$(INTDIR)\site.obj"
	-@erase "$(INTDIR)\solve.obj"
	-@erase "$(INTDIR)\stuff.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\voronoi.obj"
	-@erase "$(OUTDIR)\neatogen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neatogen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\neatogen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\adjust.obj" \
	"$(INTDIR)\circuit.obj" \
	"$(INTDIR)\edges.obj" \
	"$(INTDIR)\find_ints.obj" \
	"$(INTDIR)\geometry.obj" \
	"$(INTDIR)\heap.obj" \
	"$(INTDIR)\hedges.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\intersect.obj" \
	"$(INTDIR)\legal.obj" \
	"$(INTDIR)\lu.obj" \
	"$(INTDIR)\matinv.obj" \
	"$(INTDIR)\memory.obj" \
	"$(INTDIR)\neatoinit.obj" \
	"$(INTDIR)\neatosplines.obj" \
	"$(INTDIR)\poly.obj" \
	"$(INTDIR)\printvis.obj" \
	"$(INTDIR)\site.obj" \
	"$(INTDIR)\solve.obj" \
	"$(INTDIR)\stuff.obj" \
	"$(INTDIR)\voronoi.obj" \
	"..\..\gd\Debug\gd.lib" \
	"..\..\graph\Debug\graph.lib" \
	"..\..\pathplan\Debug\pathplan.lib"

"$(OUTDIR)\neatogen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pathplan - Win32 Debug" "graph - Win32 Debug" "gd - Win32 Debug" "$(OUTDIR)\neatogen.lib"
   copy .\Debug\neatogen.lib ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("neatogen.dep")
!INCLUDE "neatogen.dep"
!ELSE 
!MESSAGE Warning: cannot find "neatogen.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "neatogen - Win32 Release" || "$(CFG)" == "neatogen - Win32 Debug"
SOURCE=.\adjust.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\adjust.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\adjust.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\circuit.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\circuit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\circuit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\edges.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\edges.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\edges.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\find_ints.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\find_ints.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\find_ints.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\geometry.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\geometry.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\geometry.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\heap.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\heap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\heap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\hedges.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\hedges.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\hedges.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\info.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\info.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\info.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\intersect.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\intersect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\intersect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\legal.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\legal.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\legal.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\lu.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\lu.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\lu.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\matinv.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\matinv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\matinv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\memory.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\memory.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\memory.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\neatoinit.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\neatoinit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\neatoinit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\neatosplines.c

"$(INTDIR)\neatosplines.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\poly.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\poly.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\poly.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\printvis.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\printvis.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\printvis.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\site.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\site.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\site.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\solve.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\solve.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\solve.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\stuff.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\stuff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\stuff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\voronoi.c

!IF  "$(CFG)" == "neatogen - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\voronoi.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\pack" /I "..\gvrender" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neatogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\voronoi.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

!IF  "$(CFG)" == "neatogen - Win32 Release"

"gd - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Release" 
   cd "..\dotneato\neatogen"

"gd - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

"gd - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Debug" 
   cd "..\dotneato\neatogen"

"gd - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\gd"
   $(MAKE) /$(MAKEFLAGS) /F ".\gd.mak" CFG="gd - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ENDIF 

!IF  "$(CFG)" == "neatogen - Win32 Release"

"graph - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Release" 
   cd "..\dotneato\neatogen"

"graph - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

"graph - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Debug" 
   cd "..\dotneato\neatogen"

"graph - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graph"
   $(MAKE) /$(MAKEFLAGS) /F ".\graph.mak" CFG="graph - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ENDIF 

!IF  "$(CFG)" == "neatogen - Win32 Release"

"pathplan - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Release" 
   cd "..\dotneato\neatogen"

"pathplan - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ELSEIF  "$(CFG)" == "neatogen - Win32 Debug"

"pathplan - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Debug" 
   cd "..\dotneato\neatogen"

"pathplan - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\pathplan"
   $(MAKE) /$(MAKEFLAGS) /F ".\pathplan.mak" CFG="pathplan - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotneato\neatogen"

!ENDIF 


!ENDIF 

