regex_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${regex_DIR}
testregex: CFLAGS += -Wno-undef -Wno-sign-conversion -Wno-shorten-64-to-32

# This could likely both be generalized, but until we start adding more tests
# that we can actually test it against we'll just do an ad-hoc build and install
# for testregex.
install-testregex:	testregex
	mkdir -p $(INSTALLDIR)/regex
	install -m 0755 $(SYMROOT)/testregex $(INSTALLDIR)/testregex
	install -m 0644 $(regex_DIR)/datafiles/*.dat $(INSTALLDIR)/regex
