# Microsoft Developer Studio Generated NMAKE File, Based on dotty.dsp
!IF "$(CFG)" == ""
CFG=dotty - Win32 Debug
!MESSAGE No configuration specified. Defaulting to dotty - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "dotty - Win32 Release" && "$(CFG)" != "dotty - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dotty.mak" CFG="dotty - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dotty - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "dotty - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "dotty - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dotty.exe"

!ELSE 

ALL : "lefty - Win32 Release" "dot - Win32 Release" "$(OUTDIR)\dotty.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"dot - Win32 ReleaseCLEAN" "lefty - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dotty.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\dotty.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "MSWIN32" /Fp"$(INTDIR)\dotty.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dotty.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\dotty.pdb" /machine:I386 /out:"$(OUTDIR)\dotty.exe" 
LINK32_OBJS= \
	"$(INTDIR)\dotty.obj"

"$(OUTDIR)\dotty.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "lefty - Win32 Release" "dot - Win32 Release" "$(OUTDIR)\dotty.exe"
   copy   .\Release\dotty.exe   ..\makearch\win32\static\Release\dotty.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "dotty - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dotty.exe"

!ELSE 

ALL : "lefty - Win32 Debug" "dot - Win32 Debug" "$(OUTDIR)\dotty.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"dot - Win32 DebugCLEAN" "lefty - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dotty.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\dotty.exe"
	-@erase "$(OUTDIR)\dotty.ilk"
	-@erase "$(OUTDIR)\dotty.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "MSWIN32" /Fp"$(INTDIR)\dotty.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dotty.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\dotty.pdb" /debug /machine:I386 /out:"$(OUTDIR)\dotty.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\dotty.obj"

"$(OUTDIR)\dotty.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "lefty - Win32 Debug" "dot - Win32 Debug" "$(OUTDIR)\dotty.exe"
   copy   .\Debug\dotty.exe   ..\makearch\win32\static\Debug\dotty.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("dotty.dep")
!INCLUDE "dotty.dep"
!ELSE 
!MESSAGE Warning: cannot find "dotty.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dotty - Win32 Release" || "$(CFG)" == "dotty - Win32 Debug"
SOURCE=.\mswin32\dotty.c

"$(INTDIR)\dotty.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "dotty - Win32 Release"

"dot - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\dot"
   $(MAKE) /$(MAKEFLAGS) /F ".\dot.mak" CFG="dot - Win32 Release" 
   cd "..\..\dotty"

"dot - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\dot"
   $(MAKE) /$(MAKEFLAGS) /F ".\dot.mak" CFG="dot - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\dotty"

!ELSEIF  "$(CFG)" == "dotty - Win32 Debug"

"dot - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\dot"
   $(MAKE) /$(MAKEFLAGS) /F ".\dot.mak" CFG="dot - Win32 Debug" 
   cd "..\..\dotty"

"dot - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\dot"
   $(MAKE) /$(MAKEFLAGS) /F ".\dot.mak" CFG="dot - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\dotty"

!ENDIF 

!IF  "$(CFG)" == "dotty - Win32 Release"

"lefty - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Release" 
   cd "..\dotty"

"lefty - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Release" RECURSE=1 CLEAN 
   cd "..\dotty"

!ELSEIF  "$(CFG)" == "dotty - Win32 Debug"

"lefty - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Debug" 
   cd "..\dotty"

"lefty - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\dotty"

!ENDIF 


!ENDIF 

