Project               = X11proto
UserType              = Administrator
ToolType              = Commands

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
MKDIR = /bin/mkdir -p
INSTALL = /usr/bin/install -c

install::
	@echo "Building $(Project)..."
	./build install
	$(MKDIR) $(OSV)
	$(INSTALL) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL) $(SRCROOT)/freetype.txt $(OSL)/freetype.txt
	ln -s X11 $(DSTROOT)/usr/X11R6
	$(MKDIR) $(DSTROOT)/usr/include
	ln -s ../X11/include/X11 $(DSTROOT)/usr/include/X11
	$(MKDIR) $(DSTROOT)/private/etc/paths.d
	echo /usr/X11/bin > $(DSTROOT)/private/etc/paths.d/X11
	$(MKDIR) $(DSTROOT)/private/etc/manpaths.d
	echo /usr/X11/share/man > $(DSTROOT)/private/etc/manpaths.d/X11
	ln -s share/man $(DSTROOT)/usr/X11/man
	#ln -s share/info $(DSTROOT)/usr/X11/info
	/Developer/Makefiles/bin/compress-man-pages.pl -d $(DSTROOT)/usr/X11/share/man/ man1 man2 man3 man4 man5 man6 man7 man8 man9

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	find . -name ".#*" | xargs rm
	cp Makefile build uvn X11proto.plist freetype.txt $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
