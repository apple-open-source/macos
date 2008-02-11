Project = bootparamd
Install_Dir = /usr/sbin

CFILES = bootparamd.c bootparam_proc.c
RPCFILES = ../bootparams/bootparam_prot.x
RPCSVCFILES = ../bootparams/bootparam_prot.x
MANPAGES = bootparamd.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip
#Extra_LD_Flags = -lbootparam

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
