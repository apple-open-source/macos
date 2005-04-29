# Microsoft Developer Studio Generated NMAKE File, Based on lneato.dsp
!IF "$(CFG)" == ""
CFG=lneato - Win32 Debug
!MESSAGE No configuration specified. Defaulting to lneato - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "lneato - Win32 Release" && "$(CFG)" != "lneato - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lneato.mak" CFG="lneato - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lneato - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "lneato - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "lneato - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\lneato.exe"

!ELSE 

ALL : "neato - Win32 Release" "lefty - Win32 Release" "$(OUTDIR)\lneato.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"lefty - Win32 ReleaseCLEAN" "neato - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\lneato.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\lneato.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "MSWIN32" /Fp"$(INTDIR)\lneato.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\lneato.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\lneato.pdb" /machine:I386 /out:"$(OUTDIR)\lneato.exe" 
LINK32_OBJS= \
	"$(INTDIR)\lneato.obj"

"$(OUTDIR)\lneato.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "neato - Win32 Release" "lefty - Win32 Release" "$(OUTDIR)\lneato.exe"
   copy  .\Release\lneato.exe  ..\makearch\win32\static\Release\lneato.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "lneato - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\lneato.exe"

!ELSE 

ALL : "neato - Win32 Debug" "lefty - Win32 Debug" "$(OUTDIR)\lneato.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"lefty - Win32 DebugCLEAN" "neato - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\lneato.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\lneato.exe"
	-@erase "$(OUTDIR)\lneato.ilk"
	-@erase "$(OUTDIR)\lneato.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "MSWIN32" /Fp"$(INTDIR)\lneato.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\lneato.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\lneato.pdb" /debug /machine:I386 /out:"$(OUTDIR)\lneato.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\lneato.obj"

"$(OUTDIR)\lneato.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "neato - Win32 Debug" "lefty - Win32 Debug" "$(OUTDIR)\lneato.exe"
   copy  .\Debug\lneato.exe  ..\makearch\win32\static\Debug\lneato.exe
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("lneato.dep")
!INCLUDE "lneato.dep"
!ELSE 
!MESSAGE Warning: cannot find "lneato.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "lneato - Win32 Release" || "$(CFG)" == "lneato - Win32 Debug"
SOURCE=.\mswin32\lneato.c

"$(INTDIR)\lneato.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "lneato - Win32 Release"

"lefty - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Release" 
   cd "..\lneato"

"lefty - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Release" RECURSE=1 CLEAN 
   cd "..\lneato"

!ELSEIF  "$(CFG)" == "lneato - Win32 Debug"

"lefty - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Debug" 
   cd "..\lneato"

"lefty - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\lefty"
   $(MAKE) /$(MAKEFLAGS) /F ".\lefty.mak" CFG="lefty - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\lneato"

!ENDIF 

!IF  "$(CFG)" == "lneato - Win32 Release"

"neato - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\neato"
   $(MAKE) /$(MAKEFLAGS) /F ".\neato.mak" CFG="neato - Win32 Release" 
   cd "..\..\lneato"

"neato - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\neato"
   $(MAKE) /$(MAKEFLAGS) /F ".\neato.mak" CFG="neato - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\lneato"

!ELSEIF  "$(CFG)" == "lneato - Win32 Debug"

"neato - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\neato"
   $(MAKE) /$(MAKEFLAGS) /F ".\neato.mak" CFG="neato - Win32 Debug" 
   cd "..\..\lneato"

"neato - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\graphviz\neato"
   $(MAKE) /$(MAKEFLAGS) /F ".\neato.mak" CFG="neato - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\lneato"

!ENDIF 


!ENDIF 

