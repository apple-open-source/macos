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
	$(_v) $(INSTALL_FILE) -c airport	"$(Destination)/airport"
	$(_v) $(INSTALL_FILE) -c ascii		"$(Destination)/ascii"
	$(_v) $(INSTALL_FILE) -c birthtoken	"$(Destination)/birthtoken"
	$(_v) $(INSTALL_FILE) -c eqnchar	"$(Destination)/eqnchar"
	$(_v) $(INSTALL_FILE) -c flowers	"$(Destination)/flowers"
	$(_v) $(INSTALL_FILE) -c eqnchar	"$(Destination)/eqnchar"
	$(_v) $(INSTALL_FILE) -c getopt		"$(Destination)/getopt"
	$(_v) $(INSTALL_FILE) -c inter.phone	"$(Destination)/inter.phone"
	$(_v) $(INSTALL_FILE) -c man.template	"$(Destination)/man.template"
	$(_v) $(INSTALL_FILE) -c mdoc.template	"$(Destination)/mdoc.template"
	$(_v) $(INSTALL_FILE) -c na.phone	"$(Destination)/na.phone"
	$(_v) $(INSTALL_FILE) -c operator	"$(Destination)/operator"
	$(_v) $(INSTALL_FILE) -c style		"$(Destination)/style"
	$(_v) $(INSTALL_FILE) -c zipcodes	"$(Destination)/zipcodes"
	$(_v) $(INSTALL_FILE) -c termcap	"$(Destination)/termcap"
	$(_v) cap_mkdb -f "$(Destination)/termcap" "$(Destination)/termcap"
