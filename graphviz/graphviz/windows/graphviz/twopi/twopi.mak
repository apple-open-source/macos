# Microsoft Developer Studio Generated NMAKE File, Based on twopi.dsp
!IF "$(CFG)" == ""
CFG=twopi - Win32 Debug
!MESSAGE No configuration specified. Defaulting to twopi - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "twopi - Win32 Release" && "$(CFG)" != "twopi - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "twopi.mak" CFG="twopi - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "twopi - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "twopi - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "twopi - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\twopi.exe"

!ELSE 

ALL : "twopigen - Win32 Release" "$(OUTDIR)\twopi.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"twopigen - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\twopi.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\twopi.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../../dotneato/twopigen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\twopi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\twopi.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=pack.lib gd.lib graph.lib cdt.lib common.lib twopigen.lib neatogen.lib gvrender.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\twopi.pdb" /machine:I386 /out:"$(OUTDIR)\twopi.exe" /libpath:"..\..\makearch\win32\static\Release" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\twopi.obj" \
	"..\..\dotneato\twopigen\Release\twopigen.lib"

"$(OUTDIR)\twopi.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "twopigen - Win32 Release" "$(OUTDIR)\twopi.exe"
   copy .\Release\twopi.exe ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "twopi - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\twopi.exe"

!ELSE 

ALL : "twopigen - Win32 Debug" "$(OUTDIR)\twopi.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"twopigen - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\twopi.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\twopi.exe"
	-@erase "$(OUTDIR)\twopi.ilk"
	-@erase "$(OUTDIR)\twopi.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "../../dotneato/twopigen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\twopi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\twopi.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=gd.lib graph.lib cdt.lib common.lib twopigen.lib neatogen.lib gvrender.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\twopi.pdb" /debug /machine:I386 /out:"$(OUTDIR)\twopi.exe" /pdbtype:sept /libpath:"..\..\makearch\win32\static\Debug" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\twopi.obj" \
	"..\..\dotneato\twopigen\Debug\twopigen.lib"

"$(OUTDIR)\twopi.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "twopigen - Win32 Debug" "$(OUTDIR)\twopi.exe"
   copy .\Debug\twopi.exe ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("twopi.dep")
!INCLUDE "twopi.dep"
!ELSE 
!MESSAGE Warning: cannot find "twopi.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "twopi - Win32 Release" || "$(CFG)" == "twopi - Win32 Debug"
SOURCE=..\..\dotneato\twopi.c

"$(INTDIR)\twopi.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "twopi - Win32 Release"

"twopigen - Win32 Release" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\twopigen"
   $(MAKE) /$(MAKEFLAGS) /F ".\twopigen.mak" CFG="twopigen - Win32 Release" 
   cd "..\..\graphviz\twopi"

"twopigen - Win32 ReleaseCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\twopigen"
   $(MAKE) /$(MAKEFLAGS) /F ".\twopigen.mak" CFG="twopigen - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\graphviz\twopi"

!ELSEIF  "$(CFG)" == "twopi - Win32 Debug"

"twopigen - Win32 Debug" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\twopigen"
   $(MAKE) /$(MAKEFLAGS) /F ".\twopigen.mak" CFG="twopigen - Win32 Debug" 
   cd "..\..\graphviz\twopi"

"twopigen - Win32 DebugCLEAN" : 
   cd "\graphvizCVS\builddaemon\graphviz-win\dotneato\twopigen"
   $(MAKE) /$(MAKEFLAGS) /F ".\twopigen.mak" CFG="twopigen - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\graphviz\twopi"

!ENDIF 


!ENDIF 

