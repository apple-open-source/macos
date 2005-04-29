##
# Makefile for awk
##

# Project info
Project           = awk
UserType          = Developer
ToolType          = Commands

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Extra_CC_Flags    = -DHAS_ISBLANK -mno-fused-madd -mdynamic-no-pic
Sources           = $(SRCROOT)/$(Project)

install_source::
	$(MKDIR) $(Sources)
	$(TAR) -C $(Sources) -xzf $(SRCROOT)/awk.tar.gz
	cd $(Sources) && patch -p0 < $(SRCROOT)/patches/main.c.diff
	cd $(Sources) && patch -p0 < $(SRCROOT)/patches/makefile.diff

build:: shadow_source
	$(MAKE) -C $(BuildDirectory) $(Environment)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/bin
	$(INSTALL_PROGRAM) $(BuildDirectory)/a.out $(DSTROOT)/usr/bin/awk
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man1
	$(INSTALL_FILE) $(Sources)/awk.1 $(DSTROOT)/usr/share/man/man1/awk.1
	$(INSTALL_DIRECTORY) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/awk.plist $(OSV)
	$(INSTALL_DIRECTORY) $(OSL)
	$(HEAD) -n 23 $(Sources)/README > $(OSL)/awk.txt
