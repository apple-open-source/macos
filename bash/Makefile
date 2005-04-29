##
# Makefile for Bash
##

# Project info
Project               = bash
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -no-cpp-precomp -mdynamic-no-pic
Extra_Configure_Flags = --bindir=/bin --mandir=/usr/share
Extra_Install_Flags   = bindir=$(DSTROOT)/bin
Extra_LD_Flags        = -Wl,-search_paths_first
GnuAfterInstall       = after-install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Install in /usr
USRDIR       = /usr

# Bash makefiles are a bit screwy...
# Setting CCFLAGS upsets bash, so we override Environment
# so that it doesn't.
Environment =   CFLAGS="$(CFLAGS)"	\
	       LDFLAGS="$(LDFLAGS)"	\
	      $(Extra_Environment)

after-install:
	mkdir $(DSTROOT)/usr/bin
	chown root:wheel $(DSTROOT)/usr/bin
	mv $(DSTROOT)/bin/bashbug $(DSTROOT)/usr/bin
	mkdir -p $(DSTROOT)/usr/local/bin
	chown root:wheel $(DSTROOT)/usr/local/bin
	ln -s /bin/bash $(DSTROOT)/usr/local/bin/bash
	$(INSTALL_PROGRAM) -c $(DSTROOT)$(BINDIR)/bash $(DSTROOT)$(BINDIR)/sh
	mkdir -p $(DSTROOT)/private/etc
	cp $(SRCROOT)/bashrc $(DSTROOT)/private/etc/bashrc
	cp $(SRCROOT)/profile $(DSTROOT)/private/etc/profile
	rm -rf $(DSTROOT)/usr/html
	rm -f $(DSTROOT)/usr/share/info/dir
	ln -s bash.1 $(DSTROOT)/usr/share/man/man1/sh.1
	mkdir -p $(DSTROOT)/usr/share/doc/bash
	cp $(SRCROOT)/doc/*.pdf $(DSTROOT)/usr/share/doc/bash
	cp $(SRCROOT)/doc/*.html $(DSTROOT)/usr/share/doc/bash
