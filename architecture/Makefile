#
# makefile for architecture project.
#

# Defaults typically set by build system
RC_ARCHS ?= i386 arm
SDKROOT ?= /

# fold all arm subtypes to the family "arm",
# and map x86_64 -> i386
SUPPORTED_ARCHS = i386 arm
CANONICAL_ARCH_x86_64 = i386
CANONICAL_ARCH_armv5 = arm
CANONICAL_ARCH_armv6 = arm
CANONICAL_ARCH_armv7 = arm

ARCHS = $(filter $(SUPPORTED_ARCHS),$(sort $(foreach x,$(RC_ARCHS),$(if $(CANONICAL_ARCH_$(x)),$(CANONICAL_ARCH_$(x)),$(x)))))

# install machine-independent and per-arch headers
DIRS = . $(ARCHS)

ifeq ($(RC_ProjectName),architecture_Sim)
	HEADER_INSTALL_PREFIX = $(SDKROOT)
else
	HEADER_INSTALL_PREFIX =
endif

EXPORT_DSTDIR=$(HEADER_INSTALL_PREFIX)/usr/include/architecture
LOCAL_DSTDIR=$(HEADER_INSTALL_PREFIX)/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/architecture

INSTALL = /usr/bin/install
INSTALL_FLAGS= -p -m 444
MKDIRS = /bin/mkdir -p

all:

install:	all installhdrs

installhdrs: all DSTROOT $(DSTROOT)$(LOCAL_DSTDIR) \
	$(DSTROOT)$(EXPORT_DSTDIR)
	for i in ${DIRS};						\
	do								\
	    DSTDIR=$(DSTROOT)$(LOCAL_DSTDIR)/$$i;			\
	    (cd $$i;							\
                $(MKDIRS) $$DSTDIR;					\
		echo Installing *.h;					\
                install $(INSTALL_FLAGS) *.h $$DSTDIR);			\
	done
	for i in ${DIRS};						\
	do								\
	    DSTDIR=$(DSTROOT)$(EXPORT_DSTDIR)/$$i;			\
	    (cd $$i;							\
                $(MKDIRS) $$DSTDIR;					\
		echo Installing *.h;					\
                install $(INSTALL_FLAGS) *.h $$DSTDIR);			\
	done

.PHONY: clean

clean:
	rm -f *~ */*~ 
	rm -rf exports

installsrc: SRCROOT $(SRCROOT)
	pax -rw . ${SRCROOT}


$(SRCROOT) $(DSTROOT)$(EXPORT_DSTDIR) $(DSTROOT)$(LOCAL_DSTDIR):
	$(MKDIRS) $@

.PHONY: SRCROOT DSTROOT

SRCROOT DSTROOT:
	if [ -n "${$@}" ]; \
	then \
		exit 0; \
	else \
		echo Must define $@; \
		exit 1; \
	fi
