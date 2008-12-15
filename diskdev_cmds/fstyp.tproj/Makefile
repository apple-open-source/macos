Project = fstyp
ProductType = none

Products = fstyp fstyp_hfs fstyp_msdos fstyp_ufs fstyp_udf fstyp_ntfs
CFILES   = fstyp.c fstyp_hfs.c fstyp_msdos.c fstyp_ufs.c fstyp_udf.c fstyp_ntfs.c
MANPAGES = fstyp.8 fstyp_hfs.8 fstyp_msdos.8 fstyp_ufs.8 fstyp_udf.8 fstyp_ntfs.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic -Os \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/sbin/$(X)"
	$(foreach X, $(Products), \
		$(CC) $(LDFLAGS) -o "$(SYMROOT)/$(X)" "$(OBJROOT)/$(Project)/$(X).o" ; \
		$(INSTALL_PROGRAM) "$(SYMROOT)/$(X)" "$(DSTROOT)/sbin/$(X)" ; )
