##
# Makefile for am-utils
##
# Jordan K. Hubbard <jkh@apple.com>
##

# Project info
Project            = am-utils
UserType           = Administration
ToolType           = Services
Extra_CC_Flags     = -no-cpp-precomp
Extra_LD_Libraries = -bind_at_load -force_flat_namespace
GnuAfterInstall    = do-fixups install-startup install-config
Extra_Environment  = AR="$(SRCROOT)/ar.sh"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Clean up various weirdness to conform to Mac OS X standards
do-fixups:
	rm -f $(DSTROOT)/usr/share/info/dir
	rm -rf $(DSTROOT)/usr/lib

install-startup:
	mkdir -p $(DSTROOT)/System/Library/StartupItems/AMD/Resources/English.lproj
	$(INSTALL) -c -m 555 $(SRCROOT)/AMD $(DSTROOT)/System/Library/StartupItems/AMD/
	$(INSTALL) -c -m 444 $(SRCROOT)/StartupParameters.plist $(DSTROOT)/System/Library/StartupItems/AMD/
	$(INSTALL) -c -m 444 $(SRCROOT)/Localizable.strings $(DSTROOT)/System/Library/StartupItems/AMD/Resources/English.lproj/

install-config:
	mkdir -p $(DSTROOT)/private/etc
	$(INSTALL) -c -m 444 $(SRCROOT)/amd.conf.template $(DSTROOT)/private/etc
	$(INSTALL) -c -m 444 $(SRCROOT)/amd.map.template $(DSTROOT)/private/etc
