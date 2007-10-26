Project               = X11fonts
UserType              = Administrator
ToolType              = Commands

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
MKDIR = /bin/mkdir -p
INSTALL = /usr/bin/install -c

install::
	@echo "Building $(Project)..."
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

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	cp Makefile build uvn X11fonts.plist build-fonts.conf.in $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
