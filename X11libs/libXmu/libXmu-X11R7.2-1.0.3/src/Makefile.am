lib_LTLIBRARIES = libXmu.la libXmuu.la

BITMAP_DEFINES = -DBITMAPDIR=\"$(includedir)/X11/bitmaps\"

AM_CFLAGS = \
	-I${top_srcdir}/include \
	-I${top_srcdir}/include/X11/Xmu \
	$(XMU_CFLAGS) $(BITMAP_DEFINES) -D_BSD_SOURCE

libXmu_la_LDFLAGS = -version-number 6:2:0 -no-undefined
libXmuu_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXmu_la_LIBADD = $(XMU_LIBS)
libXmuu_la_LIBADD = $(XMUU_LIBS)

libXmuu_la_SOURCES = \
	ClientWin.c \
	CursorName.c \
	DefErrMsg.c \
	GetHost.c \
	Lower.c

libXmu_la_SOURCES = \
	$(libXmuu_la_SOURCES) \
	AllCmap.c \
	Atoms.c \
	Clip.c \
	CloseHook.c \
	CmapAlloc.c \
	CrCmap.c \
	CrPixFBit.c \
	CvtCache.c \
	CvtStdSel.c \
	DelCmap.c \
	DisplayQue.c \
	Distinct.c \
	DrawLogo.c \
	DrRndRect.c \
	EditresCom.c \
	ExtAgent.c \
	FToCback.c \
	GrayPixmap.c \
	Initer.c \
	LocBitmap.c \
	Lookup.c \
	LookupCmap.c \
	RdBitF.c \
	ScrOfWin.c \
	ShapeWidg.c \
	StdCmap.c \
	StrToBS.c \
	StrToBmap.c \
	StrToCurs.c \
	StrToGrav.c \
	StrToJust.c \
	StrToLong.c \
	StrToOrnt.c \
	StrToShap.c \
	StrToWidg.c \
	UpdMapHint.c \
	VisCmap.c \
	WidgetNode.c \
	Xct.c \
	sharedlib.c
