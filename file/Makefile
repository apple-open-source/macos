# Project info
Project               = file
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --enable-fsect-man5
#Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

lazy_install_source:: shadow_source
Install_Target = install-strip

