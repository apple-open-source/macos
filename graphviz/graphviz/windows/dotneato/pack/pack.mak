# Microsoft Developer Studio Generated NMAKE File, Based on pack.dsp
!IF "$(CFG)" == ""
CFG=pack - Win32 Debug
!MESSAGE No configuration specified. Defaulting to pack - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "pack - Win32 Release" && "$(CFG)" != "pack - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pack.mak" CFG="pack - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pack - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "pack - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "pack - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\pack.lib"


CLEAN :
	-@erase "$(INTDIR)\ccomps.obj"
	-@erase "$(INTDIR)\pack.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\pack.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\common" /I "." /I "..\.." /I "..\..\pathplan" /I "..\..\graph" /I "..\..\cdt" /I "..\neatogen" /I "..\gvrender" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\pack.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\pack.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\pack.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ccomps.obj" \
	"$(INTDIR)\pack.obj"

"$(OUTDIR)\pack.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\pack.lib"
   copy .\Release\pack.lib ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "pack - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\pack.lib"


CLEAN :
	-@erase "$(INTDIR)\ccomps.obj"
	-@erase "$(INTDIR)\pack.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\pack.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "..\common" /I "." /I "..\.." /I "..\..\pathplan" /I "..\..\graph" /I "..\..\cdt" /I "..\neatogen" /I "..\gvrender" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\pack.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\pack.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\pack.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ccomps.obj" \
	"$(INTDIR)\pack.obj"

"$(OUTDIR)\pack.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\pack.lib"
   copy .\Debug\pack.lib ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("pack.dep")
!INCLUDE "pack.dep"
!ELSE 
!MESSAGE Warning: cannot find "pack.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "pack - Win32 Release" || "$(CFG)" == "pack - Win32 Debug"
SOURCE=.\ccomps.c

"$(INTDIR)\ccomps.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\pack.c

"$(INTDIR)\pack.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

