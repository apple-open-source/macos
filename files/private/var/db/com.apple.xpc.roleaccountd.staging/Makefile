# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	# rdar://problem/32926465
	$(_v) $(ECHO) "32926465" > "$(Destination)/.file"
	$(_v) $(CHMOD) 0644 "$(Destination)/.file"
	$(_v) $(CHOWN) root:wheel "$(Destination)/.file"
endif
