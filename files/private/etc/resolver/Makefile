Destination = $(DSTROOT)/private/etc/resolver

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs =

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install:: install-files

install-files:
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c -m 644 local		 "$(Destination)/local"
	$(_v) $(LN) -s local              "$(Destination)/254.169.in-addr.arpa"
