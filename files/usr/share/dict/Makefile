Destination = $(DSTROOT)/usr/share/dict

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
	$(_v) $(INSTALL_FILE) -c connectives	"$(Destination)/connectives"
	$(_v) $(INSTALL_FILE) -c propernames	"$(Destination)/propernames"
	$(_v) $(INSTALL_FILE) -c web2		"$(Destination)/web2"
	$(_v) $(INSTALL_FILE) -c web2a		"$(Destination)/web2a"
	$(_v) $(INSTALL_FILE) -c README		"$(Destination)/README"
	$(_v) $(LN) -fs web2 "$(Destination)/words"
