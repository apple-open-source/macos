noinst_LTLIBRARIES = libsieve_ext_notify.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

commands = \
	cmd-notify.c \
	cmd-denotify.c

libsieve_ext_notify_la_SOURCES = \
	ext-notify.c \
	ext-notify-common.c \
	$(commands)

noinst_HEADERS = \
	ext-notify-common.h \
	ext-notify-limits.h

