Project = diskdev_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = disklib \
	clri.tproj dev_mkdb.tproj dump.tproj\
        dumpfs.tproj edquota.tproj fdisk.tproj fsck.tproj\
        fsck_hfs.tproj fstyp.tproj fuser.tproj mount.tproj\
        mount_devfs.tproj\
        mount_fdesc.tproj mount_hfs.tproj \
        newfs.tproj newfs_hfs.tproj\
        quot.tproj quota.tproj quotacheck.tproj\
        quotaon.tproj repquota.tproj restore.tproj\
        tunefs.tproj umount.tproj ufs.tproj vsdbutil.tproj\
        vndevice.tproj

ifeq ($(Embedded),NO)
SubProjects += mount_cd9660.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
