Destination = $(DSTROOT)/private/var/root/Library/Preferences

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_File_Mode = 600

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 700 "$(Destination)"
