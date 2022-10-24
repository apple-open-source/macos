# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing $(Destination)"
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) $(INSTALL_FILE) -c firmlinks "$(Destination)"
endif
