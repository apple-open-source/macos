noinst_LTLIBRARIES = libsieve_ext_mailbox.la

AM_CPPFLAGS = \
	-I$(srcdir)/../.. \
	$(LIBDOVECOT_INCLUDE)

tags = \
	tag-mailbox-create.c

tests = \
	tst-mailboxexists.c

libsieve_ext_mailbox_la_SOURCES = \
	$(tags) \
	$(tests) \
	ext-mailbox.c

noinst_HEADERS = \
	ext-mailbox-common.h
