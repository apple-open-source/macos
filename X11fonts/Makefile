Project               = X11fonts
UserType              = Administrator
ToolType              = Commands

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
MKDIR = /bin/mkdir -p
INSTALL = /usr/bin/install -c

install::
	@echo "Building $(Project)..."
	$(MKDIR) $(DSTROOT)/usr/X11/bin
	$(INSTALL) -m 755 $(SRCROOT)/font_cache.sh $(DSTROOT)/usr/X11/bin/font_cache
	sed s+DSTROOT+$(DSTROOT)+ < $(SRCROOT)/build-fonts.conf.in > $(OBJROOT)/build-fonts.conf
	$(MKDIR) $(DSTROOT)/usr/X11/var/cache/fontconfig
	./build install
	$(MKDIR) $(DSTROOT)/usr/X11/lib/X11/fonts/TTF
	ditto $(SRCROOT)/TTF $(DSTROOT)/usr/X11/lib/X11/fonts/TTF
	chown -R root:wheel $(DSTROOT)/usr/X11/lib/X11/fonts/TTF
	rm -rf $(DSTROOT)/usr/X11/var/cache/fontconfig
	$(MKDIR) $(OSV)
	$(INSTALL) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL) $(SRCROOT)/bitstreamvera.txt $(OSL)/bitstreamvera.txt
	$(MKDIR) $(DSTROOT)/usr/X11/lib/X11/fontconfig/conf.avail
	$(MKDIR) $(DSTROOT)/usr/X11/lib/X11/fontconfig/conf.d
	$(INSTALL) $(SRCROOT)/fontconfig.osx-fonts.conf $(DSTROOT)/usr/X11/lib/X11/fontconfig/conf.avail/05-osx-fonts.conf
	ln -s ../conf.avail/05-osx-fonts.conf $(DSTROOT)/usr/X11/lib/X11/fontconfig/conf.d/05-osx-fonts.conf
	/Developer/Makefiles/bin/compress-man-pages.pl -d $(DSTROOT)/usr/X11/share/man/ man1 man2 man3 man4 man5 man6 man7 man8 man9

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	cp Makefile build uvn X11fonts.plist build-fonts.conf.in font_cache.sh $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
