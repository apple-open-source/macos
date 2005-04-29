# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES =  @TCL_INCLUDES@  @TK_INCLUDES@ @TK_XINCLUDES@
                                                                                
LIBS = @TK_STUB_LIB_SPEC@

if WITH_TK
noinst_LTLIBRARIES = libtkstubs.la
endif
noinst_HEADERS = tkInt.h

# tkStubLib.c is a dummy file just to fool automake
libtkstubs_la_SOURCES = tkStubLib.c tkStubImg.c tkInt.h

EXTRA_DIST = $(libtkstubs_la_SOURCES)
