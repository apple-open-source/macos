# Microsoft Developer Studio Generated NMAKE File, Based on graph.dsp
!IF "$(CFG)" == ""
CFG=graph - Win32 Debug
!MESSAGE No configuration specified. Defaulting to graph - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "graph - Win32 Release" && "$(CFG)" != "graph - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "graph.mak" CFG="graph - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "graph - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "graph - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "graph - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\graph.lib"


CLEAN :
	-@erase "$(INTDIR)\agxbuf.obj"
	-@erase "$(INTDIR)\attribs.obj"
	-@erase "$(INTDIR)\edge.obj"
	-@erase "$(INTDIR)\graph.obj"
	-@erase "$(INTDIR)\graphio.obj"
	-@erase "$(INTDIR)\lexer.obj"
	-@erase "$(INTDIR)\node.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\refstr.obj"
	-@erase "$(INTDIR)\trie.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\graph.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "../cdt" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\graph.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\graph.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\graph.lib" 
LIB32_OBJS= \
	"$(INTDIR)\agxbuf.obj" \
	"$(INTDIR)\attribs.obj" \
	"$(INTDIR)\edge.obj" \
	"$(INTDIR)\graph.obj" \
	"$(INTDIR)\graphio.obj" \
	"$(INTDIR)\lexer.obj" \
	"$(INTDIR)\node.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\refstr.obj" \
	"$(INTDIR)\trie.obj"

"$(OUTDIR)\graph.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\graph.lib"
   copy .\Release\graph.lib ..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "graph - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\graph.lib"


CLEAN :
	-@erase "$(INTDIR)\agxbuf.obj"
	-@erase "$(INTDIR)\attribs.obj"
	-@erase "$(INTDIR)\edge.obj"
	-@erase "$(INTDIR)\graph.obj"
	-@erase "$(INTDIR)\graphio.obj"
	-@erase "$(INTDIR)\lexer.obj"
	-@erase "$(INTDIR)\node.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\refstr.obj"
	-@erase "$(INTDIR)\trie.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\graph.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I "../cdt" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\graph.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\graph.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\graph.lib" 
LIB32_OBJS= \
	"$(INTDIR)\agxbuf.obj" \
	"$(INTDIR)\attribs.obj" \
	"$(INTDIR)\edge.obj" \
	"$(INTDIR)\graph.obj" \
	"$(INTDIR)\graphio.obj" \
	"$(INTDIR)\lexer.obj" \
	"$(INTDIR)\node.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\refstr.obj" \
	"$(INTDIR)\trie.obj"

"$(OUTDIR)\graph.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\graph.lib"
   copy .\Debug\graph.lib ..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("graph.dep")
!INCLUDE "graph.dep"
!ELSE 
!MESSAGE Warning: cannot find "graph.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "graph - Win32 Release" || "$(CFG)" == "graph - Win32 Debug"
SOURCE=.\agxbuf.c

"$(INTDIR)\agxbuf.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\attribs.c

"$(INTDIR)\attribs.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\edge.c

"$(INTDIR)\edge.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\graph.c

"$(INTDIR)\graph.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\graphio.c

"$(INTDIR)\graphio.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\lexer.c

"$(INTDIR)\lexer.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\node.c

"$(INTDIR)\node.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\parser.c

"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\refstr.c

"$(INTDIR)\refstr.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\trie.c

"$(INTDIR)\trie.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

