# Microsoft Developer Studio Generated NMAKE File, Based on agraph.dsp
!IF "$(CFG)" == ""
CFG=agraph - Win32 Debug
!MESSAGE No configuration specified. Defaulting to agraph - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "agraph - Win32 Release" && "$(CFG)" != "agraph - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "agraph.mak" CFG="agraph - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "agraph - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "agraph - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "agraph - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\agraph.lib"


CLEAN :
	-@erase "$(INTDIR)\agerror.obj"
	-@erase "$(INTDIR)\apply.obj"
	-@erase "$(INTDIR)\attr.obj"
	-@erase "$(INTDIR)\edge.obj"
	-@erase "$(INTDIR)\flatten.obj"
	-@erase "$(INTDIR)\grammar.obj"
	-@erase "$(INTDIR)\graph.obj"
	-@erase "$(INTDIR)\id.obj"
	-@erase "$(INTDIR)\imap.obj"
	-@erase "$(INTDIR)\io.obj"
	-@erase "$(INTDIR)\mem.obj"
	-@erase "$(INTDIR)\node.obj"
	-@erase "$(INTDIR)\obj.obj"
	-@erase "$(INTDIR)\pend.obj"
	-@erase "$(INTDIR)\rec.obj"
	-@erase "$(INTDIR)\refstr.obj"
	-@erase "$(INTDIR)\scan.obj"
	-@erase "$(INTDIR)\subg.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\write.obj"
	-@erase "$(OUTDIR)\agraph.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I ".." /I "../cdt" /D "NDEBUG" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /D YY_NEVER_INTERACTIVE=1 /Fp"$(INTDIR)\agraph.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\agraph.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\agraph.lib" 
LIB32_OBJS= \
	"$(INTDIR)\agerror.obj" \
	"$(INTDIR)\apply.obj" \
	"$(INTDIR)\attr.obj" \
	"$(INTDIR)\edge.obj" \
	"$(INTDIR)\flatten.obj" \
	"$(INTDIR)\grammar.obj" \
	"$(INTDIR)\graph.obj" \
	"$(INTDIR)\id.obj" \
	"$(INTDIR)\imap.obj" \
	"$(INTDIR)\io.obj" \
	"$(INTDIR)\mem.obj" \
	"$(INTDIR)\node.obj" \
	"$(INTDIR)\obj.obj" \
	"$(INTDIR)\pend.obj" \
	"$(INTDIR)\rec.obj" \
	"$(INTDIR)\refstr.obj" \
	"$(INTDIR)\scan.obj" \
	"$(INTDIR)\subg.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\write.obj"

"$(OUTDIR)\agraph.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\agraph.lib"
   copy .\Release\agraph.lib ..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "agraph - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\agraph.lib"


CLEAN :
	-@erase "$(INTDIR)\agerror.obj"
	-@erase "$(INTDIR)\apply.obj"
	-@erase "$(INTDIR)\attr.obj"
	-@erase "$(INTDIR)\edge.obj"
	-@erase "$(INTDIR)\flatten.obj"
	-@erase "$(INTDIR)\grammar.obj"
	-@erase "$(INTDIR)\graph.obj"
	-@erase "$(INTDIR)\id.obj"
	-@erase "$(INTDIR)\imap.obj"
	-@erase "$(INTDIR)\io.obj"
	-@erase "$(INTDIR)\mem.obj"
	-@erase "$(INTDIR)\node.obj"
	-@erase "$(INTDIR)\obj.obj"
	-@erase "$(INTDIR)\pend.obj"
	-@erase "$(INTDIR)\rec.obj"
	-@erase "$(INTDIR)\refstr.obj"
	-@erase "$(INTDIR)\scan.obj"
	-@erase "$(INTDIR)\subg.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\write.obj"
	-@erase "$(OUTDIR)\agraph.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I ".." /I "." /I "../cdt" /D "_DEBUG" /D "MSWIN32" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_LIB" /D YY_NEVER_INTERACTIVE=1 /Fp"$(INTDIR)\agraph.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\agraph.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\agraph.lib" 
LIB32_OBJS= \
	"$(INTDIR)\agerror.obj" \
	"$(INTDIR)\apply.obj" \
	"$(INTDIR)\attr.obj" \
	"$(INTDIR)\edge.obj" \
	"$(INTDIR)\flatten.obj" \
	"$(INTDIR)\grammar.obj" \
	"$(INTDIR)\graph.obj" \
	"$(INTDIR)\id.obj" \
	"$(INTDIR)\imap.obj" \
	"$(INTDIR)\io.obj" \
	"$(INTDIR)\mem.obj" \
	"$(INTDIR)\node.obj" \
	"$(INTDIR)\obj.obj" \
	"$(INTDIR)\pend.obj" \
	"$(INTDIR)\rec.obj" \
	"$(INTDIR)\refstr.obj" \
	"$(INTDIR)\scan.obj" \
	"$(INTDIR)\subg.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\write.obj"

"$(OUTDIR)\agraph.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\agraph.lib"
   copy .\Debug\agraph.lib ..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("agraph.dep")
!INCLUDE "agraph.dep"
!ELSE 
!MESSAGE Warning: cannot find "agraph.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "agraph - Win32 Release" || "$(CFG)" == "agraph - Win32 Debug"
SOURCE=.\agerror.c

"$(INTDIR)\agerror.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\apply.c

"$(INTDIR)\apply.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\attr.c

"$(INTDIR)\attr.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\edge.c

"$(INTDIR)\edge.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\flatten.c

"$(INTDIR)\flatten.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\grammar.c

"$(INTDIR)\grammar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\graph.c

"$(INTDIR)\graph.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\id.c

"$(INTDIR)\id.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\imap.c

"$(INTDIR)\imap.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\io.c

"$(INTDIR)\io.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\mem.c

"$(INTDIR)\mem.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\node.c

"$(INTDIR)\node.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\obj.c

"$(INTDIR)\obj.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\pend.c

"$(INTDIR)\pend.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\rec.c

"$(INTDIR)\rec.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\refstr.c

"$(INTDIR)\refstr.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\scan.c

"$(INTDIR)\scan.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\subg.c

"$(INTDIR)\subg.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\utils.c

"$(INTDIR)\utils.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\write.c

"$(INTDIR)\write.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

