#
# Makefile for installing .keyboard and .keymapping files into the system
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make

INSTALL_DIR=$(DSTROOT)$(SYSTEM_LIBRARY_DIR)/Keyboards
RELNOTES_DIR=$(DSTROOT)/usr/local/RelNotes

KEYMAPPING_DIR=keymappings
KEYBOARD_DIR=keyboards

OTHER_SRCS=Makefile Keymaps.rtf

SRCFILES = $(KEYBOARD_DIR) $(KEYMAPPING_DIR) $(OTHER_SRCS)

SRCROOT=
OBJROOT=./obj
SYMROOT=.
DSTROOT=/

ifneq "" "$(wildcard /bin/mkdirs)"
	MKDIRS = /bin/mkdirs
else
	MKDIRS = /bin/mkdir -p
endif

clean:
installhdrs:
all:

install: $(INSTALL_DIR) $(RELNOTES_DIR)
	install -c -m 444 $(KEYMAPPING_DIR)/*.keymapping $(INSTALL_DIR)
	install -c -m 444 $(KEYBOARD_DIR)/*.keyboard $(INSTALL_DIR)
	install -c -m 444 Keymaps.rtf $(RELNOTES_DIR)

installsrc: SRCROOT $(SRCROOT)
	gnutar cf - $(SRCFILES) | (cd $(SRCROOT); gnutar xf -)
	
SRCROOT:
	@if [ -n "${$@}" ]; then exit 0; \
	else echo Must define $@; exit 1; fi

$(SRCROOT)::
	-rm -rf $(SRCROOT)
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)

$(INSTALL_DIR) $(RELNOTES_DIR)::
	-rm -rf $@
	$(MKDIRS) $@
	chmod 755 $@

