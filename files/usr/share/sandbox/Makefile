# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_FILE) -c bsd.sb		"$(Destination)/bsd.sb"
	# rdar://problem/8207011
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)/../../local/share/sandbox/profiles/embedded/imports"
	$(_v) $(INSTALL_FILE) -c bsd.sb		"$(Destination)/../../local/share/sandbox/profiles/embedded/imports/bsd.sb"
