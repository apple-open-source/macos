Project = membership
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

PRODUCT = $(shell tconf --product)

HFILES = membership.h membershipPriv.h ntsid.h

CFILES = membership.c

MANPAGES = mbr_check_membership.3 mbr_uid_to_uuid.3

ifeq ($(PRODUCT),MacOSX)
USERDEFS = /usr/local/include/DSmemberdMIG.defs
endif

Install_Headers = membership.h ntsid.h 
Install_Private_Headers = membershipPriv.h

Extra_CC_Flags = -Wall -fno-common \
	-D__MigTypeCheck=1 -D__DARWIN_NON_CANCELABLE=1
ifeq ($(PRODUCT),MacOSX)
Extra_CC_Flags += -DDS_AVAILABLE
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	@for LINK in mbr_gid_to_uuid.3 mbr_sid_to_uuid.3 mbr_uuid_to_id.3 \
		mbr_uuid_to_sid.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/mbr_uid_to_uuid.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done

