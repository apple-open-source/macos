#
# Makefile for the NFS project
#

SUBPROJECTS = mount_nfs nfs_fs nfsd nfsiod nfsstat rpc.lockd rpc.rquotad rpc.statd showmount files

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

