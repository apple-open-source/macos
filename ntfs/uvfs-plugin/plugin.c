/* Copyright © 2017-2019 Apple Inc. All rights reserved. */
/* Modifications for NTFS support:
 * Copyright © 2019-2020 Tuxera Inc. All rights reserved. */

#include <os/log.h>
#include <sys/errno.h>
#include <string.h>

#include <UserFS/UserVFS.h>

#include "ntfs_xpl.h"
#include "ntfs_endian.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

extern kern_return_t ntfs_module_start(kmod_info_t *ki __unused,
		void *data __unused);

extern kern_return_t ntfs_module_stop(kmod_info_t *ki __unused,
		void *data __unused);

static int
plugin_fsops_init(void)
{
	kern_return_t ret;

	ret = ntfs_module_start(NULL, NULL);
	xpl_debug("ntfs_module_start: %d\n", ret);

	return (ret == KERN_SUCCESS) ? 0 : ENOMEM;
}

static void
plugin_fsops_fini(void)
{
	ntfs_module_stop(NULL, NULL);
}

static int
plugin_fsops_check(__unused int diskFd,
                   __unused UVFSVolumeId volId,
                   __unused UVFSVolumeCredential *volumeCreds,
                   __unused check_flags_t how)
{
    //For now just success!
    return 0;
}

static int
plugin_fsops_taste(int diskFd)
{
	/* Preliminary taste which just checks a simple signature. */
	int err = 0;

	char sb_region[4096];
	ssize_t read_res;

	os_log_debug(OS_LOG_DEFAULT, "ntfs:taste:start");

	read_res = pread(diskFd, sb_region, 4096, 0);
	if (read_res < 0) {
		err = (err = errno) ? err : EIO;
		fprintf(stderr, "Error from pread: %s\n", strerror(errno));
		goto out;
	}
	else if (read_res < 512) {
		fprintf(stderr, "Partial pread: %zd/4096\n", read_res);
		err = EIO;
		goto out;
	}

	if (memcmp(&sb_region[3], "NTFS    ", 8)) {
		fprintf(stderr, "Not NTFS.\n");
		err = EILSEQ;
		goto out;
	}
	xpl_debug("Tastes like NTFS.");
out:
	os_log_debug(OS_LOG_DEFAULT, "ntfs:taste:finish:%d", err);
	return err;
}

static int
plugin_fsops_scanvols(__unused int diskFd,
                      UVFSScanVolsRequest *request,
                      UVFSScanVolsReply *reply)
{
    os_log_debug(OS_LOG_DEFAULT, "ntfs:scanvols:start");
    if (request == NULL || reply == NULL)
    {
        os_log_debug(OS_LOG_DEFAULT, "ntfs:scanvols:finish:EINVAL");
        return EINVAL;
    }

    if (request->sr_volid > 0)
    {
        os_log_debug(OS_LOG_DEFAULT, "ntfs:scanvols:finish:UVFS_SCANVOLS_EOF_REACHED");
        return UVFS_SCANVOLS_EOF_REACHED;
    }

    reply->sr_volid = 0;
    reply->sr_volac = UAC_UNLOCKED;
    memset(reply->sr_volname, 0, sizeof(reply->sr_volname));
    reply->sr_readOnly = true;

    os_log_debug(OS_LOG_DEFAULT, "ntfs:scanvols:finish:0");
    return 0;
}

static int
plugin_fsops_mount(int diskFd,
                   UVFSVolumeId volId,
                   __unused UVFSMountFlags mountFlags,
                   __unused UVFSVolumeCredential *volumeCreds,
                   UVFSFileNode *outRootFileNode)
{
	int err = 0;

	struct stat st = { 0 };
	uint32_t blocksize = 0;

	os_log_debug(OS_LOG_DEFAULT, "ntfs:mount:start");

	struct mount *mp = NULL;
	vnode_t devvp = NULL;
	struct {
		char reserved[8];
		u8 major_ver;	/* The major version of the mount options structure. */
		u8 minor_ver;	/* The minor version of the mount options structure. */
		/* NTFS_MNT_OPTS */ le32 flags;
	} __attribute__((__packed__)) mount_data;
	struct vfs_context vfscontext;
	int mounted = 0;
	vnode_t root_vnode = NULL;

	if (outRootFileNode == NULL || volId != 0 || !fsadded) {
		os_log_debug(OS_LOG_DEFAULT, "ntfs:mount:finish:EINVAL");
		return EINVAL;
	}

	if (fstat(diskFd, &st)) {
		err = (err = errno) ? err : EIO;
		xpl_perror(errno, "Error while issuing fstat for file "
			"descriptor");
		goto out;
	}

	if ((st.st_mode & S_IFMT) != S_IFBLK &&
		(st.st_mode & S_IFMT) != S_IFCHR)
	{
		/*
		 * Not a block device, so we can't get the block size by ioctl.
		 * Read the boot sector and extract the sector size value so
		 * that we can adapt to images collected from 4k sector volumes.
		 */
		unsigned char buf[512];
		u16 ntfs_sector_size;

		if (pread(diskFd, buf, sizeof(buf), 0) == sizeof(buf) &&
			/* Assemble sector size from bytes 11-12 (unaligned). */
			(ntfs_sector_size = (buf[12] << 8) | buf[11]) &&
			/* Check that the sector size value is a power of 2. */
			!((ntfs_sector_size - 1) & ntfs_sector_size) &&
			/* Check that the sector size is within valid range. */
			ntfs_sector_size >= 512 && ntfs_sector_size <= 4096)
		{
			xpl_debug("Block size read from NTFS boot sector: %hu",
				ntfs_sector_size);
			blocksize = ntfs_sector_size;
		}
		else {
			/* Silently fall back to sector size 512. We'll likely
			 * not be able to mount this volume anyway. */
			xpl_debug("Unable to read a sane block size value from "
				"the boot sector. Falling back to 512.");
			blocksize = 512;
		}
	}
	else if (ioctl(diskFd, DKIOCGETBLOCKSIZE, &blocksize)) {
		err = (err = errno) ? err : EIO;
		xpl_perror(errno, "Error while getting block size for %s "
			"device",
			((st.st_mode & S_IFMT) == S_IFBLK) ? "block" :
			"character");
		return err;
	}
	else if (blocksize & (blocksize - 1)) {
		xpl_error("Unsupported block size: %" PRIu32, blocksize);
		return ENOTSUP;
	}
	else {
		xpl_debug("Got block size from device: %lu",
			(unsigned long)blocksize);
	}

	err = xpl_vfs_mount_alloc_init(
		/* mount_t *mp */
		&mp,
		/* int fd */
		diskFd,
		/* uint32_t blocksize */
		blocksize,
		/* vnode_t *out_devvp */
		&devvp);
	if (err) {
		xpl_perror(err, "Error while initializing mount data");
		return err;
	}

	memset(&mount_data, 0, sizeof(mount_data));
	memset(&vfscontext, 0, sizeof(vfscontext));

	mount_data.major_ver = 1;
	mount_data.minor_ver = 0;
	mount_data.flags =
		/* NTFS_MNT_OPT_CASE_SENSITIVE */ const_cpu_to_le32(0x00000001);

	err = fstable.vfe_vfsops->vfs_mount(
		/* struct mount *mp */
		mp,
		/* vnode_t devvp */
		devvp,
		/* user_addr_t data */
		(user_addr_t)(uintptr_t)&mount_data,
		/* vfs_context_t context */
		&vfscontext);
	if (err) {
        os_log_debug(OS_LOG_DEFAULT, "ntfs:mount:finish:vfsop_mount:err:%d", err);
		xpl_perror(err, "Error from mount vfsop");
		goto out;
	}

	mounted = 1;

	err = fstable.vfe_vfsops->vfs_root(
		/* struct mount *mp */
		mp,
		/* struct vnode **vpp */
		&root_vnode,
		/* vfs_context_t context */
		&vfscontext);
	if (err) {
        os_log_debug(OS_LOG_DEFAULT, "ntfs:mount:finish:vfsop_root:err:%d", err);
		xpl_perror(err, "Error from vfs_root");
		goto out;
	}

	*outRootFileNode = root_vnode;
out:
	if (err) {
		if (mounted) {
			err = fstable.vfe_vfsops->vfs_unmount(
				/* struct mount *mp */
				mp,
				/* int mntflags */
				MNT_FORCE,
				/* vfs_context_t context */
				&vfscontext);
			if (err) {
				xpl_perror(err, "Error from unmount vfsop");
				goto out;
			}
		}

		if (mp) {
			xpl_vfs_mount_teardown(mp);
		}
	}

	os_log_debug(OS_LOG_DEFAULT, "ntfs:mount:finish:finish:%d", err);
	return err;
}

