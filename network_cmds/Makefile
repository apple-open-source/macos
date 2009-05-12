Project = network_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),YES)
SubProjects = arp.tproj \
	alias \
	ifconfig.tproj netstat.tproj\
	ping.tproj\
	route.tproj\
	traceroute.tproj\
        natd.tproj
else
SubProjects = arp.tproj \
	alias \
	bootparams \
        ifconfig.tproj netstat.tproj\
        ping.tproj rarpd.tproj\
        route.tproj routed.tproj\
        slattach.tproj spray.tproj\
        traceroute.tproj trpt.tproj\
        natd.tproj ipfw.tproj\
        ping6.tproj traceroute6.tproj rtsol.tproj ndp.tproj rtadvd.tproj\
	ip6conf.tproj ip6fw.tproj kdumpd.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
