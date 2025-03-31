FreeBSD_strings_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

VPATH += ${FreeBSD_strings_DIR}
t_strchrnul: OTHER_LDFLAGS += -latf-c

# This could likely both be generalized, but until we start adding more tests
# that we can actually test it against we'll just do an ad-hoc build and install
# for t_strchrnul.
install-t_strchrnul:	t_strchrnul
	mkdir -p $(dir $(INSTALLDIR)/t_strchrnul)
	cp $(SYMROOT)/t_strchrnul $(INSTALLDIR)/t_strchrnul
