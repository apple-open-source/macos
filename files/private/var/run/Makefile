# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_File_Mode = 644

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_FILE) -c /dev/null "$(Destination)/.scratch"
