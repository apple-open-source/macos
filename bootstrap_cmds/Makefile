Project = bootstrap_cmds

SubProjects = migcom.tproj config.tproj relpath.tproj decomment.tproj

MANPAGES = vers_string.1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/usr/bin
	$(INSTALL_SCRIPT) vers_string.sh "$(DSTROOT)"/usr/bin/vers_string
