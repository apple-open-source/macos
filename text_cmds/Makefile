Project = text_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = banner cat col colrm column comm csplit cut ed expand fmt\
        fold head join lam look nl paste pr rev rs sed sort split\
        tail tr ul unexpand uniq unvis vis wc

ifeq ($(Embedded),NO)
# md5 requires openssl
SubProjects += md5
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)$(OSV)
	$(INSTALL_FILE) $(SRCROOT)/text_cmds.plist $(DSTROOT)$(OSV)
