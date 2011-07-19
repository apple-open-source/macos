AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib-sieve \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_DICT_INCLUDE) \
	$(LIBDOVECOT_LDA_INCLUDE)

lib90_sieve_plugin_la_LDFLAGS = -module -avoid-version

dovecot_module_LTLIBRARIES = lib90_sieve_plugin.la

lib90_sieve_plugin_la_LIBADD = \
	$(top_builddir)/src/lib-sieve/libdovecot-sieve.la

lib90_sieve_plugin_la_SOURCES = \
	lda-sieve-plugin.c 

noinst_HEADERS = \
	lda-sieve-plugin.h
