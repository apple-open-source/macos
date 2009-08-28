Project = syslog
ProductType = tool
Install_Dir = /usr/bin

CFILES = syslog.c
MANPAGES = syslog.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-DINET6 \
        -I"$(OBJROOT)"/aslcommon
Extra_LD_Flags = -dead_strip -L"$(SYMROOT)" -laslcommon

# Determine product configuartion
PRODUCT = $(shell tconf --product)
ifeq ($(PRODUCT),iPhone)
Extra_CC_Flags += -DCONFIG_IPHONE
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	codesign -s- $(DSTROOT)/usr/bin/syslog
