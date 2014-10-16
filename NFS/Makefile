#
# Makefile for the NFS project
#

SUBPROJECTS = mount_nfs nfs_fs nfsiod nfsstat showmount files

ifneq "$(RC_TARGET_CONFIG)" "iPhone"
SUBPROJECTS += nfsd rpc.lockd rpc.statd rpc.rquotad ncdestroy nfs4mapid
endif

.PHONY: installsrc clean installhdrs install inplace

inplace:
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && $(MAKE) $@ SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/ ) || exit 1; \
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
		(cd $${proj} && $(MAKE) $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
		) || exit 1; \
	done

