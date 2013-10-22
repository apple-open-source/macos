Project = remote_cmds

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
SubProjects = rcp.tproj rlogin.tproj rlogind.tproj\
        rsh.tproj rshd.tproj\
        telnetd.tproj
else
SubProjects = domainname.tproj \
        logger.tproj\
        rcp.tproj rexecd.tproj rlogin.tproj rlogind.tproj\
        rsh.tproj rshd.tproj\
        ruptime.tproj rwho.tproj rwhod.tproj\
        talk.tproj talkd.tproj telnet.tproj telnetd.tproj tftp.tproj\
	timed.tproj \
        tftpd.tproj wall.tproj\
        ypbind.tproj ypcat.tproj ypmatch.tproj yppoll.tproj\
        ypset.tproj ypwhich.tproj\
        revnetgroup.tproj \
        stdethers.tproj stdhosts.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
