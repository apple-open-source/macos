ifndef $(OBJROOT)
OBJROOT = .
endif
ifndef $(SYMROOT)
SYMROOT = .
endif
ARCH = $(shell arch)
ifndef RC_ARCHS 
RC_$(ARCH) = 1
RC_ARCHS = $(ARCH)
endif
ifdef ALTUSRLOCALLIBSYSTEM
LIBSYS = $(ALTUSRLOCALLIBSYSTEM)
else
LIBSYS = $(NEXT_ROOT)/usr/local/lib/system
endif
NJOBS = $(shell perl -e '$$n = `/usr/sbin/sysctl -n hw.ncpu`; printf "%d\n", $$n < 2 ? 2 : ($$n * 1.5)')
BSDMAKE = bsdmake -j $(NJOBS)

# Remove the arch stuff, since we know better here.  
LOCAL_CFLAGS = $(filter-out -arch ppc -arch ppc64 -arch i386,$(RC_CFLAGS))

all: build

# These are the non B&I defaults
ifndef RC_ProjectName
installhdrs: installhdrs-real
build: build-static build-profile build-debug build-dynamic
install: installhdrs install-all
endif

# And these are to deal with B&I building libc differently 
# based on RC_ProjectName.
ifeq ($(RC_ProjectName),Libc)
installhdrs:
build: build-dynamic
install: BI-install-dynamic
endif
ifeq ($(RC_ProjectName),Libc_headers)
installhdrs: installhdrs-real
build:
install: installhdrs-real
endif
ifeq ($(RC_ProjectName),Libc_man)
installhdrs:
build:
install: install-man
endif
ifeq ($(RC_ProjectName),Libc_static)
installhdrs:
build: build-static
install: BI-install-static
endif
ifeq ($(RC_ProjectName),Libc_debug)
installhdrs:
build: build-debug
install: BI-install-debug
endif
ifeq ($(RC_ProjectName),Libc_profile)
installhdrs:
build: build-profile
install: BI-install-profile
endif

# Because of 3780028, there are random failures on HFS because the
# CC_PRINT_OPTIONS_FILE can't be created.  So we touch it first.
ifneq ($(CC_PRINT_OPTIONS_FILE),)
$(CC_PRINT_OPTIONS_FILE):
	touch $(CC_PRINT_OPTIONS_FILE)
endif

build-static: autopatch $(CC_PRINT_OPTIONS_FILE) build-ppc-static build-ppc64-static build-i386-static
	@echo "Creating final libc_static.a"
ifeq ($(words $(RC_ARCHS)),1)
	cp -p "$(OBJROOT)/obj.$(RC_ARCHS)/libc_static.a" "$(SYMROOT)"
else
	lipo -create \
		$(foreach A,$(RC_ARCHS),-arch $(A) "$(OBJROOT)/obj.$(A)/libc_static.a") \
		-output $(SYMROOT)/libc_static.a
endif
build-profile: autopatch $(CC_PRINT_OPTIONS_FILE) build-ppc-profile build-ppc64-profile build-i386-profile
	@echo "Creating final libc_profile.a"
ifeq ($(words $(RC_ARCHS)),1)
	cp -p "$(OBJROOT)/obj.$(RC_ARCHS)/libc_profile.a" "$(SYMROOT)"
else
	lipo -create \
		$(foreach A,$(RC_ARCHS),-arch $(A) "$(OBJROOT)/obj.$(A)/libc_profile.a") \
		-output $(SYMROOT)/libc_profile.a
endif
build-debug: autopatch $(CC_PRINT_OPTIONS_FILE) build-ppc-debug build-ppc64-debug build-i386-debug
	@echo "Creating final libc_debug.a"
ifeq ($(words $(RC_ARCHS)),1)
	cp -p "$(OBJROOT)/obj.$(RC_ARCHS)/libc_debug.a" "$(SYMROOT)"
else
	lipo -create \
		$(foreach A,$(RC_ARCHS),-arch $(A) "$(OBJROOT)/obj.$(A)/libc_debug.a") \
		-output $(SYMROOT)/libc_debug.a
