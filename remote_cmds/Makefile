Project = remote_cmds

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
SubProjects = telnetd.tproj
else
SubProjects = domainname.tproj \
        logger.tproj\
        talk.tproj talkd.tproj telnet.tproj telnetd.tproj tftp.tproj\
        tftpd.tproj wall.tproj\
        ypbind.tproj ypcat.tproj ypmatch.tproj yppoll.tproj\
        ypset.tproj ypwhich.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
