Project = syslog

SubProjects = aslcommon aslmanager.tproj syslogd.tproj util.tproj

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(RMDIR) "$(DSTROOT)"/scratch
