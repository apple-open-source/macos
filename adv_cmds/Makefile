Project = adv_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),YES)
SubProjects = ps stty
else
SubProjects = ps cap_mkdb colldef finger fingerd \
	gencat last locale localedef lsvfs md \
	mklocale stty tabs tty whois
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
