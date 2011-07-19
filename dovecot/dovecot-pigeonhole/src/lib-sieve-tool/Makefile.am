noinst_LTLIBRARIES = libsieve-tool.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib-sieve \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_SERVICE_INCLUDE)

libsieve_tool_la_SOURCES = \
	sieve-tool.c \
	mail-raw.c

noinst_HEADERS = \
	sieve-tool.h \
	mail-raw.h
