$(srcdir)/ircg_scanner.c: $(srcdir)/ircg_scanner.re
	$(RE2C) $(srcdir)/ircg_scanner.re > $@
