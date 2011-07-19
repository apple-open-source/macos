noinst_LTLIBRARIES = libsieve_ext_environment.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

tests = \
	tst-environment.c

libsieve_ext_environment_la_SOURCES = \
	$(tests) \
	ext-environment-common.c \
	ext-environment.c

public_headers = \
	sieve-ext-environment.h

headers = \
	ext-environment-common.h

pkginc_libdir=$(dovecot_pkgincludedir)/sieve
pkginc_lib_HEADERS = $(public_headers)
noinst_HEADERS = $(headers)

