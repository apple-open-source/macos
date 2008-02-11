Project = rtadvd
Install_Dir = /usr/sbin

CFILES = advcap.c config.c dump.c if.c rrenum.c rtadvd.c timer.c
HFILES = advcap.h config.h dump.h if.h pathnames.h rrenum.h rtadvd.h timer.h 
MANPAGES = rtadvd.8 rtadvd.conf.5

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DHAVE_GETIFADDRS \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

# 6to4.conf should be root-writable
Install_File_Mode = 0644

after_install:
	$(INSTALL_DIRECTORY) -d "$(DSTROOT)"/private/etc
	$(INSTALL_FILE) rtadvd.conf "$(DSTROOT)"/private/etc
