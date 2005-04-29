# Microsoft Developer Studio Generated NMAKE File, Based on neato.dsp
!IF "$(CFG)" == ""
CFG=neato - Win32 Debug
!MESSAGE No configuration specified. Defaulting to neato - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "neato - Win32 Release" && "$(CFG)" != "neato - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "neato.mak" CFG="neato - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "neato - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "neato - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "neato - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\neato.exe"

!ELSE 

ALL : "pack - Win32 Release" "cdt - Win32 Release" "neatogen - Win32 Release" "common - Win32 Release" "$(OUTDIR)\neato.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"common - Win32 ReleaseCLEAN" "neatogen - Win32 ReleaseCLEAN" "cdt - Win32 ReleaseCLEAN" "pack - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\neato.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\neato.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../../dotneato/neatogen" /I "../../dotneato/gvrender" /I "../../dotneato/common" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neato.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neato.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=gd.lib graph.lib cdt.lib common.lib gvrender.lib neatogen.lib pack.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\neato.pdb" /machine:I386 /out:"$(OUTDIR)\neato.exe" /libpath:"..\..\makearch\win32\static\Release" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\neato.obj" \
	"..\..\dotneato\common\Release\common.lib" \
	"..\..\dotneato\neatogen\Release\neatogen.lib" \
	"..\..\cdt\Release\cdt.lib" \
	"..\..\dotneato\pack\Release\pack.lib"

"$(OUTDIR)\neato.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pack - Win32 Release" "cdt - Win32 Release" "neatogen - Win32 Release" "common - Win32 Release" "$(OUTDIR)\neato.exe"
   copy .\Release\neato.exe ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "neato - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\neato.exe"

!ELSE 

ALL : "pack - Win32 Debug" "cdt - Win32 Debug" "neatogen - Win32 Debug" "common - Win32 Debug" "$(OUTDIR)\neato.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"common - Win32 DebugCLEAN" "neatogen - Win32 DebugCLEAN" "cdt - Win32 DebugCLEAN" "pack - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\neato.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\neato.exe"
	-@erase "$(OUTDIR)\neato.ilk"
	-@erase "$(OUTDIR)\neato.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "../../dotneato/neatogen" /I "../../dotneato/gvrender" /I "../../dotneato/common" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\neato.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neato.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=pathplan.lib gd.lib graph.lib cdt.lib common.lib gvrender.lib neatogen.lib pack.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\neato.pdb" /debug /machine:I386 /out:"$(OUTDIR)\neato.exe" /pdbtype:sept /libpath:"..\..\makearch\win32\static\Debug" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\neato.obj" \
	"..\..\dotneato\common\Debug\common.lib" \
	"..\..\dotneato\neatogen\Debug\neatogen.lib" \
	"..\..\cdt\Debug\cdt.lib" \
	"..\..\dotneato\pack\Debug\pack.lib"

"$(OUTDIR)\neato.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "pack - Win32 Debug" "cdt - Win32 Debug" "neatogen - Win32 Debug" "common - Win32 Debug" "$(OUTDIR)\neato.exe"
   copy .\Debug\neato.exe ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("neato.dep")
!INCLUDE "neato.dep"
!ELSE 
!MESSAGE Warning: cannot find "neato.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "neato - Win32 Release" || "$(CFG)" == "neato - Win32 Debug"
SOURCE=..\..\dotneato\neato.c

"$(INTDIR)\neato.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "neato - Win32 Release"

"common - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Release" 
   cd "..\..\graphviz\neato"

"common - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ELSEIF  "$(CFG)" == "neato - Win32 Debug"

"common - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Debug" 
   cd "..\..\graphviz\neato"

"common - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ENDIF 

!IF  "$(CFG)" == "neato - Win32 Release"

"neatogen - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\neatogen"
   $(MAKE) /$(MAKEFLAGS) /F ".\neatogen.mak" CFG="neatogen - Win32 Release" 
   cd "..\..\graphviz\neato"

"neatogen - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\neatogen"
   $(MAKE) /$(MAKEFLAGS) /F ".\neatogen.mak" CFG="neatogen - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ELSEIF  "$(CFG)" == "neato - Win32 Debug"

"neatogen - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\neatogen"
   $(MAKE) /$(MAKEFLAGS) /F ".\neatogen.mak" CFG="neatogen - Win32 Debug" 
   cd "..\..\graphviz\neato"

"neatogen - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\neatogen"
   $(MAKE) /$(MAKEFLAGS) /F ".\neatogen.mak" CFG="neatogen - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ENDIF 

!IF  "$(CFG)" == "neato - Win32 Release"

"cdt - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Release" 
   cd "..\graphviz\neato"

"cdt - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Release" RECURSE=1 CLEAN 
   cd "..\graphviz\neato"

!ELSEIF  "$(CFG)" == "neato - Win32 Debug"

"cdt - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Debug" 
   cd "..\graphviz\neato"

"cdt - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\graphviz\neato"

!ENDIF 

!IF  "$(CFG)" == "neato - Win32 Release"

"pack - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Release" 
   cd "..\..\graphviz\neato"

"pack - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ELSEIF  "$(CFG)" == "neato - Win32 Debug"

"pack - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Debug" 
   cd "..\..\graphviz\neato"

"pack - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\pack"
   $(MAKE) /$(MAKEFLAGS) /F ".\pack.mak" CFG="pack - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\neato"

!ENDIF 


!ENDIF 

