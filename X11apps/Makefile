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
# next two lines contain workaround for 5327952
	gcc $(RC_CFLAGS) -I/usr/X11/include -L/usr/X11/lib -o $(DSTROOT)/usr/X11/bin/glxinfo glxinfo.c -lX11 -lGL -dylib_file /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib
	gcc $(RC_CFLAGS) -I/usr/X11/include -L/usr/X11/lib -o $(DSTROOT)/usr/X11/bin/glxgears glxgears.c -lX11 -lGL -dylib_file /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib
	$(MKDIR) $(OSV)
	$(INSTALL) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	rm $(DSTROOT)/usr/X11/bin/uxterm $(DSTROOT)/usr/X11/bin/xauth_switch_to_sun-des-1

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
