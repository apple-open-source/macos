$(srcdir)/ircg_scanner.c: $(srcdir)/ircg_scanner.re
	re2c $(srcdir)/ircg_scanner.re > $@
