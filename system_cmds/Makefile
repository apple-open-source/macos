Project = system_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = ac.tproj accton.tproj arch.tproj \
	dmesg.tproj dp_notify_lib dynamic_pager.tproj fs_usage.tproj	\
	getconf.tproj getty.tproj hostinfo.tproj iostat.tproj		\
	latency.tproj login.tproj makekey.tproj mkfile.tproj		\
	newgrp.tproj nologin.tproj nvram.tproj pagesize.tproj		\
	passwd.tproj pwd_mkdb.tproj reboot.tproj sa.tproj sadc.tproj	\
	sar.tproj sc_usage.tproj sync.tproj sysctl.tproj trace.tproj	\
	vipw.tproj vifs.tproj vm_stat.tproj zdump.tproj zic.tproj	\
	zprint.tproj

ifeq "$(Embedded)" "NO"
SubProjects += at.tproj atrun.tproj \
	chkpasswd.tproj chpass.tproj dirhelper.tproj shutdown.tproj
else
SubProjects += mean.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
