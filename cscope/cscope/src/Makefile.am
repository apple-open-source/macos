## Process this file with automake to produce Makefile.in

bin_PROGRAMS = cscope

if USING_LEX
LEXER_SOURCE=scanner.l
else
LEXER_SOURCE=fscanner.l
endif

cscope_SOURCES = $(LEXER_SOURCE) egrep.y \
	alloc.c basename.c command.c compath.c constants.h crossref.c dir.c \
	display.c edit.c exec.c find.c global.h help.c history.c input.c \
	invlib.c invlib.h library.h logdir.c lookup.c lookup.h main.c mouse.c \
	mygetenv.c mypopen.c scanner.h version.h vp.h vpaccess.c vpfopen.c \
	vpinit.c vpopen.c

EXTRA_cscope_SOURCES = scanner.l fscanner.l

LEXLIB = @LEXLIB@

cscope_LDADD =  $(CURSES_LIBS) $(LEXLIB)

INCLUDES = $(CURSES_INCLUDEDIR)

## Hack to clean the generated files and not include them in a "make dist".
CLEANFILES = scanner.c fscanner.c egrep.c
dist-hook:
	rm -f $(distdir)/scanner.c $(distdir)/fscanner.c $(distdir)/egrep.c

EXTRA_DIST = emacs.e gmacs.ml
