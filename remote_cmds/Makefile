Project = remote_cmds

ifeq "$(RC_TARGET_CONFIG)" "iPhone"
SubProjects = telnetd.tproj
else
SubProjects = \
        logger.tproj\
        talk.tproj talkd.tproj telnet.tproj telnetd.tproj tftp.tproj\
        tftpd.tproj wall.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
