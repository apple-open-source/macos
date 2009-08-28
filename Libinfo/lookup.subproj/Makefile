Project = lookup
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

PRODUCT = $(shell tconf --product)

HFILES = aliasdb.h bootparams.h ils.h kvbuf.h libinfo.h netdb.h netdb_async.h \
	printerdb.h si_data.h si_module.h thread_data.h

CFILES = cache_module.c file_module.c ils.c kvbuf.c libinfo.c\
	mdns_module.c \
         search_module.c si_data.c si_getaddrinfo.c si_module.c thread_data.c

ifeq ($(PRODUCT),MacOSX)
CFILES += ds_module.c
USERDEFS = /usr/local/include/DSlibinfoMIG.defs
SERVERDEFS = /usr/local/include/DSlibinfoMIGAsyncReply.defs
endif

MANPAGES = bootparams.5 gai_strerror.3 getaddrinfo.3 getfsent.3 getgrent.3 \
	getgrouplist.3 getnameinfo.3 getnetgrent.3 getpwent.3 initgroups.3

Install_Headers = aliasdb.h bootparams.h netdb.h printerdb.h
Install_Private_Headers = ils.h kvbuf.h libinfo.h netdb_async.h si_data.h \
	si_module.h thread_data.h

Extra_CC_Flags = -Wall -Werror -fno-common -I. \
	-I../gen.subproj \
	-D__MigTypeCheck=1 -D__DARWIN_NON_CANCELABLE=1

ifeq ($(PRODUCT),MacOSX)
Extra_CC_Flags += -DCONFIG_MAC -DDS_AVAILABLE -DSYNTH_ROOTFS
endif
ifeq ($(PRODUCT),AppleTV)
Extra_CC_Flags += -DCONFIG_APPLETV
endif
ifeq ($(PRODUCT),iPhone)
Extra_CC_Flags += -DCONFIG_IPHONE
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f "$(DSTROOT)/usr/share/man/man3/getaddrinfo.3" \
		"$(DSTROOT)/usr/share/man/man3/freeaddrinfo.3"
	@for LINK in endfsent.3 getfsfile.3 getfsspec.3 getfstype.3 \
		setfsent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getfsent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endgrent.3 getgrgid.3 getgrgid_r.3 getgrnam.3 \
		getgrnam_r.3 setgrent.3 setgroupent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getgrent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endnetgrent.3 innetgr.3 setnetgrent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getnetgrent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endpwent.3 getpwnam.3 getpwnam_r.3 getpwuid.3 \
		getpwuid_r.3 setpassent.3 setpwent.3 setpwfile.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getpwent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
