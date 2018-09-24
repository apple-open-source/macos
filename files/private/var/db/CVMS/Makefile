# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
ifeq "$(CONTENT_PLATFORM)" "osx"
	# <rdar://problem/41544772>
	$(_v) $(ECHO) " " > "$(Destination)/.file"
	$(_v) $(CHMOD) 644 "$(Destination)/.file"
	$(_v) $(CHOWN) root:wheel "$(Destination)/.file"
endif
