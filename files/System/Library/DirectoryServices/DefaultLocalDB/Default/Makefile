# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SUBDIRS=aliases \
	computers \
	groups \
	networks \
	users

install::
	@echo "Installing $(Destination)"

	@for DIR in $(SUBDIRS); do \
		$(MKDIR) "$(Destination)/$$DIR" ; \
		$(CHOWN) root:wheel "$(Destination)/$$DIR" ; \
		$(CHMOD) 0700 "$(Destination)/$$DIR" ; \
		$(INSTALL) -m 0600 -o root -g wheel \
			"$$DIR"/*.plist "$(Destination)/$$DIR"; \
		for PLIST in "$(Destination)/$$DIR"/*.plist ; do \
			plutil -convert binary1 "$$PLIST" ; \
		done ; \
	done

	$(INSTALL) -d -m 0755 -o root -g wheel \
		"$(DSTROOT)/private/var/db/dslocal/nodes/Default"
	ditto "$(Destination)" \
		"$(DSTROOT)/private/var/db/dslocal/nodes/Default"
	$(CHMOD) 0600 "$(DSTROOT)/private/var/db/dslocal/nodes/Default"

	$(_v) python users.py users/*.plist > \
		"$(DSTROOT)"/private/etc/master.passwd
	$(_v) $(CHOWN) root:wheel "$(DSTROOT)/private/etc/master.passwd"
	$(_v) $(CHMOD) 0600 "$(DSTROOT)/private/etc/master.passwd" 

	$(_v) cat "$(DSTROOT)"/private/etc/master.passwd | \
		cut -d ':' -f 1-4,8-10 > "$(DSTROOT)/private/etc/passwd"
	$(_v) $(CHOWN) root:wheel "$(DSTROOT)/private/etc/passwd"
	$(_v) $(CHMOD) 0644 "$(DSTROOT)/private/etc/passwd" 

	$(_v) python groups.py groups/*.plist > \
		"$(DSTROOT)"/private/etc/group
	$(_v) $(CHOWN) root:wheel "$(DSTROOT)/private/etc/group"
	$(_v) $(CHMOD) 0644 "$(DSTROOT)/private/etc/group" 

