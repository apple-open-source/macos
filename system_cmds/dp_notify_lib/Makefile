Project = dp_notify_lib
ProductType = staticlib
Install_Dir = /usr/local/lib

SERVERDEFS = ../dynamic_pager.tproj/backing_store_alerts.defs
USERDEFS = ../dynamic_pager.tproj/backing_store_triggers.defs

Extra_MIG_Flags = -no-cpp-precomp -R -untyped -DNO_DIRECT_RPC -I$(SRCROOT)
Extra_LD_Flags = -static

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install: 
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/include/mach
	$(INSTALL_FILE) $(OBJROOT)/$(Project)/backing_store_alerts_server.h \
		$(DSTROOT)/usr/local/include/mach
	$(INSTALL_FILE) $(OBJROOT)/$(Project)/backing_store_triggers.h \
		$(DSTROOT)/usr/local/include/mach
