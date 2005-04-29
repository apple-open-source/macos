# Microsoft Developer Studio Generated NMAKE File, Based on circo.dsp
!IF "$(CFG)" == ""
CFG=circo - Win32 Debug
!MESSAGE No configuration specified. Defaulting to circo - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "circo - Win32 Release" && "$(CFG)" != "circo - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "circo.mak" CFG="circo - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "circo - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "circo - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "circo - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\circo.exe"


CLEAN :
	-@erase "$(INTDIR)\circo.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\circo.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "../../dotneato/circogen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\circo.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\circo.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=pack.lib gd.lib graph.lib cdt.lib common.lib circogen.lib neatogen.lib gvrender.lib z.lib png.lib ttf.lib jpeg.lib ft.lib libexpat.lib libexpatw.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\circo.pdb" /machine:I386 /out:"$(OUTDIR)\circo.exe" /libpath:"..\..\makearch\win32\static\Release" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\circo.obj"

"$(OUTDIR)\circo.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\circo.exe"
   copy .\Release\circo.exe ..\..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "circo - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\circo.exe"


CLEAN :
	-@erase "$(INTDIR)\circo.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\circo.exe"
	-@erase "$(OUTDIR)\circo.ilk"
	-@erase "$(OUTDIR)\circo.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "../../dotneato/circogen" /I "../../dotneato/common" /I "../../dotneato/gvrender" /I "../.." /I "../../pathplan" /I "../../cdt" /I "../../gd" /I "../../graph" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\circo.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\circo.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=pack.lib gd.lib graph.lib cdt.lib common.lib circogen.lib neatogen.lib gvrender.lib libexpat.lib libexpatw.lib z.lib png.lib ttf.lib jpeg.lib ft.lib libexpat.lib libexpatw.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\circo.pdb" /debug /machine:I386 /out:"$(OUTDIR)\circo.exe" /pdbtype:sept /libpath:"..\..\makearch\win32\static\Debug" /libpath:"..\..\third-party\lib" 
LINK32_OBJS= \
	"$(INTDIR)\circo.obj"

"$(OUTDIR)\circo.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\circo.exe"
   copy .\Debug\circo.exe ..\..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("circo.dep")
!INCLUDE "circo.dep"
!ELSE 
!MESSAGE Warning: cannot find "circo.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "circo - Win32 Release" || "$(CFG)" == "circo - Win32 Debug"
SOURCE=..\..\dotneato\circo.c

"$(INTDIR)\circo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

