Destination = $(DSTROOT)/private/var/root/Library/Preferences

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_File_Mode = 644

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c "com.apple.finder.plist" "$(Destination)"
	$(_v) $(INSTALL_FILE) -c "loginwindow.plist"      "$(Destination)"
	$(_v) $(INSTALL_FILE) -c "LSApplications"      "$(Destination)"
	$(_v) $(INSTALL_FILE) -c "LSClaimedTypes"      "$(Destination)"
	$(_v) $(INSTALL_FILE) -c "LSSchemes"      "$(Destination)"