endif
build-dynamic: autopatch $(CC_PRINT_OPTIONS_FILE) build-ppc-dynamic build-ppc64-dynamic build-i386-dynamic
	@echo "Creating final libc.a"
ifeq ($(words $(RC_ARCHS)),1)
	cp -p "$(OBJROOT)/obj.$(RC_ARCHS)/libc.a" "$(SYMROOT)"
else
	lipo -create \
		$(foreach A,$(RC_ARCHS),-arch $(A) "$(OBJROOT)/obj.$(A)/libc.a") \
		-output $(SYMROOT)/libc.a
endif
build-ppc-static:
ifneq ($(RC_ppc),)
	mkdir -p $(OBJROOT)/obj.ppc ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
		MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" $(BSDMAKE) libc_static.a
endif
build-ppc64-static:
ifneq ($(RC_ppc64),)
	mkdir -p $(OBJROOT)/obj.ppc64 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
		MAKEFLAGS="" CFLAGS="-arch ppc64 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_static.a
endif
build-i386-static:
ifneq ($(RC_i386),)
	mkdir -p $(OBJROOT)/obj.i386 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
		MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_static.a
endif
build-ppc-profile:
ifneq ($(RC_ppc),)
	mkdir -p $(OBJROOT)/obj.ppc ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
		MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" $(BSDMAKE) libc_profile.a
endif
build-ppc64-profile:
ifneq ($(RC_ppc64),)
	mkdir -p $(OBJROOT)/obj.ppc64 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
		MAKEFLAGS="" CFLAGS="-arch ppc64 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_profile.a
endif
build-i386-profile:
ifneq ($(RC_i386),)
	mkdir -p $(OBJROOT)/obj.i386 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
		MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_profile.a
endif
build-ppc-debug:
ifneq ($(RC_ppc),)
	mkdir -p $(OBJROOT)/obj.ppc ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
		MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" $(BSDMAKE) libc_debug.a
endif
build-ppc64-debug:
ifneq ($(RC_ppc64),)
	mkdir -p $(OBJROOT)/obj.ppc64 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
		MAKEFLAGS="" CFLAGS="-arch ppc64 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_debug.a
endif
build-i386-debug:
ifneq ($(RC_i386),)
	mkdir -p $(OBJROOT)/obj.i386 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
		MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" $(BSDMAKE) libc_debug.a
endif
build-ppc-dynamic:
ifneq ($(RC_ppc),)
	mkdir -p $(OBJROOT)/obj.ppc ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
		MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" $(BSDMAKE) libc.a
endif
build-ppc64-dynamic:
ifneq ($(RC_ppc64),)
	mkdir -p $(OBJROOT)/obj.ppc64 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
		MAKEFLAGS="" CFLAGS="-arch ppc64 $(LOCAL_CFLAGS)" $(BSDMAKE) libc.a
endif
build-i386-dynamic:
ifneq ($(RC_i386),)
	mkdir -p $(OBJROOT)/obj.i386 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
		MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" $(BSDMAKE) libc.a
endif
build-ppc:
ifneq ($(RC_ppc),)
	mkdir -p $(OBJROOT)/obj.ppc ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
		MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" $(BSDMAKE) build
endif
build-ppc64:
ifneq ($(RC_ppc64),)
	mkdir -p $(OBJROOT)/obj.ppc64 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
		MAKEFLAGS="" CFLAGS="-arch ppc64 $(LOCAL_CFLAGS)" $(BSDMAKE) build
endif
build-i386:
ifneq ($(RC_i386),)
	mkdir -p $(OBJROOT)/obj.i386 ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
		MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" $(BSDMAKE) build
endif

# We have to separately call bsdmake to patch the FreeBSD files, because of
# the way its cache works, it would otherwise pick a file in ${SYMROOT}, even
# over a .s file.
autopatch:
ifneq ($(RC_i386),)
	MACHINE_ARCH="i386" $(BSDMAKE) autopatch
endif
ifneq ($(RC_ppc),)
	MACHINE_ARCH="ppc" $(BSDMAKE) autopatch
endif
ifneq ($(RC_ppc64),)
	MACHINE_ARCH="ppc64" $(BSDMAKE) autopatch
