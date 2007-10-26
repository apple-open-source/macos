##
# Makefile for tidy
##


# Project info
Project               = tidy
BuildNumber           = 13
UserType              = Administrator
ToolType              = Libraries

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

#Install_Target = install-strip
lazy_install_source:: shadow_source

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses
SECTORDER_FLAGS=-sectorder __TEXT __text /usr/local/lib/OrderFiles/libtidy.order

ifdef TIDY_DEBUG
CFLAGS= -O0
endif

CFLAGS+= -DTIDY_APPLE_CHANGES=1 -DTIDY_APPLE_BUILD_NUMBER=$(BuildNumber) -DTIDY_APPLE_BUILD_NUMBER_STR='"\"$(BuildNumber)\""'


# Strip out 64-bit archs for installexes
EXECFLAGS = $(shell echo "$(CFLAGS)" | sed 's/-arch ppc64//g;s/-arch x86_64//g')

install::
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installhdrs devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installib devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
	$(CC) $(CFLAGS) -dynamiclib $(SECTORDER_FLAGS) -o "$(DSTROOT)/usr/lib/libtidy.A.dylib" "$(OBJROOT)/tidy/lib/libtidy.a" -install_name "/usr/lib/libtidy.A.dylib" -all_load -compatibility_version 1.0.0 -current_version 1.0.0
	ln -s "libtidy.A.dylib" "$(DSTROOT)/usr/lib/libtidy.dylib"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(EXECFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installexes devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr" LIBDIR="$(DSTROOT)/usr/lib"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installmanpage_apple devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
ifndef TIDY_DEBUG
	strip "$(DSTROOT)/usr/bin/tab2space"
	strip "$(DSTROOT)/usr/bin/tidy"
	strip -x "$(DSTROOT)/usr/lib/libtidy.A.dylib"
endif
	rm -f "$(DSTROOT)/usr/lib/libtidy.a"

	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
