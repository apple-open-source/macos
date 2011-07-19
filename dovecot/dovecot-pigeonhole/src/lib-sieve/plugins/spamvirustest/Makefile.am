noinst_LTLIBRARIES = libsieve_ext_spamvirustest.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

tests = \
	tst-spamvirustest.c

libsieve_ext_spamvirustest_la_SOURCES = \
	$(tests) \
	ext-spamvirustest-common.c \
	ext-spamvirustest.c

noinst_HEADERS = \
	ext-spamvirustest-common.h
