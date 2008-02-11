# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 0750 "$(Destination)/Media"
	$(_v) $(INSTALL_FILE) -c .forward "$(Destination)"
