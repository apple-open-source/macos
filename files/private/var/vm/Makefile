Destination = $(DSTROOT)/private/var/vm

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(TOUCH) "$(Destination)/swapfile"
ifeq ($(USER),root) # Can't set sticky on a file if not root
	$(_v) $(CHMOD) 1600 "$(Destination)/swapfile"
endif