static int
plugin_fsops_unmount(UVFSFileNode rootFileNode,
                     UVFSUnmountHint hint)
{
	int err;
	vnode_t root_vfsnode;
	mount_t mp;
	struct vfs_context vfscontext;

	root_vfsnode = (vnode_t)rootFileNode;
	mp = root_vfsnode->mp;
	memset(&vfscontext, 0, sizeof(vfscontext));

	err = vnode_put(root_vfsnode);
	if (err) {
		xpl_perror(err, "Error while invoking vnode_put");
		goto out;
	}

	err = fstable.vfe_vfsops->vfs_unmount(
		/* struct mount *mp */
		mp,
		/* int mntflags */
		(hint & UVFSUnmountHintForce) ? MNT_FORCE : 0,
		/* vfs_context_t context */
		&vfscontext);
	if (!err) {
		xpl_vfs_mount_teardown(mp);
	}
out:
	return err;
}

static int
plugin_fsops_sync(__unused UVFSFileNode node)
{
	//Since for now it's RO filesystem, return 0, in theory EROFS should be return, but we've hit problems with that.
	return 0;
}

static int
plugin_fsops_getattr(UVFSFileNode Node,
                     UVFSFileAttributes *outAttrs)
{
	const vnode_t vnode = (vnode_t)Node;
	int err;
	struct vnop_getattr_args args;
	struct vnode_attr attr;

	memset(&attr, 0, sizeof(attr));
	VATTR_SET_ACTIVE(&attr, va_type);
	VATTR_SET_ACTIVE(&attr, va_mode);
	VATTR_SET_ACTIVE(&attr, va_nlink);
	VATTR_SET_ACTIVE(&attr, va_uid);
	VATTR_SET_ACTIVE(&attr, va_gid);
	VATTR_SET_ACTIVE(&attr, va_flags);
	VATTR_SET_ACTIVE(&attr, va_data_size); // Or va_total_size?
	VATTR_SET_ACTIVE(&attr, va_data_alloc); // Or va_total_alloc?
	VATTR_SET_ACTIVE(&attr, va_fileid);
	VATTR_SET_ACTIVE(&attr, va_parentid);
	VATTR_SET_ACTIVE(&attr, va_access_time);
	VATTR_SET_ACTIVE(&attr, va_modify_time);
	VATTR_SET_ACTIVE(&attr, va_change_time);
	VATTR_SET_ACTIVE(&attr, va_create_time);
	VATTR_SET_ACTIVE(&attr, va_backup_time);
	// TODO: added time is not supported by the filesystem.

	memset(&args, 0, sizeof(args));
	args.a_vp = vnode;
	args.a_vap = &attr;

	err = xpl_vnops->vnop_getattr(&args);
	if (err) {
		xpl_perror(err, "Error while invoking getattr vnop");
		goto out;
	}

	memset(outAttrs, 0, sizeof(*outAttrs));

	outAttrs->fa_validmask |= UVFS_FA_VALID_TYPE;
	if (VATTR_IS_SUPPORTED(&attr, va_type)) {
		outAttrs->fa_type = attr.va_type;
	}
	else {
		outAttrs->fa_type = args.a_vp->vtype;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_mode)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_MODE;
		outAttrs->fa_mode = attr.va_mode;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_nlink) && attr.va_nlink <= UINT32_MAX)
	{
		/* TODO: Is this rejection of attribute when over 2^32
		 * acceptable or should we suggest changes to the userspace VFS
		 * API? I don't see why a file couldn't have more than 2^32 hard
		 * links. */
		outAttrs->fa_validmask |= UVFS_FA_VALID_NLINK;
		outAttrs->fa_nlink = (uint32_t) attr.va_nlink;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_uid)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_UID;
		outAttrs->fa_uid =
			(vfs_flags(vnode->mp) & MNT_IGNORE_OWNERSHIP) ? 99 :
			attr.va_uid;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_gid)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_GID;
		outAttrs->fa_gid =
			(vfs_flags(vnode_mount(vnode)) & MNT_IGNORE_OWNERSHIP) ? 99 :
			attr.va_gid;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_flags)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_BSD_FLAGS;
		outAttrs->fa_bsd_flags = attr.va_flags;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_data_size)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_SIZE;
		outAttrs->fa_size = attr.va_data_size;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_data_alloc)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_ALLOCSIZE;
		outAttrs->fa_allocsize = attr.va_data_alloc;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_fileid)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_FILEID;
		outAttrs->fa_fileid = attr.va_fileid;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_parentid)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_PARENTID;
		outAttrs->fa_parentid = attr.va_parentid;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_access_time)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_ATIME;
		outAttrs->fa_atime = attr.va_access_time;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_modify_time)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_MTIME;
		outAttrs->fa_mtime = attr.va_modify_time;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_change_time)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_CTIME;
		outAttrs->fa_ctime = attr.va_change_time;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_create_time)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_BIRTHTIME;
		outAttrs->fa_birthtime = attr.va_create_time;
	}
	if (VATTR_IS_SUPPORTED(&attr, va_backup_time)) {
		outAttrs->fa_validmask |= UVFS_FA_VALID_BACKUPTIME;
		outAttrs->fa_backuptime = attr.va_backup_time;
	}
