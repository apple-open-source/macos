Project = remote_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),YES)
SubProjects = rcp.tproj rlogin.tproj rlogind.tproj\
        rsh.tproj rshd.tproj\
        telnetd.tproj
else
SubProjects = domainname.tproj \
        logger.tproj\
        rcp.tproj rexecd.tproj rlogin.tproj rlogind.tproj\
        rpcinfo.tproj rsh.tproj rshd.tproj\
        ruptime.tproj rwho.tproj rwhod.tproj\
        talk.tproj talkd.tproj telnet.tproj telnetd.tproj tftp.tproj\
	timed.tproj \
        tftpd.tproj wall.tproj\
        ypbind.tproj ypcat.tproj ypmatch.tproj yppoll.tproj\
        yppush.tproj ypserv.tproj ypset.tproj ypwhich.tproj\
        ypxfr.tproj makedbm.tproj revnetgroup.tproj rpc_yppasswdd.tproj\
        stdethers.tproj stdhosts.tproj \
	ypinit.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
