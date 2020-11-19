# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
ifeq "$(CONTENT_PLATFORM)" "osx"
	# <rdar://problem/63985973>
	$(_v) $(ECHO) " " > "$(Destination)/.file"
	$(_v) $(CHMOD) 750 "$(Destination)/.file"
	$(_v) $(CHOWN) root:wheel "$(Destination)/.file"
endif
