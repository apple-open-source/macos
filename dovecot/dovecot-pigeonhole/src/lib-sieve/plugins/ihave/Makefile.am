noinst_LTLIBRARIES = libsieve_ext_ihave.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib-sieve \
	$(LIBDOVECOT_INCLUDE)

tests = \
	tst-ihave.c

commands = \
	cmd-error.c

libsieve_ext_ihave_la_SOURCES = \
	$(tests) \
	$(commands) \
	ext-ihave-binary.c \
	ext-ihave-common.c \
	ext-ihave.c

noinst_HEADERS = \
	ext-ihave-binary.h \
	ext-ihave-common.h
