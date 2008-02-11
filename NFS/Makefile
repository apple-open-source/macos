#
# Makefile for the NFS project
#

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SUBPROJECTS = mount_nfs nfs_fs nfsiod nfsstat rpc.rquotad showmount files

ifeq "$(Embedded)" "NO"
SUBPROJECTS += nfsd rpc.lockd rpc.statd
endif

.PHONY: installsrc clean installhdrs install all

all:
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && make $@ ) || exit 1; \
	done

installsrc::
	@cp Makefile $(SRCROOT)

install::
	@for proj in $(SUBPROJECTS); do \
		mkdir -p $(OBJROOT)/$${proj}; \
		mkdir -p $(SYMROOT)/$${proj}; \
	done

installsrc clean installhdrs install::
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && make $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
		) || exit 1; \
	done

