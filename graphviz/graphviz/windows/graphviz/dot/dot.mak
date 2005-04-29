# Microsoft Developer Studio Generated NMAKE File, Based on dot.dsp
!IF "$(CFG)" == ""
CFG=dot - Win32 Debug
!MESSAGE No configuration specified. Defaulting to dot - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "dot - Win32 Release" && "$(CFG)" != "dot - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "dot.mak" CFG="dot - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "dot - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "dot - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "dot - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dot.exe"

!ELSE 

ALL : "cdt - Win32 Release" "dotgen - Win32 Release" "common - Win32 Release" "$(OUTDIR)\dot.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"common - Win32 ReleaseCLEAN" "dotgen - Win32 ReleaseCLEAN" "cdt - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dot.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\dot.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../../dotneato/dotgen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\dot.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dot.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=gd.lib graph.lib cdt.lib common.lib gvrender.lib dotgen.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\dot.pdb" /machine:I386 /out:"$(OUTDIR)\dot.exe" /libpath:"..\..\makearch\win32\static\Release" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\dot.obj" \
	"..\..\dotneato\common\Release\common.lib" \
	"..\..\dotneato\dotgen\Release\dotgen.lib" \
	"..\..\cdt\Release\cdt.lib"

"$(OUTDIR)\dot.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "cdt - Win32 Release" "dotgen - Win32 Release" "common - Win32 Release" "$(OUTDIR)\dot.exe"
   copy .\Release\dot.exe ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "dot - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\dot.exe"

!ELSE 

ALL : "cdt - Win32 Debug" "dotgen - Win32 Debug" "common - Win32 Debug" "$(OUTDIR)\dot.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"common - Win32 DebugCLEAN" "dotgen - Win32 DebugCLEAN" "cdt - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\dot.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\dot.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "../../dotneato/dotgen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\dot.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\dot.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=gd.lib graph.lib cdt.lib common.lib gvrender.lib dotgen.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /profile /debug /machine:I386 /out:"$(OUTDIR)\dot.exe" /libpath:"..\..\makearch\win32\static\Debug" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\dot.obj" \
	"..\..\dotneato\common\Debug\common.lib" \
	"..\..\dotneato\dotgen\Debug\dotgen.lib" \
	"..\..\cdt\Debug\cdt.lib"

"$(OUTDIR)\dot.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "cdt - Win32 Debug" "dotgen - Win32 Debug" "common - Win32 Debug" "$(OUTDIR)\dot.exe"
   copy .\Debug\dot.exe ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("dot.dep")
!INCLUDE "dot.dep"
!ELSE 
!MESSAGE Warning: cannot find "dot.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "dot - Win32 Release" || "$(CFG)" == "dot - Win32 Debug"
SOURCE=..\..\dotneato\dot.c

"$(INTDIR)\dot.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "dot - Win32 Release"

"common - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Release" 
   cd "..\..\graphviz\dot"

"common - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\dot"

!ELSEIF  "$(CFG)" == "dot - Win32 Debug"

"common - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Debug" 
   cd "..\..\graphviz\dot"

"common - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\common"
   $(MAKE) /$(MAKEFLAGS) /F ".\common.mak" CFG="common - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\dot"

!ENDIF 

!IF  "$(CFG)" == "dot - Win32 Release"

"dotgen - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\dotgen"
   $(MAKE) /$(MAKEFLAGS) /F ".\dotgen.mak" CFG="dotgen - Win32 Release" 
   cd "..\..\graphviz\dot"

"dotgen - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\dotgen"
   $(MAKE) /$(MAKEFLAGS) /F ".\dotgen.mak" CFG="dotgen - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\dot"

!ELSEIF  "$(CFG)" == "dot - Win32 Debug"

"dotgen - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\dotgen"
   $(MAKE) /$(MAKEFLAGS) /F ".\dotgen.mak" CFG="dotgen - Win32 Debug" 
   cd "..\..\graphviz\dot"

"dotgen - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\dotgen"
   $(MAKE) /$(MAKEFLAGS) /F ".\dotgen.mak" CFG="dotgen - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\dot"

!ENDIF 

!IF  "$(CFG)" == "dot - Win32 Release"

"cdt - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Release" 
   cd "..\graphviz\dot"

"cdt - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Release" RECURSE=1 CLEAN 
   cd "..\graphviz\dot"

!ELSEIF  "$(CFG)" == "dot - Win32 Debug"

"cdt - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Debug" 
   cd "..\graphviz\dot"

"cdt - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\cdt"
   $(MAKE) /$(MAKEFLAGS) /F ".\cdt.mak" CFG="cdt - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\graphviz\dot"

!ENDIF 


!ENDIF 

