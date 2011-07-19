AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/lib-master \
	-I$(top_srcdir)/src/lib-storage \
	-I$(top_srcdir)/src/imap

lib99_stats_plugin_la_LDFLAGS = -module -avoid-version

module_LTLIBRARIES = \
	lib99_stats_plugin.la

lib99_stats_plugin_la_SOURCES = \
	stats-plugin.c
