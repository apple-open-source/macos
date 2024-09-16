FreeBSD_gen_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_gen_DIR}
popen_test: OTHER_LDFLAGS += -latf-c

# This could likely both be generalized, but until we start adding more tests
# that we can actually test it against we'll just do an ad-hoc build and install
# for popen_test.
install-popen_test:	popen_test
	mkdir -p $(INSTALLDIR)
	install -m 0755 $(SYMROOT)/popen_test $(INSTALLDIR)
