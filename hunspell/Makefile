##
# Makefile for hunspell
##

# Project info
Project           = hunspell
UserType          = Developer
ToolType          = Libraries
GnuAfterInstall   = install-strip move-files install-plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install

install-strip:
	$(CP) $(DSTROOT)/usr/lib/libhunspell-1.2.0.0.0.dylib $(SYMROOT)/libhunspell-1.2.0.0.0.dylib
	xcrun dsymutil -o $(SYMROOT)/libhunspell-1.2.0.0.0.dSYM $(SYMROOT)/libhunspell-1.2.0.0.0.dylib
	$(STRIP) -S $(DSTROOT)/usr/lib/libhunspell-1.2.dylib

move-files:
	$(MKDIR) $(DSTROOT)/usr/local
	$(RMDIR) $(DSTROOT)/usr/bin
	$(MV) $(DSTROOT)/usr/include $(DSTROOT)/usr/local/include
	$(RM) $(DSTROOT)/usr/lib/charset.alias
	$(RM) $(DSTROOT)/usr/lib/libhunspell-1.2.a
	$(RM) $(DSTROOT)/usr/lib/libhunspell-1.2.la
	$(RM) $(DSTROOT)/usr/lib/libparsers.a
	$(RMDIR) $(DSTROOT)/usr/lib/pkgconfig
	$(MV) $(DSTROOT)/usr/share $(DSTROOT)/usr/local/share

SSD     = $(DSTROOT)/System/Library/Spelling
LSD     = $(DSTROOT)/Library/Spelling

install-pl:
	$(MKDIR) $(SSD)
	$(MKDIR) $(LSD)
	$(CHMOD) 0775 $(LSD)
	$(INSTALL_FILE) $(SRCROOT)/pl_PL.aff $(SSD)/pl_PL.aff
	$(INSTALL_FILE) $(SRCROOT)/pl_PL.dic $(SSD)/pl_PL.dic

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/license.hunspell $(OSL)/$(Project).txt
	$(INSTALL_FILE) $(SRCROOT)/README_pl_PL.txt $(OSL)/sjp.pl.txt
