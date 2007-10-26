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
	mkdir -p $(DSTROOT)/System/Library/LaunchAgents
	cp $(SRCROOT)/org.x.X11.plist $(DSTROOT)/System/Library/LaunchAgents/
	$(MKDIR) $(OSV) $(DSTROOT)/usr/X11/lib/X11/xinit $(DSTROOT)/usr/X11/lib/X11/xserver
	$(INSTALL) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL) $(SRCROOT)/Xquartz.plist $(DSTROOT)/usr/X11/lib/X11/xserver/Xquartz.plist
	$(INSTALL) $(SRCROOT)/xinitrc $(DSTROOT)/usr/X11/lib/X11/xinit/xinitrc


clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	find . -name ".#*" | xargs rm
	cp Makefile build org.x.X11.plist X11server.plist Xquartz.plist xinitrc $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
