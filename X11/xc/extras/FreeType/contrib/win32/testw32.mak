# Microsoft Developer Studio Generated NMAKE File, Format Version 4.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=testw32 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to testw32 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "testw32 - Win32 Release" && "$(CFG)" !=\
 "testw32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "testw32.mak" CFG="testw32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testw32 - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "testw32 - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "testw32 - Win32 Debug"
MTL=mktyplib.exe
RSC=rc.exe
CPP=cl.exe

!IF  "$(CFG)" == "testw32 - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\testw32.exe" "$(OUTDIR)\testw32.pch"

CLEAN : 
	-@erase ".\Release\testw32.pch"
	-@erase ".\Release\testw32.exe"
	-@erase ".\Release\hack_ftview.obj"
	-@erase ".\Release\testw32.obj"
	-@erase ".\Release\hack_fttimer.obj"
	-@erase ".\Release\hack_common.obj"
	-@erase ".\Release\driver32.obj"
	-@erase ".\Release\stdafx.obj"
	-@erase ".\Release\hack_ftlint.obj"
	-@erase ".\Release\testw32dlg.obj"
	-@erase ".\Release\gmain.obj"
	-@erase ".\Release\hack_ftdump.obj"
	-@erase ".\Release\hack_ftstring.obj"
	-@erase ".\Release\display.obj"
	-@erase ".\Release\testw32.res"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\lib" /I "..\..\" /I "..\..\..\" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /YX"stdafx.h" /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "..\..\..\lib" /I "..\..\" /I "..\..\..\"\
 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS"\
 /Fp"$(INTDIR)/testw32.pch" /YX"stdafx.h" /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x410 /fo"$(INTDIR)/testw32.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/testw32.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 freetype.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=freetype.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/testw32.pdb" /machine:I386 /out:"$(OUTDIR)/testw32.exe" 
LINK32_OBJS= \
	"$(INTDIR)/hack_ftview.obj" \
	"$(INTDIR)/testw32.obj" \
	"$(INTDIR)/hack_fttimer.obj" \
	"$(INTDIR)/hack_common.obj" \
	"$(INTDIR)/driver32.obj" \
	"$(INTDIR)/stdafx.obj" \
	"$(INTDIR)/hack_ftlint.obj" \
	"$(INTDIR)/testw32dlg.obj" \
	"$(INTDIR)/gmain.obj" \
	"$(INTDIR)/hack_ftdump.obj" \
	"$(INTDIR)/hack_ftstring.obj" \
	"$(INTDIR)/display.obj" \
	"$(INTDIR)/testw32.res"

"$(OUTDIR)\testw32.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "testw32 - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\testw32.exe" "$(OUTDIR)\testw32.pch"

CLEAN : 
	-@erase ".\Debug\vc40.pdb"
	-@erase ".\Debug\vc40.idb"
	-@erase ".\Debug\testw32.pch"
	-@erase ".\Debug\testw32.exe"
	-@erase ".\Debug\stdafx.obj"
	-@erase ".\Debug\display.obj"
	-@erase ".\Debug\hack_ftlint.obj"
	-@erase ".\Debug\hack_fttimer.obj"
	-@erase ".\Debug\hack_ftdump.obj"
	-@erase ".\Debug\driver32.obj"
	-@erase ".\Debug\hack_ftview.obj"
	-@erase ".\Debug\testw32dlg.obj"
	-@erase ".\Debug\testw32.obj"
	-@erase ".\Debug\gmain.obj"
	-@erase ".\Debug\hack_ftstring.obj"
	-@erase ".\Debug\hack_common.obj"
	-@erase ".\Debug\testw32.res"
	-@erase ".\Debug\testw32.ilk"
	-@erase ".\Debug\testw32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "c:\winlib\freetype.orig\test" /I "..\..\..\lib" /I "..\..\..\lib\arch\win32" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /YX"stdafx.h" /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "c:\winlib\freetype.orig\test" /I\
 "..\..\..\lib" /I "..\..\..\lib\arch\win32" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/testw32.pch" /YX"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x410 /fo"$(INTDIR)/testw32.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/testw32.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 ..\..\..\lib\arch\win32\debug\freetype.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=..\..\..\lib\arch\win32\debug\freetype.lib /nologo\
 /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)/testw32.pdb" /debug\
 /machine:I386 /out:"$(OUTDIR)/testw32.exe" 