out:
	return err;
}

static int
plugin_fsops_setattr(__unused UVFSFileNode Node,
                     __unused const UVFSFileAttributes *attrs,
                     __unused UVFSFileAttributes *outAttrs)
{
    return EROFS;
}

static int
plugin_fsops_lookup(UVFSFileNode dirNode,
                    const char *name,
                    UVFSFileNode *outNode)
{
	int err;
	struct vnop_lookup_args args;
	struct componentname cn;
	vnode_t child_node = NULL;

	memset(&cn, 0, sizeof(cn));
	cn.cn_nameiop = LOOKUP;
	cn.cn_flags = ISLASTCN;
	cn.cn_nameptr = (char*)name; // TODO: Do we really need to duplicate this string?
	cn.cn_namelen = (int)strlen(name); // TODO: Range check?
	cn.cn_hash = 0; // ?

	memset(&args, 0, sizeof(args));
	args.a_desc = &vnop_lookup_desc;
	args.a_dvp = (vnode_t)dirNode;
	args.a_vpp = &child_node;
	args.a_cnp = &cn;

	err = xpl_vnops->vnop_lookup(&args);
	if (err == ENOENT); // Perfectly accepted and reasonable.
	else if (err) {
		xpl_perror(err, "Error while invoking lookup vnop");
		goto out;
	}
	else {
		*outNode = child_node;
	}
out:
	return err;
}

static int
plugin_fsops_reclaim(UVFSFileNode Node)
{
	int err;

	err = vnode_put((vnode_t)Node);
	if (err) {
		xpl_perror(err, "Error while invoking vnode_put");
		goto out;
	}
out:
	return err;
}

static int
plugin_fsops_readlink(UVFSFileNode Node,
                      void *outBuf,
                      size_t bufsize,
                      size_t *actuallyRead,
                      UVFSFileAttributes *outAttrs)
{
	int err;
	struct uio *uio = NULL;
	struct vnop_readlink_args readlink_args;

	uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
	if (!uio) {
		xpl_error("Error while creating uio.");
		err = ENOMEM;
		goto out;
	}

	err = uio_addiov(uio, (uintptr_t)outBuf, bufsize);
	if (err) {
		xpl_perror(err, "Error from uio_addiov");
		goto out;
	}

	memset(&readlink_args, 0, sizeof(readlink_args));
	readlink_args.a_vp = (vnode_t)Node;
	readlink_args.a_uio = uio;

	err = xpl_vnops->vnop_readlink(&readlink_args);
	if (err) {
		xpl_perror(err, "Error while invoking readlink vnop");
		goto out;
	}

	if (outAttrs) {
		err = plugin_fsops_getattr(Node, outAttrs);
		if (err) {
			goto out;
		}
	}

	*actuallyRead = bufsize - uio_resid(uio);
out:
	if (uio) {
		uio_free(uio);
	}

	return err;
}

static int
plugin_fsops_read(UVFSFileNode Node,
                  uint64_t offset,
                  size_t length,
                  void *outBuf,
                  size_t *actuallyRead)
{
	int err;
	struct vnop_read_args args;
	struct uio *uio = NULL;

	uio = uio_create(1, offset, UIO_USERSPACE64, UIO_READ);
	if (!uio) {
		xpl_error("Error while creating uio.");
		err = ENOMEM;
		goto out;
	}

	err = uio_addiov(uio, (uintptr_t)outBuf, length);
	if (err) {
		xpl_perror(err, "Error from uio_addiov");
		goto out;
	}

	memset(&args, 0, sizeof(args));
	args.a_vp = (vnode_t)Node;
	args.a_uio = uio;
	args.a_ioflag = 0;

	err = xpl_vnops->vnop_read(&args);
	if (err) {
		xpl_perror(err, "Error while invoking read vnop");
		goto out;
	}

	*actuallyRead = length - uio_resid(uio);
out:
	if (uio) {
		uio_free(uio);
	}

	return err;
}

static int
plugin_fsops_write(__unused UVFSFileNode Node,
                   __unused uint64_t offset,
                   __unused size_t length,
                   __unused const void *buf,
                   __unused size_t *actuallyWritten)
{
    return EROFS;
}

static int
plugin_fsops_create(__unused UVFSFileNode dirNode,
                    __unused const char *name,
                    __unused const UVFSFileAttributes *attrs,
                    __unused UVFSFileNode *outNode)
{
    return EROFS;
}

static int
plugin_fsops_mkdir(__unused UVFSFileNode dirNode,
                   __unused const char *name,
                   __unused const UVFSFileAttributes *attrs,
                   __unused UVFSFileNode *outNode)
{
    return EROFS;
}

