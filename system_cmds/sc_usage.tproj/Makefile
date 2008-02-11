Project = sc_usage
Install_Dir = /usr/bin

CFILES = sc_usage.c
MANPAGES = sc_usage.1

Extra_LD_Flags = -lcurses

Extra_CC_Flags = -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders\
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders/bsd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/misc
	$(INSTALL_FILE) trace.codes $(DSTROOT)/usr/share/misc

