lib_LTLIBRARIES = libXTrap.la

libXTrap_la_SOURCES = 	\
	XECallBcks.c	\
	XEConTxt.c	\
	XEDsptch.c	\
	XEPrInfo.c	\
	XERqsts.c	\
	XEStrMap.c	\
	XETrapInit.c	\
	XEWrappers.c	

AM_CFLAGS = @XTRAP_CFLAGS@

libXTrap_la_LIBADD = @XTRAP_LIBS@

libXTrap_la_LDFLAGS = -version-number 6:4:0 -no-undefined
