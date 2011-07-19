AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-lda \
	-I$(top_srcdir)/src/lib-mail \
	-I$(top_srcdir)/src/lib-storage

lib20_push_notify_plugin_la_LDFLAGS = -module -avoid-version \
	-F/System/Library/PrivateFrameworks \
	-framework XMPP \
	-framework CoreFoundation

module_LTLIBRARIES = \
	lib20_push_notify_plugin.la

lib20_push_notify_plugin_la_SOURCES = \
	push-notify-plugin.c

noinst_HEADERS = \
	push-notify-plugin.h
