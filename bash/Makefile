##
# Makefile for Bash
##

# Project info
Project               = bash
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -no-cpp-precomp
Extra_Configure_Flags = --bindir=/bin --mandir=/usr/share
Extra_Install_Flags   = bindir=$(DSTROOT)/bin
GnuAfterInstall       = bashcleanup

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

bashcleanup:
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
