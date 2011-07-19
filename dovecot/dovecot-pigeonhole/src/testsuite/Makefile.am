noinst_PROGRAMS = testsuite

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib-sieve \
	-I$(top_srcdir)/src/lib-sieve-tool \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_SERVICE_INCLUDE)

testsuite_LDFLAGS = -export-dynamic

libs = \
	$(top_builddir)/src/lib-sieve/libdovecot-sieve.la \
	$(top_builddir)/src/lib-sieve-tool/libsieve-tool.la

testsuite_LDADD = $(libs) $(LIBDOVECOT_STORAGE) $(LIBDOVECOT_LDA) $(LIBDOVECOT)
testsuite_DEPENDENCIES = $(libs) $(LIBDOVECOT_STORAGE_DEPS) $(LIBDOVECOT_LDA_DEPS) $(LIBDOVECOT_DEPS)

commands = \
	cmd-test.c \
	cmd-test-fail.c \
	cmd-test-config.c \
	cmd-test-set.c \
	cmd-test-result.c \
	cmd-test-message.c \
	cmd-test-mailbox.c \
	cmd-test-binary.c

tests = \
	tst-test-script-compile.c \
	tst-test-script-run.c \
	tst-test-multiscript.c \
	tst-test-error.c \
	tst-test-result-action.c \
	tst-test-result-execute.c

testsuite_SOURCES = \
	testsuite-common.c \
	testsuite-settings.c \
	testsuite-objects.c \
	testsuite-substitutions.c \
	testsuite-arguments.c \
	testsuite-message.c \
	testsuite-log.c \
	testsuite-script.c \
	testsuite-result.c \
	testsuite-smtp.c \
	testsuite-mailstore.c \
	testsuite-binary.c \
	$(commands) \
	$(tests) \
	ext-testsuite.c \
	testsuite.c

noinst_HEADERS = \
	testsuite-common.h \
	testsuite-settings.h \
	testsuite-objects.h \
	testsuite-substitutions.h \
	testsuite-arguments.h \
	testsuite-message.h \
	testsuite-log.h \
	testsuite-script.h \
	testsuite-result.h \
	testsuite-smtp.h \
	testsuite-mailstore.h \
	testsuite-binary.h

