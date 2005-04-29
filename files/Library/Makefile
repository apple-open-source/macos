Destination = $(DSTROOT)/Library

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = Documentation 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 1775 "$(Destination)"
	@echo "Installing $(Destination)/Caches"
	$(_v) $(INSTALL_DIRECTORY) -m 1777 "$(Destination)/Caches"
	@echo "Installing $(Destination)/Frameworks"
	$(_v) $(INSTALL_DIRECTORY) -m 775 "$(Destination)/Frameworks"
