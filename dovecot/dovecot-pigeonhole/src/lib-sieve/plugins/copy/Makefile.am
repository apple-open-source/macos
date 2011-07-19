noinst_LTLIBRARIES = libsieve_ext_copy.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

libsieve_ext_copy_la_SOURCES = \
	ext-copy.c

public_headers = \
	sieve-ext-copy.h 

headers =

pkginc_libdir=$(dovecot_pkgincludedir)/sieve
pkginc_lib_HEADERS = $(public_headers)
noinst_HEADERS = $(headers)

