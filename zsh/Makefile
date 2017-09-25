##
# Makefile for zsh
##

# Project info
Project		      = zsh
ProjectVersion	      = 5.3
UserType	      = Administration
ToolType	      = Commands
Extra_CC_Flags	      = -no-cpp-precomp
Extra_Configure_Flags = --bindir="$(BINDIR)" --with-tcsetpgrp --enable-multibyte \
                        --enable-unicode9 \
                        --enable-max-function-depth=700
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"
GnuAfterInstall	      = post-install install-plist strip-binaries

Patches = no_strip.patch arg_zero.patch \
          zsh-Doc.patch svn-zsh-complete.patch no_auto.patch \
          export-22966068.patch \
          log-24988289.patch

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

post-install:
	rm -f $(DSTROOT)/bin/zsh-$(ProjectVersion)
	rm $(DSTROOT)/usr/share/zsh/$(ProjectVersion)/scripts/newuser
	mkdir -p $(DSTROOT)/private/etc
	install -m 0444 -o root -g wheel zprofile $(DSTROOT)/private/etc
	install -m 0444 -o root -g wheel zshrc $(DSTROOT)/private/etc

ZSH_MODULE_DIR = /usr/lib/zsh/$(ProjectVersion)/zsh

strip-binaries:
	$(MKDIR) $(SYMROOT)/bin $(SYMROOT)$(ZSH_MODULE_DIR) $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(CP) $(DSTROOT)/bin/zsh $(SYMROOT)/bin
	$(STRIP) -x $(DSTROOT)/bin/zsh
	$(DSYMUTIL) $(SYMROOT)/bin/zsh
	$(MKDIR) $(SYMROOT)$(ZSH_MODULE_DIR)
	$(CP) $(DSTROOT)$(ZSH_MODULE_DIR)/*.so $(SYMROOT)$(ZSH_MODULE_DIR)
	$(STRIP) -x $(DSTROOT)$(ZSH_MODULE_DIR)/*.so
	$(DSYMUTIL) $(SYMROOT)$(ZSH_MODULE_DIR)/*.so
	$(MKDIR) $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(CP) $(DSTROOT)$(ZSH_MODULE_DIR)/net/*.so $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(STRIP) -x $(DSTROOT)$(ZSH_MODULE_DIR)/net/*.so
	$(DSYMUTIL) $(SYMROOT)$(ZSH_MODULE_DIR)/net/*.so

install_source::
	$(RMDIR) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.xz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
	    patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENCE $(OSL)/$(Project).txt
