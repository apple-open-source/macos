##
# Makefile for tidy
##


# Project info
Project               = tidy
BuildNumber           = 15.6
UserType              = Administrator
ToolType              = Libraries

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

#Install_Target = install-strip
lazy_install_source:: shadow_source

SECTORDER_FLAGS=-sectorder __TEXT __text /usr/local/lib/OrderFiles/libtidy.order

ifdef TIDY_DEBUG
CFLAGS= -O0
endif

ifneq ($(SDKROOT),)
CFLAGS+= -isysroot $(SDKROOT)
endif

CFLAGS+= -DTIDY_APPLE_CHANGES=1 -DTIDY_APPLE_BUILD_NUMBER=$(BuildNumber) -DTIDY_APPLE_BUILD_NUMBER_STR='"\"$(BuildNumber)\""'

# seriously gross B&I hackery
# blame molson
ifeq "$(RC_ProjectName)" "tidy_Sim"
include /Developer/AppleInternal/Makefiles/Makefile.indigo
ActualDSTROOT = ${DSTROOT}/${INDIGO_PREFIX}
else
ActualDSTROOT = ${DSTROOT}
endif

OSV     = $(ActualDSTROOT)/usr/local/OpenSourceVersions
OSL     = $(ActualDSTROOT)/usr/local/OpenSourceLicenses

install::
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActuallDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installhdrs devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installib devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
	$(CC) $(CFLAGS) -dynamiclib $(SECTORDER_FLAGS) -o "$(ActualDSTROOT)/usr/lib/libtidy.A.dylib" "$(OBJROOT)/tidy/lib/libtidy.a" -install_name "/usr/lib/libtidy.A.dylib" -all_load -compatibility_version 1.0.0 -current_version 1.0.0
	ln -s "libtidy.A.dylib" "$(ActualDSTROOT)/usr/lib/libtidy.dylib"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installexes devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr" LIBDIR="$(ActualDSTROOT)/usr/lib"
	TIDY_APPLE_CHANGES=1 RANLIB=ranlib CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installmanpage_apple devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
ifndef TIDY_DEBUG
	strip "$(ActualDSTROOT)/usr/bin/tab2space"
	strip "$(ActualDSTROOT)/usr/bin/tidy"
	strip -x "$(ActualDSTROOT)/usr/lib/libtidy.A.dylib"
endif
	rm -f "$(ActualDSTROOT)/usr/lib/libtidy.a"

	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