static int
plugin_fsops_symlink(__unused UVFSFileNode dirNode,
                     __unused const char *name,
                     __unused const char *contents,
                     __unused const UVFSFileAttributes *attrs,
                     __unused UVFSFileNode *outNode)
{
    return EROFS;
}

static int
plugin_fsops_link(__unused UVFSFileNode fromNode,
                  __unused UVFSFileNode toDirNode,
                  __unused const char *toName,
                  __unused UVFSFileAttributes *outFileAttrs,
                  __unused UVFSFileAttributes *outDirAttrs)
{
    return EROFS;
}

static int
plugin_fsops_rename(__unused UVFSFileNode fromDirNode,
                    __unused UVFSFileNode fromNode,
                    __unused const char *fromName,
                    __unused UVFSFileNode toDirNode,
                    __unused UVFSFileNode toNode,
                    __unused const char *toName,
                    __unused uint32_t flags)
{
    return EROFS;
}

static int
plugin_fsops_remove(__unused UVFSFileNode dirNode,
                    __unused const char *name,
                    __unused UVFSFileNode victimNode)
{
    return EROFS;
}

static int
plugin_fsops_rmdir(__unused UVFSFileNode dirNode,
                   __unused const char *name)
{
    return EROFS;
}

static int
plugin_fsops_getfsattr(UVFSFileNode Node,
                       const char *attr,
                       LIFSAttributeValue *val,
                       size_t len,
                       size_t *retlen)
{
	int err = 0;
	vnode_t vfsnode = NULL;
	struct vfs_attr vfsattr;
	struct vfs_context vfscontext;
	int pathconf_name = 0;

	xpl_trace_enter("Node=%p attr=\"%s\" val=%p len=%zu retlen=%p",
		Node, attr, val, len, retlen);

	memset(&vfsattr, 0, sizeof(vfsattr));
	memset(&vfscontext, 0, sizeof(vfscontext));

	if (!strcmp(LI_FSATTR_PC_LINK_MAX, attr)) {
		pathconf_name = _PC_LINK_MAX;
	}
	else if (!strcmp(LI_FSATTR_PC_NAME_MAX, attr)) {
		pathconf_name = _PC_NAME_MAX;
	}
	else if (!strcmp(LI_FSATTR_PC_NO_TRUNC, attr)) {
		pathconf_name = _PC_NO_TRUNC;
	}
	else if (!strcmp(LI_FSATTR_PC_FILESIZEBITS, attr)) {
		pathconf_name = _PC_FILESIZEBITS;
	}
	else if (!strcmp(LI_FSATTR_PC_XATTR_SIZE_BITS, attr)) {
		pathconf_name = _PC_XATTR_SIZE_BITS;
	}
	else if (!strcmp(LI_FSATTR_BLOCKSIZE, attr)) {
		VFSATTR_WANTED(&vfsattr, f_bsize);
	}
	else if (!strcmp(LI_FSATTR_IOSIZE, attr)) {
		VFSATTR_WANTED(&vfsattr, f_iosize);
	}
	else if (!strcmp(LI_FSATTR_TOTALBLOCKS, attr)) {
		VFSATTR_WANTED(&vfsattr, f_blocks);
	}
	else if (!strcmp(LI_FSATTR_BLOCKSFREE, attr)) {
		VFSATTR_WANTED(&vfsattr, f_bfree);
	}
	else if (!strcmp(LI_FSATTR_BLOCKSAVAIL, attr)) {
		VFSATTR_WANTED(&vfsattr, f_bavail);
	}
	else if (!strcmp(LI_FSATTR_BLOCKSUSED, attr)) {
		VFSATTR_WANTED(&vfsattr, f_bused);
	}
	else if (!strcmp(LI_FSATTR_CNAME, attr)) {
		// TODO: What is this attribute?
		xpl_error("Unsupported filesystem attribute CNAME.");
		err = ENOTSUP;
		goto out;
	}
	else if (!strcmp(LI_FSATTR_FSTYPENAME, attr)) {
		/* TODO: Assuming that fstypename is the type name displayed in
		 * the mount list. So it should correspond to vfe_fsname and no
		 * filesystem query is needed. */
		const size_t fsname_length = strlen(fstable.vfe_fsname);

		*retlen = fsname_length + 1;
		if (len < fsname_length + 1) {
			err = E2BIG;
			goto out;
		}

		memcpy(&val->fsa_string, fstable.vfe_fsname, fsname_length);
		val->fsa_string[fsname_length] = '\0';
		goto out;
	}
	else if (!strcmp(LI_FSATTR_FSSUBTYPE, attr)) {
		VFSATTR_WANTED(&vfsattr, f_fssubtype);
	}
	else if (!strcmp(LI_FSATTR_FSLOCATION, attr)) {
		/* TODO: Don't know what this is supposed to be. */
		xpl_error("Unsupported filesystem attribute FSLOCATION.");
		err = ENOTSUP;
		goto out;
	}
	else if (!strcmp(LI_FSATTR_VOLNAME, attr)) {
		char *vol_name_buffer;

		vol_name_buffer = malloc(MAXPATHLEN);
		if (!vol_name_buffer) {
			xpl_error("Error while allocating temporary volume "
				"name buffer");
			err = (err = errno) ? err : ENOMEM;
			goto out;
		}

		vfsattr.f_vol_name = vol_name_buffer;

		VFSATTR_WANTED(&vfsattr, f_vol_name);
	}
	else if (!strcmp(LI_FSATTR_VOLUUID, attr)) {
		VFSATTR_WANTED(&vfsattr, f_uuid);
	}
	else if (!strcmp(LI_FSATTR_HAS_PERM_ENFORCEMENT, attr)) {
		/* No permissions supported for NTFS.
		 * TODO: Should we still query this with getattrlist? */
		*retlen = sizeof(val->fsa_bool);
		if (len < sizeof(val->fsa_number)) {
			err = E2BIG;
			goto out;
		}

		val->fsa_bool = FALSE;
		goto out;
	}
	else if (!strcmp(LI_FSATTR_HAS_ACCESS_CHECK, attr)) {
		/* No access checks.
		 * TODO: Should we still query this with getattrlist? */
		*retlen = sizeof(val->fsa_bool);
		if (len < sizeof(val->fsa_number)) {
			err = E2BIG;
			goto out;
		}

		val->fsa_bool = FALSE;
		goto out;
	}
	else if (!strcmp(LI_FSATTR_CAPS_FORMAT, attr)) {
		VFSATTR_WANTED(&vfsattr, f_capabilities);
	}
	else if (!strcmp(LI_FSATTR_CAPS_INTERFACES, attr)) {
		VFSATTR_WANTED(&vfsattr, f_capabilities);
	}
	else if (!strcmp(LI_FSATTR_MOUNT_TIME, attr)) {
		val->fsa_number =
			xpl_vfs_mount_get_mount_time(((vnode_t)Node)->mp);
		goto out;
	}
	else if (!strcmp(LI_FSATTR_LAST_MTIME, attr)) {
		/* TODO: We currently don't keep track of this. Could we? */
		xpl_error("Unsupported filesystem attribute LAST_MTIME.");
		err = ENOTSUP;
		goto out;
	}
	else if (!strcmp(LI_FSATTR_MOUNTFLAGS, attr)) {
		/* This implementation is read-only. */
		val->fsa_number = LI_MNT_RDONLY;
		goto out;
	}
	else {
		xpl_error("Unrecognized filesystem attribute \"%s\".", attr);
		err = ENOTSUP;
		goto out;
	}

	if (pathconf_name) {
		struct vnop_pathconf_args args;
		int32_t retval = 0;

		memset(&args, 0, sizeof(args));
		args.a_vp = Node;
		args.a_name = pathconf_name;
		args.a_retval = &retval;

		err = xpl_vnops->vnop_pathconf(&args);
		if (err) {
			xpl_perror(err, "Error from pathconf vnop");
			goto out;
		}

		*retlen = sizeof(val->fsa_number);
		if (len < sizeof(val->fsa_number)) {
			err = E2BIG;
			goto out;
		}

		val->fsa_number = retval;
	}
	else {
		uint64_t f_active;

		/* Save a copy of f_active to check that it doesn't get modified behind
		 * our backs. We use it to keep track of what to return and rely on only
		 * one attribute being requested at a time. If this changes behind our
		 * backs we return EINVAL. The filesystem is not expected to act this
		 * way. */
		f_active = vfsattr.f_active;

		vfsnode = (vnode_t)Node;

		err = fstable.vfe_vfsops->vfs_getattr(
			/* struct mount *mp */
			vfsnode->mp,
			/* struct vfs_attr *attr */
			&vfsattr,
			/* vfs_context_t context */
			&vfscontext);
		if (err) {
			xpl_perror(err, "Error from getattr vfsop");
			goto out;
		}

		switch (f_active) {
		case VFSATTR_f_bsize:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_bsize)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_bsize;
			break;
		case VFSATTR_f_iosize:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_iosize)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_iosize;
			break;
		case VFSATTR_f_blocks:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_blocks)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_blocks;
			break;
		case VFSATTR_f_bfree:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_bfree)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_bfree;
			break;
		case VFSATTR_f_bavail:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_bavail)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_bavail;
			break;
		case VFSATTR_f_bused:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_bused)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_bused;
			break;
		case VFSATTR_f_fssubtype:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_fssubtype)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			val->fsa_number = vfsattr.f_fssubtype;
			break;
		case VFSATTR_f_vol_name: {
			size_t vol_name_length;

			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_vol_name)) {
				err = ENOTSUP;
				goto out;
			}

			vol_name_length = strlen(vfsattr.f_vol_name);
			*retlen = vol_name_length + 1;
			if (len < vol_name_length + 1) {
				err = E2BIG;
				goto out;
			}

			memcpy(val->fsa_string, vfsattr.f_vol_name, vol_name_length);
			val->fsa_string[vol_name_length] = '\0';
			break;
		}
		case VFSATTR_f_uuid:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_uuid)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(uuid_t);
			if (len < sizeof(uuid_t)) {
				err = E2BIG;
				goto out;
			}

			memcpy(val->fsa_opaque, vfsattr.f_uuid, sizeof(uuid_t));
			break;
		case VFSATTR_f_capabilities:
			if (!VFSATTR_IS_SUPPORTED(&vfsattr, f_capabilities)) {
				err = ENOTSUP;
				goto out;
			}

			*retlen = sizeof(val->fsa_number);
			if (len < sizeof(val->fsa_number)) {
				err = E2BIG;
				goto out;
			}

			/* In this case the request is one of LI_FSATTR_CAPS_FORMAT or
			 * LI_FSATTR_CAPS_INTERFACES. We must check again. */
			if (!strcmp(LI_FSATTR_CAPS_FORMAT, attr)) {
				val->fsa_number =
					vfsattr.f_capabilities.
					capabilities[VOL_CAPABILITIES_FORMAT];
			}
			else {
				val->fsa_number =
					vfsattr.f_capabilities.
					capabilities[VOL_CAPABILITIES_INTERFACES];
			}
			break;
		default:
			xpl_error("Unexpected f_active value: 0x%" PRIX64, f_active);
			err = EINVAL;
			goto out;
		}
	}
