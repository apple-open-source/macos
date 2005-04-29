## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/tools/ast \
	-I$(top_srcdir)/tools/sfio

LIBS =

noinst_HEADERS = vmalloc.h vmhdr.h

noinst_LTLIBRARIES = libvmalloc.la
man_MANS = 

libvmalloc_la_SOURCES = malloc.c vmbest.c vmclear.c vmclose.c vmdcheap.c \
	vmdebug.c vmdisc.c vmexit.c vmlast.c vmopen.c vmpool.c vmprivate.c \
	vmprofile.c vmregion.c vmsegment.c vmset.c vmstat.c vmstrdup.c \
	vmtrace.c vmwalk.c vmalloc.h vmhdr.h

${top_builddir}/FEATURE/vmalloc: ${top_srcdir}/tools/vmalloc/features/vmalloc
	mkdir -p ${top_builddir}/FEATURE
	@KSH@ ${top_srcdir}/iffe - set cc $(CC) $(CCMODE) $(CXFLAGS) : run ${top_srcdir}/tools/vmalloc/features/vmalloc > $@

$(libvmalloc_la_OBJECTS): ${top_builddir}/FEATURE/vmalloc

DISTCLEANFILES = ${top_builddir}/FEATURE/vmalloc

EXTRA_DIST = $(man_MANS) README Makefile.old features
