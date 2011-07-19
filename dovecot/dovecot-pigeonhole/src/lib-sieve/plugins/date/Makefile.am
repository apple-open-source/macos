noinst_LTLIBRARIES = libsieve_ext_date.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

tests = \
	tst-date.c

libsieve_ext_date_la_SOURCES = \
	$(tests) \
	ext-date-common.c \
	ext-date.c

noinst_HEADERS = \
	ext-date-common.h
