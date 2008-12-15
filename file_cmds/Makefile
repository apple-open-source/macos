Project = file_cmds

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = chflags chmod chown cksum compress cp dd df du install \
	ipcrm ipcs ln ls\
        mkdir mkfifo mknod mv pathchk pax rm rmdir rmt shar stat\
        touch

ifeq ($(Embedded),NO)
#libcrypto missing
SubProjects += mtree
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
