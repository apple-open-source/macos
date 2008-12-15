ALLARCHS = arm i386 ppc ppc64 x86_64 # installsrc doesn't set RC_ARCHS
PWD != pwd
.ifdef DSTROOT
DESTDIR = $(DSTROOT)
.else
.ifdef DESTDIR
DSTROOT = $(DESTDIR)
.else
DSTROOT = /
DESTDIR = /
.endif
.endif
.ifndef OBJROOT
OBJROOT = $(PWD)/OBJROOT
.endif
.ifndef SRCROOT
SRCROOT = $(PWD)
.endif
.ifndef SYMROOT
SYMROOT = $(PWD)/SYMROOT
.endif
ARCH != arch
.ifndef RC_ARCHS
RC_ARCHS = $(ARCH)
RC_$(RC_ARCHS) = 1
.endif
.ifndef RC_NONARCH_CFLAGS
RC_NONARCH_CFLAGS = -pipe
.endif
.ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
.else
LIBSYS = $(SDKROOT)/usr/local/lib/system
.endif
NJOBS != perl -e '$$n = `/usr/sbin/sysctl -n hw.ncpu`; printf "%d\n", $$n < 2 ? 2 : ($$n * 1.5)'
BSDMAKE = bsdmake -f Makefile
#BSDMAKEJ = $(BSDMAKE) -j $(NJOBS) -P
BSDMAKEJ = $(BSDMAKE) -j $(NJOBS)

# Set the DONT-BUILD-arch-form variable to non-empty to turn off building
#DONT-BUILD-x86_64-static = 1

# These variables are to guarantee that the left-hand side of an expression is
# always a variable
dynamic = dynamic
static = static

# Map RC_ARCHS to MACHINE_ARCH
.for A in $(RC_ARCHS) $(ARCH) # {
MACHINE_ARCH-$(A) = $(A:C/^armv.*/arm/)
.endfor # RC_ARCHS }

FORMS := dynamic debug profile static

OBJSUFFIX-dynamic = So
OBJSUFFIX-debug = do
OBJSUFFIX-profile = po
OBJSUFFIX-static = o

all: build

ROOTS := DSTROOT OBJROOT SYMROOT
.for R in $(ROOTS) # {
roots: $($(R))
$($(R)):
	mkdir -p '$($(R))'
.endfor # ROOTS }

# These are the non B&I defaults
.ifndef RC_ProjectName

installhdrs: roots installhdrs-real
build: roots build-static build-profile build-debug build-dynamic
install: roots installhdrs install-all

.else # RC_ProjectName

# And these are to deal with B&I building libc differently 
# based on RC_ProjectName.
.if $(RC_ProjectName) == Libc
installhdrs:
build: roots build-dynamic
install: roots BI-install-dynamic
.endif
.if $(RC_ProjectName) == Libc_headers
installhdrs: roots installhdrs-real
build:
install: roots installhdrs-real
.endif
.if $(RC_ProjectName) == Libc_man
installhdrs:
build:
install: roots install-man
.endif
.if $(RC_ProjectName) == Libc_static
installhdrs:
build: roots build-static
install: roots BI-install-static
.endif
.if $(RC_ProjectName) == Libc_debug
installhdrs:
build: roots build-debug
install: roots BI-install-debug
.endif
.if $(RC_ProjectName) == Libc_profile
installhdrs:
build: roots build-profile
install: roots BI-install-profile
.endif
.endif # RC_ProjectName

# Make a copy of System.framework/Versions/B/PrivateHeaders, with headers
# patched so that we can build variant symbols independently
SYSTEMFRAMEWORK = System.framework
VERSIONSB = Versions/B
PRIVATEHEADERPATH = $(SYSTEMFRAMEWORK)/$(VERSIONSB)/PrivateHeaders
FRAMEWORKS = $(OBJROOT)/Frameworks
.ifdef ALTFRAMEWORKSPATH
FRAMEWORKPATH = ${ALTFRAMEWORKSPATH}
.else
FRAMEWORKPATH = ${SDKROOT}/System/Library/Frameworks
.endif
$(FRAMEWORKS):
	$(SRCROOT)/patchHeaders $(FRAMEWORKPATH)/$(PRIVATEHEADERPATH) $(FRAMEWORKS)/$(PRIVATEHEADERPATH:H)
	ln -fs $(VERSIONSB)/PrivateHeaders $(FRAMEWORKS)/$(SYSTEMFRAMEWORK)/PrivateHeaders

AUTOPATCHED = $(SRCROOT)/.autopatched
PARTIAL = -partial
.for F in $(FORMS) # {
.if $(dynamic) == $(F) # {
SUFFIX-$(F) =
.else # } {
SUFFIX-$(F) = _$(F)
.endif # }
PSUFFIX-$(F) = $(PARTIAL)$(SUFFIX-$(F))

.for A in $(RC_ARCHS) # {
.if empty(DONT-BUILD-$(A)-$(F)) # {
ARCHS-$(F) += $(A)
build-$(A)-$(F):
	mkdir -p $(OBJROOT)/obj.$(A) && \
	MAKEOBJDIR="$(OBJROOT)/obj.$(A)" MACHINE_ARCH=$(MACHINE_ARCH-$(A)) CCARCH=$(A) \
	    DSTROOT=$(DSTROOT) OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) \
	    RC_NONARCH_CFLAGS="$(RC_NONARCH_CFLAGS)" MAKEFLAGS="" \
	    OBJSUFFIX="$(OBJSUFFIX-$(F))" \
	    $(BSDMAKEJ) libc$(SUFFIX-$(F)).a
