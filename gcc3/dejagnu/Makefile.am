## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

SUBDIRS = doc testsuite example

# driver script goes in /usr/local/bin
bin_SCRIPTS = runtest

# auxiliary scripts go in /usr/local/share/dejagnu
pkgdata_SCRIPTS = config.guess runtest.exp

# Below, host-independent data files that need to get installed.
# We do it this way so we can use globbing.

lib_dest = $(DESTDIR)$(pkgdatadir)
lib_files = $(srcdir)/*.c $(srcdir)/lib/*.exp

baseboards_dest = $(DESTDIR)$(pkgdatadir)/baseboards
baseboards_files = $(srcdir)/baseboards/README $(srcdir)/baseboards/*.exp

config_dest = $(DESTDIR)$(pkgdatadir)/config
config_files = $(srcdir)/config/README $(srcdir)/config/*.exp

install-data-local:
	$(mkinstalldirs) $(lib_dest)
	for f in $(lib_files); do \
	  test ! -f "$$f" || $(INSTALL_DATA) $$f $(lib_dest); \
	done
	#
	$(mkinstalldirs) $(baseboards_dest)
	for f in $(baseboards_files); do \
	  test ! -f "$$f" || $(INSTALL_DATA) $$f $(baseboards_dest); \
	done
	#
	$(mkinstalldirs) $(config_dest)
	for f in $(config_files); do \
	  test ! -f "$$f" || $(INSTALL_DATA) $$f $(config_dest); \
	done

uninstall-local:
	for f in $(lib_files); do \
	  test ! -f "$$f" || rm -f $(lib_dest)/`basename "$$f"`; \
	done
	for f in $(config_files); do \
	  test ! -f "$$f" || rm -f $(config_dest)/`basename "$$f"`; \
	done
	for f in $(baseboards_files); do \
	  test ! -f "$$f" || rm -f $(baseboards_dest)/`basename "$$f"`; \
	done
