Destination = $(DSTROOT)/private/etc

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = resolver

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install:: install-sysconf install-files

install-sysconf:
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c -m 644 afpovertcp.cfg	 "$(Destination)/afpovertcp.cfg"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/find.codes"
	$(_v) $(INSTALL_FILE) -c -m 644 ftpusers	 "$(Destination)/ftpusers"
	$(_v) $(INSTALL_FILE) -c -m 644 fstab.hd	 "$(Destination)/fstab.hd"
	$(_v) $(INSTALL_FILE) -c -m 644 fstab.rd	 "$(Destination)/fstab.rd"
	$(_v) $(INSTALL_FILE) -c -m 644 fstab.sd	 "$(Destination)/fstab.sd"
	$(_v) $(INSTALL_FILE) -c -m 644 gettytab	 "$(Destination)/gettytab"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/hosts.lpd"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/hosts.equiv"
	$(_v) $(INSTALL_FILE) -c -m 644 inetd.conf	 "$(Destination)/inetd.conf"
	$(_v) $(INSTALL_FILE) -c -m 644 kern_loader.conf "$(Destination)/kern_loader.conf"
	$(_v) $(INSTALL_FILE) -c -m 644 motd		 "$(Destination)/motd"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/rmtab"
	$(_v) $(INSTALL_FILE) -c -m 644 shells		 "$(Destination)/shells"
	$(_v) $(INSTALL_FILE) -c -m 644 syslog.conf	 "$(Destination)/syslog.conf"
ifeq "$(RC_RELEASE)" "Darwin"
	$(_v) cat ttys | sed -e 's/^console/##console/' -e 's/^#console/console/' > "$(Destination)/ttys"
	$(_v) chmod 644 "$(Destination)/ttys" ; chown root:wheel "$(Destination)/ttys"
else
	$(_v) $(INSTALL_FILE) -c -m 644 ttys		 "$(Destination)/ttys"
endif
	$(_v) $(INSTALL_FILE) -c	ttys.installer	 "$(Destination)/ttys.installer"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/xtab"
	$(_v) $(LN) -s /var/run/resolv.conf              "$(Destination)"

# We should stop installing the files which are pulled into NetInfo, but we should
# first make sure that doesn't break anything.
# - at boot time, we (possibly nominal) errors messages if master.passwd is missing.
# - syslogd doesn't like it if services is missing.
install-files:
	$(_v) $(INSTALL_FILE) -c -m 644 group		 "$(Destination)/group"
	$(_v) $(INSTALL_FILE) -c -m 644 hosts		 "$(Destination)/hosts"
	$(_v) $(INSTALL_FILE) -c -m 600 master.passwd	 "$(Destination)/master.passwd"
	$(_v) $(INSTALL_FILE) -c -m 644 networks	 "$(Destination)/networks"
	$(_v) $(INSTALL_FILE) -c -m 644 passwd		 "$(Destination)/passwd"
	$(_v) $(INSTALL_FILE) -c -m 644 protocols	 "$(Destination)/protocols"
	$(_v) $(INSTALL_FILE) -c -m 644 rpc		 "$(Destination)/rpc"
	$(_v) $(INSTALL_FILE) -c -m 644 services	 "$(Destination)/services"
