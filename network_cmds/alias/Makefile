Project = alias
ProductType = dylib
Install_Dir = /usr/lib
#Install_Headers = alias.h

Library_Version = A

HFILES = alias.h alias_local.h
CFILES = alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c\
         alias_nbt.c alias_pptp.c alias_proxy.c alias_smedia.c alias_util.c
#MANPAGES = libalias.3

#Extra_CC_Flags = \
	-I$(SDKROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