LINK32_OBJS= \
	"$(INTDIR)/stdafx.obj" \
	"$(INTDIR)/display.obj" \
	"$(INTDIR)/hack_ftlint.obj" \
	"$(INTDIR)/hack_fttimer.obj" \
	"$(INTDIR)/hack_ftdump.obj" \
	"$(INTDIR)/driver32.obj" \
	"$(INTDIR)/hack_ftview.obj" \
	"$(INTDIR)/testw32dlg.obj" \
	"$(INTDIR)/testw32.obj" \
	"$(INTDIR)/gmain.obj" \
	"$(INTDIR)/hack_ftstring.obj" \
	"$(INTDIR)/hack_common.obj" \
	"$(INTDIR)/testw32.res"

"$(OUTDIR)\testw32.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "testw32 - Win32 Release"
# Name "testw32 - Win32 Debug"

!IF  "$(CFG)" == "testw32 - Win32 Release"

!ELSEIF  "$(CFG)" == "testw32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\ReadMe.txt

!IF  "$(CFG)" == "testw32 - Win32 Release"

!ELSEIF  "$(CFG)" == "testw32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\testw32.cpp
DEP_CPP_TESTW=\
	".\stdafx.h"\
	".\testw32.h"\
	".\testw32dlg.h"\
	

"$(INTDIR)\testw32.obj" : $(SOURCE) $(DEP_CPP_TESTW) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\testw32dlg.cpp
DEP_CPP_TESTW3=\
	".\stdafx.h"\
	".\testw32.h"\
	".\testw32dlg.h"\
	".\..\..\gdriver.h"\
	".\..\..\gevents.h"\
	

"$(INTDIR)\testw32dlg.obj" : $(SOURCE) $(DEP_CPP_TESTW3) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\stdafx.cpp
DEP_CPP_STDAF=\
	".\stdafx.h"\
	

!IF  "$(CFG)" == "testw32 - Win32 Release"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MD /W3 /GX /O2 /I "..\..\..\lib" /I "..\..\" /I "..\..\..\" /D\
 "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS"\
 /Fp"$(INTDIR)/testw32.pch" /Yc"stdafx.h" /Fo"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\stdafx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\testw32.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ELSEIF  "$(CFG)" == "testw32 - Win32 Debug"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MDd /W3 /Gm /GX /Zi /Od /I "c:\winlib\freetype.orig\test" /I\
 "..\..\..\lib" /I "..\..\..\lib\arch\win32" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/testw32.pch" /Yc"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\stdafx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\testw32.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\testw32.rc
DEP_RSC_TESTW32=\
	".\res\testw32.ico"\
	".\res\testw32.rc2"\
	

"$(INTDIR)\testw32.res" : $(SOURCE) $(DEP_RSC_TESTW32) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_ftview.c
DEP_CPP_HACK_=\
	".\..\..\ftview.c"\
	".\..\..\..\lib\freetype.h"\
	".\..\..\common.h"\
	".\..\..\gmain.h"\
	".\..\..\gevents.h"\
	".\..\..\gdriver.h"\
	".\..\..\display.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	
NODEP_CPP_HACK_=\
	".\..\..\std.h"\
	".\..\..\graflink.h"\
	".\..\..\armsup.c"\
	

"$(INTDIR)\hack_ftview.obj" : $(SOURCE) $(DEP_CPP_HACK_) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_common.c
DEP_CPP_HACK_C=\
	".\..\..\common.c"\
	".\..\..\common.h"\
	

