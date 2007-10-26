FONTENCDIR=@ENCODINGSDIR@
FONTENCDEFS = -DFONT_ENCODINGS_DIRECTORY=\"$(FONTENCDIR)/encodings.dir\"

INCLUDES =			\
	-I${top_srcdir}/include \
	$(FONTENCDEFS)		

noinst_LTLIBRARIES = libfontfile.la

libfontfile_la_LIBADD = 	\
	$(Z_LIBS)

libfontfile_la_SOURCES = 	\
	bitsource.c		\
	bufio.c			\
	decompress.c		\
	defaults.c		\
	dirfile.c		\
	ffcheck.c		\
	fileio.c		\
	filewr.c		\
	fontdir.c		\
	fontencc.c		\
	fontfile.c		\
	fontscale.c		\
	gunzip.c		\
	printerfont.c		\
	register.c		\
	renderers.c		\
	catalogue.c
