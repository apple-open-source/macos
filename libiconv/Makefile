##
# Makefile for libiconv
##

# Project info
Project               = libiconv
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --disable-static --enable-extra-encodings
#Extra_LD_Flags        = -arch i386 -arch ppc
GnuAfterInstall       = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

strip:
	strip -x $(DSTROOT)/usr/lib/libiconv.2.2.0.dylib
	strip -x $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib
	strip -x $(DSTROOT)/usr/bin/iconv
	rm -f $(DSTROOT)/usr/lib/libiconv.2.dylib
	mv $(DSTROOT)/usr/lib/libiconv.2.2.0.dylib $(DSTROOT)/usr/lib/libiconv.2.dylib
	ln -s libiconv.2.dylib $(DSTROOT)/usr/lib/libiconv.2.2.0.dylib
	rm -f $(DSTROOT)/usr/lib/libcharset.1.dylib
	mv $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib $(DSTROOT)/usr/lib/libcharset.1.dylib
	ln -s libcharset.1.dylib $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib
