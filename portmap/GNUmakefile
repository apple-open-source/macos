Project = portmap
UserType = Developer
ToolType = Tool
BSD_Executable_Path = /usr/sbin
BSD_After_Install = install_plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

CC_Debug = -Wall

install_plist::
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	cp com.apple.portmap.plist $(DSTROOT)/System/Library/LaunchDaemons
