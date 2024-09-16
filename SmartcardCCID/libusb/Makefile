Project               = libusb
UserType              = Administrator
ToolType              = Libraries
GnuAfterInstall       = remove-autom4te-cache remove-shared-libusb
Extra_Configure_Flags = 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
	
remove-autom4te-cache:
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache

remove-shared-libusb:
	rm -f $(DSTROOT)/usr/lib/libusb*{dylib,la}
