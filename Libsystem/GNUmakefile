##---------------------------------------------------------------------
# Makefile for Libsystem
# Call Makefile to do the work, but for the install case, we need to
# call Makefile for each arch separately, and create fat dylibs at the
# end.  This is because the comm page symbols are added as a special segment,
# which the linker will not thin, so we have to build thin and combine.
##---------------------------------------------------------------------
Project = Libsystem
VersionLetter = B

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifeq ($(Version),0)
ifdef RC_ProjectSourceVersion
Version = $(RC_ProjectSourceVersion)
endif
endif

no_target:
	@$(MAKE) -f Makefile

ifndef RC_TARGET_CONFIG
export RC_TARGET_CONFIG = MacOSX
endif
include Platforms/$(RC_TARGET_CONFIG)/Makefile.inc

##---------------------------------------------------------------------
# For each arch, we setup the independent OBJROOT and DSTROOT, and adjust
# the other flags.  After all the archs are built, we copy over one on
# time (for the non-dylib files), and then call lipo to create fat files
# for the three dylibs.
##---------------------------------------------------------------------
NARCHS = $(words $(RC_ARCHS))
USRLIB = /usr/lib
ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
else
LIBSYS = $(SDKROOT)/usr/local/lib/system
endif
FORMS = dynamic
SUFFIX = ''
ifdef FEATURE_DEBUG_DYLIB
FORMS += debug
SUFFIX += _debug
endif
ifdef FEATURE_PROFILE_DYLIB
FORMS += profile
SUFFIX += _profile
endif
BSD_LIBS = c info m pthread dbm poll dl rpcsvc proc
FPATH = /System/Library/Frameworks/System.framework

build:: fake libSystem
	@set -x && \
	cd $(DSTROOT)/usr/lib && \
	for i in $(BSD_LIBS); do \
	    ln -sf libSystem.dylib lib$$i.dylib || exit 1; \
	done
	find $(DSTROOT) -type l ! -perm 755 | xargs chmod -hv 755
	install -d $(DSTROOT)$(FPATH)/Versions/$(VersionLetter)/Resources
	@set -x && \
	cd $(DSTROOT)$(FPATH) && \
	ln -sf Versions/Current/PrivateHeaders && \
	ln -sf Versions/Current/Resources && \
	for S in $(SUFFIX); do \
	    ln -sf Versions/Current/System$$S || exit 1; \
	done && \
	cd Versions && \
	ln -sf $(VersionLetter) Current && \
	cd $(VersionLetter) && \
	for S in $(SUFFIX); do \
	    ln -sf ../../../../../../usr/lib/libSystem.$(VersionLetter)$$S.dylib System$$S || exit 1; \
	done && \
	cp -f $(SRCROOT)/Info.plist Resources

# 4993197: force dependency generation for libsyscall.a
fake:
	@set -x && \
	cd $(OBJROOT) && \
	echo 'main() { __getpid(); return 0; }' > fake.c && \
	cc -c $(RC_CFLAGS) fake.c && \
	ld -r -o fake $(foreach ARCH,$(RC_ARCHS),-arch $(ARCH)) fake.o -lsyscall -L$(LIBSYS)

libc:
	mkdir -p '$(OBJROOT)/libc'
	bsdmake -C libsys install \
	DSTROOT='$(DSTROOT)' \
	OBJROOT='$(OBJROOT)/libc' \
	SRCROOT='$(SRCROOT)' \
	SYMROOT='$(SYMROOT)'

libSystem: libc
	mkdir -p '$(OBJROOT)/libSystem'
	bsdmake install \
	FEATURE_LIBMATHCOMMON=$(FEATURE_LIBMATHCOMMON) \
	FEATURE_ORDER_FILE=$(FEATURE_ORDER_FILE) \
	FORMS='$(FORMS)' \
	Version=$(Version) \
	VersionLetter=$(VersionLetter) \
	DSTROOT='$(DSTROOT)' \
	OBJROOT='$(OBJROOT)/libSystem' \
	SRCROOT='$(SRCROOT)' \
	SYMROOT='$(SYMROOT)'
