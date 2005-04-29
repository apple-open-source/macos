# Microsoft Developer Studio Generated NMAKE File, Based on circogen.dsp
!IF "$(CFG)" == ""
CFG=circogen - Win32 Debug
!MESSAGE No configuration specified. Defaulting to circogen - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "circogen - Win32 Release" && "$(CFG)" != "circogen - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "circogen.mak" CFG="circogen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "circogen - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "circogen - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "circogen - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\circogen.lib"


CLEAN :
	-@erase "$(INTDIR)\block.obj"
	-@erase "$(INTDIR)\blockpath.obj"
	-@erase "$(INTDIR)\blocktree.obj"
	-@erase "$(INTDIR)\circpos.obj"
	-@erase "$(INTDIR)\circular.obj"
	-@erase "$(INTDIR)\circularinit.obj"
	-@erase "$(INTDIR)\deglist.obj"
	-@erase "$(INTDIR)\edgelist.obj"
	-@erase "$(INTDIR)\nodelist.obj"
	-@erase "$(INTDIR)\nodeset.obj"
	-@erase "$(INTDIR)\stack.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\circogen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "..\.." /I "..\common" /I "..\neatogen" /I "..\gvrender" /I "..\pack" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\circogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\circogen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\circogen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\block.obj" \
	"$(INTDIR)\blockpath.obj" \
	"$(INTDIR)\blocktree.obj" \
	"$(INTDIR)\circpos.obj" \
	"$(INTDIR)\circular.obj" \
	"$(INTDIR)\circularinit.obj" \
	"$(INTDIR)\deglist.obj" \
	"$(INTDIR)\edgelist.obj" \
	"$(INTDIR)\nodelist.obj" \
	"$(INTDIR)\nodeset.obj" \
	"$(INTDIR)\stack.obj"

"$(OUTDIR)\circogen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\circogen.lib"
   copy .\Release\circogen.lib ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "circogen - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\circogen.lib"


CLEAN :
	-@erase "$(INTDIR)\block.obj"
	-@erase "$(INTDIR)\blockpath.obj"
	-@erase "$(INTDIR)\blocktree.obj"
	-@erase "$(INTDIR)\circpos.obj"
	-@erase "$(INTDIR)\circular.obj"
	-@erase "$(INTDIR)\circularinit.obj"
	-@erase "$(INTDIR)\deglist.obj"
	-@erase "$(INTDIR)\edgelist.obj"
	-@erase "$(INTDIR)\nodelist.obj"
	-@erase "$(INTDIR)\nodeset.obj"
	-@erase "$(INTDIR)\stack.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\circogen.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I "..\.." /I "..\common" /I "..\neatogen" /I "..\gvrender" /I "..\pack" /I "..\..\gd" /I "..\..\cdt" /I "..\..\graph" /I "..\..\pathplan" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\circogen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\circogen.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\circogen.lib" 
LIB32_OBJS= \
	"$(INTDIR)\block.obj" \
	"$(INTDIR)\blockpath.obj" \
	"$(INTDIR)\blocktree.obj" \
	"$(INTDIR)\circpos.obj" \
	"$(INTDIR)\circular.obj" \
	"$(INTDIR)\circularinit.obj" \
	"$(INTDIR)\deglist.obj" \
	"$(INTDIR)\edgelist.obj" \
	"$(INTDIR)\nodelist.obj" \
	"$(INTDIR)\nodeset.obj" \
	"$(INTDIR)\stack.obj"

"$(OUTDIR)\circogen.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\circogen.lib"
   copy .\Debug\circogen.lib ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("circogen.dep")
!INCLUDE "circogen.dep"
!ELSE 
!MESSAGE Warning: cannot find "circogen.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "circogen - Win32 Release" || "$(CFG)" == "circogen - Win32 Debug"
SOURCE=.\block.c

"$(INTDIR)\block.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\blockpath.c

"$(INTDIR)\blockpath.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\blocktree.c

"$(INTDIR)\blocktree.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\circpos.c

"$(INTDIR)\circpos.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\circular.c

"$(INTDIR)\circular.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\circularinit.c

"$(INTDIR)\circularinit.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\deglist.c

"$(INTDIR)\deglist.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\edgelist.c

"$(INTDIR)\edgelist.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\nodelist.c

"$(INTDIR)\nodelist.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\nodeset.c

"$(INTDIR)\nodeset.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\stack.c

"$(INTDIR)\stack.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

