INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/libxslt -I$(top_srcdir)/libexslt \
           -I$(top_builddir) -I$(top_builddir)/libxslt \
	   -I$(top_builddir)/libexslt $(LIBXML_CFLAGS) $(CFLAGS)

EXTRA_PROGRAMS=
bin_PROGRAMS = xsltproc $(XSLTPROCDV)

noinst_PROGRAMS=testThreads

AM_CFLAGS = $(LIBGCRYPT_CFLAGS)

xsltproc_SOURCES = xsltproc.c
xsltproc_LDFLAGS = 
xsltproc_DEPENDENCIES = $(DEPS)

testThreads_SOURCES=testThreads.c
testThreads_LDFLAGS =
testThreads_DEPENDENCIES = $(DEPS)
testThreads_LDADD=  $(THREAD_LIBS) $(LDADDS)

DEPS = $(top_builddir)/libxslt/libxslt.la \
	$(top_builddir)/libexslt/libexslt.la 

LDADDS = $(top_builddir)/libxslt/libxslt.la \
         $(top_builddir)/libexslt/libexslt.la \
	@LIBXML_LIBS@ $(EXTRA_LIBS) @WIN32_EXTRA_LIBADD@

xsltproc_LDADD = $(LIBGCRYPT_LIBS) $(LDADDS)

xsltproc.dv: xsltproc.o
	$(CC) $(CFLAGS) -o xsltproc xsltproc.o ../libexslt/.libs/libexslt.a ../libxslt/.libs/libxslt.a @LIBXML_LIBS@ $(EXTRA_LIBS) $(LIBGCRYPT_LIBS)

tests: testThreads
	@echo '## Running testThreads'
	@($(CHECKER) ./testThreads ; grep "MORY ALLO" .memdump  | grep -v "MEMORY ALLOCATED : 0" || true)