out:
	if (vfsattr.f_vol_name) {
		free(vfsattr.f_vol_name);
	}

	return err;
}

static int
plugin_fsops_setfsattr(__unused UVFSFileNode Node,
                       __unused const char *attr,
                       __unused const LIFSAttributeValue *val,
                       __unused size_t len,
                       __unused LIFSAttributeValue *out_value,
                       __unused size_t out_len)
{
    return EROFS;
}

static uint8_t
dt_to_li_fa_type(int dt_type)
{
	switch (dt_type) {
	case DT_FIFO:		return LI_FA_TYPE_FIFO;
	case DT_CHR:		return LI_FA_TYPE_CHAR;
	case DT_DIR:		return LI_FA_TYPE_DIR;
	case DT_BLK:		return LI_FA_TYPE_BLOCK;
	case DT_REG:		return LI_FA_TYPE_FILE;
	case DT_LNK:		return LI_FA_TYPE_SYMLINK;
	case DT_SOCK:		return LI_FA_TYPE_SOCKET;
	case DT_WHT:		/* FALLTHROUGH */
	case DT_UNKNOWN:	/* FALLTHROUGH */
	default:			return LI_FA_TYPE_UNKNOWN;
	}
}

static int
plugin_fsops_readdir(UVFSFileNode dirNode,
                     void *buf,
                     size_t buflen,
                     uint64_t cookie,
                     size_t *readBytes,
                     uint64_t *verifier)
{
	int err = 0;
	char *tmpbuf = NULL;
	size_t tmpbuflen = 0;
	struct uio *uio = NULL;
	struct vnop_readdir_args args;
	int eof = 0;
	uint64_t numdirent = 0;
	uint64_t entrycount = 0;
	size_t i = tmpbuflen;
	size_t buf_i = 0;
	LIDirEntry_t *prev_entry = NULL;

	if (cookie == LI_DIRCOOKIE_EOF) {
		return LI_READDIR_EOF_REACHED;
	}

	uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
	if (!uio) {
		xpl_error("Error while creating uio.");
		err = ENOMEM;
		goto out;
	}

	while (buf_i < buflen) {
		struct dirent *readdir_entry = (struct dirent*)&tmpbuf[i];
		LIDirEntry_t *const out_entry =
			(LIDirEntry_t*)&((u8*)buf)[buf_i];

		size_t buf_fill_size;

		if (!eof && entrycount >= numdirent) {
			const size_t newtmpbuflen =
				tmpbuflen ? tmpbuflen * 2 : buflen;
			char *newtmpbuf;
			int cur_numdirent = 0;

			newtmpbuf = realloc(tmpbuf, newtmpbuflen);
			if (!newtmpbuf) {
				err = (err = errno) ? err : ENOMEM;
				xpl_perror(errno, "Error while %s temporary "
					"buffer",
					newtmpbuf ? "expanding" : "allocating");
				goto out;
			}

			tmpbuf = newtmpbuf;
			tmpbuflen = newtmpbuflen;

			uio_reset(uio, cookie, UIO_USERSPACE64, UIO_READ);
			err = uio_addiov(uio, (uintptr_t)&tmpbuf[i],
				tmpbuflen - i);
			if (err) {
				xpl_perror(err, "Error from uio_addiov");
				goto out;
			}

			memset(&args, 0, sizeof(args));
			args.a_vp = (vnode_t)dirNode;
			args.a_uio = uio;
			args.a_flags = 0;
			args.a_eofflag = &eof;
			args.a_numdirent = &cur_numdirent;

			err = xpl_vnops->vnop_readdir(&args);
			if (err) {
				xpl_perror(err, "Error while invoking readdir "
					"vnop");
				goto out;
			}
			else if (cur_numdirent < 0) {
				xpl_error("Unexpected number of dirents "
					"returned from readdir vnop: %d",
					cur_numdirent);
				err = EIO;
				goto out;
			}

			numdirent += (uint64_t)cur_numdirent;
			readdir_entry = (struct dirent*)&tmpbuf[i];
		}

		if (eof && entrycount >= numdirent) {
			if (prev_entry) {
				prev_entry->de_nextrec = 0;
				prev_entry->de_nextcookie = LI_DIRCOOKIE_EOF;
			}

			break;
		}

		if (!readdir_entry->d_namlen ||
			readdir_entry->d_reclen <
			offsetof(struct dirent, d_name) + 1)
		{
			xpl_error("Invalid entry %" PRIu64 " returned from "
				"readdir vnop: reclen=%" PRIu16 " "
				"namlen=%" PRIu16,
				entrycount, readdir_entry->d_reclen,
				readdir_entry->d_namlen);
			err = EIO;
			goto out;
		}

		if (readdir_entry->d_seekoff <= cookie) {
			/* Skip over any entries preceding our cookie. */
			i += readdir_entry->d_reclen;
			continue;
		}

		++entrycount;

		buf_fill_size = readdir_entry->d_reclen;
		if (buf_fill_size > buflen - buf_i) {
			prev_entry->de_nextrec = 0;
			break;
		}

		out_entry->de_fileid = readdir_entry->d_ino;
		out_entry->de_nextcookie = readdir_entry->d_seekoff;
		out_entry->de_nextrec = buf_fill_size;
		out_entry->de_namelen = readdir_entry->d_namlen;
		out_entry->de_filetype = dt_to_li_fa_type(readdir_entry->d_type);
		memcpy(out_entry->de_name,
			readdir_entry->d_name,
			readdir_entry->d_namlen + 1);

		buf_i += buf_fill_size;
		i += readdir_entry->d_reclen;
		cookie = readdir_entry->d_seekoff;

		if (!readdir_entry->d_reclen && eof) {
			out_entry->de_nextrec = 0;
			out_entry->de_nextcookie = LI_DIRCOOKIE_EOF;
			break;
		}
		else if (!readdir_entry->d_reclen) {
			/* Out of entries in tmpbuf. Trigger vfs readdir. */
			i = buflen;
		}

		prev_entry = out_entry;
	}

	*readBytes = buf_i;
	/* We are read-only meaning the directory contents will not change, so
	 * we just set a placeholder verifier for now. */
	*verifier = 1;
out:
	if (uio) {
		uio_free(uio);
	}

	if (tmpbuf) {
		free(tmpbuf);
	}

	return err;
}

