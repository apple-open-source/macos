# Microsoft Developer Studio Generated NMAKE File, Based on gd.dsp
!IF "$(CFG)" == ""
CFG=gd - Win32 Debug
!MESSAGE No configuration specified. Defaulting to gd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "gd - Win32 Release" && "$(CFG)" != "gd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gd.mak" CFG="gd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gd - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "gd - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "gd - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\gd.lib"


CLEAN :
	-@erase "$(INTDIR)\gd.obj"
	-@erase "$(INTDIR)\gd_gd.obj"
	-@erase "$(INTDIR)\gd_gd2.obj"
	-@erase "$(INTDIR)\gd_gif.obj"
	-@erase "$(INTDIR)\gd_io.obj"
	-@erase "$(INTDIR)\gd_io_dp.obj"
	-@erase "$(INTDIR)\gd_io_file.obj"
	-@erase "$(INTDIR)\gd_io_ss.obj"
	-@erase "$(INTDIR)\gd_jpeg.obj"
	-@erase "$(INTDIR)\gd_png.obj"
	-@erase "$(INTDIR)\gd_ss.obj"
	-@erase "$(INTDIR)\gd_wbmp.obj"
	-@erase "$(INTDIR)\gdcache.obj"
	-@erase "$(INTDIR)\gdfontg.obj"
	-@erase "$(INTDIR)\gdfontl.obj"
	-@erase "$(INTDIR)\gdfontmb.obj"
	-@erase "$(INTDIR)\gdfonts.obj"
	-@erase "$(INTDIR)\gdfontt.obj"
	-@erase "$(INTDIR)\gdft.obj"
	-@erase "$(INTDIR)\gdhelpers.obj"
	-@erase "$(INTDIR)\gdkanji.obj"
	-@erase "$(INTDIR)\gdtables.obj"
	-@erase "$(INTDIR)\gdxpm.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\wbmp.obj"
	-@erase "$(OUTDIR)\gd.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I ".." /I "..\third-party\include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\gd.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gd.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\gd.lib" 
LIB32_OBJS= \
	"$(INTDIR)\gd.obj" \
	"$(INTDIR)\gd_gd.obj" \
	"$(INTDIR)\gd_gd2.obj" \
	"$(INTDIR)\gd_gif.obj" \
	"$(INTDIR)\gd_io.obj" \
	"$(INTDIR)\gd_io_dp.obj" \
	"$(INTDIR)\gd_io_file.obj" \
	"$(INTDIR)\gd_io_ss.obj" \
	"$(INTDIR)\gd_jpeg.obj" \
	"$(INTDIR)\gd_png.obj" \
	"$(INTDIR)\gd_ss.obj" \
	"$(INTDIR)\gd_wbmp.obj" \
	"$(INTDIR)\gdcache.obj" \
	"$(INTDIR)\gdfontg.obj" \
	"$(INTDIR)\gdfontl.obj" \
	"$(INTDIR)\gdfontmb.obj" \
	"$(INTDIR)\gdfonts.obj" \
	"$(INTDIR)\gdfontt.obj" \
	"$(INTDIR)\gdft.obj" \
	"$(INTDIR)\gdhelpers.obj" \
	"$(INTDIR)\gdkanji.obj" \
	"$(INTDIR)\gdtables.obj" \
	"$(INTDIR)\gdxpm.obj" \
	"$(INTDIR)\wbmp.obj"

"$(OUTDIR)\gd.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\gd.lib"
   copy .\Release\gd.lib ..\makearch\win32\static\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "gd - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\gd.lib" "$(OUTDIR)\gd.bsc"


