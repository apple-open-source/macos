Destination = $(DSTROOT)/private/var/spool

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
	$(_v) $(INSTALL_DIRECTORY) -c -m 775 -o daemon -g daemon "$(Destination)/Faxes"
	$(_v) $(INSTALL_DIRECTORY) -c -m 750                     "$(Destination)/mqueue"
