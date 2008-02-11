Project = system_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = dynamic_pager.tproj ac.tproj accton.tproj arch.tproj\
	bootlog.tproj\
        dmesg.tproj\
        getconf.tproj getty.tproj hostinfo.tproj iostat.tproj\
        login.tproj makekey.tproj\
        mkfile.tproj newgrp.tproj nvram.tproj passwd.tproj pwd_mkdb.tproj\
        reboot.tproj sync.tproj sysctl.tproj\
        update.tproj vipw.tproj vifs.tproj zic.tproj zdump.tproj vm_stat.tproj\
        zprint.tproj latency.tproj sc_usage.tproj fs_usage.tproj\
        sadc.tproj sar.tproj sa.tproj \
	dp_notify_lib nologin.tproj pagesize.tproj

ifeq "$(Embedded)" "NO"
SubProjects += at.tproj atrun.tproj\
	auditd.tproj audit.tproj\
	chkpasswd.tproj chpass.tproj\
	dirhelper.tproj\
	shutdown.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