CLEAN :
	-@erase "$(INTDIR)\gd.obj"
	-@erase "$(INTDIR)\gd.sbr"
	-@erase "$(INTDIR)\gd_gd.obj"
	-@erase "$(INTDIR)\gd_gd.sbr"
	-@erase "$(INTDIR)\gd_gd2.obj"
	-@erase "$(INTDIR)\gd_gd2.sbr"
	-@erase "$(INTDIR)\gd_gif.obj"
	-@erase "$(INTDIR)\gd_gif.sbr"
	-@erase "$(INTDIR)\gd_io.obj"
	-@erase "$(INTDIR)\gd_io.sbr"
	-@erase "$(INTDIR)\gd_io_dp.obj"
	-@erase "$(INTDIR)\gd_io_dp.sbr"
	-@erase "$(INTDIR)\gd_io_file.obj"
	-@erase "$(INTDIR)\gd_io_file.sbr"
	-@erase "$(INTDIR)\gd_io_ss.obj"
	-@erase "$(INTDIR)\gd_io_ss.sbr"
	-@erase "$(INTDIR)\gd_jpeg.obj"
	-@erase "$(INTDIR)\gd_jpeg.sbr"
	-@erase "$(INTDIR)\gd_png.obj"
	-@erase "$(INTDIR)\gd_png.sbr"
	-@erase "$(INTDIR)\gd_ss.obj"
	-@erase "$(INTDIR)\gd_ss.sbr"
	-@erase "$(INTDIR)\gd_wbmp.obj"
	-@erase "$(INTDIR)\gd_wbmp.sbr"
	-@erase "$(INTDIR)\gdcache.obj"
	-@erase "$(INTDIR)\gdcache.sbr"
	-@erase "$(INTDIR)\gdfontg.obj"
	-@erase "$(INTDIR)\gdfontg.sbr"
	-@erase "$(INTDIR)\gdfontl.obj"
	-@erase "$(INTDIR)\gdfontl.sbr"
	-@erase "$(INTDIR)\gdfontmb.obj"
	-@erase "$(INTDIR)\gdfontmb.sbr"
	-@erase "$(INTDIR)\gdfonts.obj"
	-@erase "$(INTDIR)\gdfonts.sbr"
	-@erase "$(INTDIR)\gdfontt.obj"
	-@erase "$(INTDIR)\gdfontt.sbr"
	-@erase "$(INTDIR)\gdft.obj"
	-@erase "$(INTDIR)\gdft.sbr"
	-@erase "$(INTDIR)\gdhelpers.obj"
	-@erase "$(INTDIR)\gdhelpers.sbr"
	-@erase "$(INTDIR)\gdkanji.obj"
	-@erase "$(INTDIR)\gdkanji.sbr"
	-@erase "$(INTDIR)\gdtables.obj"
	-@erase "$(INTDIR)\gdtables.sbr"
	-@erase "$(INTDIR)\gdxpm.obj"
	-@erase "$(INTDIR)\gdxpm.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\wbmp.obj"
	-@erase "$(INTDIR)\wbmp.sbr"
	-@erase "$(OUTDIR)\gd.bsc"
	-@erase "$(OUTDIR)\gd.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Gi /GX /ZI /Od /I "." /I ".." /I "..\third-party\include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "MSWIN32" /D "HAVE_CONFIG_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\gd.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gd.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\gd.sbr" \
	"$(INTDIR)\gd_gd.sbr" \
	"$(INTDIR)\gd_gd2.sbr" \
	"$(INTDIR)\gd_gif.sbr" \
	"$(INTDIR)\gd_io.sbr" \
	"$(INTDIR)\gd_io_dp.sbr" \
	"$(INTDIR)\gd_io_file.sbr" \
	"$(INTDIR)\gd_io_ss.sbr" \
	"$(INTDIR)\gd_jpeg.sbr" \
	"$(INTDIR)\gd_png.sbr" \
	"$(INTDIR)\gd_ss.sbr" \
	"$(INTDIR)\gd_wbmp.sbr" \
	"$(INTDIR)\gdcache.sbr" \
	"$(INTDIR)\gdfontg.sbr" \
	"$(INTDIR)\gdfontl.sbr" \
	"$(INTDIR)\gdfontmb.sbr" \
	"$(INTDIR)\gdfonts.sbr" \
	"$(INTDIR)\gdfontt.sbr" \
	"$(INTDIR)\gdft.sbr" \
	"$(INTDIR)\gdhelpers.sbr" \
	"$(INTDIR)\gdkanji.sbr" \
	"$(INTDIR)\gdtables.sbr" \
	"$(INTDIR)\gdxpm.sbr" \
	"$(INTDIR)\wbmp.sbr"

