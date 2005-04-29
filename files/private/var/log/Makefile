Destination = $(DSTROOT)/private/var/log

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Directory_Mode = 755
Install_File_Mode      = 640
Install_File_Group     = admin

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c -m 640	/dev/null "$(Destination)/ipfw.log"
	$(_v) $(INSTALL_FILE) -c -m 600	/dev/null "$(Destination)/secure.log"
	$(_v) $(INSTALL_FILE) -c -m 644	/dev/null "$(Destination)/wtmp"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/lastlog"
