Project = bpwhoami
Install_Dir = /usr/sbin

CFILES = bpwhoami.c
RPCFILES = ../bootparams/bootparam_prot.x
MANPAGES = bpwhoami.1

Extra_CC_Flags = -mdynamic-no-pic -dead_strip
#Extra_LD_Flags = -lbootparam

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
