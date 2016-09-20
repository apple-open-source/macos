# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# The following files vary by target platform.  Start with the MacOSX defaults
# and override as necessary.
SRC_PASSWD=
SRC_GROUP=
SRC_TTYS=ttys

ifeq "$(CONTENT_PLATFORM)" "ios"
SRC_PASSWD=master.passwd.iPhone
SRC_GROUP=group.iPhone
SRC_TTYS=ttys.embedded
endif

install:: validate-all install-files

validate-all:
	@echo "Validating files are UTF-8"
	$(_v) for X in * ; do \
		[ \! -f "$$X" ] || iconv -f UTF-8 -t UTF-8 "$$X" >/dev/null ; \
	done

install-files:
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	@echo "Installing $(Destination) for $(CONTENT_PLATFORM)"
	$(_v) $(INSTALL_FILE) -m 0644 -c afpovertcp.cfg	 "$(Destination)/afpovertcp.cfg"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/find.codes"
	$(_v) $(INSTALL_FILE) -m 0644 -c ftpusers	 "$(Destination)/ftpusers"
ifeq "$(CONTENT_PLATFORM)" "ios"
	$(_v) $(INSTALL_FILE) -m 0644 -c fstab		 "$(Destination)/fstab"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c fstab.hd	 "$(Destination)/fstab.hd"
	$(_v) $(INSTALL_FILE) -m 0644 -c gettytab	 "$(Destination)/gettytab"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/hosts.equiv"
	$(_v) $(INSTALL_FILE) -m 0644 -c kern_loader.conf "$(Destination)/kern_loader.conf"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/rmtab"
	$(_v) $(INSTALL_FILE) -m 0644 -c shells		 "$(Destination)/shells"
	$(_v) $(INSTALL_FILE) -m 0644 -c $(SRC_TTYS)	 "$(Destination)/ttys"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/xtab"
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) $(LN) -hfs ../var/run/resolv.conf            "$(Destination)"
endif
ifneq "$(SRC_GROUP)" ""
	$(_v) $(INSTALL_FILE) -m 0644 -c $(SRC_GROUP)	 "$(Destination)/group"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c hosts		 "$(Destination)/hosts"
ifeq "$(RC_BRIDGE)" "YES"
	$(_v) cat hosts.bridge >> "$(Destination)/hosts"
	# NOTE: make sure to override (not append) the fstab with fstab.bridge
	$(_v) $(INSTALL_FILE) -m 0644 -c fstab.bridge	 "$(Destination)/fstab"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c manpaths	 "$(Destination)/manpaths"
ifneq "$(SRC_PASSWD)" ""
	$(_v) $(INSTALL_FILE) -m 0600 -c $(SRC_PASSWD)	 "$(Destination)/master.passwd"
	$(_v) cat $(SRC_PASSWD) | cut -d ':' -f 1-4,8-10 > "$(Destination)/passwd"
	$(_v) chmod 0644 "$(Destination)/passwd" ; chown root:wheel "$(Destination)/passwd"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c networks	 "$(Destination)/networks"
	$(_v) $(INSTALL_FILE) -m 0644 -c paths		 "$(Destination)/paths"
ifeq "$(CONTENT_PLATFORM)" "ios"
	$(_v) cat paths.iPhone >> "$(Destination)/paths"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c protocols	 "$(Destination)/protocols"
	$(_v) $(INSTALL_FILE) -m 0644 -c rpc		 "$(Destination)/rpc"
	$(_v) $(INSTALL_FILE) -m 0644 -c services	 "$(Destination)/services"
endif
