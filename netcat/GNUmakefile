Project           = netcat
Extra_CC_Flags    = -mdynamic-no-pic -DUSE_SELECT
BSD_After_Install = install-plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(HEAD) -n 32 $(SRCROOT)/netcat.c > $(OSL)/$(Project).txt
	$(CHMOD) $(Install_File_Mode) $(OSL)/$(Project).txt
