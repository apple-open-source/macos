#
# Makefile for the NFS project
#

BUILDROOT ?= /tmp/NFS
CCOVERAGEDIR ?= /var/tmp/cc/nfs

ifndef SDKROOT
SDKROOT := $(shell xcrun --sdk macosx.internal --show-sdk-path)
endif

DSTROOT ?= /tmp

SUBPROJECTS = mount_nfs nfs_fs nfsiod nfsstat showmount files

ifneq "$(RC_TARGET_CONFIG)" "iPhone"
SUBPROJECTS += nfsd rpc.lockd rpc.statd rpc.rquotad ncctl nfs4mapid nfs_acl
endif

.PHONY: installsrc clean installhdrs install inplace coverage $(CCOVERAGEDIR)

inplace:
	@$(MAKE) inplace-all

inplace-%:
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && $(MAKE) $(subst inplace-,,$@) SRCROOT=. OBJROOT=. SYMROOT=. SDKROOT=$(SDKROOT) DSTROOT=/ ) || exit 1; \
	done

install-coverage: $(CCOVERAGEDIR)
	@$(MAKE) install CC_COVERAGE_FLAGS='-fprofile-instr-generate -fcoverage-mapping -O0' LD_COVERAGE_FLAGS='-fprofile-instr-generate'

$(CCOVERAGEDIR):
	@mkdir -p $(CCOVERAGEDIR); \
	chmod 777 $(CCOVERAGEDIR)

#
# BUILDIT like targets for NFS project and NFS project with code coverage
#
build-nfs: $(BUILDROOT)
	@$(MAKE) installsrc clean installhdrs install SRCROOT=$(BUILDROOT)/src SYMROOT=$(BUILDROOT)/sym OBJROOT=$(BUILDROOT)/obj DSTROOT=$(BUILDROOT)/root

build-nfs-coverage: $(BUILDROOT)
	@$(MAKE) installsrc clean installhdrs install CC_COVERAGE_FLAGS='-fprofile-instr-generate -fcoverage-mapping -O0' LD_COVERAGE_FLAGS='-fprofile-instr-generate' \
	SRCROOT=$(BUILDROOT)/src SYMROOT=$(BUILDROOT)/sym OBJROOT=$(BUILDROOT)/obj DSTROOT=$(BUILDROOT)/root CCOVERAGEDIR=$(CCOVERAGEDIR)

$(BUILDROOT):
	@mkdir -p $(BUILDROOT)/{src,sym,obj,root}

installsrc::
	@cp Makefile $(SRCROOT)

install::
	@for proj in $(SUBPROJECTS); do \
		mkdir -p $(OBJROOT)/$${proj}; \
		mkdir -p $(SYMROOT)/$${proj}; \
	done

installsrc clean installhdrs install coverage::
	for proj in $(SUBPROJECTS); do \
		(cd $${proj} && $(MAKE) $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
			SDKROOT=$(SDKROOT) \
			CC_COVERAGE_FLAGS="$${CC_COVERAGE_FLAGS:+$(CC_COVERAGE_FLAGS) -fprofile-generate=$(CCOVERAGEDIR)/$${proj}.%p}" \
			LD_COVERAGE_FLAGS=$(LD_COVERAGE_FLAGS) \
		) || exit 1; \
	done
