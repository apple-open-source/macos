# This is a passthrough make file which supports automatic
# building with buildit.

#TEMP_FILES_DIR = /tmp
#DERIVED_SOURCES_DIR = /tmp
#OBJROOT = /tmp

build::
	echo "***";echo "*** BUILD";echo "***";\
pbxbuild build "DSTROOT=$(DSTROOT)";\
#	pbxbuild install "DSTROOT=$(DSTROOT)";
#	pbxbuild install "$(MAKEFLAGS)"

installsrc:
	echo "***";echo "*** INSTALLSRC";echo "***";\
/bin/rm -rf $(SRCROOT)/IOFireWireLib.CFPlugInProj;\
/bin/mkdir -p $(SRCROOT)/IOFireWireLib.CFPlugInProj;\
pbxbuild installsrc "SRCROOT=$(SRCROOT)/IOFireWireLib.CFPlugInProj"

installhdrs:
	echo "***";echo "*** INSTALLHDRS";echo "***";\
pbxbuild installhdrs "SRCROOT=$(SRCROOT)/IOFireWireLib.CFPlugInProj"
#	pbxbuild installhdrs

clean:
	echo "***";echo "*** CLEAN";echo "***";\
pbxbuild clean "SRCROOT=$(SRCROOT)/IOFireWireLib.CFPlugInProj"

install:
	echo "***";echo "*** INSTALL";echo "***";\
pbxbuild install "DSTROOT=$(DSTROOT)";

prebuild:
	echo "***";echo "*** PRE-BUILD";echo "***";\
#pbxbuild prebuild "DSTROOT=$(DSTROOT)";

postinstall:
	echo "***";echo "*** POST-INSTALL";echo "***";\
/usr/bin/strip -S \
$(SRCROOT)/IOFireWireLib.CFPlugInProj/build/IOFireWireLib.plugin/Contents/MacOS/IOFireWireLib;\
rm -r $(SRCROOT)/IOFireWireLib.CFPlugInProj/build;\

