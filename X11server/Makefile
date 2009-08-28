Project               = X11server
UserType              = Administrator
ToolType              = Commands

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
MKDIR = /bin/mkdir -p
INSTALL = /usr/bin/install -c -m 0644 

install::
	@echo "Building $(Project)..."
	./build install
	$(MKDIR) $(OSV)
	$(INSTALL) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	/Developer/Makefiles/bin/compress-man-pages.pl -d $(DSTROOT)/usr/X11/share/man/ man1 man2 man3 man4 man5 man6 man7 man8 man9

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	find . -name ".#*" | xargs rm
	cp Makefile build X11server.plist $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
