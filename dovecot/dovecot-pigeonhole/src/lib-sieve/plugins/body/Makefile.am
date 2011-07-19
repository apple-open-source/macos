noinst_LTLIBRARIES = libsieve_ext_body.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

tsts = \
	tst-body.c

libsieve_ext_body_la_SOURCES = \
	ext-body-common.c \
	$(tsts) \
	ext-body.c

noinst_HEADERS = \
	ext-body-common.h
