Project = sort
Install_Dir = /usr/bin

CFILES = sort.c error.c getopt.c getopt1.c long-options.c version.c xstrtod.c \
	closeout.c exitfail.c fopen-safer.c hard-locale.c mkstemp-safer.c \
	physmem.c posixver.c quote.c strnumcmp.c version-etc.c xmalloc.c \
	xmemcoll.c __fpending.c calloc.c dup-safer.c fd-safer.c memcoll.c \
	quotearg.c umaxtostr.c version-etc-fsf.c xalloc-die.c xstrtoumax.c
MANPAGES = sort.1

Extra_CC_Flags = -Wall \
	-D__FBSDID=__RCSID \
	-DHAVE_CONFIG_H \
	-I.
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
