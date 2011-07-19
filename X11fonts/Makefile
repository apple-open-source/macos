Project               = X11fonts
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
	$(INSTALL) -m 644 $(SRCROOT)/bitstreamvera.txt $(OSL)/bitstreamvera.txt

clean::
	@echo "Cleaning $(Project)..."
	./build clean

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

installsrc:
	@echo calling make $@
	cp Makefile build uvn X11fonts.plist font_cache.sh $(SRCROOT)
	./build $@

installhdrs:
	echo "make: installhdrs"
