Project = file_cmds

SubProjects = chflags chmod chown cksum compress cp dd df du install \
	ipcrm ipcs ln ls mkdir mkfifo mknod mv pathchk pax rm \
	rmdir shar stat touch

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)
ifneq ($(Embedded),YES)
	#libcrypto missing
	SubProjects += mtree
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
