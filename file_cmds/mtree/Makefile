Project = mtree
Install_Dir = /usr/sbin

CFILES = compare.c create.c excludes.c misc.c mtree.c spec.c specspec.c \
	verify.c \
	../cksum/crc.c
MANPAGES = mtree.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	-DENABLE_MD5 \
	-DENABLE_RMD160 \
	-DENABLE_SHA1
Extra_LD_Flags = -dead_strip \
	-lmd -lcrypto

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