static int
plugin_fsops_readdirattr(UVFSFileNode dirNode,
                        void *buf,
                        size_t buflen,
                        uint64_t cookie,
                        size_t *readBytes,
                        uint64_t *verifier)
{
	int err = 0;
	char *tmpbuf = NULL;
	size_t tmpbuflen = 0;
	struct uio *uio = NULL;
	struct vnop_readdir_args args;
	int eof = 0;
	uint64_t numdirent = 0;
	uint64_t entrycount = 0;
	size_t i = tmpbuflen;
	size_t buf_i = 0;
	LIDirEntryAttr_t *prev_entry = NULL;

	if (cookie == LI_DIRCOOKIE_EOF) {
		return LI_READDIR_EOF_REACHED;
	}

	uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
	if (!uio) {
		xpl_error("Error while creating uio.");
		err = ENOMEM;
		goto out;
	}

	while (buf_i < buflen) {
		struct dirent *readdir_entry = (struct dirent*)&tmpbuf[i];
		LIDirEntryAttr_t *const out_entry =
			(LIDirEntryAttr_t*)&((u8*)buf)[buf_i];

		UVFSFileNode child_node = NULL;
		int reclaim_err;
		size_t buf_fill_size;

		if (!eof && entrycount >= numdirent) {
			const size_t newtmpbuflen =
				tmpbuflen ? tmpbuflen * 2 : buflen;
			char *newtmpbuf;
			int cur_numdirent = 0;

			newtmpbuf = realloc(tmpbuf, newtmpbuflen);
			if (!newtmpbuf) {
				err = (err = errno) ? err : ENOMEM;
				xpl_perror(errno, "Error while %s temporary "
					"buffer",
					newtmpbuf ? "expanding" : "allocating");
				goto out;
			}

			tmpbuf = newtmpbuf;
			tmpbuflen = newtmpbuflen;

			uio_reset(uio, cookie, UIO_USERSPACE64, UIO_READ);
			err = uio_addiov(uio, (uintptr_t)&tmpbuf[i],
				tmpbuflen - i);
			if (err) {
				xpl_perror(err, "Error from uio_addiov");
				goto out;
			}

			memset(&args, 0, sizeof(args));
			args.a_vp = (vnode_t)dirNode;
			args.a_uio = uio;
			args.a_flags = 0;
			args.a_eofflag = &eof;
			args.a_numdirent = &cur_numdirent;

			err = xpl_vnops->vnop_readdir(&args);
			if (err) {
				xpl_perror(err, "Error while invoking readdir "
					"vnop");
				goto out;
			}
			else if (cur_numdirent < 0) {
				xpl_error("Unexpected number of dirents "
					"returned from readdir vnop: %d",
					cur_numdirent);
				err = EIO;
				goto out;
			}

			numdirent += (uint64_t)cur_numdirent;
			readdir_entry = (struct dirent*)&tmpbuf[i];
		}

		if (eof && entrycount >= numdirent) {
			if (prev_entry) {
				prev_entry->dea_nextrec = 0;
				prev_entry->dea_nextcookie = LI_DIRCOOKIE_EOF;
			}

			break;
		}

		if (!readdir_entry->d_namlen ||
			readdir_entry->d_reclen <
			offsetof(struct dirent, d_name) + 1)
		{
			xpl_error("Invalid entry %" PRIu64 " returned from "
				"readdir vnop: reclen=%" PRIu16 " "
				"namlen=%" PRIu16,
				entrycount, readdir_entry->d_reclen,
				readdir_entry->d_namlen);
			err = EIO;
			goto out;
		}

		if (readdir_entry->d_seekoff <= cookie) {
			/* Skip over any entries preceding our cookie. */
			i += readdir_entry->d_reclen;
			continue;
		}

		++entrycount;

		if ((readdir_entry->d_namlen == 1 &&
			readdir_entry->d_name[0] == '.') ||
			(readdir_entry->d_namlen == 2 &&
			readdir_entry->d_name[0] == '.' &&
			readdir_entry->d_name[1] == '.'))
		{
			/* Skip over the '.' and '..' entries as they aren't
			 * supposed to be returned from readdirattr. */
			i += readdir_entry->d_reclen;
			cookie = readdir_entry->d_seekoff;
			continue;
		}

		buf_fill_size =
			_LI_DIRENTRYATTR_RECLEN(LI_DIRENTRYATTR_NAMEOFF, 
			readdir_entry->d_namlen);
		if (buf_fill_size > buflen - buf_i) {
			prev_entry->dea_nextrec = 0;
			break;
		}

		out_entry->dea_nextcookie = readdir_entry->d_seekoff;
		out_entry->dea_nextrec = buf_fill_size;
		out_entry->dea_nameoff = LI_DIRENTRYATTR_NAMEOFF;
		out_entry->dea_namelen = readdir_entry->d_namlen;
		out_entry->dea_spare0 = 0;
		memcpy(out_entry->_dea_name_placeholder_,
			readdir_entry->d_name,
			readdir_entry->d_namlen + 1);

		err = plugin_fsops_lookup(dirNode, readdir_entry->d_name,
			&child_node);
		if (err) {
			xpl_perror(err, "Error during lookup");
			err = 0;
			buf_fill_size = 0;
		}
		else {
			UVFSFileAttributes child_attrs;

			err = plugin_fsops_getattr(child_node, &child_attrs);
			reclaim_err = plugin_fsops_reclaim(child_node);
			if (err) {
				xpl_perror(err, "Error during getattr");
			}

			out_entry->dea_attrs = child_attrs;

			if (reclaim_err) {
				xpl_perror(reclaim_err, "Error during reclaim");
				if (!err) {
					err = reclaim_err;
				}
			}
		}

		if (err) {
			goto out;
		}

		buf_i += buf_fill_size;
		i += readdir_entry->d_reclen;
		cookie = readdir_entry->d_seekoff;

		if (!readdir_entry->d_reclen && eof) {
			out_entry->dea_nextrec = 0;
			out_entry->dea_nextcookie = LI_DIRCOOKIE_EOF;
			break;
		}
		else if (!readdir_entry->d_reclen) {
			/* Out of entries in tmpbuf. Trigger vfs readdir. */
			i = buflen;
		}

		prev_entry = out_entry;
	}

	*readBytes = buf_i;
	/* We are read-only meaning the directory contents will not change, so
	 * we just set a placeholder verifier for now. */
	*verifier = 1;
out:
	if (uio) {
		uio_free(uio);
	}

	if (tmpbuf) {
		free(tmpbuf);
	}

	return err;
}

