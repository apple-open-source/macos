Project               = X11apps
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
	$(INSTALL) -m 644 $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	cp Makefile build uvn $(Project).plist $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"

