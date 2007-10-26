# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_File_Group     = admin

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_FILE) -m 0640 -c /dev/null "$(Destination)/ipfw.log"
	$(_v) $(INSTALL_FILE) -m 0600 -c /dev/null "$(Destination)/secure.log"
