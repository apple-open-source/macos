Destination = $(DSTROOT)/private/var/log

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Directory_Mode = 755
Install_File_Mode      = 640

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/ftp.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/lookupd.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/lpr.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/mail.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/netinfo.log"
	$(_v) $(INSTALL_FILE) -c -m 600	/dev/null "$(Destination)/secure.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/system.log"
	$(_v) $(INSTALL_FILE) -c	/dev/null "$(Destination)/wtmp"
