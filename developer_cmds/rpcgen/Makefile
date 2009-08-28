Project = rpcgen
Install_Dir = /usr/bin

HFILES = rpc_parse.h rpc_scan.h rpc_util.h
CFILES = rpc_clntout.c rpc_cout.c rpc_hout.c rpc_main.c rpc_parse.c\
         rpc_sample.c rpc_scan.c rpc_svcout.c rpc_tblout.c rpc_util.c
MANPAGES = rpcgen.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