"$(INTDIR)\hack_common.obj" : $(SOURCE) $(DEP_CPP_HACK_C) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_ftdump.c

!IF  "$(CFG)" == "testw32 - Win32 Release"

DEP_CPP_HACK_F=\
	".\..\..\ftdump.c"\
	".\..\..\..\lib\freetype.h"\
	".\..\..\common.h"\
	".\..\..\..\lib\ttobjs.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	".\..\..\..\lib\ttconfig.h"\
	".\..\..\..\lib\ttengine.h"\
	".\..\..\..\lib\ttmutex.h"\
	".\..\..\..\lib\ttcache.h"\
	".\..\..\..\lib\tttables.h"\
	".\..\..\..\lib\ttcmap.h"\
	".\..\..\..\lib\tttypes.h"\
	
NODEP_CPP_HACK_F=\
	".\..\..\std.h"\
	".\..\..\graflink.h"\
	".\..\..\armsup.c"\
	".\..\..\ftxerr18.h"\
	".\..\..\..\lib\ft_conf.h"\
	

"$(INTDIR)\hack_ftdump.obj" : $(SOURCE) $(DEP_CPP_HACK_F) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "testw32 - Win32 Debug"

DEP_CPP_HACK_F=\
	".\..\..\ftdump.c"\
	

"$(INTDIR)\hack_ftdump.obj" : $(SOURCE) $(DEP_CPP_HACK_F) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_ftlint.c
DEP_CPP_HACK_FT=\
	".\..\..\ftlint.c"\
	".\..\..\..\lib\freetype.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	
NODEP_CPP_HACK_FT=\
	".\..\..\std.h"\
	".\..\..\graflink.h"\
	".\..\..\armsup.c"\
	".\..\..\ftxerr18.h"\
	

"$(INTDIR)\hack_ftlint.obj" : $(SOURCE) $(DEP_CPP_HACK_FT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_ftstring.c
DEP_CPP_HACK_FTS=\
	".\..\..\ftstring.c"\
	".\..\..\..\lib\freetype.h"\
	".\..\..\common.h"\
	".\..\..\gmain.h"\
	".\..\..\gevents.h"\
	".\..\..\gdriver.h"\
	".\..\..\display.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	
NODEP_CPP_HACK_FTS=\
	".\..\..\std.h"\
	".\..\..\graflink.h"\
	".\..\..\armsup.c"\
	

"$(INTDIR)\hack_ftstring.obj" : $(SOURCE) $(DEP_CPP_HACK_FTS) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\hack_fttimer.c
DEP_CPP_HACK_FTT=\
	".\..\..\fttimer.c"\
	".\..\..\..\lib\freetype.h"\
	".\..\..\common.h"\
	".\..\..\gmain.h"\
	".\..\..\gdriver.h"\
	".\..\..\gevents.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	

"$(INTDIR)\hack_fttimer.obj" : $(SOURCE) $(DEP_CPP_HACK_FTT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\driver32.c
DEP_CPP_DRIVE=\
	".\..\..\gdriver.h"\
	".\..\..\..\lib\freetype.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	

"$(INTDIR)\driver32.obj" : $(SOURCE) $(DEP_CPP_DRIVE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=\winlib\freetype.orig\test\gmain.c
DEP_CPP_GMAIN=\
	".\..\..\gmain.h"\
	".\..\..\gdriver.h"\
	

"$(INTDIR)\gmain.obj" : $(SOURCE) $(DEP_CPP_GMAIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\winlib\freetype.orig\test\display.c
DEP_CPP_DISPL=\
	".\..\..\..\lib\freetype.h"\
	".\..\..\gmain.h"\
	".\..\..\display.h"\
	"..\..\..\lib\fterrid.h"\
	"..\..\..\lib\ftnameid.h"\
	

"$(INTDIR)\display.obj" : $(SOURCE) $(DEP_CPP_DISPL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
