##
# libstdcxx Makefile
##

# Project info
Project               = libstdcxx
UserType              = Developer
ToolType              = Libraries
Extra_Configure_Flags = --disable-multilib \
	--build $(BUILD)$(DARWIN_VERS) \
	--host `echo $$arch | $(TRANSLATE_ARCH)`-apple-darwin$(DARWIN_VERS) \
	--target `echo $$arch | $(TRANSLATE_ARCH)`-apple-darwin$(DARWIN_VERS)
GnuAfterInstall       = post-install install-plist
Environment	     += CXX_FOR_TARGET="$(CC) -arch $$arch -shared-libgcc \
				-nostdinc++" \
			RAW_CXX_FOR_TARGET="$(CC) -arch $$arch -shared-libgcc \
				-nostdinc++" \
			CC_FOR_TARGET="$(CC) -arch $$arch" \
			GCC_FOR_TARGET="$(CC) -arch $$arch" \
			AS_FOR_TARGET="$(AS) -arch $$arch" \
			LD_FOR_TARGET="$(LD) -arch $$arch" \
			NM_FOR_TARGET="nm -arch $$arch" \
			AR_FOR_TARGET="$(AR)" \
			RANLIB_FOR_TARGET="ranlib" \
			OPT_LDFLAGS="-Wl,-dead_strip"


# It's a GNU Source project
include ./GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = gcc
#AEP_Version    = 4.0.0
AEP_Version    = 4.2.1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
ifeq ($(AEP_Version), 4.0.0)
AEP_Patches    = tilde-in-pathnames.patch emergency-buffer-reduction.patch \
		 keymgr.patch testing-installed.patch align-natural-abi.patch \
		 export-control.patch cross-configury.patch eprintf.patch \
		 testsuite-4.0.1.patch \
		 libtool-jaguar.patch jaguar-semun.patch \
		 jaguar-abilimits.patch \
		 stdexcept_vis.patch testuite-06-03-10.patch fstream.patch \
		 x86_vis.patch vector_bool.patch  pr21244.patch
else
AEP_Patches    = tilde-in-pathnames.patch emergency-buffer-reduction.patch \
		 keymgr.patch testing-installed.patch align-natural-abi.patch \
		 libtool-jaguar.patch jaguar-semun.patch jaguar-abilimits.patch \
		 stdexcept_vis.patch x86_vis.patch libiberty-config.patch typeinfo-abi.patch \
		 libtool-darwin10.patch abi-search.patch \
		 noppc64.patch abi-stdio.patch abi-stdio-filebuf.patch \
		 abi-setfill.patch block.patch version.patch string_compare.patch \
		 stl_tree_system_header.patch copy_doc.patch test_cleanup.patch \
		 fix_64bit_test.patch vector_copy_no_alloc.patch \
		 nodefault.patch dtrace.patch abi-list.patch demangle.patch
endif

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

ifeq ($(AEP_Version), 4.0.0)
AEP_PatchDir = patches
else
AEP_PatchDir = patches-$(AEP_Version)
endif

Install_Target = install

CUR_OS_VERS	:= $(shell uname -r | cut -f 1 -d .)

ifeq ($(RC_ProjectName),libstdcxx_Jaguar)
DARWIN_VERS	= 6
MACOSX_DEPLOYMENT_TARGET=10.2
SYSROOT		= -isysroot /Developer/SDKs/MacOSX10.2.8.internal.sdk
CC		:= /usr/bin/gcc-4.2 $(SYSROOT)
CXX		:= /usr/bin/g++-4.2 $(SYSROOT)
Extra_Cxx_Flags	+= -DLIBSTDCXX_FOR_JAGUAR
SDKPFXs		= /Developer/SDKs/MacOSX10.2.8.sdk \
		  /Developer/SDKs/MacOSX10.2.8.internal.sdk
SDKEXCLUDE	=
else
ifeq ($(RC_ProjectName),libstdcxx_SUPanWheat)
DARWIN_VERS	= 7
MACOSX_DEPLOYMENT_TARGET=10.3
# The internal SDK doesn't have GCC 4 support, Radar 4301583.
SYSROOT		= -isysroot /Developer/SDKs/MacOSX10.3.9.sdk
CC		:= /usr/bin/gcc-4.2 $(SYSROOT)
CXX		:= /usr/bin/g++-4.2 $(SYSROOT)
SDKPFXs		= /Developer/SDKs/MacOSX10.3.9.sdk \
		  /Developer/SDKs/MacOSX10.3.internal.sdk
SDKEXCLUDE	= \! -name \*.dylib
else
ifeq ($(RC_ProjectName),libstdcxx_Inca)
DARWIN_VERS	= 8
MACOSX_DEPLOYMENT_TARGET=10.4
SYSROOT		= -isysroot /Developer/SDKs/MacOSX10.4u.sdk
CC		:= /usr/bin/gcc-4.2 $(SYSROOT)
CXX		:= /usr/bin/g++-4.2 $(SYSROOT)
SDKPFXs		= /Developer/SDKs/MacOSX10.4u.sdk \
		  /Developer/SDKs/MacOSX10.4.0.Internal.sdk
SDKEXCLUDE	= \! -name \*.dylib
else
DARWIN_VERS	= $(CUR_OS_VERS)
ifdef SDKROOT
SYSROOT		= -isysroot $(SDKROOT)
SDKPFXs		= $(SDKROOT)
endif
CC		:= /usr/bin/gcc-4.2 $(SYSROOT)
CXX		:= /usr/bin/g++-4.2 $(SYSROOT)
endif
endif
endif


# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for deldir in libada libcpp libffi libgfortran libgomp libjava libobjc libssp zlib \
		      boehm-gc fastjar fixincludes intl ; do \
		$(RMDIR) $(SRCROOT)/$(Project)/$$deldir ; \
	done
	for delconfigdir in gcc ; do \
		$(RM) $(SRCROOT)/$(Project)/$$delconfigdir/configure ; \
	done
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && \
		patch -p0 < $(SRCROOT)/$(AEP_PatchDir)/$$patchfile || exit 1 ;  \
	done
endif

# Rearrange the final destroot to be just the way we want it.
post-install:
	for arch64 in ppc64 x86_64 v6 ; do \
	  if [ -d $(DSTROOT)/usr/lib/$$arch64 ] ; then \
	    install_name_tool -id /usr/lib/libstdc++.6.dylib \
	      $(DSTROOT)/usr/lib/$$arch64/libstdc++.6.*.dylib && \
	    for f in `cd $(DSTROOT)/usr/lib/$$arch64 && echo *.{dylib,a}` ; do \
	      if [ ! -f $(DSTROOT)/usr/lib/$$f ] ; then \
		  mv $(DSTROOT)/usr/lib/$$arch64/$${f} $(DSTROOT)/usr/lib/$${f} ; \
	      elif [ ! -L $(DSTROOT)/usr/lib/$$arch64/$$f ] ; then \
		  lipo -create -output $(DSTROOT)/usr/lib/$${f}~ \
		    $(DSTROOT)/usr/lib/$${f} $(DSTROOT)/usr/lib/$$arch64/$${f} && \
		  mv $(DSTROOT)/usr/lib/$${f}~ $(DSTROOT)/usr/lib/$${f} || \
		  exit 1 ; \
	      fi ; \
	    done && \
	    $(RM) -r $(DSTROOT)/usr/lib/$$arch64 ; \
	  fi ; \
	done
	$(RM) $(DSTROOT)/usr/lib/*.la
	$(RM) $(DSTROOT)/usr/lib/libiberty.a
	$(RM) $(DSTROOT)/usr/lib/libstdc++.dylib
	mv $(DSTROOT)/usr/lib/libstdc++.a $(DSTROOT)/usr/lib/libstdc++-static.a
	nmedit -p $(DSTROOT)/usr/lib/*.a
	cp -Rp $(DSTROOT)/usr/lib/*.{a,dylib} $(SYMROOT)/
	# We want local symbols 6401837
	strip -S $(DSTROOT)/usr/lib/*.dylib
	strip -X -S $(DSTROOT)/usr/lib/*.a
	for (( i = 8 ; i <= $(CUR_OS_VERS) ; i++)) ; do \
	  [ $$i == $(DARWIN_VERS) ] || \
	  for t in powerpc powerpc64 i686 x86_64 ; do \
	    [ \! -d $(DSTROOT)/usr/include/c++/$(AEP_Version)/$${t}-apple-darwin$(DARWIN_VERS) ] \
	      || ln -s $${t}-apple-darwin$(DARWIN_VERS) \
		$(DSTROOT)/usr/include/c++/$(AEP_Version)/$${t}-apple-darwin$${i} \
	      || exit 1 ; \
	  done \
	done
	[ ! -d $(DSTROOT)/usr/include/c++/$(AEP_Version)/powerpc-apple-darwin$(CUR_OS_VERS) ] || \
	  ln -s ../powerpc64-apple-darwin$(CUR_OS_VERS) \
	    $(DSTROOT)/usr/include/c++/$(AEP_Version)/powerpc-apple-darwin$(CUR_OS_VERS)/ppc64
	[ ! -d $(DSTROOT)/usr/include/c++/$(AEP_Version)/i686-apple-darwin$(CUR_OS_VERS) ] || \
	  ln -s ../x86_64-apple-darwin$(CUR_OS_VERS) \
	    $(DSTROOT)/usr/include/c++/$(AEP_Version)/i686-apple-darwin$(CUR_OS_VERS)/x86_64
	# For ARM, we want to create the arm-apple-darwinX directory if it doesn't exist
	[ -d $(DSTROOT)/usr/include/c++/$(AEP_Version)/arm-apple-darwin$(CUR_OS_VERS) ] || \
	  mkdir $(DSTROOT)/usr/include/c++/$(AEP_Version)/arm-apple-darwin$(CUR_OS_VERS)
	ln -s ../armv6-apple-darwin$(CUR_OS_VERS) \
	  $(DSTROOT)/usr/include/c++/$(AEP_Version)/arm-apple-darwin$(CUR_OS_VERS)/v6
	if [ "x$(SDKPFXs)" != x ] ; then \
	  for i in $(SDKPFXs) ; do \
	    $(MKDIR) $(DSTROOT)/$i && \
	    (cd $(DSTROOT) && find usr $(SDKEXCLUDE) -print | \
	     cpio -pdm $(DSTROOT)/$$i ) || exit 1 ; \
	  done ; \
	  $(RM) -r $(DSTROOT)/[^D]* ; \
	fi
ifeq ($(AEP_Version), 4.0.0)
	$(RM) $(DSTROOT)/usr/lib/libstdc++.6.dylib
	$(RM) $(DSTROOT)/usr/lib/libsupc++.a
	$(RM) $(DSTROOT)/usr/lib/libstdc++-static.a
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist \
	   $(OSV)/$(RC_ProjectName).plist
	if [ "x$(SDKPFXs)" == x ] ; then \
	  $(MKDIR) $(OSL) && \
	  $(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt || exit 1 ;\
	fi
