##
# Makefile for hunspell
##

# Project info
Project           = hunspell
UserType          = Developer
ToolType          = Libraries
GnuAfterInstall   = install-strip move-files install-pl install-plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install

install-strip:
	$(STRIP) -S $(DSTROOT)/usr/lib/libhunspell-1.2.dylib

move-files:
	$(MKDIR) $(DSTROOT)/usr/local
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/bin $(DSTROOT)/usr/local/bin
	$(MV) $(DSTROOT)/usr/include $(DSTROOT)/usr/local/include
	$(MV) $(DSTROOT)/usr/lib/charset.alias $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libhunspell-1.2.a $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libhunspell-1.2.la $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libparsers.a $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/pkgconfig $(DSTROOT)/usr/local/lib
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
	$(INSTALL_FILE) $(SRCROOT)/sjp.pl.plist $(OSV)/sjp.pl.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/license.hunspell $(OSL)/$(Project).txt
	$(INSTALL_FILE) $(SRCROOT)/README_pl_PL.txt $(OSL)/sjp.pl.txt
