ifndef $(OBJROOT)
OBJROOT = .
endif
ifndef $(SYMROOT)
SYMROOT = .
endif
ARCH = $(shell arch)
ifndef RC_ppc 
ifndef RC_i386
RC_ppc = 1
endif
endif

# Remove the arch stuff, since we know better here.  
LOCAL_CFLAGS = $(filter-out -arch ppc -arch i386,$(RC_CFLAGS))

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
installhdrs: installhdrs-real
build: build-man build-dynamic
install: installhdrs-real BI-install-dynamic install-man
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

build-man:
	MAKEOBJDIR="$(OBJROOT)" MACHINE_ARCH="$(shell arch)" \
		MAKEFLAGS="" bsdmake buildman
build-static: build-ppc-static build-i386-static
	@echo "Checking for libc_static.a"
	@if [ -f "$(OBJROOT)/obj.ppc/libc_static.a" -a -f "$(OBJROOT)/obj.i386/libc_static.a" ]; then\
		lipo -create -arch ppc "$(OBJROOT)/obj.ppc/libc_static.a" \
			-arch i386 "$(OBJROOT)/obj.i386/libc_static.a" \
			-output $(SYMROOT)/libc_static.a;\
	elif [ -f "$(OBJROOT)/obj.ppc/libc_static.a" ]; then \
		cp -p "$(OBJROOT)/obj.ppc/libc_static.a" "$(SYMROOT)"; \
	elif [ -f "$(OBJROOT)/obj.i386/libc_static.a" ]; then \
		cp -p "$(OBJROOT)/obj.i386/libc_static.a" "$(SYMROOT)"; \
	fi
build-profile: build-ppc-profile build-i386-profile
	@echo "Checking for libc_profile.a"
	@if [ -f "$(OBJROOT)/obj.ppc/libc_profile.a" -a -f "$(OBJROOT)/obj.i386/libc_profile.a" ]; then\
		lipo -create -arch ppc "$(OBJROOT)/obj.ppc/libc_profile.a" \
			-arch i386 "$(OBJROOT)/obj.i386/libc_profile.a" \
			-output $(SYMROOT)/libc_profile.a;\
	elif [ -f "$(OBJROOT)/obj.ppc/libc_profile.a" ]; then \
		cp -p "$(OBJROOT)/obj.ppc/libc_profile.a" "$(SYMROOT)"; \
	elif [ -f "$(OBJROOT)/obj.i386/libc_profile.a" ]; then \
		cp -p "$(OBJROOT)/obj.i386/libc_profile.a" "$(SYMROOT)"; \
	fi
build-debug: build-ppc-debug build-i386-debug
	@echo "Checking for libc_debug.a"
	@if [ -f "$(OBJROOT)/obj.ppc/libc_debug.a" -a -f "$(OBJROOT)/obj.i386/libc_debug.a" ]; then\
		lipo -create -arch ppc "$(OBJROOT)/obj.ppc/libc_debug.a" \
			-arch i386 "$(OBJROOT)/obj.i386/libc_debug.a" \
			-output $(SYMROOT)/libc_debug.a;\
	elif [ -f "$(OBJROOT)/obj.ppc/libc_debug.a" ]; then \
		cp -p "$(OBJROOT)/obj.ppc/libc_debug.a" "$(SYMROOT)"; \
	elif [ -f "$(OBJROOT)/obj.i386/libc_debug.a" ]; then \
		cp -p "$(OBJROOT)/obj.i386/libc_debug.a" "$(SYMROOT)"; \
	fi
build-dynamic: build-ppc-dynamic build-i386-dynamic
	@echo "Checking for libc.a"
	@if [ -f "$(OBJROOT)/obj.ppc/libc.a" -a -f "$(OBJROOT)/obj.i386/libc.a" ]; then\
		echo "Creating FAT libc.a"; \
		lipo -create -arch ppc "$(OBJROOT)/obj.ppc/libc.a" -arch i386 \
			"$(OBJROOT)/obj.i386/libc.a" -output $(SYMROOT)/libc.a;\
	elif [ -f "$(OBJROOT)/obj.ppc/libc.a" ]; then \
		echo "Using thin PPC libc.a" ;\
		cp -p "$(OBJROOT)/obj.ppc/libc.a" "$(SYMROOT)"; \
	elif [ -f "$(OBJROOT)/obj.i386/libc.a" ]; then \
		echo "Using thin i386 libc.a" ;\
		cp -p "$(OBJROOT)/obj.i386/libc.a" "$(SYMROOT)"; \
	fi
