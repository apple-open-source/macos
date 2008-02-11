Project = ip6
Install_Dir = /usr/sbin

CFILES = ip6tool.c
MANPAGES = ip6.8 ip6config.8
OTHERSRCS = Makefile.preamble Makefile Makefile.postamble 6to4.conf \
            ip6config ip6config.8 ip6.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

# 6to4.conf should be root-writable
Install_File_Mode = 0644

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/etc
	$(INSTALL_FILE) 6to4.conf "$(DSTROOT)"/private/etc
	$(INSTALL_SCRIPT) ip6config "$(DSTROOT)"/usr/sbin
