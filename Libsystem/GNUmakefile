##---------------------------------------------------------------------
# Makefile for Libsystem
# Call Makefile to do the work, but for the install case, we need to
# call Makefile for each arch separately, and create fat dylibs at the
# end.  This is because the comm page symbols are added as a special segment,
# which the linker will not thin, so we have to build thin and combine.
##---------------------------------------------------------------------
PROJECT = Libsystem

no_target:
	@$(MAKE) -f Makefile

##---------------------------------------------------------------------
# For each arch, we setup the independent OBJROOT and DSTROOT, and adjust
# the other flags.  After all the archs are built, we copy over one on
# time (for the non-dylib files), and then call lipo to create fat files
# for the three dylibs.
##---------------------------------------------------------------------
ALLARCHS = hppa i386 m68k ppc ppc64 sparc x86_64
NARCHS = $(words $(RC_ARCHS))
USRLIB = /usr/lib
ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
else
LIBSYS = $(NEXT_ROOT)/usr/local/lib/system
endif

install: fake $(DSTROOT)/usr/local/lib/system/libc.a $(RC_ARCHS)
ifneq "$(NARCHS)" "1"
	rsync -aH $(OBJROOT)/$(word 1,$(RC_ARCHS))/dstroot/ $(DSTROOT)
	rsync -aH $(OBJROOT)/$(word 1,$(RC_ARCHS))/symroot/ $(SYMROOT)
	@set -x && \
	for i in libSystem.B.dylib libSystem.B_debug.dylib libSystem.B_profile.dylib; do \
	    rm -f $(DSTROOT)$(USRLIB)/$$i && \
	    lipo -create -o $(DSTROOT)$(USRLIB)/$$i $(foreach ARCH,$(RC_ARCHS),$(OBJROOT)/$(ARCH)/dstroot$(USRLIB)/$$i) && \
	    rm -f $(SYMROOT)/$$i && \
	    lipo -create -o $(SYMROOT)/$$i $(foreach ARCH,$(RC_ARCHS),$(OBJROOT)/$(ARCH)/symroot/$$i) || exit 1; \
	done
endif
	@set -x && \
	for i in libSystem.B.dylib libSystem.B_debug.dylib libSystem.B_profile.dylib; do \
	    dsymutil $(SYMROOT)/$$i || exit 1; \
	done

# 4993197: force dependency generation for libsyscall.a
fake:
	@set -x && \
	cd $(OBJROOT) && \
	echo 'main() { __getpid(); return 0; }' > fake.c && \
	cc -c fake.c && \
	ld -r -o fake fake.o -lsyscall -L$(LIBSYS)

$(DSTROOT)/usr/local/lib/system/libc.a:
	bsdmake -C libsys install

$(ALLARCHS):
ifneq "$(NARCHS)" "1"
	mkdir -p $(OBJROOT)/$@/objroot $(OBJROOT)/$@/dstroot $(OBJROOT)/$@/symroot
	$(MAKE) -f Makefile install \
	    OBJROOT='$(OBJROOT)/$@/objroot' \
	    TOPOBJROOT='$(OBJROOT)' \
	    DSTROOT='$(OBJROOT)/$@/dstroot' \
	    SYMROOT='$(OBJROOT)/$@/symroot' \
	    DESTDIR='$(DSTROOT)' \
	    RC_CFLAGS='-arch $@ $(RC_NONARCH_CFLAGS)' \
	    RC_ARCHS='$@' \
	    RC_$@=YES $(foreach ARCH,$(filter-out $@,$(ALLARCHS)),RC_$(ARCH)=)
else # NARCHS == 1
	$(MAKE) -f Makefile install TOPOBJROOT='$(OBJROOT)' DESTDIR='$(DSTROOT)'
endif # NARCHS != 1

.DEFAULT:
	@$(MAKE) -f Makefile $@
