# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	@echo "Installing $(Destination) for $(CONTENT_PLATFORM)"
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) sed -e "/^#/d" -e "/^$$/d" Data.symlink-patterns >"$(Destination)/Data.symlink-patterns"
endif
