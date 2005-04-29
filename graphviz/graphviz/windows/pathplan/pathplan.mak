# Microsoft Developer Studio Generated NMAKE File, Based on pathplan.dsp
!IF "$(CFG)" == ""
CFG=pathplan - Win32 Debug
!MESSAGE No configuration specified. Defaulting to pathplan - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "pathplan - Win32 Release" && "$(CFG)" != "pathplan - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pathplan.mak" CFG="pathplan - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pathplan - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pathplan - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "pathplan - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\pathplan.lib"


CLEAN :
	-@erase "$(INTDIR)\cvt.obj"
	-@erase "$(INTDIR)\inpoly.obj"
	-@erase "$(INTDIR)\route.obj"
	-@erase "$(INTDIR)\shortest.obj"
	-@erase "$(INTDIR)\shortestpth.obj"
	-@erase "$(INTDIR)\solvers.obj"
	-@erase "$(INTDIR)\triang.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\visibility.obj"
	-@erase "$(OUTDIR)\pathplan.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I ".." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\pathplan.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\pathplan.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\pathplan.lib" 
LIB32_OBJS= \
	"$(INTDIR)\cvt.obj" \
	"$(INTDIR)\inpoly.obj" \
	"$(INTDIR)\route.obj" \
	"$(INTDIR)\shortest.obj" \
	"$(INTDIR)\shortestpth.obj" \
	"$(INTDIR)\solvers.obj" \
	"$(INTDIR)\triang.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\visibility.obj"

"$(OUTDIR)\pathplan.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\pathplan.lib"
   copy .\Release\pathplan.lib ..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "pathplan - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\pathplan.lib"


CLEAN :
	-@erase "$(INTDIR)\cvt.obj"
	-@erase "$(INTDIR)\inpoly.obj"
	-@erase "$(INTDIR)\route.obj"
	-@erase "$(INTDIR)\shortest.obj"
	-@erase "$(INTDIR)\shortestpth.obj"
	-@erase "$(INTDIR)\solvers.obj"
	-@erase "$(INTDIR)\triang.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\visibility.obj"
	-@erase "$(OUTDIR)\pathplan.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I ".." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\pathplan.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\pathplan.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\pathplan.lib" 
LIB32_OBJS= \
	"$(INTDIR)\cvt.obj" \
	"$(INTDIR)\inpoly.obj" \
	"$(INTDIR)\route.obj" \
	"$(INTDIR)\shortest.obj" \
	"$(INTDIR)\shortestpth.obj" \
	"$(INTDIR)\solvers.obj" \
	"$(INTDIR)\triang.obj" \
	"$(INTDIR)\util.obj" \
	"$(INTDIR)\visibility.obj"

"$(OUTDIR)\pathplan.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\pathplan.lib"
   copy .\Debug\pathplan.lib ..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("pathplan.dep")
!INCLUDE "pathplan.dep"
!ELSE 
!MESSAGE Warning: cannot find "pathplan.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "pathplan - Win32 Release" || "$(CFG)" == "pathplan - Win32 Debug"
SOURCE=.\cvt.c

"$(INTDIR)\cvt.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\inpoly.c

"$(INTDIR)\inpoly.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\route.c

"$(INTDIR)\route.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\shortest.c

"$(INTDIR)\shortest.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\shortestpth.c

"$(INTDIR)\shortestpth.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\solvers.c

"$(INTDIR)\solvers.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\triang.c

"$(INTDIR)\triang.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\util.c

"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\visibility.c

"$(INTDIR)\visibility.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

