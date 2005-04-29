XCOMM $XFree86: xc/lib/font/X-TrueType/Imakefile.inc,v 1.6 2003/07/27 02:01:20 dawes Exp $
/* -*- Makefile -*- */

DEFAULT_CCONV_SUBDIR = ISO8859.1
#if XTrueTypeUseOptEncodings
XTT_USE_OPT_ENC_DEFINES = -DOPT_ENCODINGS
CCONV_SUBDIRS = BIG5 BIG5HKSCS GB2312 GBK GB18030 JISX0201 JISX0208 JISX0212 \
	   KSC5601 KSCJOHAB GEORGIAN \
	   ISO8859.2 ISO8859.3 ISO8859.4 ISO8859.5 ISO8859.6 \
	   ISO8859.7 ISO8859.8 ISO8859.9 ISO8859.10 ISO8859.11 \
	   ISO8859.13 ISO8859.14 ISO8859.15 KOI8 VISCII TCVN \
	   ARMSCII8 ARABIC MULEENCODING DOSENCODING
#endif

SUBDIRS = $(MODULE_SUBDIRS) $(DEFAULT_CCONV_SUBDIR) $(CCONV_SUBDIRS)

#if XTrueTypeDebugXTT
  XTT_DEBUG_SRCS = xttdump.c
  XTT_DEBUG_OBJS = xttdump.o
#endif
/*
#if XTrueTypeCConvModule
  XTT_CCONV_MODULE_DIR = XTrueTypeCConvModuleDir
  XTT_CCONV_MODULE_SUBDIR = XTrueTypeCConvModuleSubDir
  XTT_CCONV_MODULE_DEFINES = \
    -DCCONV_MODULE \
	-DCCONV_MODULE_DIR=\"$(XTT_CCONV_MODULE_DIR)\" \
	-DCCONV_MODULE_SUBDIR=\"$(XTT_CCONV_MODULE_SUBDIR)\"
#endif
*/
SRCS = \
  xttprop.c xttfuncs.c xttcconv.c xttcap.c xttmisc.c xttblit.c \
  $(XTT_DEBUG_SRCS) freetype.c ftxcmap.c ftxsbit.c $(MODULE_C)
OBJS = \
  xttprop.o xttfuncs.o xttcconv.o xttcap.o xttmisc.o xttblit.o \
  $(XTT_DEBUG_OBJS) freetype.o ftxcmap.o ftxsbit.o $(MODULE_O)

LinkSourceFile(freetype.c,$(FTSOURCEDIR)/lib/arch/unix)
LinkSourceFile(ftxcmap.c,$(FTSOURCEDIR)/lib/extend)
LinkSourceFile(ftxsbit.c,$(FTSOURCEDIR)/lib/extend)
LinkSourceFile(xttblit.c,$(XTTSOURCEDIR))
LinkSourceFile(xttblit.h,$(XTTSOURCEDIR))
LinkSourceFile(xttcap.c,$(XTTSOURCEDIR))
LinkSourceFile(xttcap.h,$(XTTSOURCEDIR))
LinkSourceFile(xttcconv.c,$(XTTSOURCEDIR))
LinkSourceFile(xttcconv.h,$(XTTSOURCEDIR))
LinkSourceFile(xttcconvP.h,$(XTTSOURCEDIR))
LinkSourceFile(xttchinfo.c,$(XTTSOURCEDIR))
LinkSourceFile(xttdump.c,$(XTTSOURCEDIR))
LinkSourceFile(xttfuncs.c,$(XTTSOURCEDIR))
LinkSourceFile(xttstruct.h,$(XTTSOURCEDIR))
LinkSourceFile(xttcommon.h,$(XTTSOURCEDIR))
LinkSourceFile(xttmisc.c,$(XTTSOURCEDIR))
LinkSourceFile(xttprop.c,$(XTTSOURCEDIR))
LinkSourceFile(xttversion.h,$(XTTSOURCEDIR))
