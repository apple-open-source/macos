Destination = $(DSTROOT)/private/var/run

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_File_Mode = 644

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 775 -g daemon "$(Destination)"
	$(_v) $(INSTALL_FILE) -c /dev/null "$(Destination)/utmp"
