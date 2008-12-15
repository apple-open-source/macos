Project = syslogd
ProductType = tool
Install_Dir = /usr/sbin

CFILES = asl_action.c asl_in.c bsd_in.c bsd_out.c daemon.c dbserver.c klog_in.c remote.c syslogd.c udp_in.c

MANPAGES = asl.conf.5 syslogd.8 syslog.conf.5
#syslogd.sb
LAUNCHD_PLISTS = com.apple.syslogd.plist

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-DINET6 \
	-I"$(OBJROOT)"/aslcommon \
	-I../aslcommon \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders

# Determine product configuartion
PRODUCT = $(shell tconf --product)
ifeq ($(PRODUCT),MacOSX)
Extra_CC_Flags += -DCONFIG_MAC -DREMOTE_IPV4
endif
ifeq ($(PRODUCT),AppleTV)
Extra_CC_Flags += -DCONFIG_APPLETV -DREMOTE_IPV4
endif
ifeq ($(PRODUCT),iPhone)
Extra_CC_Flags += -DCONFIG_IPHONE -DLOCKDOWN
endif

Extra_LD_Flags = -dead_strip -L"$(SYMROOT)" -laslcommon

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/var/log/asl
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/usr/share/sandbox
	$(INSTALL_FILE) syslogd.sb "$(DSTROOT)"/usr/share/sandbox
