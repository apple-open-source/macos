Project = diskdev_cmds

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

SubProjects = disklib \
	dev_mkdb.tproj\
        edquota.tproj fdisk.tproj fsck.tproj\
        fsck_hfs.tproj fstyp.tproj fuser.tproj mount.tproj\
        mount_devfs.tproj\
        mount_fdesc.tproj mount_hfs.tproj \
        newfs_hfs.tproj\
        quota.tproj quotacheck.tproj\
        quotaon.tproj repquota.tproj\
        umount.tproj vndevice.tproj

ifeq ($(Embedded),NO)
	SubProjects += vsdbutil.tproj
else
	SubProjects += setclass.tproj
	SubProjects += newfs_hfs_debug.tproj
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
