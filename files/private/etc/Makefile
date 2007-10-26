# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# The following files vary by target platform.  Start with the MacOSX defaults
# and override as necessary.
SRC_PASSWD=master.passwd
SRC_GROUP=group
SRC_TTYS=ttys

Product=$(shell tconf --product)
ifeq "$(Product)" "AppleTV"
SRC_PASSWD=master.passwd.AppleTV
SRC_GROUP=group
SRC_TTYS=ttys.embedded
endif
ifeq "$(Product)" "iPhone"
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
	@echo "Installing $(Destination) for $(Product)"
	$(_v) $(INSTALL_FILE) -m 0644 -c afpovertcp.cfg	 "$(Destination)/afpovertcp.cfg"
	$(_v) $(INSTALL_FILE) -m 0644 -c asl.conf	 "$(Destination)/asl.conf"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/find.codes"
	$(_v) $(INSTALL_FILE) -m 0644 -c ftpusers	 "$(Destination)/ftpusers"
	$(_v) $(INSTALL_FILE) -m 0644 -c fstab.hd	 "$(Destination)/fstab.hd"
	$(_v) $(INSTALL_FILE) -m 0644 -c gettytab	 "$(Destination)/gettytab"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/hosts.equiv"
	$(_v) $(INSTALL_FILE) -m 0644 -c kern_loader.conf "$(Destination)/kern_loader.conf"
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/rmtab"
	$(_v) $(INSTALL_FILE) -m 0644 -c shells		 "$(Destination)/shells"
	$(_v) $(INSTALL_FILE) -m 0644 -c syslog.conf	 "$(Destination)/syslog.conf"
ifeq "$(RC_RELEASE)" "Darwin"
	$(_v) cat ttys | sed -e 's/^console/##console/' -e 's/^#console/console/' > "$(Destination)/ttys"
	$(_v) chmod 0644 "$(Destination)/ttys" ; chown root:wheel "$(Destination)/ttys"
else
	$(_v) $(INSTALL_FILE) -m 0644 -c $(SRC_TTYS)	 "$(Destination)/ttys"
endif
	$(_v) $(INSTALL_FILE) -m 0644 -c /dev/null	 "$(Destination)/xtab"
	$(_v) $(LN) -s /var/run/resolv.conf              "$(Destination)"
	$(_v) $(INSTALL_FILE) -m 0644 -c $(SRC_GROUP)	 "$(Destination)/group"
	$(_v) $(INSTALL_FILE) -m 0644 -c hosts		 "$(Destination)/hosts"
	$(_v) $(INSTALL_FILE) -m 0644 -c manpaths	 "$(Destination)/manpaths"
	$(_v) $(INSTALL_FILE) -m 0600 -c $(SRC_PASSWD)	 "$(Destination)/master.passwd"
	$(_v) cat $(SRC_PASSWD) | cut -d ':' -f 1-4,8-10 > "$(Destination)/passwd"
	$(_v) chmod 0644 "$(Destination)/passwd" ; chown root:wheel "$(Destination)/passwd"
	$(_v) $(INSTALL_FILE) -m 0644 -c networks	 "$(Destination)/networks"
	$(_v) $(INSTALL_FILE) -m 0644 -c paths		 "$(Destination)/paths"
	$(_v) $(INSTALL_FILE) -m 0644 -c protocols	 "$(Destination)/protocols"
	$(_v) $(INSTALL_FILE) -m 0644 -c rpc		 "$(Destination)/rpc"
	$(_v) $(INSTALL_FILE) -m 0644 -c services	 "$(Destination)/services"