static int
plugin_fsops_getxattr(UVFSFileNode node,
                      const char *attr,
                      void *buf,
                      size_t bufsize,
                      size_t *actual_size)
{
	int err;
	struct uio *uio = NULL;
	struct vnop_getxattr_args args;

	if (!strcmp("com.apple.ResourceFork", attr)) {
		/* As per the documentation in UserVFS.h, we reject attempts to
		 * access stream-type xattrs through this interface with
		 * ENOATTR. Currently there is no replacement for this
		 * functionality. */
		err = ENOATTR;
		goto out;
	}

	if (buf && bufsize) {
		uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
		if (!uio) {
			xpl_error("Error while creating uio.");
			err = ENOMEM;
			goto out;
		}

		err = uio_addiov(uio, (uintptr_t)buf, bufsize);
		if (err) {
			xpl_perror(err, "Error from uio_addiov");
			goto out;
		}
	}
	else if (!actual_size) {
		xpl_error("Invalid argument: No buffer and no actual size "
				"pointer.");
		err = EINVAL;
		goto out;
	}

	memset(&args, 0, sizeof(args));
	args.a_vp = (vnode_t)node;
	args.a_name = attr;
	args.a_uio = uio;
	args.a_size = uio ? NULL : actual_size;
	args.a_options = 0;

	err = xpl_vnops->vnop_getxattr(&args);
	if (err == ENOATTR) {
		/* Quite normal, no need to log in non-debug mode. */
		xpl_debug("No such attribute: \"%s\"", attr);
		goto out;
	}
	else if (err) {
		xpl_perror(err, "Error while invoking getxattr vnop");
		goto out;
	}

	if (uio && actual_size) {
		*actual_size = bufsize - uio_resid(uio);
	}

	/* Otherwise the getxattr vnop should have filled it with the right
	 * value. */
out:
	if (uio) {
		uio_free(uio);
	}

	return err;
}

