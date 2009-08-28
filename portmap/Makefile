Project = portmap
Install_Dir = /usr/sbin

CFILES = portmap.c from_local.c pmap_check.c
MANPAGES = portmap.8

Extra_CC_Flags = -Wall -Werror -Wno-deprecated-declarations
Extra_CC_Flags += -DHOSTS_ACCESS -DINSTANT_OFF

Extra_LD_Flags = -lwrap -Wl,-pie

SubProjects = pmap_set pmap_dump

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
SANDBOX = $(DSTROOT)/usr/share/sandbox

after_install:
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	cp com.apple.portmap.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/LICENSE $(OSL)/$(Project).txt
	$(MKDIR) $(SANDBOX)
	$(INSTALL_FILE) portmap.sb $(SANDBOX)/portmap.sb
