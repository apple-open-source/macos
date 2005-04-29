# Microsoft Visual C++ generated build script - Do not modify

PROJ = CATFISH
DEBUG = 0
PROGTYPE = 0
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = /d_DEBUG 
R_RCDEFINES = /dNDEBUG 
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = C:\JCW\OSS\MK\WIN\CATFISH\
USEMFC = 1
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = /YcSTDAFX.H
CUSEPCHFLAG = 
CPPUSEPCHFLAG = /YuSTDAFX.H
FIRSTC =             
FIRSTCPP = STDAFX.CPP  
RC = rc
CFLAGS_D_WEXE = /nologo /G2 /W3 /Zi /AL /Od /D "_DEBUG" /D "q4_MFC" /I "..\..\include" /FR /GA /Fd"CATFISH.PDB" 
CFLAGS_R_WEXE = /nologo /Gs /G2 /Gy /W3 /Gf /AL /O1 /Oa /Ox /Ob2 /OV0 /D "NDEBUG" /D "q4_MFC" /I "..\..\include" /GA 
LFLAGS_D_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:15360 /ALIGN:16 /ONERROR:NOEXE /CO  
LFLAGS_R_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:10240 /ALIGN:16 /ONERROR:NOEXE  
LIBS_D_WEXE = lafxcwd ..\msvc152\mk4wvmsd oldnames libw llibcew commdlg.lib shell.lib 
LIBS_R_WEXE = lafxcw ..\msvc152\mk4wvms oldnames libw llibcew commdlg.lib shell.lib 
RCFLAGS = /nologo /d_AFXDLL 
RESFLAGS = /nologo 
RUNFLAGS = 
DEFFILE = CATFISH.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WEXE)
LFLAGS = $(LFLAGS_D_WEXE)
LIBS = $(LIBS_D_WEXE)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WEXE)
LFLAGS = $(LFLAGS_R_WEXE)
LIBS = $(LIBS_R_WEXE)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = STDAFX.SBR \
		CATFISH.SBR \
		PICKDIR.SBR \
		SETUPDLG.SBR \
		SCANDISK.SBR \
		FINDDLG.SBR \
		ABOUTBOX.SBR \
		UTIL.SBR \
		CONVDLG.SBR \
		RNAMEDLG.SBR


STDAFX_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h


CATFISH_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\setupdlg.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h \
	c:\jcw\oss\mk\win\catfish\aboutbox.h \
	c:\jcw\oss\mk\win\catfish\convdlg.h \
	c:\jcw\oss\mk\win\catfish\rnamedlg.h


PICKDIR_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h \
	c:\jcw\oss\mk\win\catfish\util.h


SETUPDLG_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\setupdlg.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h \
	c:\jcw\oss\mk\win\catfish\pickdir.h


CATFISH_RCDEP = c:\jcw\oss\mk\win\catfish\res\catfish.ico \
	c:\jcw\oss\mk\win\catfish\res\equi4.bmp \
	c:\jcw\oss\mk\win\catfish\res\hier.bmp \
	c:\jcw\oss\mk\win\catfish\url_curs.cur \
	c:\jcw\oss\mk\win\catfish\res\catfish.rc2


SCANDISK_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\dtwinver.cpp \
	c:\jcw\oss\mk\win\catfish\dtwinver.h


FINDDLG_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h


ABOUTBOX_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\aboutbox.h


UTIL_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\scandisk.h


CONVDLG_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\convdlg.h


RNAMEDLG_DEP = c:\jcw\oss\mk\win\catfish\stdafx.h \
	c:\jcw\oss\mk\win\catfish\catfish.h \
	c:\jcw\oss\mk\win\catfish\finddlg.h \
	c:\jcw\oss\mk\win\catfish\util.h \
	c:\jcw\oss\mk\win\catfish\rnamedlg.h


all:	$(PROJ).EXE

STDAFX.OBJ:	STDAFX.CPP $(STDAFX_DEP)
	$(CPP) $(CFLAGS) $(CPPCREATEPCHFLAG) /c STDAFX.CPP

CATFISH.OBJ:	CATFISH.CPP $(CATFISH_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c CATFISH.CPP

PICKDIR.OBJ:	PICKDIR.CPP $(PICKDIR_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c PICKDIR.CPP

SETUPDLG.OBJ:	SETUPDLG.CPP $(SETUPDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c SETUPDLG.CPP

CATFISH.RES:	CATFISH.RC $(CATFISH_RCDEP)
	$(RC) $(RCFLAGS) $(RCDEFINES) -r CATFISH.RC

SCANDISK.OBJ:	SCANDISK.CPP $(SCANDISK_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c SCANDISK.CPP

FINDDLG.OBJ:	FINDDLG.CPP $(FINDDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c FINDDLG.CPP

ABOUTBOX.OBJ:	ABOUTBOX.CPP $(ABOUTBOX_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ABOUTBOX.CPP

UTIL.OBJ:	UTIL.CPP $(UTIL_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c UTIL.CPP

CONVDLG.OBJ:	CONVDLG.CPP $(CONVDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c CONVDLG.CPP

RNAMEDLG.OBJ:	RNAMEDLG.CPP $(RNAMEDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c RNAMEDLG.CPP


$(PROJ).EXE::	CATFISH.RES

$(PROJ).EXE::	STDAFX.OBJ CATFISH.OBJ PICKDIR.OBJ SETUPDLG.OBJ SCANDISK.OBJ FINDDLG.OBJ \
	ABOUTBOX.OBJ UTIL.OBJ CONVDLG.OBJ RNAMEDLG.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
STDAFX.OBJ +
CATFISH.OBJ +
PICKDIR.OBJ +
SETUPDLG.OBJ +
SCANDISK.OBJ +
FINDDLG.OBJ +
ABOUTBOX.OBJ +
UTIL.OBJ +
CONVDLG.OBJ +
RNAMEDLG.OBJ +
$(OBJS_EXT)
$(PROJ).EXE
$(MAPFILE)
c:\msvc\lib\+
c:\msvc\mfc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) CATFISH.RES $@
	@copy $(PROJ).CRF MSVC.BND

$(PROJ).EXE::	CATFISH.RES
	if not exist MSVC.BND 	$(RC) $(RESFLAGS) CATFISH.RES $@

run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
