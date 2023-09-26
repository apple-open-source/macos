# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_FILE) files.conf "$(Destination)/files.conf"
endif
