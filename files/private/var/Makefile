Destination = $(DSTROOT)/private/var

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = at log root run spool vm

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY)			"$(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 700		"$(Destination)/backups"
	$(_v) $(INSTALL_DIRECTORY)			"$(Destination)/db"
	$(_v) $(INSTALL_DIRECTORY) -g sys		"$(Destination)/empty"
	$(_v) $(INSTALL_DIRECTORY) -m  775 -g mail	"$(Destination)/mail"
	$(_v) $(INSTALL_DIRECTORY)			"$(Destination)/netboot"
	$(_v) $(INSTALL_DIRECTORY)			"$(Destination)/msgs"
	$(_v) $(INSTALL_DIRECTORY)			"$(Destination)/rwho"
	$(_v) $(INSTALL_DIRECTORY) -m 1777		"$(Destination)/tmp"
