Project = portmap
UserType = Developer
ToolType = Tool
BSD_Executable_Path = /usr/sbin
BSD_After_Install = install_plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

CC_Debug = -Wall

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
SANDBOX = $(DSTROOT)/usr/share/sandbox
install_plist::
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	cp com.apple.portmap.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/LICENSE $(OSL)/$(Project).txt
	$(MKDIR) $(SANDBOX)
	$(INSTALL_FILE) portmap.sb $(SANDBOX)/portmap.sb

