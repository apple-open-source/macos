Destination = $(DSTROOT)/usr/share/misc

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
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c ascii		"$(Destination)/ascii"
	$(_v) $(INSTALL_FILE) -c birthtoken	"$(Destination)/birthtoken"
	$(_v) $(INSTALL_FILE) -c eqnchar	"$(Destination)/eqnchar"
	$(_v) $(INSTALL_FILE) -c flowers	"$(Destination)/flowers"
	$(_v) $(INSTALL_FILE) -c eqnchar	"$(Destination)/eqnchar"
	$(_v) $(INSTALL_FILE) -c getopt		"$(Destination)/getopt"
	$(_v) $(INSTALL_FILE) -c man.template	"$(Destination)/man.template"
	$(_v) $(INSTALL_FILE) -c mdoc.template	"$(Destination)/mdoc.template"
	$(_v) $(INSTALL_FILE) -c operator	"$(Destination)/operator"
