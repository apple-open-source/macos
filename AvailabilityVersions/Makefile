USR_LOCAL_LIBEXEC=${DRIVERKITROOT}/usr/local/libexec
USR_LOCAL_INCLUDE=${DRIVERKITROOT}/usr/local/include
USR_INCLUDE = ${DRIVERKITROOT}/usr/include
RUNTIME = ${DRIVERKITROOT}/Runtime/usr/include
RUNTIME_LOCAL =  ${DRIVERKITROOT}/Runtime/usr/local/include

SRCROOT ?= $(shell pwd)
OBJROOT ?= $(SRCROOT)/obj
SYMROOT ?= $(SRCROOT)/sym
DSTROOT ?= $(SRCROOT)/dst

ifeq "$(DRIVERKIT)" "1"
	DO_RUNTIME_HEADER      = install_runtime_header
	DO_KERNEL_SYMLINK      =
	DO_SCRIPT_INSTALL      = install_script
        DYLD_HEADERS_LOCATION  = $(RUNTIME_LOCAL)
else
	DO_RUNTIME_HEADER      =
	DO_KERNEL_SYMLINK      = install_kernel_symlink
	DO_SCRIPT_INSTALL      = install_script
        DYLD_HEADERS_LOCATION  = $(USR_LOCAL_INCLUDE)
endif

install: install_header install_dyld_headers ${DO_RUNTIME_HEADER} $(DO_KERNEL_SYMLINK) $(DO_SCRIPT_INSTALL)

install_header:
	mkdir -p $(DSTROOT)$(USR_INCLUDE)
	cp $(SRCROOT)/AvailabilityVersions.h $(DSTROOT)$(USR_INCLUDE)/AvailabilityVersions.h

install_runtime_header:
	mkdir -p $(DSTROOT)$(RUNTIME)
	cp $(SRCROOT)/AvailabilityVersions.h $(DSTROOT)$(RUNTIME)/AvailabilityVersions.h

install_kernel_symlink:
	mkdir -p $(DSTROOT)/System/Library/Frameworks/Kernel.framework/Versions/A/Headers
	cd $(DSTROOT)/System/Library/Frameworks/Kernel.framework/Versions/A/Headers && \
	ln -s ../../../../../../../usr/include/AvailabilityVersions.h

install_script:
	mkdir -p $(DSTROOT)$(USR_LOCAL_LIBEXEC)
	cp $(SRCROOT)/availability.pl $(DSTROOT)$(USR_LOCAL_LIBEXEC)/availability.pl

install_dyld_headers:
	mkdir -p $(DSTROOT)$(DYLD_HEADERS_LOCATION)/dyld
	$(SRCROOT)/build_version_map.rb $(SRCROOT)/availability.pl > $(DSTROOT)$(DYLD_HEADERS_LOCATION)/dyld/VersionMap.h
	$(SRCROOT)/print_dyld_os_versions.rb $(SRCROOT)/availability.pl > $(DSTROOT)$(DYLD_HEADERS_LOCATION)/dyld/for_dyld_priv.inc

installhdrs: install

installsrc: $(SRCROOT)
	rsync -Crv --exclude '.svn' --exclude '.git' ./ $(SRCROOT)/

clean:
