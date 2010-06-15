Project = dns
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

Install_Headers_Directory = /usr/include/arpa
Install_Headers = inet.h

CFILES = herror.c res_comp.c res_data.c res_debug.c\
	 res_init.c res_mkquery.c res_query.c res_send.c

Extra_CC_Flags = -Wall -fno-common -I. \
	-I../gen.subproj -I../lookup.subproj \
	-DUSE_OPTIONS_H -D__DARWIN_NON_CANCELABLE=1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
