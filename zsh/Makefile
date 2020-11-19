##
# Makefile for zsh
##

# Project info
Project		      = zsh
ProjectVersion	      = 5.8
UserType	      = Administration
ToolType	      = Commands
Extra_CC_Flags	      = -no-cpp-precomp
Extra_CPP_Flags	      = -DUSE_GETCWD
Extra_Configure_Flags = --bindir="$(BINDIR)" --with-tcsetpgrp --enable-multibyte \
                        --enable-unicode9 \
                        --enable-max-function-depth=700
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"
GnuAfterInstall	      = post-install install-plist strip-binaries

ifeq ($(RC_PURPLE),YES)
Extra_Configure_Flags += --host=arm-apple-darwin
Extra_Configure_Flags += --cache-file=$(SRCROOT)/configure.cache-embedded

Extra_CC_Flags += -isysroot $(SDKROOT)
Extra_CPP_Flags += -isysroot $(SDKROOT)
Extra_LD_Flags += -isysroot $(SDKROOT)

Extra_CPP_Flags += $(wordlist 1, 2, $(CC_Archs))
endif

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

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENCE $(OSL)/$(Project).txt