static int
plugin_fsops_setxattr(__unused UVFSFileNode node,
                      __unused const char *attr,
                      __unused const void *buf,
                      __unused size_t bufsize,
                      __unused UVFSXattrHow how)
{
    return EROFS;
}

static int
plugin_fsops_listxattr(UVFSFileNode node,
                       void *buf,
                       size_t bufsize,
                       size_t *actual_size)
{
	int err;
	struct uio *uio = NULL;
	struct vnop_listxattr_args args;

	if (buf && bufsize) {
		uio = uio_create(1, 0, UIO_USERSPACE64, UIO_READ);
		if (!uio) {
			xpl_error("Error while creating uio.");
			err = ENOMEM;
			goto out;
		}

		err = uio_addiov(uio, (uintptr_t)buf, bufsize);
		if (err) {
			xpl_perror(err, "Error from uio_addiov");
			goto out;
		}
	}
	else if (!actual_size) {
		xpl_error("Invalid argument: No buffer and no actual size "
				"pointer.");
		err = EINVAL;
		goto out;
	}

	memset(&args, 0, sizeof(args));
	args.a_vp = (vnode_t)node;
	args.a_uio = uio;
	args.a_size = uio ? NULL : actual_size;

	err = xpl_vnops->vnop_listxattr(&args);
	if (err) {
		xpl_perror(err, "Error while invoking listxattr vnop");
		goto out;
	}

	if (uio && actual_size) {
		*actual_size = bufsize - uio_resid(uio);
	}

	/* Otherwise the listxattr vnop should have filled it with the right
	 * value. */
out:
	if (uio) {
		uio_free(uio);
	}

	return err;
}

static int
plugin_fsops_clonefile(__unused UVFSFileNode sourceNode,
                       __unused UVFSFileNode toDirNode,
                       __unused const char *toName,
                       __unused const LIFileAttributes_t *attrs,
                       __unused uint32_t flags,
                       __unused UVFSFileNode *outNode)
{
    return EROFS;
}

static int
plugin_fsops_scandir(__unused UVFSFileNode dir_node,
                     __unused scandir_matching_request_t *matching_criteria,
                     __unused scandir_matching_reply_t   *matching_result)
{
    return ENOTSUP;
}

static int
plugin_fsops_scanids(__unused UVFSFileNode node,
                     __unused uint64_t requested_attributes,
                     __unused const uint64_t *fileid_array,
                     __unused unsigned int fileid_count,
                     __unused scanids_match_block_t match_callback)
{
    return ENOTSUP;
}

UVFSFSOps pluginops = {
    .fsops_version  = UVFS_FSOPS_VERSION_CURRENT,

    .fsops_init     = plugin_fsops_init,
    .fsops_fini     = plugin_fsops_fini,

    .fsops_taste    = plugin_fsops_taste,
    .fsops_scanvols = plugin_fsops_scanvols,
    .fsops_check    = plugin_fsops_check,
    .fsops_mount    = plugin_fsops_mount,
    .fsops_sync     = plugin_fsops_sync,
    .fsops_unmount  = plugin_fsops_unmount,

    .fsops_getfsattr = plugin_fsops_getfsattr,
    .fsops_setfsattr = plugin_fsops_setfsattr,

    .fsops_getattr      = plugin_fsops_getattr,
    .fsops_setattr      = plugin_fsops_setattr,
    .fsops_lookup       = plugin_fsops_lookup,
    .fsops_reclaim      = plugin_fsops_reclaim,
    .fsops_readlink     = plugin_fsops_readlink,
    .fsops_read         = plugin_fsops_read,
    .fsops_write        = plugin_fsops_write,
    .fsops_create       = plugin_fsops_create,
    .fsops_mkdir        = plugin_fsops_mkdir,
    .fsops_symlink      = plugin_fsops_symlink,
    .fsops_remove       = plugin_fsops_remove,
    .fsops_rmdir        = plugin_fsops_rmdir,
    .fsops_rename       = plugin_fsops_rename,
    .fsops_readdir      = plugin_fsops_readdir,
    .fsops_readdirattr  = plugin_fsops_readdirattr,
    .fsops_link         = plugin_fsops_link,

    .fsops_getxattr     = plugin_fsops_getxattr,
    .fsops_setxattr     = plugin_fsops_setxattr,
    .fsops_listxattr    = plugin_fsops_listxattr,

    .fsops_clonefile    = plugin_fsops_clonefile,

    .fsops_scandir      = plugin_fsops_scandir,
    .fsops_scanids      = plugin_fsops_scanids,
};

void livefiles_plugin_init(UVFSFSOps **ops);

__attribute__((visibility("default")))
void livefiles_plugin_init(UVFSFSOps **ops)
{
    if (ops) {
        *ops = &pluginops;
    }

    return;
}
