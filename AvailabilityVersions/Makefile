USR_LOCAL_LIBEXEC=${DRIVERKITROOT}/usr/local/libexec
USR_INCLUDE = ${DRIVERKITROOT}/usr/include
RUNTIME = ${DRIVERKITROOT}/Runtime/usr/include

SRCROOT ?= $(shell pwd)
OBJROOT ?= $(SRCROOT)/obj
SYMROOT ?= $(SRCROOT)/sym
DSTROOT ?= $(SRCROOT)/dst

ifeq "$(DRIVERKIT)" "1"
	DO_RUNTIME_HEADER      = install_runtime_header
	DO_KERNEL_SYMLINK      =
	DO_SCRIPT_INSTALL      = install_script
else
	DO_RUNTIME_HEADER      =
	DO_KERNEL_SYMLINK      = install_kernel_symlink
	DO_SCRIPT_INSTALL      = install_script
endif

install: install_header ${DO_RUNTIME_HEADER} $(DO_KERNEL_SYMLINK) $(DO_SCRIPT_INSTALL)

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

installhdrs: install

installsrc: $(SRCROOT)
	rsync -Crv --exclude '.svn' --exclude '.git' ./ $(SRCROOT)/

clean:
