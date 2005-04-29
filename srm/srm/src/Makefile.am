INCLUDES = -I../lib

bin_PROGRAMS = srm
srm_SOURCES = error.c main.c random.c rename_unlink.c sunlink.c \
	tree_walker.c srm.h
srm_CFLAGS = -mdynamic-no-pic $(AM_CFLAGS)
srm_LDADD = ../lib/libsrm.a
