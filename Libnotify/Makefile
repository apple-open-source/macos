Project = notify
Install_Dir = /usr/local/lib/system
ProductType = staticlib
BuildProfile = YES
BuildDebug = YES

CFILES = libnotify.c notify_client.c table.c $(OBJROOT)/$(Project)/_version.c
USERDEFS = notify_ipc.defs
MANPAGES = notify.3 notify_cancel.3 notify_check.3 notify_get_state.3 \
	notify_post.3 notify_register_check.3 \
	notify_register_dispatch.3 \
	notify_register_file_descriptor.3 notify_register_mach_port.3 \
	notify_register_signal.3 notify_set_state.3

Install_Headers = notify.h notify_keys.h
Install_Private_Headers = libnotify.h notify_ipc.defs notify_ipc_types.h

Extra_CC_Flags = -Wall -fno-common \
	-D__MigTypeCheck=1 -D__DARWIN_NON_CANCELABLE=1 -I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(OBJROOT)/$(Project)/_version.c:
	/Developer/Makefiles/bin/version.pl Libnotify > $@
