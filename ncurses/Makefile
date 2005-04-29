# Project info
Project           = ncurses
UserType          = Developer
ToolType          = Commands

Configure = mkdir -p $(OBJROOT);cd $(OBJROOT); $(SRCROOT)/ncurses/configure --prefix=/usr --with-shared --without-debug --enable-termcap --without-cxx-binding --without-cxx --enable-widec --with-abi-version=5.4

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 5.4
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
#AEP_Patches    = 1to3.diff 4to9.diff
AEP_Patches    = make.diff hex.diff no-static-archives.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do                  cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile;          done
endif

install::
	cd $(DSTROOT)/usr/lib && rm -f libtermcap.dylib && ln -s libncurses.5.4.dylib libtermcap.dylib
