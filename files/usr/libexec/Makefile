Destination = $(DSTROOT)/usr/libexec

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = 

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && echo $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) $(Destination)
	$(_v) $(INSTALL_FILE) -m 755 -c create_nidb "$(Destination)/create_nidb"
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/private/var/db/netinfo
	$(_v) $(PERL) create_nidb local localhost $(DSTROOT)/private
