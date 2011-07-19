Project               = X11libs
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
	$(MKDIR) $(OSL)
	$(INSTALL) -m 644 $(SRCROOT)/libpng.txt $(OSL)/libpng.txt

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	find . -name ".#*" | xargs rm
	cp Makefile build uvn X11libs.plist libpng.txt $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