build-ppc-static:
	@if [ ! -z "$(RC_ppc)" ]; then \
		mkdir -p $(OBJROOT)/obj.ppc ; \
		MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
			MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" bsdmake libc_static.a;\
	fi
build-i386-static:
	@if [ ! -z "$(RC_i386)" ]; then \
		mkdir -p $(OBJROOT)/obj.i386 ; \
		MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
			MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" bsdmake libc_static.a;\
	fi
build-ppc-profile:
	@if [ ! -z "$(RC_ppc)" ]; then \
		mkdir -p $(OBJROOT)/obj.ppc ; \
		MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
			MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" bsdmake libc_profile.a;\
	fi
build-i386-profile:
	@if [ ! -z "$(RC_i386)" ]; then \
		mkdir -p $(OBJROOT)/obj.i386 ; \
		MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
			MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" bsdmake libc_profile.a;\
	fi
build-ppc-debug:
	@if [ ! -z "$(RC_ppc)" ]; then \
		mkdir -p $(OBJROOT)/obj.ppc ; \
		MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
			MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" bsdmake libc_debug.a;\
	fi
build-i386-debug:
	@if [ ! -z "$(RC_i386)" ]; then \
		mkdir -p $(OBJROOT)/obj.i386 ; \
		MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
			MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" bsdmake libc_debug.a;\
	fi
build-ppc-dynamic:
	@if [ ! -z "$(RC_ppc)" ]; then \
		mkdir -p $(OBJROOT)/obj.ppc ; \
		MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
			MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" bsdmake libc.a;\
	fi
build-i386-dynamic:
	@if [ ! -z "$(RC_i386)" ]; then \
		mkdir -p $(OBJROOT)/obj.i386 ; \
		MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
			MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" bsdmake libc.a;\
	fi

build-ppc:
	@if [ ! -z "$(RC_ppc)" ]; then \
		mkdir -p $(OBJROOT)/obj.ppc ; \
		MAKEOBJDIR="$(OBJROOT)/obj.ppc" MACHINE_ARCH="ppc" \
			MAKEFLAGS="" CFLAGS="-arch ppc $(LOCAL_CFLAGS)" bsdmake build;\
	fi
build-i386:
	@if [ ! -z "$(RC_i386)" ]; then \
		mkdir -p $(OBJROOT)/obj.i386 ; \
		MAKEOBJDIR="$(OBJROOT)/obj.i386" MACHINE_ARCH="i386" \
			MAKEFLAGS="" CFLAGS="-arch i386 $(LOCAL_CFLAGS)" bsdmake build;\
	fi
installsrc:
	$(_v) pax -rw . "$(SRCROOT)"
installhdrs-real:
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" \
	MACHINE_ARCH="$(shell arch)" MAKEFLAGS="" bsdmake installhdrs

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
BI-install-debug: build-debug
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc_debug.a" ]; then \
		echo "Installing libc_debug.a" ; \
		install -c -m 444 "$(SYMROOT)/libc_debug.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc_debug.a"; \
	fi
BI-install-dynamic: build-dynamic
	mkdir -p $(DSTROOT)/usr/local/lib/system
	if [ -f "$(SYMROOT)/libc.a" ]; then \
		echo "Installing libc.a" ; \
		install -c -m 444 "$(SYMROOT)/libc.a" \
			$(DSTROOT)/usr/local/lib/system; \
		ranlib "$(DSTROOT)/usr/local/lib/system/libc.a"; \
	fi

install-man:
	mkdir -p $(DSTROOT)/usr/share/man/man2
	mkdir -p $(DSTROOT)/usr/share/man/man3
	mkdir -p $(DSTROOT)/usr/share/man/man4
	mkdir -p $(DSTROOT)/usr/share/man/man5
	mkdir -p $(DSTROOT)/usr/share/man/man7
	MAKEOBJDIR="$(OBJROOT)" DESTDIR="$(DSTROOT)" NOMANCOMPRESS=1 \
		MACHINE_ARCH="$(shell arch)" MAKEFLAGS="" bsdmake maninstall

install-all: build install-man BI-install-dynamic BI-install-static BI-install-profile

clean:
	rm -rf $(OBJROOT)/obj.ppc $(OBJROOT)/obj.i386 $(OBJROOT)/libc.a \
		$(OBJROOT)/libc_static.a $(OBJROOT)/libc_debug.a \
		$(OBJROOT)/libc_profile.a
