noinst_LTLIBRARIES = libsieve_ext_variables.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

cmds = \
	cmd-set.c

tsts = \
	tst-string.c

libsieve_ext_variables_la_SOURCES = \
	ext-variables-common.c \
	ext-variables-name.c \
	ext-variables-namespaces.c \
	ext-variables-arguments.c \
	ext-variables-operands.c \
	ext-variables-modifiers.c \
	ext-variables-dump.c \
	$(cmds) \
	$(tsts) \
	ext-variables.c

public_headers = \
	sieve-ext-variables.h 

headers = \
	ext-variables-common.h \
	ext-variables-limits.h \
	ext-variables-name.h \
	ext-variables-namespaces.h \
	ext-variables-arguments.h \
	ext-variables-operands.h \
	ext-variables-modifiers.h \
	ext-variables-dump.h

pkginc_libdir=$(dovecot_pkgincludedir)/sieve
pkginc_lib_HEADERS = $(public_headers)
noinst_HEADERS = $(headers)

