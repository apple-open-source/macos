Destination = $(DSTROOT)/private/var/root/Library

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = Preferences

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 700 "$(Destination)"
