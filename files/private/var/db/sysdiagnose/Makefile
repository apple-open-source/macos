# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
ifeq "$(CONTENT_PLATFORM)" "ios"
	# <rdar://problem/64704241>
	$(_v) $(ECHO) " " > "$(Destination)/.file"
	$(_v) $(CHMOD) 755 "$(Destination)/.file"
	$(_v) $(CHOWN) root:wheel "$(Destination)/.file"
endif
