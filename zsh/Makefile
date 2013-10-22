##
# Makefile for zsh
##

# Project info
Project		      = zsh
ProjectVersion	      = 5.0.2
UserType	      = Administration
ToolType	      = Commands
Extra_CC_Flags	      = -no-cpp-precomp
Extra_Configure_Flags = --bindir="$(BINDIR)" --with-tcsetpgrp --enable-multibyte \
                        --enable-max-function-depth=700
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"
GnuAfterInstall	      = post-install install-plist strip-binaries

Patches = utmpx_ut_user.patch no_strip.patch arg_zero.patch \
          zsh-Doc.patch svn-zsh-complete.patch no_auto.patch

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# hack to resurrect zshall
COMPRESSMANPAGES = true

post-install:
	rm -f $(DSTROOT)/bin/zsh-$(ProjectVersion)
	rm $(DSTROOT)/usr/share/zsh/$(ProjectVersion)/scripts/newuser
	mkdir -p $(DSTROOT)/private/etc
	install -m 0444 -o root -g wheel zshenv $(DSTROOT)/private/etc

ZSH_MODULE_DIR = /usr/lib/zsh/$(ProjectVersion)/zsh

strip-binaries:
	$(MKDIR) $(SYMROOT)/bin $(SYMROOT)$(ZSH_MODULE_DIR) $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(CP) $(DSTROOT)/bin/zsh $(SYMROOT)/bin
	$(STRIP) -x $(DSTROOT)/bin/zsh
	$(MKDIR) $(SYMROOT)$(ZSH_MODULE_DIR)
	$(CP) $(DSTROOT)$(ZSH_MODULE_DIR)/*.so $(SYMROOT)$(ZSH_MODULE_DIR)
	$(STRIP) -x $(DSTROOT)$(ZSH_MODULE_DIR)/*.so
	$(MKDIR) $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(CP) $(DSTROOT)$(ZSH_MODULE_DIR)/net/*.so $(SYMROOT)$(ZSH_MODULE_DIR)/net
	$(STRIP) -x $(DSTROOT)$(ZSH_MODULE_DIR)/net/*.so

install_source::
	$(RMDIR) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
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

##---------------------------------------------------------------------
# Patch config.h just after running configure
#
# RLIMIT_RSS is now the save value as the new RLIMIT_AS, which causes
# a duplicate case value.  So we undefine HAVE_RLIMIT_RSS.
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) ed - ${BuildDirectory}/config.h < $(SRCROOT)/patches/config.h.ed
	$(_v) $(TOUCH) $(ConfigStamp2)
