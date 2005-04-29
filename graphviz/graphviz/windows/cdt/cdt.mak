# Microsoft Developer Studio Generated NMAKE File, Based on cdt.dsp
!IF "$(CFG)" == ""
CFG=cdt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to cdt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "cdt - Win32 Release" && "$(CFG)" != "cdt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cdt.mak" CFG="cdt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cdt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "cdt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "cdt - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\cdt.lib"


CLEAN :
	-@erase "$(INTDIR)\dtclose.obj"
	-@erase "$(INTDIR)\dtdisc.obj"
	-@erase "$(INTDIR)\dtextract.obj"
	-@erase "$(INTDIR)\dtflatten.obj"
	-@erase "$(INTDIR)\dthash.obj"
	-@erase "$(INTDIR)\dtlist.obj"
	-@erase "$(INTDIR)\dtmethod.obj"
	-@erase "$(INTDIR)\dtopen.obj"
	-@erase "$(INTDIR)\dtrenew.obj"
	-@erase "$(INTDIR)\dtrestore.obj"
	-@erase "$(INTDIR)\dtsize.obj"
	-@erase "$(INTDIR)\dtstat.obj"
	-@erase "$(INTDIR)\dtstrhash.obj"
	-@erase "$(INTDIR)\dttree.obj"
	-@erase "$(INTDIR)\dtview.obj"
	-@erase "$(INTDIR)\dtwalk.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\cdt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\cdt.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\cdt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\dtclose.obj" \
	"$(INTDIR)\dtdisc.obj" \
	"$(INTDIR)\dtextract.obj" \
	"$(INTDIR)\dtflatten.obj" \
	"$(INTDIR)\dthash.obj" \
	"$(INTDIR)\dtlist.obj" \
	"$(INTDIR)\dtmethod.obj" \
	"$(INTDIR)\dtopen.obj" \
	"$(INTDIR)\dtrenew.obj" \
	"$(INTDIR)\dtrestore.obj" \
	"$(INTDIR)\dtsize.obj" \
	"$(INTDIR)\dtstat.obj" \
	"$(INTDIR)\dtstrhash.obj" \
	"$(INTDIR)\dttree.obj" \
	"$(INTDIR)\dtview.obj" \
	"$(INTDIR)\dtwalk.obj"

"$(OUTDIR)\cdt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\cdt.lib"
   copy .\Release\cdt.lib ..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\cdt.lib"


CLEAN :
	-@erase "$(INTDIR)\dtclose.obj"
	-@erase "$(INTDIR)\dtdisc.obj"
	-@erase "$(INTDIR)\dtextract.obj"
	-@erase "$(INTDIR)\dtflatten.obj"
	-@erase "$(INTDIR)\dthash.obj"
	-@erase "$(INTDIR)\dtlist.obj"
	-@erase "$(INTDIR)\dtmethod.obj"
	-@erase "$(INTDIR)\dtopen.obj"
	-@erase "$(INTDIR)\dtrenew.obj"
	-@erase "$(INTDIR)\dtrestore.obj"
	-@erase "$(INTDIR)\dtsize.obj"
	-@erase "$(INTDIR)\dtstat.obj"
	-@erase "$(INTDIR)\dtstrhash.obj"
	-@erase "$(INTDIR)\dttree.obj"
	-@erase "$(INTDIR)\dtview.obj"
	-@erase "$(INTDIR)\dtwalk.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\cdt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\cdt.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\cdt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\dtclose.obj" \
	"$(INTDIR)\dtdisc.obj" \
	"$(INTDIR)\dtextract.obj" \
	"$(INTDIR)\dtflatten.obj" \
	"$(INTDIR)\dthash.obj" \
	"$(INTDIR)\dtlist.obj" \
	"$(INTDIR)\dtmethod.obj" \
	"$(INTDIR)\dtopen.obj" \
	"$(INTDIR)\dtrenew.obj" \
	"$(INTDIR)\dtrestore.obj" \
	"$(INTDIR)\dtsize.obj" \
	"$(INTDIR)\dtstat.obj" \
	"$(INTDIR)\dtstrhash.obj" \
	"$(INTDIR)\dttree.obj" \
	"$(INTDIR)\dtview.obj" \
	"$(INTDIR)\dtwalk.obj"

"$(OUTDIR)\cdt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\cdt.lib"
   copy .\Debug\cdt.lib ..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("cdt.dep")
!INCLUDE "cdt.dep"
!ELSE 
!MESSAGE Warning: cannot find "cdt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "cdt - Win32 Release" || "$(CFG)" == "cdt - Win32 Debug"
SOURCE=.\dtclose.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtclose.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtclose.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtdisc.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtdisc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtdisc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtextract.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtextract.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtextract.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtflatten.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtflatten.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtflatten.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dthash.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dthash.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dthash.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtlist.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtlist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtlist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtmethod.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtmethod.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtmethod.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtopen.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtopen.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtopen.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtrenew.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtrenew.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtrenew.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtrestore.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtrestore.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtrestore.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtsize.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtsize.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtsize.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtstat.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtstat.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtstat.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtstrhash.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtstrhash.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtstrhash.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dttree.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dttree.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dttree.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtview.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dtwalk.c

!IF  "$(CFG)" == "cdt - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dtwalk.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "cdt - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /Fp"$(INTDIR)\cdt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\dtwalk.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 


!ENDIF 

