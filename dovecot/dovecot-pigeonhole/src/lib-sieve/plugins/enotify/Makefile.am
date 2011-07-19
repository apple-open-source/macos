SUBDIRS = mailto

noinst_LTLIBRARIES = libsieve_ext_enotify.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	-I$(srcdir)/../variables \
	$(LIBDOVECOT_INCLUDE)

commands = \
	cmd-notify.c

tests = \
	tst-valid-notify-method.c \
	tst-notify-method-capability.c

var_modifiers = \
	vmodf-encodeurl.c

notify_methods = \
	./mailto/libsieve_ext_enotify_mailto.la

libsieve_ext_enotify_la_DEPENDENCIES = \
	$(notify_methods)
libsieve_ext_enotify_la_LIBADD = \
	$(notify_methods)
	
libsieve_ext_enotify_la_SOURCES = \
	ext-enotify.c \
	ext-enotify-common.c \
	$(commands) \
	$(tests) \
	$(var_modifiers)

public_headers = \
	sieve-ext-enotify.h

headers = \
	ext-enotify-limits.h \
	ext-enotify-common.h

pkginc_libdir=$(dovecot_pkgincludedir)/sieve
pkginc_lib_HEADERS = $(public_headers)
noinst_HEADERS = $(headers)
