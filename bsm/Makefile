##
## Apple top-level Makefile for bsm, containing Apple-specific rules.  
##

Project = bsm
Extra_CC_Flags = -fno-common -Wall
## XXX  This probably doesn't do anything and should be removed
# Extra_CPP_Includes = -I$(OBJROOT)/usr/include -I../lib -I.

## MAKEFILEPATH == /Developer/Makefiles/
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

##
## These must be kept in sync with the build products, et al., at the 
## subproject levels.  We can't use the "install" targets in those 
## Makefiles since that would mean adding Apple-specific dependencies to
## them.  
##
BSMSbinFiles = praudit auditreduce
BSMSbinDst = $(DSTROOT)/$(USRSBINDIR)
BSMEtcOpenFiles = audit_class audit_event
BSMEtcRestrictedFiles = audit_control audit_user
BSMEtcExecFiles = audit_warn
BSMEtcDst = $(DSTROOT)/$(ETCDIR)/security
BSMLib = libbsm.dylib
BSMLibDst = $(DSTROOT)/$(USRLIBDIR)
BSMLibHdrs = libbsm.h audit_uevents.h
BSMLibHdrsDst = $(DSTROOT)/$(USRINCLUDEDIR)/bsm
BSMMan1Pages = auditreduce.1 praudit.1
BSMMan1Dst = $(DSTROOT)/$(MANDIR)/man1
BSMMan5Pages = audit_class.5 audit_control.5 audit_event.5 audit_user.5 audit_warn.5
BSMMan5Dst = $(DSTROOT)/$(MANDIR)/man5

##
## Admins are free to use another log directory, but this one will be 
## unconditionally created.  XXX  Not created by lower-level Makefiles
## 
AuditLogDir = $(DSTROOT)/$(VARDIR)/audit

##
## shadow_source symlinks into SRCROOT so make can build in OBJROOT.  
##
## The "all" target sidesteps the lower-level Makefiles' "install" target 
## so this target can take its own, Apple-specific installation steps.  
##
install:: bsm
	$(INSTALL) -g wheel -d $(BSMSbinDst) $(BSMEtcDst) $(BSMLibDst) $(BSMLibHdrsDst) $(BSMMan1Dst) $(BSMMan5Dst) $(AuditLogDir)

	$(_v) for bsmfile in $(BSMSbinFiles) ; do \
		$(INSTALL) $(OBJROOT)/$(Project)/bin/$${bsmfile} $(BSMSbinDst) ; \
		$(STRIP) -S $(BSMSbinDst)/$${bsmfile} ; \
		$(INSTALL) $(OBJROOT)/$(Project)/bin/$${bsmfile} $(SYMROOT) ; \
	done

	$(_v) for bsmfile in $(BSMEtcOpenFiles) ; do \
		$(INSTALL) -m 0444 $(OBJROOT)/$(Project)/etc/$${bsmfile} $(BSMEtcDst) ; \
	done

	$(_v) for bsmfile in $(BSMEtcRestrictedFiles) ; do \
		$(INSTALL) -m 0400 $(OBJROOT)/$(Project)/etc/$${bsmfile} $(BSMEtcDst) ; \
	done

	$(_v) for bsmfile in $(BSMEtcExecFiles) ; do \
		$(INSTALL) -m 0555 $(OBJROOT)/$(Project)/etc/$${bsmfile} $(BSMEtcDst) ; \
	done

	$(INSTALL) $(OBJROOT)/$(Project)/lib/$(BSMLib) $(BSMLibDst)
	$(_v) for bsmfile in $(BSMLibHdrs) ; do \
		$(INSTALL) -m 0644 $(OBJROOT)/$(Project)/lib/$${bsmfile} $(BSMLibHdrsDst) ; \
	done

	$(_v) for bsmfile in $(BSMMan1Pages) ; do \
		$(INSTALL) -m 0444 $(OBJROOT)/$(Project)/man/$${bsmfile} $(BSMMan1Dst) ; \
	done

	$(_v) for bsmfile in $(BSMMan5Pages) ; do \
		$(INSTALL) -m 0444 $(OBJROOT)/$(Project)/man/$${bsmfile} $(BSMMan5Dst) ; \
	done

	## XXX  Why is the chgrp not working?  
	$(CHGRP) -v admin $(BSMLibDst)/$(BSMLib)
	$(STRIP) -S $(BSMLibDst)/$(BSMLib)
	$(INSTALL) $(OBJROOT)/$(Project)/lib/$(BSMLib) $(SYMROOT)

	$(CHMOD) 0750 $(AuditLogDir)
	$(CHGRP) -v admin $(AuditLogDir)

bsm:: shadow_source
	$(_v) $(MAKE) -C $(OBJROOT)/$(Project) $(Environment) all
