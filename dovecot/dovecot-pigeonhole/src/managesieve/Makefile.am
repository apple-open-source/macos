settingsdir = $(dovecot_moduledir)/settings

dovecot_pkglibexec_PROGRAMS = managesieve

AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_SERVICE_INCLUDE) \
	-DMODULEDIR=\""$(dovecot_moduledir)"\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/lib-sieve \
	-I$(top_srcdir)/src/lib-managesieve \
	-I$(top_srcdir)/src/lib-sievestorage

libmanagesieve_settings_la_LDFLAGS = -module -avoid-version

settings_LTLIBRARIES = \
	libmanagesieve_settings.la

libmanagesieve_settings_la_SOURCES = \
	managesieve-settings.c

managesieve_LDFLAGS = -export-dynamic

libs = \
	managesieve-settings.lo \
	$(top_builddir)/src/lib-managesieve/libmanagesieve.a \
	$(top_builddir)/src/lib-sievestorage/libsievestorage.a \
	$(top_builddir)/src/lib-sieve/libdovecot-sieve.la

managesieve_LDADD = $(libs) $(LIBDOVECOT_STORAGE) $(LIBDOVECOT_LDA) $(LIBDOVECOT)

managesieve_DEPENDENCIES = $(libs) $(LIBDOVECOT_STORAGE_DEPS) $(LIBDOVECOT_LDA_DEPS) $(LIBDOVECOT_DEPS)

cmds = \
	cmd-capability.c \
	cmd-logout.c \
	cmd-putscript.c \
	cmd-getscript.c \
	cmd-setactive.c \
	cmd-deletescript.c \
	cmd-listscripts.c \
	cmd-havespace.c \
	cmd-renamescript.c \
	cmd-noop.c

managesieve_SOURCES = \
	$(cmds) \
	managesieve-quota.c \
	managesieve-client.c \
	managesieve-commands.c \
	managesieve-capabilities.c \
	main.c 

noinst_HEADERS = \
	managesieve-quota.h \
	managesieve-client.h \
	managesieve-commands.h \
	managesieve-capabilities.h \
	managesieve-settings.h \
	managesieve-common.h 
