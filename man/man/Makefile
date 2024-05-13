
ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/Standard/Standard.make

install:
	mkdir -p $(DESTDIR)$(ETCDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p $(DESTDIR)$(USRBINDIR)

	$(INSTALL_FILE) man.conf $(DESTDIR)$(ETCDIR)
	$(INSTALL_SCRIPT) man.sh $(DESTDIR)$(USRBINDIR)/man
	$(LN) -f $(DESTDIR)$(USRBINDIR)/man $(DESTDIR)$(USRBINDIR)/manpath
	$(LN) -f $(DESTDIR)$(USRBINDIR)/man $(DESTDIR)$(USRBINDIR)/apropos
	$(LN) -f $(DESTDIR)$(USRBINDIR)/man $(DESTDIR)$(USRBINDIR)/whatis

	$(INSTALL_FILE) apropos.1 $(DESTDIR)$(MANDIR)/man1/apropos.1
	$(INSTALL_FILE) man.1 $(DESTDIR)$(MANDIR)/man1/man.1
	$(INSTALL_FILE) manpath.1 $(DESTDIR)$(MANDIR)/man1/manpath.1
	$(INSTALL_FILE) whatis.1 $(DESTDIR)$(MANDIR)/man1/whatis.1
	$(INSTALL_FILE) man.conf.5 $(DESTDIR)$(MANDIR)/man5/man.conf.5

	# Install test material as well
	$(MAKE) -C $(SRCROOT)/tests install
