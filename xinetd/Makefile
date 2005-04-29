##
# Makefile for xinetd
##

# Project info
Project               = xinetd
ProjectName           = xinetd
Extra_CC_Flags        = -no-cpp-precomp -mdynamic-no-pic
Extra_Configure_Flags = --with-libwrap
GnuAfterInstall       = install-macosx-config install-launchd-plist

lazy_install_source:: shadow_source

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
	
install-macosx-config:
	rm $(DSTROOT)/usr/sbin/itox
	rm $(DSTROOT)/usr/sbin/xconv.pl
	rm $(DSTROOT)/usr/share/man/man8/itox.8
	mkdir -p $(DSTROOT)/private/etc/xinetd.d
	chmod 755 $(DSTROOT)/private/etc/xinetd.d
	install -m 644 xinetd.conf $(DSTROOT)/private/etc/xinetd.conf
	install -m 644 inetd.conf $(DSTROOT)/private/etc/inetd.conf

install-launchd-plist:
	$(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_FILE) $(SRCROOT)/org.xinetd.xinetd.plist \
		$(DSTROOT)/System/Library/LaunchDaemons
