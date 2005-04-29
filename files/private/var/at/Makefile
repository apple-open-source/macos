Destination = $(DSTROOT)/private/var/at

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -o daemon "$(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -o daemon "$(Destination)/jobs"
