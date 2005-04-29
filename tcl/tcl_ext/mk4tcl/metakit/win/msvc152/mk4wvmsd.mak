# Microsoft Visual C++ generated build script - Do not modify

PROJ = MK4WVMSD
DEBUG = 1
PROGTYPE = 4
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = C:\JCW\OSS\MK\WIN\MSVC152\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = /YcHEADER.H
CUSEPCHFLAG = 
CPPUSEPCHFLAG = /YuHEADER.H
FIRSTC =             
FIRSTCPP = FIELD.CPP   
RC = rc
CFLAGS_D_LIB = /nologo /G2 /W3 /Zi /AL /Od /D "_DEBUG" /D "q4_MFC" /I "../../src" /I "../../include." /I ".." /FR /GA /Fd"MK4WVMSD.PDB" 
CFLAGS_R_LIB = /nologo /Gs /G2 /W3 /AL /O1 /Ox /Ob2 /OV1 /D "NDEBUG" /D "q4_MFC" /I "../../src" /I "../../include." /I ".." /GA 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_LIB)
LFLAGS = 
LIBS = 
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_LIB)
LFLAGS = 
LIBS = 
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = FIELD.SBR \
		TABLE.SBR \
		VIEW.SBR \
		VIEWX.SBR \
		PERSIST.SBR \
		FORMAT.SBR \
		HANDLER.SBR \
		DERIVED.SBR \
		STORE.SBR \
		COLUMN.SBR \
		CUSTOM.SBR \
		FILEIO.SBR


FIELD_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl


TABLE_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl \
	c:\jcw\oss\mk\src\format.h \
	c:\jcw\oss\mk\src\persist.h


VIEW_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\derived.h \
	c:\jcw\oss\mk\src\custom.h \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl


VIEWX_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl \
	c:\jcw\oss\mk\src\column.h \
	c:\jcw\oss\mk\src\column.inl


PERSIST_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\persist.h \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl


FORMAT_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\column.h \
	c:\jcw\oss\mk\src\column.inl \
	c:\jcw\oss\mk\src\format.h \
	c:\jcw\oss\mk\src\persist.h


HANDLER_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\format.h \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl \
	c:\jcw\oss\mk\src\persist.h \
	c:\jcw\oss\mk\src\column.h \
	c:\jcw\oss\mk\src\column.inl


DERIVED_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl \
	c:\jcw\oss\mk\src\derived.h


STORE_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\store.h \
	c:\jcw\oss\mk\src\store.inl \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl \
	c:\jcw\oss\mk\src\persist.h \
	c:\jcw\oss\mk\src\column.h \
	c:\jcw\oss\mk\src\column.inl \
	c:\jcw\oss\mk\src\format.h


COLUMN_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\column.h \
	c:\jcw\oss\mk\src\column.inl


CUSTOM_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl \
	c:\jcw\oss\mk\src\custom.h \
	c:\jcw\oss\mk\src\field.h \
	c:\jcw\oss\mk\src\field.inl \
	c:\jcw\oss\mk\src\handler.h \
	c:\jcw\oss\mk\src\handler.inl \
	c:\jcw\oss\mk\src\format.h


FILEIO_DEP = c:\jcw\oss\mk\src\header.h \
	c:\jcw\oss\mk\src\borc.h \
	c:\jcw\oss\mk\src\gnuc.h \
	c:\jcw\oss\mk\src\mwcw.h \
	c:\jcw\oss\mk\src\msvc.h \
	c:\jcw\oss\mk\src\mfc.h \
	c:\jcw\oss\mk\src\std.h \
	c:\jcw\oss\mk\src\univ.h \
	c:\jcw\oss\mk\src\univ.inl


all:	$(PROJ).LIB $(PROJ).BSC

FIELD.OBJ:	..\..\SRC\FIELD.CPP $(FIELD_DEP)
	$(CPP) $(CFLAGS) $(CPPCREATEPCHFLAG) /c ..\..\SRC\FIELD.CPP

TABLE.OBJ:	..\..\SRC\TABLE.CPP $(TABLE_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\TABLE.CPP

VIEW.OBJ:	..\..\SRC\VIEW.CPP $(VIEW_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\VIEW.CPP

VIEWX.OBJ:	..\..\SRC\VIEWX.CPP $(VIEWX_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\VIEWX.CPP

PERSIST.OBJ:	..\..\SRC\PERSIST.CPP $(PERSIST_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\PERSIST.CPP

FORMAT.OBJ:	..\..\SRC\FORMAT.CPP $(FORMAT_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\FORMAT.CPP

HANDLER.OBJ:	..\..\SRC\HANDLER.CPP $(HANDLER_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\HANDLER.CPP

DERIVED.OBJ:	..\..\SRC\DERIVED.CPP $(DERIVED_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\DERIVED.CPP

STORE.OBJ:	..\..\SRC\STORE.CPP $(STORE_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\STORE.CPP

COLUMN.OBJ:	..\..\SRC\COLUMN.CPP $(COLUMN_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\COLUMN.CPP

CUSTOM.OBJ:	..\..\SRC\CUSTOM.CPP $(CUSTOM_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\CUSTOM.CPP

FILEIO.OBJ:	..\..\SRC\FILEIO.CPP $(FILEIO_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c ..\..\SRC\FILEIO.CPP

$(PROJ).LIB::	FIELD.OBJ TABLE.OBJ VIEW.OBJ VIEWX.OBJ PERSIST.OBJ FORMAT.OBJ HANDLER.OBJ \
	DERIVED.OBJ STORE.OBJ COLUMN.OBJ CUSTOM.OBJ FILEIO.OBJ $(OBJS_EXT)
	echo >NUL @<<$(PROJ).CRF
$@ /PAGESIZE:64
y
+FIELD.OBJ &
+TABLE.OBJ &
+VIEW.OBJ &
+VIEWX.OBJ &
+PERSIST.OBJ &
+FORMAT.OBJ &
+HANDLER.OBJ &
+DERIVED.OBJ &
+STORE.OBJ &
+COLUMN.OBJ &
+CUSTOM.OBJ &
+FILEIO.OBJ &
;
<<
	if exist $@ del $@
	lib @$(PROJ).CRF

$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