.else # } {
build-$(A)-$(F):
	@echo Not building libc$(PSUFFIX-$(F)).a for $(A)
.endif # }
.endfor # RC_ARCHS }

NARCHS-$(F) != echo $(ARCHS-$(F)) | wc -w

build-$(F): $(FRAMEWORKS) $(AUTOPATCHED)
.for A in $(RC_ARCHS) # {
build-$(F): build-$(A)-$(F)
.endfor # RC_ARCHS }
.if $(NARCHS-$(F)) == 0 # {
build-$(F):
	@echo No libc$(PSUFFIX-$(F)).a built
.else # } {
LIPOARGS-$(F) != perl -e 'printf "%s\n", join(" ", map(qq(-arch $$_ \"$(OBJROOT)/obj.$$_/libc$(SUFFIX-$(F)).a\"), qw($(ARCHS-$(F)))))'
build-$(F):
.if $(NARCHS-$(F)) == 1 # {
	cp -p "$(OBJROOT)/obj.$(RC_ARCHS)/libc$(SUFFIX-$(F)).a" "$(SYMROOT)/libc$(PSUFFIX-$(F)).a"
.else # } {
	lipo -create $(LIPOARGS-$(F)) -output "$(SYMROOT)/libc$(PSUFFIX-$(F)).a"
.endif # }
.endif # }
.endfor # FORMS }

# We autopatch the files into the directory containing the Makefile.inc.  This
# will happen at installsrc.
$(AUTOPATCHED):
.for A in $(ALLARCHS) # {
	MACHINE_ARCH=$(A) SRCROOT="$(SRCROOT)" \
	    $(BSDMAKE) -C "$(SRCROOT)" autopatch
.endfor # ALLARCHS # }
	touch $(AUTOPATCHED)

copysrc:
	pax -rw -p p . "$(SRCROOT)"

installsrc: copysrc $(AUTOPATCHED)

installhdrs-real:
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" MAKEFLAGS="" \
	    DSTROOT=$(DSTROOT) OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) \
	    $(BSDMAKEJ) installhdrs
.for A in $(RC_ARCHS) # {
	mkdir -p "$(OBJROOT)/obj.$(A)" && \
	MAKEOBJDIR="$(OBJROOT)/obj.$(A)" MACHINE_ARCH=$(MACHINE_ARCH-$(A)) CCARCH=$(A) \
	    DSTROOT=$(DSTROOT) OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) \
	    MAKEFLAGS="" RC_NONARCH_CFLAGS="$(RC_NONARCH_CFLAGS)" \
	    $(BSDMAKEJ) installhdrs-md
.endfor # RC_ARCHS # }

.for F in $(FORMS) # {
BI-install-$(F): build-$(F)
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc$(PSUFFIX-$(F)).a" ]; then \
		echo "Installing libc$(PSUFFIX-$(F)).a" && \
		install -c -m 444 "$(SYMROOT)/libc$(PSUFFIX-$(F)).a" \
			$(DSTROOT)/usr/local/lib/system && \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc$(PSUFFIX-$(F)).a" || exit 1; \
	fi
.if $(dynamic) == $(F) # {
.for A in $(RC_ARCHS) # {
	MAKEOBJDIR="$(OBJROOT)/obj.$(A)" MACHINE_ARCH=$(MACHINE_ARCH-$(A)) CCARCH=$(A) \
	DSTROOT=$(DSTROOT) OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) \
	    DSTROOT=$(DSTROOT) OBJROOT=$(OBJROOT) SYMROOT=$(SYMROOT) \
	    MAKEFLAGS="" RC_NONARCH_CFLAGS="$(RC_NONARCH_CFLAGS)" \
	    $(BSDMAKE) copyfiles
.endfor # RC_ARCHS # }
.endif # }
.endfor # FORMS }

# Don't use -j here; it may try to make links before the files are copied
MANARGS != test `id -u` -eq 0 || echo MINSTALL=/usr/bin/install
install-man:
	mkdir -p $(DSTROOT)/usr/share/man/man2
	mkdir -p $(DSTROOT)/usr/share/man/man3
	mkdir -p $(DSTROOT)/usr/share/man/man4
	mkdir -p $(DSTROOT)/usr/share/man/man5
	mkdir -p $(DSTROOT)/usr/share/man/man7
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" \
		DSTROOT='$(DSTROOT)' OBJROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' \
		MACHINE_ARCH="$(MACHINE_ARCH-$(ARCH))" CCARCH=$(ARCH) MAKEFLAGS="" \
		RC_NONARCH_CFLAGS="$(RC_NONARCH_CFLAGS)" \
		$(BSDMAKE) all-man maninstall $(MANARGS)

install-all: build install-man
.for F in $(FORMS) # {
install-all: BI-install-$(F)
.endfor # FORMS }

clean:
.for F in $(FORMS) # {
	rm -f $(SYMROOT)/libc$(PSUFFIX-$(F)).a
.endfor # FORMS }
.for A in $(RC_ARCHS) # {
	rm -rf $(OBJROOT)/obj.$(A)
.endfor # RC_ARCHS # }
