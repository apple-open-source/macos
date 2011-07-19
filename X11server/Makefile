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
	$(INSTALL) -m 644 $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist

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