endif

installsrc:
	$(_v) pax -rw . "$(SRCROOT)"

installhdrs-real: $(CC_PRINT_OPTIONS_FILE)
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" MAKEFLAGS="" \
		$(BSDMAKE) installhdrs
ifneq ($(RC_i386),)
	mkdir -p "$(OBJROOT)/obj.i386" ; \
	MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
	    MAKEFLAGS="" $(BSDMAKE) installhdrs-md
endif
ifneq ($(RC_ppc),)
	mkdir -p "$(OBJROOT)/obj.ppc" ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
	    MAKEFLAGS="" $(BSDMAKE) installhdrs-md
endif
ifneq ($(RC_ppc64),)
	mkdir -p "$(OBJROOT)/obj.ppc64" ; \
	MAKEOBJDIR="$(OBJROOT)/obj.ppc64" MACHINE_ARCH="ppc64" \
	    MAKEFLAGS="" $(BSDMAKE) installhdrs-md
endif

BI-install-static: build-static
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc_static.a" ]; then \
		echo "Installing libc_static.a" ; \
		install -c -m 444 "$(SYMROOT)/libc_static.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc_static.a"; \
	fi
BI-install-profile: build-profile
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc_profile.a" ]; then \
		echo "Installing libc_profile.a" ; \
		install -c -m 444 "$(SYMROOT)/libc_profile.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc_profile.a"; \
	fi
ifneq ($(RC_ppc),)
	if [ -f "$(OBJROOT)/obj.ppc/libc-ldbl128_profile.a" ]; then \
		echo "Installing libldbl128_profile.a" ; \
		$(SRCROOT)/make_libldbl128 "$(OBJROOT)/obj.ppc/libc-ldbl128_profile.a" \
			"_profile" "$(DSTROOT)/usr/local/lib/system" $(LIBSYS) ; \
	fi
endif
BI-install-debug: build-debug
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc_debug.a" ]; then \
		echo "Installing libc_debug.a" ; \
		install -c -m 444 "$(SYMROOT)/libc_debug.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc_debug.a"; \
	fi
ifneq ($(RC_ppc),)
	if [ -f "$(OBJROOT)/obj.ppc/libc-ldbl128_debug.a" ]; then \
		echo "Installing libldbl128_debug.a" ; \
		$(SRCROOT)/make_libldbl128 "$(OBJROOT)/obj.ppc/libc-ldbl128_debug.a" \
			"_debug" "$(DSTROOT)/usr/local/lib/system" $(LIBSYS) ; \
	fi
endif
BI-install-dynamic: build-dynamic
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc.a" ]; then \
		echo "Installing libc.a" ; \
		install -c -m 444 "$(SYMROOT)/libc.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc.a"; \
	fi
ifneq ($(RC_ppc),)
	if [ -f "$(OBJROOT)/obj.ppc/libc-ldbl128.a" ]; then \
		echo "Installing libldbl128.a" ; \
		$(SRCROOT)/make_libldbl128 "$(OBJROOT)/obj.ppc/libc-ldbl128.a" \
			"" "$(DSTROOT)/usr/local/lib/system" $(LIBSYS) ; \
	fi
endif

# Don't use -j here; it may try to make links before the files are copied
install-man:
	mkdir -p $(DSTROOT)/usr/share/man/man2
	mkdir -p $(DSTROOT)/usr/share/man/man3
	mkdir -p $(DSTROOT)/usr/share/man/man4
	mkdir -p $(DSTROOT)/usr/share/man/man5
	mkdir -p $(DSTROOT)/usr/share/man/man7
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" NOMANCOMPRESS=1 \
		MACHINE_ARCH="$(shell arch)" MAKEFLAGS="" bsdmake autopatchman maninstall

install-all: build install-man BI-install-dynamic BI-install-static BI-install-profile BI-install-debug

clean:
	rm -rf $(OBJROOT)/obj.ppc $(OBJROOT)/obj.i386 $(OBJROOT)/libc.a \
		$(OBJROOT)/libc_static.a $(OBJROOT)/libc_debug.a \
		$(OBJROOT)/libc_profile.a
