# The legacy PBWO makefile was producing this as a "project" library and header.
# We're currently not building it, and instead referencing bootparam_prot.x
# from both bootparamd.tproj and bpwhoami.tproj.
# In the event we do wish to create a libbootparam.a again, this will do it.

Project = bootparams
ProductType = staticlib
Install_Dir = /usr/local/lib

Library_Version = A

RPCFILES = bootparam_prot.x
RPCSVCFILES = bootparam_prot.x

Install_Private_Headers = $(OBJROOT)/$(Project)/bootparam_prot.h

Extra_CC_Flags = -fno-common

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