"$(OUTDIR)\gd.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\gd.lib" 
LIB32_OBJS= \
	"$(INTDIR)\gd.obj" \
	"$(INTDIR)\gd_gd.obj" \
	"$(INTDIR)\gd_gd2.obj" \
	"$(INTDIR)\gd_gif.obj" \
	"$(INTDIR)\gd_io.obj" \
	"$(INTDIR)\gd_io_dp.obj" \
	"$(INTDIR)\gd_io_file.obj" \
	"$(INTDIR)\gd_io_ss.obj" \
	"$(INTDIR)\gd_jpeg.obj" \
	"$(INTDIR)\gd_png.obj" \
	"$(INTDIR)\gd_ss.obj" \
	"$(INTDIR)\gd_wbmp.obj" \
	"$(INTDIR)\gdcache.obj" \
	"$(INTDIR)\gdfontg.obj" \
	"$(INTDIR)\gdfontl.obj" \
	"$(INTDIR)\gdfontmb.obj" \
	"$(INTDIR)\gdfonts.obj" \
	"$(INTDIR)\gdfontt.obj" \
	"$(INTDIR)\gdft.obj" \
	"$(INTDIR)\gdhelpers.obj" \
	"$(INTDIR)\gdkanji.obj" \
	"$(INTDIR)\gdtables.obj" \
	"$(INTDIR)\gdxpm.obj" \
	"$(INTDIR)\wbmp.obj"

"$(OUTDIR)\gd.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\gd.lib" "$(OUTDIR)\gd.bsc"
   copy .\Debug\gd.lib ..\makearch\win32\static\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("gd.dep")
!INCLUDE "gd.dep"
!ELSE 
!MESSAGE Warning: cannot find "gd.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "gd - Win32 Release" || "$(CFG)" == "gd - Win32 Debug"
SOURCE=.\gd.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd.obj"	"$(INTDIR)\gd.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_gd.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_gd.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_gd.obj"	"$(INTDIR)\gd_gd.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_gd2.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_gd2.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_gd2.obj"	"$(INTDIR)\gd_gd2.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_gif.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_gif.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_gif.obj"	"$(INTDIR)\gd_gif.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_io.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_io.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_io.obj"	"$(INTDIR)\gd_io.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_io_dp.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_io_dp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_io_dp.obj"	"$(INTDIR)\gd_io_dp.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_io_file.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_io_file.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_io_file.obj"	"$(INTDIR)\gd_io_file.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_io_ss.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_io_ss.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_io_ss.obj"	"$(INTDIR)\gd_io_ss.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_jpeg.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_jpeg.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_jpeg.obj"	"$(INTDIR)\gd_jpeg.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_png.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_png.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_png.obj"	"$(INTDIR)\gd_png.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_ss.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_ss.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_ss.obj"	"$(INTDIR)\gd_ss.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gd_wbmp.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gd_wbmp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gd_wbmp.obj"	"$(INTDIR)\gd_wbmp.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdcache.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdcache.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdcache.obj"	"$(INTDIR)\gdcache.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdfontg.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdfontg.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdfontg.obj"	"$(INTDIR)\gdfontg.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdfontl.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdfontl.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdfontl.obj"	"$(INTDIR)\gdfontl.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdfontmb.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdfontmb.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdfontmb.obj"	"$(INTDIR)\gdfontmb.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdfonts.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdfonts.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdfonts.obj"	"$(INTDIR)\gdfonts.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdfontt.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdfontt.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdfontt.obj"	"$(INTDIR)\gdfontt.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdft.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdft.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdft.obj"	"$(INTDIR)\gdft.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdhelpers.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdhelpers.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdhelpers.obj"	"$(INTDIR)\gdhelpers.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdkanji.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdkanji.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdkanji.obj"	"$(INTDIR)\gdkanji.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdtables.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdtables.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdtables.obj"	"$(INTDIR)\gdtables.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\gdxpm.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\gdxpm.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\gdxpm.obj"	"$(INTDIR)\gdxpm.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\wbmp.c

!IF  "$(CFG)" == "gd - Win32 Release"


"$(INTDIR)\wbmp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "gd - Win32 Debug"


"$(INTDIR)\wbmp.obj"	"$(INTDIR)\wbmp.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 


!ENDIF 

