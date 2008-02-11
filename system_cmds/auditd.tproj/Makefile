Project = auditd
Install_Dir = /usr/sbin

HFILES = auditd.h
CFILES = audit_warn.c auditd.c
MANPAGES = auditd.8
SERVERDEFS = auditd_control.defs audit_triggers.defs

Extra_CC_Flags = -I.
Extra_LD_Flags = -lbsm
Extra_MIG_Flags = -no-cpp-precomp -R -untyped -DNO_DIRECT_RPC

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc/security
	install -c -m 400 rc.audit $(DSTROOT)/private/etc/security
