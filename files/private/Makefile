Destination = $(DSTROOT)/private

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = etc var

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 1777 "$(Destination)/tmp"
