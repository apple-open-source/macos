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
# the other flags.  After all the archs are built, we ditto over one on
# time (for the non-dylib files), and then call lipo to create fat files
# for the three dylibs.
##---------------------------------------------------------------------
ALLARCHS = hppa i386 m68k ppc sparc
NARCHS = $(words $(RC_ARCHS))
USRLIB = /usr/lib

install: $(RC_ARCHS)
	ditto $(OBJROOT)/$(word 1,$(RC_ARCHS))/dstroot $(DSTROOT)
ifneq "$(NARCHS)" "1"
	@for i in libSystem.B.dylib libSystem.B_debug.dylib libSystem.B_profile.dylib; do \
	    echo rm -f $(DSTROOT)$(USRLIB)/$$i; \
	    rm -f $(DSTROOT)$(USRLIB)/$$i; \
	    echo lipo -create -o $(DSTROOT)$(USRLIB)/$$i $(foreach ARCH,$(RC_ARCHS),$(OBJROOT)/$(ARCH)/dstroot$(USRLIB)/$$i); \
	    lipo -create -o $(DSTROOT)$(USRLIB)/$$i $(foreach ARCH,$(RC_ARCHS),$(OBJROOT)/$(ARCH)/dstroot$(USRLIB)/$$i); \
	done
endif

$(ALLARCHS):
	mkdir -p $(OBJROOT)/$@/objroot $(OBJROOT)/$@/dstroot
	$(MAKE) -f Makefile install \
	    OBJROOT='$(OBJROOT)/$@/objroot' \
	    DSTROOT='$(OBJROOT)/$@/dstroot' \
	    RC_CFLAGS='-arch $@ $(RC_NONARCH_CFLAGS)' \
	    RC_ARCHS='$@' \
	    RC_$@=YES $(foreach ARCH,$(filter-out $@,$(ALLARCHS)),RC_$(ARCH)=)

.DEFAULT:
	@$(MAKE) -f Makefile $@
