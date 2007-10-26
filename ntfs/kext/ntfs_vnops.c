/*
 * ntfs_vnops.c - NTFS kernel vnode operations.
 *
 * Copyright (c) 2006, 2007 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006, 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#include <sys/attr.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
/*
 * struct nameidata is defined in <sys/namei.h>, but it is private so put in a
 * forward declaration for now so that vnode_internal.h can compile.  All we
 * need vnode_internal.h for is the declaration of vnode_getparent(),
 * vnode_getname(), and vnode_putname(),  which are exported in
 * Unsupported.exports.
 */
// #include <sys/namei.h>
struct nameidata;
#include <sys/vnode_internal.h>

#include <string.h>

#include <mach/kern_return.h>
#include <mach/memory_object_types.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include <vfs/vfs_support.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_compress.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_mft.h"
#include "ntfs_mst.h"
#include "ntfs_page.h"
#include "ntfs_unistr.h"
#include "ntfs_vnops.h"
#include "ntfs_volume.h"

/* Global ntfs vnode operations. */
vnop_t **ntfs_vnodeop_p;

/**
 * ntfs_cluster_iodone - complete i/o on a memory region
 * @cbp:	cluster head buffer for which i/o is being completed
 * @arg:	callback argument, we do not use it at present
 *
 * In the read case:
 *
 * For an mst protected attribute we do the post read mst deprotection and for
 * an encrypted attribute we do the decryption (not supported at present).
 * Note we ignore mst fixup errors as those are detected when
 * ntfs_mft_record_map() is called later which gives us per record granularity.
 *
 * In the write case:
 *
 * For an mst protected attribute we do the post write mst deprotection.
 * Writing to encrypted attributes is not supported at present.
 *
 * Return 0 on success and errno on error.
 */
int ntfs_cluster_iodone(buf_t cbp, void *arg __unused)
{
	long size;
	ntfs_inode *ni;
	u8 *kend, *kaddr;
	errno_t err;
	BOOL is_read = buf_flags(cbp) & B_READ;

	ni = NTFS_I(buf_vnode(cbp));
	size = buf_count(cbp);
	if (size & (ni->block_size - 1))
		panic("%s(): Called with size not a multiple of the inode "
				"block size.\n", __FUNCTION__);
	err = buf_map(cbp, (caddr_t*)&kaddr);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map buffer (error %d).",
				err);
		goto err;
	}
	kend = kaddr + size;
	if (NInoMstProtected(ni)) {
		u32 rec_size = ni->block_size;
		/* Do the mst deprotection ignoring errors. */
		do {
			if (is_read)
				(void)ntfs_mst_fixup_post_read(
						(NTFS_RECORD*)kaddr, rec_size);
			else
				ntfs_mst_fixup_post_write((NTFS_RECORD*)kaddr);
		} while ((kaddr += rec_size) < kend);
	} else if (NInoEncrypted(ni)) {
		// TODO: Need to decrypt the encrypted sectors here.  This
		// cannot happen at present as we deny opening/reading/writing/
		// paging encrypted vnodes.
		panic("%s(): Called for encrypted vnode.\n", __FUNCTION__);
	} else
		panic("%s(): Called for normal vnode.\n", __FUNCTION__);
	err = buf_unmap(cbp);
	if (err)
		ntfs_error(ni->vol->mp, "Failed to unmap buffer (error %d).",
				err);
err:
	return err;
}

/**
 * ntfs_vnop_strategy - prepare and issue the i/o described by a buffer
 * @a:		arguments to strategy function
 *
 * @a contains:
 *	buf_t a_bp;	buffer for which to prepare and issue the i/o
 *
 * Prepare and issue the i/o described by the buffer @a->a_bp.  Adapted from
 * buf_strategy().
 *
 * In NTFS, we only ever get called for buffers which have a page list
 * attached.  The page list is mapped and the address of the mapping is stored
 * in (u8*)buf_dataptr(@a->a_bp).
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_strategy(struct vnop_strategy_args *a)
{
	long b_flags;
	buf_t buf = a->a_bp;
	vnode_t vn = buf_vnode(buf);
	ntfs_inode *ni;

	/* Same checks as in buf_strategy(). */
	if (!vn || vnode_ischr(vn) || vnode_isblk(vn))
		panic("%s(): !vn || vnode_ischr(vn) || vnode_isblk(vn)\n",
				__FUNCTION__);
	ni = NTFS_I(vn);
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"logical block 0x%llx.", (unsigned long long)ni->mft_no,
			le32_to_cpu(ni->type), (unsigned)ni->name_len,
			(unsigned long long)buf_lblkno(buf));
	if (S_ISDIR(ni->mode))
		panic("%s(): Called for directory vnode.\n", __FUNCTION__);
	/*
	 * We should never be called for i/o that does not involve a page list.
	 */
	if (!buf_upl(buf))
		panic("%s(): Buffer does not have a page list attached.\n",
				__FUNCTION__);
	b_flags = buf_flags(buf);
	/*
	 * If we are called from cluster_io() then pass the request down to the
	 * underlying device containing the NTFS volume.  We have no KPI way of
	 * doing this directly so we invoke buf_strategy() and rely on the fact
	 * that it does not do anything other than associate the physical
	 * device with the buffer and then pass the buffer down to the device.
	 */
	if (b_flags & B_CLUSTER)
		return buf_strategy(ni->vol->dev_vn, a);
	/*
	 * We never do i/o via file system buffers thus we should never get
	 * here.
	 */
	panic("%s(): Called for non-cluster i/o buffer.\n", __FUNCTION__);
	/*
	 * panic() should be marked as a noreturn function but since it is not
	 * we need to do a return here so the compiler does not complain...
	 */
	return EIO;
}

/**
 * ntfs_vnop_lookup - find a vnode inside an ntfs directory given its name
 * @a:		arguments to lookup function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory vnode in which to search
 *	vnode_t *a_vpp;			destination pointer for the found vnode
 *	struct componentname *a_cnp;	name to find in the directory vnode
 *	vfs_context_t a_context;
 *
 * In short, ntfs_vnop_lookup() looks for the vnode represented by the name
 * @a->a_cnp in the directory vnode @a->a_dvp and if found returns the vnode in
 * *@a->a_vpp.
 *
 * Return 0 on success and the error code on error.  A return value of ENOENT
 * does not signify an error as such but merely the fact that the name
 * @a->a_cnp is not present in the directory @a->a_dvp.  When the lookup is
 * done for purposes of create, including for the destination of a rename, we
 * return EJUSTRETURNED instead of ENOENT when the name is not found.  This
 * allows the VFS to proceed with the create/rename.
 *
 * To simplify matters for us, we do not treat the DOS and WIN32 filenames as
 * two hard links but instead if the lookup matches a DOS filename, we return
 * the corresponding WIN32 filename instead.
 *
 * There are three cases we need to distinguish here:
 *
 * 1) The name perfectly matches (i.e. including case) a directory entry with a
 *    filename in the WIN32 or POSIX namespaces.  In this case
 *    ntfs_lookup_inode_by_name() will return with name set to NULL and we
 *    just use the name as supplied in @a->a_cnp.
 * 2) The name matches (not including case) a directory entry with a filename
 *    in the WIN32 or POSIX namespaces.  In this case
 *    ntfs_lookup_inode_by_name() will return with name set to point to an
 *    allocated ntfs_dir_lookup_name structure containing the properly cased
 *    little endian Unicode name.  We convert the name to decomposed UTF-8 and
 *    use that name.
 * 3) The name matches either perfectly or not (i.e. we do not care about case)
 *    a directory entry with a filename in the DOS namespace.  In this case
 *    ntfs_lookup_inode_by_name() will return with name set to point to an
 *    allocated ntfs_dir_lookup_name structure which just tells us that the
 *    name is in the DOS namespace.  We read the inode and find the filename in
 *    the WIN32 namespace corresponding to the matched DOS name.  We then
 *    convert the name to decomposed UTF-8 and use that name to update the
 *    vnode identity with.
 */
static int ntfs_vnop_lookup(struct vnop_lookup_args *a)
{
	MFT_REF mref;
	ino64_t mft_no;
	unsigned long op;
	struct componentname *name_cn, *cn;
	ntfs_inode *ni,  *dir_ni = NTFS_I(a->a_dvp);
	vnode_t vn;
	ntfs_volume *vol = dir_ni->vol;
	ntfschar *ntfs_name;
	ntfs_dir_lookup_name *name = NULL;
	u8 *utf8_name = NULL;
	size_t ntfs_name_size, utf8_size;
	signed ntfs_name_len;
	int err;
	/*
	 * This is rather gross but several other file systems do it so perhaps
	 * the stack in the OSX kernel is big and large static allocations do
	 * not matter...  If they do, simply set ntfs_name to NULL and
	 * utf8_to_ntfs() will allocate the memory for us.  (We then have to
	 * free it, see utf8_to_ntfs() description for details.)
	 */
	ntfschar ntfs_name_buf[NTFS_MAX_NAME_LEN];
	struct componentname cn_buf;
#ifdef DEBUG
	static const char *ops[4] = { "LOOKUP", "CREATE", "DELETE", "RENAME" };
#endif

	name_cn = cn = a->a_cnp;
	op = cn->cn_nameiop;
	ntfs_debug("Looking up %.*s in directory inode 0x%llx for %s, flags "
			"0x%lx.", (int)cn->cn_namelen, cn->cn_nameptr,
			(unsigned long long)dir_ni->mft_no,
			op < 4 ? ops[op] : "UNKNOWN", cn->cn_flags);
	/*
	 * FIXME: Is this check necessary?  Can we ever get here for
	 * non-directories?
	 */
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_error(vol->mp, "Not a directory!");
		return ENOTDIR;
	}
	/*
	 * First, look for the name in the name cache.  cache_lookup() returns
	 * -1 if found and @vn is set to the vnode, ENOENT if found and it is a
	 * negative entry thus @vn is not set to anything, or 0 if the lookup
	 * failed in which case we need to do a file system based lookup.
	 */
	err = cache_lookup(dir_ni->vn, &vn, cn);
	if (err) {
		if (err == -1) {
			*a->a_vpp = vn;
			ntfs_debug("Done (cached).");
			return 0;
		}
		if (err == ENOENT) {
			ntfs_debug("Done (cached, negative).");
			return err;
		}
		ntfs_error(vol->mp, "cache_lookup() failed (error %d).", err);
		return err;
	}
	/* We special case "." and ".." as they are emulated on NTFS. */
	if (cn->cn_namelen == 1 && cn->cn_nameptr[0] == '.') {
		/* "." is not cached. */
		cn->cn_flags &= ~MAKEENTRY;
		if (op == RENAME) {
			ntfs_warning(vol->mp, "Op is RENAME but name is "
					"\".\", returning EISDIR.");
			return EISDIR;
		}
		err = vnode_get(dir_ni->vn);
		if (err) {
			ntfs_error(vol->mp, "Failed to get iocount reference "
					"on current directory (error %d).",
					err);
			return err;
		}
		ni = dir_ni;
		ntfs_debug("Got \".\" directory 0x%llx.",
				(unsigned long long)ni->mft_no);
		goto found;
	} else if (cn->cn_flags & ISDOTDOT) {
		/* ".." is not cached. */
		cn->cn_flags &= ~MAKEENTRY;
		vn = vnode_getparent(dir_ni->vn);
		if (!vn) {
			// FIXME: Need to lookup the first filename attribute
			// in the mft record of the directory @dir_ni and use
			// its parent mft reference to run an ntfs_inode_get()
			// on it.
			ntfs_error(vol->mp, "Called for \"..\" but no parent "
					"vnode stored in current directory.  "
					"This case is not supported yet, "
					"sorry.");
			return ENOTSUP;
		}
		ni = NTFS_I(vn);
		ntfs_debug("Got \"..\" directory 0x%llx for directory 0x%llx.",
				(unsigned long long)ni->mft_no,
				(unsigned long long)dir_ni->mft_no);
		goto found;
	}
	/* Convert the name from utf8 to Unicode. */
	ntfs_name = ntfs_name_buf;
	ntfs_name_size = sizeof(ntfs_name_buf);
	ntfs_name_len = utf8_to_ntfs(vol, (u8*)cn->cn_nameptr, cn->cn_namelen,
			&ntfs_name, &ntfs_name_size);
	if (ntfs_name_len < 0) {
		err = -ntfs_name_len;
		if (err == ENAMETOOLONG)
			ntfs_debug("Failed (name is too long).");
		else
			ntfs_error(vol->mp, "Failed to convert name to "
					"Unicode (error %d).", err);
		return err;
	}
	/* Look up the converted name in the directory index. */
	lck_rw_lock_shared(&dir_ni->lock);
	err = ntfs_lookup_inode_by_name(dir_ni, ntfs_name, ntfs_name_len,
			&mref, &name);
	// TODO: We are dropping the lock here so once we implement delete and
	// rename operations we need to handle errors later on which indicate
	// that the inode has been deleted in the meantime and instead of
	// bailing out we need to return ENOENT or EJUSTRETURN depending as we
	// do here...
	lck_rw_unlock_shared(&dir_ni->lock);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to find name in directory "
					"(error %d).", err);
			return err;
		}
		/*
		 * The name does not exist in the directory @dir_ni.
		 *
		 * If creating (or renaming and the name is the destination
		 * name) and we are at the end of a pathname we can consider
		 * allowing the file to be created so return EJUSTRETURN
		 * instead of ENOENT.
		 */
		if (cn->cn_flags & ISLASTCN && (op == CREATE || op == RENAME)) {
			ntfs_debug("Done (not found but for CREATE or RENAME, "
					"returning EJUSTRETURN).");
			return EJUSTRETURN;
		}
		/*
		 * Insert a negative entry into the name cache if caching of
		 * this name is desired.
		 */
		if (cn->cn_flags & MAKEENTRY)
			cache_enter(dir_ni->vn, NULL, cn);
		 /*
		  * Prevent the caller from trying to add the name to the cache
		  * as well.
		  */
		cn->cn_flags &= ~MAKEENTRY;
		ntfs_debug("Done (not found%s).", cn->cn_flags & MAKEENTRY ?
				"adding negative name cache entry" : "");
		return err;
	}
	/* The lookup succeeded. */
	mft_no = MREF(mref);
	ntfs_debug("Name matches inode number 0x%llx.",
			(unsigned long long)mft_no);
	/*
	 * If the name is at the end of a pathname and is about to be deleted
	 * either directly or as a consequence of a rename with the name as the
	 * target, do not cache it.
	 */
	if (cn->cn_flags & ISLASTCN && (op == DELETE || op == RENAME))
		cn->cn_flags &= ~MAKEENTRY;
	/*
	 * If a name was returned from the lookup and it is in the POSIX or
	 * WIN32 namespaces we need to convert it into a componentname so we
	 * can use it instead of the existing componentname @cn when getting
	 * the inode.
	 *
	 * If the returned name is in the DOS namespace we have to get the
	 * inode without a name as we need the inode in order to be able to
	 * find the WIN32 name corresponding to the DOS name.  Once we have the
	 * name we will update the vnode identity with it.
	 *
	 * If no name was returned, the match was perfect and we just use the
	 * componentname that was passed in by the caller.
	 */
	if (name) {
		if (name->type == FILENAME_DOS) {
			name_cn = NULL;
			/*
			 * We do not need @name any more but do not set it to
			 * NULL because we use that fact to distinguish between
			 * the DOS and WIN32/POSIX cases.
			 */
			OSFree(name, sizeof(ntfs_dir_lookup_name),
					ntfs_malloc_tag);
		} else {
			signed res_size;

			res_size = ntfs_to_utf8(vol, name->name, name->len <<
					NTFSCHAR_SIZE_SHIFT, &utf8_name,
					&utf8_size);
			OSFree(name, sizeof(ntfs_dir_lookup_name),
					ntfs_malloc_tag);
			if (res_size < 0) {
				/* Failed to convert name. */
				err = -res_size;
				ntfs_error(vol->mp, "Failed to convert inode "
						"name to decomposed UTF-8 "
						"(error %d).", err);
				return err;
			}
			name = NULL;
			cn_buf = (struct componentname) {
				.cn_flags = cn->cn_flags,
				.cn_nameptr = (char*)utf8_name,
				.cn_namelen = res_size,
			};
			name_cn = &cn_buf;
		}
	}
	/*
	 * @name_cn now contains the correct name of the inode or is NULL.
	 *
	 * If @name_cn is not NULL and its cn_flags indicate that the name is
	 * to be entered into the name cache, ntfs_inode_get() will do this and
	 * clear the MAKEENTRY bit in the cn_flags.
	 */
	err = ntfs_inode_get(vol, mft_no, FALSE, &ni, dir_ni->vn, name_cn);
	if (name_cn == &cn_buf) {
		/* Pick up any modifications to the cn_flags. */
		cn->cn_flags = cn_buf.cn_flags;
		OSFree(utf8_name, utf8_size, ntfs_malloc_tag);
	}
	if (!err) {
		/* Consistency check. */
		// FIXME: I cannot remember why we need the "mft_no ==
		// FILE_MFT" check...
		if (MSEQNO(mref) == ni->seq_no || mft_no == FILE_MFT) {
			/*
			 * Perfect WIN32/POSIX match or wrong case WIN32/POSIX
			 * match. -- Cases 1 and 2 respectively.
			 */
			if (!name) {
found:
				*a->a_vpp = ni->vn;
				ntfs_debug("Done (case %d).",
						name_cn == &cn_buf ? 2 : 1);
				return 0;
			}
			/*
			 * We are too indented.  Handle DOS matches further
			 * below.
			 */
			goto handle_dos_name;
		}
		ntfs_error(vol->mp, "Found stale reference to inode 0x%llx "
				"(reference sequence number 0x%x, inode "
				"sequence number 0x%x), returning EIO.  Run "
				"chkdsk.", (unsigned long long)mft_no,
				(unsigned)MSEQNO(mref), (unsigned)ni->seq_no);
		(void)vnode_put(ni->vn);
		err = EIO;
	} else
		ntfs_error(vol->mp, "Failed to get inode 0x%llx (error %d).",
				(unsigned long long)mft_no, err);
	return err;
	// TODO: Consider moving this lot to a separate function.
handle_dos_name:
   {
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	FILENAME_ATTR *fn;
	const char *old_name;
	signed res_size;

	vn = ni->vn;
	/*
	 * DOS match. -- Case 3.
	 *
	 * Find the WIN32 name corresponding to the matched DOS name.
	 *
	 * At present @ni is guaranteed to be a base inode.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record (error %d).",
				err);
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to allocate search context.");
		err = ENOMEM;
		goto unm_err;
	}
	do {
		ATTR_RECORD *attr;
		u32 val_len;
		u16 val_ofs;

		err = ntfs_attr_lookup(AT_FILENAME, NULL, 0, 0, 0, NULL, 0,
				ctx);
		if (err) {
			if (err == ENOENT) {
				ntfs_error(vol->mp, "WIN32 namespace name is "
						"missing from inode.  Run "
						"chkdsk.");
				err = EIO;
			} else
				ntfs_error(vol->mp, "Failed to find WIN32 "
						"namespace name in inode "
						"(error %d).", err);
			goto put_err;
		}
		/* Consistency checks. */
		attr = ctx->a;
		if (attr->non_resident || attr->flags)
			goto attr_err;
		val_len = le32_to_cpu(attr->value_length);
		val_ofs = le16_to_cpu(attr->value_offset);
		if (val_ofs + val_len > le32_to_cpu(attr->length))
			goto attr_err;
		fn = (FILENAME_ATTR*)((u8*)attr + val_ofs);
		if ((u32)(sizeof(FILENAME_ATTR) + (fn->filename_length <<
				NTFSCHAR_SIZE_SHIFT)) > val_len)
			goto attr_err;
	} while (fn->filename_type != FILENAME_WIN32);
	/* Convert the name to decomposed UTF-8. */
	res_size = ntfs_to_utf8(vol, fn->filename, fn->filename_length <<
			NTFSCHAR_SIZE_SHIFT, &utf8_name, &utf8_size);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	if (res_size < 0) {
		/* Failed to convert name. */
		err = -res_size;
		ntfs_error(vol->mp, "Failed to convert inode name to "
				"decomposed UTF-8 (error %d).", err);
		goto err;
	}
	/* Update the vnode with the new name if it differs from the old one. */
	old_name = vnode_getname(vn);
	if (!old_name || (ni->link_count > 1 && ((long)strlen(old_name) !=
			res_size || bcmp(old_name, utf8_name, res_size)))) {
		vnode_update_identity(vn, NULL, (char*)utf8_name, res_size, 0,
				VNODE_UPDATE_NAME | VNODE_UPDATE_CACHE);
	}
	if (old_name)
		vnode_putname(old_name);
	/*
	 * Enter the name into the cache (if it is already there this is a
	 * no-op) and prevent the caller from trying to add the name to the
	 * cache as well.
	 */
	cn_buf = (struct componentname) {
		.cn_flags = cn->cn_flags,
		.cn_nameptr = (char*)utf8_name,
		.cn_namelen = res_size,
	};
	cache_enter(dir_ni->vn, vn, &cn_buf);
	cn->cn_flags &= ~MAKEENTRY;
	OSFree(utf8_name, utf8_size, ntfs_malloc_tag);
	*a->a_vpp = ni->vn;
	ntfs_debug("Done (case 3).");
	return 0;
attr_err:
	ntfs_error(vol->mp, "Filename attribute is corrupt.  Run chkdsk.");
	err = EIO;
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(ni);
err:
	(void)vnode_put(vn);
	return err;
   }
}

/**
 * ntfs_vnop_create -
 *
 */
static int ntfs_vnop_create(struct vnop_create_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_mknod -
 *
 */
static int ntfs_vnop_mknod(struct vnop_mknod_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_open - open a vnode
 * @a:		arguments to open function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to open
 *	int a_mode;		mode to open the file with
 *	vfs_context_t a_context;
 *
 * Open the vnode @a->a_vp with mode @a->a_mode.
 *
 * Note the VFS does a lot of checking before ntfs_vnop_open() is called
 * including permissions and checking for a read-only file system thus we do
 * not need to worry about the case where the driver is compiled read-only as
 * the volume is then mounted read-only so the vfs catches all write accesses
 * very early on and denies them.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_open(struct vnop_open_args *a)
{
	ntfs_inode *ni = NTFS_I(a->a_vp);
	errno_t err = 0;

	ntfs_debug("Entering for mft_no 0x%llx, mode 0x%x.",
			(unsigned long long)ni->mft_no, (unsigned)a->a_mode);
	/*
	 * At present the only thing we check for is that $MFT and $MFTMirr may
	 * not be opened from outside the ntfs driver because we need to apply
	 * mst fixups and lock records and we do not want to have to deal with
	 * all of that in our ntfs_vnop_read() and ntfs_vnop_write().
	 *
	 * TODO: If we wanted, we could deny opening of all system files here,
	 * perhaps depending on a mount option or an otherwise tunable option.
	 */
	if (ni->mft_no <= FILE_MFTMirr)
		err = EACCES;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_close - close a vnode
 * @a:		arguments to close function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to close
 *	int a_fflag;		close flags (FREAD and/or FWRITE for example)
 *	vfs_context_t a_context;
 *
 * Close the vnode @a->a_vp with flags @a->a_fflag.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_close(struct vnop_close_args *a)
{
	ntfs_debug("Entering for mft_no 0x%llx, fflag 0x%x.",
			(unsigned long long)NTFS_I(a->a_vp)->mft_no,
			a->a_fflag);
	/* At present there is nothing to do. */
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_vnop_access -
 *
 */
static int ntfs_vnop_access(struct vnop_access_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
ntfs_error(NULL, "Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_getattr - get attributes about a vnode or about the mounted volume
 * @a:		arguments to getattr function
 *
 * @a contains:
 *	vnode_t a_vp;			vnode for which to return attributes
 *	struct vnode_attr *a_vap;	attributes to return and destination
 *	vfs_context_t a_context;
 *
 * Return the attributes described in @a_vap about the vnode @a_vp.  Some
 * attributes are intercepted by the VFS in getattrlist() and getvolattrlist()
 * so we do not bother with them.
 *
 * At present we do not support all attributes.  We declare what we support to
 * the world in our VFS_GETATTR() function (ntfs_vfsops.c::ntfs_getattr()) so
 * do not forget to update that when support for further attributes is added
 * here.
 *
 * Return 0 on success and EINVAL if an unsupported @a_name was queried for.
 *
 * TODO: Implement more attributes.
 */
static int ntfs_vnop_getattr(struct vnop_getattr_args *a)
{
	ntfs_inode *ni = NTFS_I(a->a_vp);
	ntfs_volume *vol = ni->vol;
	struct vnode_attr *va = a->a_vap;
	FILE_ATTR_FLAGS file_attributes;
	u32 flags;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/* For directories always return a link count of 1. */
	if (!S_ISDIR(ni->mode))
		VATTR_RETURN(va, va_nlink, ni->link_count);
	else
		VATTR_RETURN(va, va_nlink, 1);
	// TODO: Set this to the device for block and character special files.
	va->va_rdev = (dev_t)0;
	/*
	 * We cheat for both the total size and the total allocated size and
	 * just return the attribute size rather than looping over all ($DATA?)
	 * attributes and adding up their sizes.
	 */
	va->va_total_size = va->va_data_size = ni->data_size;
	va->va_iosize = MAXBSIZE;
	va->va_uid = ni->uid;
	va->va_gid = ni->gid;
	va->va_mode = ni->mode;
	file_attributes = ni->file_attributes;
	flags = 0;
	if (file_attributes & FILE_ATTR_READONLY)
		flags |= UF_IMMUTABLE;
	/*
	 * We do not want to hide the root directory of a volume so pretend it
	 * does not have the hidden bit set.
	 */
	if (file_attributes & FILE_ATTR_HIDDEN && ni->mft_no != FILE_root)
		flags |= UF_HIDDEN;
	/* Windows does not set the "needs archiving" bit on directories. */
	if (!S_ISDIR(ni->mode) && !(file_attributes & FILE_ATTR_ARCHIVE))
		flags |= SF_ARCHIVED;
	va->va_flags = flags;
	va->va_create_time = ni->creation_time;
	va->va_access_time = ni->last_access_time;
	va->va_modify_time = ni->last_data_change_time;
	va->va_change_time = ni->last_mft_change_time;
	/* NTFS does not distinguish between the inode and its hard links. */
	va->va_fileid = ni->mft_no;
	va->va_linkid = ni->mft_no;
	va->va_fsid = vol->dev;
	/* FIXME: What is the difference between the below two? */
	va->va_filerev = ni->seq_no;
	va->va_gen = ni->seq_no;
	va->va_encoding = 0x7E; /* = kTextEncodingMacUnicode */
	va->va_supported |=
			VNODE_ATTR_BIT(va_rdev) |
			VNODE_ATTR_BIT(va_total_size) |
			VNODE_ATTR_BIT(va_data_size) |
			VNODE_ATTR_BIT(va_iosize) |
			VNODE_ATTR_BIT(va_uid) |
			VNODE_ATTR_BIT(va_gid) |
			VNODE_ATTR_BIT(va_mode) |
			VNODE_ATTR_BIT(va_flags) |
			VNODE_ATTR_BIT(va_create_time) |
			VNODE_ATTR_BIT(va_access_time) |
			VNODE_ATTR_BIT(va_modify_time) |
			VNODE_ATTR_BIT(va_change_time) |
			VNODE_ATTR_BIT(va_fileid) |
			VNODE_ATTR_BIT(va_linkid) |
			VNODE_ATTR_BIT(va_fsid) |
			VNODE_ATTR_BIT(va_filerev) |
			VNODE_ATTR_BIT(va_gen) |
			VNODE_ATTR_BIT(va_encoding) |
			0;
	/*
	 * For now only support va_parentid if we have the parent vnode
	 * attached to the vnode being queried or the vnode being queried is
	 * the root directory of the volume in which case return the root
	 * directory as its own parent.
	 *
	 * TODO: Implement va_parentid properly, i.e. in absence of an attached
	 * parent vnode to the vnode being queried.  This is a bit of work on
	 * NTFS but once done makes returning @va_name a piece of cake.  The
	 * biggest problem is the hard link case in which it is not possible to
	 * know which is the correct parent and hence name to return.
	 */
	if (VATTR_IS_ACTIVE(va, va_parentid)) {
		ino64_t parent_ino;
		vnode_t parent_vn;

		parent_vn = vnode_getparent(ni->vn);
		if (parent_vn) {
			parent_ino = NTFS_I(parent_vn)->mft_no;
			(void)vnode_put(parent_vn);
		}
		if (parent_vn)
			VATTR_RETURN(va, va_parentid, parent_ino);
// This breaks copying from the Finder...
//		else if (ni->mft_no == FILE_root)
//			VATTR_RETURN(va, va_parentid, (u64)FILE_root);
	}
	/*
	 * Resident attributes reside inside the on-disk inode and thus have no
	 * on-disk allocation because the on-disk inode itself is already
	 * accounted for in the allocated size of the $MFT system file which
	 * contains the table of on-disk inodes.
	 */
	if (va->va_active & (VNODE_ATTR_BIT(va_total_alloc) |
			VNODE_ATTR_BIT(va_data_alloc))) {
		s64 on_disk_size = 0;
		if (NInoNonResident(ni)) {
			if (ni->type == AT_DATA && (NInoCompressed(ni) ||
					NInoSparse(ni)))
				on_disk_size = ni->compressed_size;
			else
				on_disk_size = ni->allocated_size;
		}
		va->va_total_alloc = va->va_data_alloc = on_disk_size;
		va->va_supported |= VNODE_ATTR_BIT(va_total_alloc) |
				VNODE_ATTR_BIT(va_data_alloc);
	}
	/*
	 * For now only support va_name if we have a name attached to the
	 * vnode being queried.
	 *
	 * If this is the root directory of the volume, leave it to the VFS to
	 * find the mounted-on name, which is different from the real volume
	 * root directory name of ".".
	 *
	 * TODO: Implement va_name properly, i.e. in absence of an attached
	 * name to the vnode being queried.  This requires implementing
	 * va_parent properly, too (see above).
	 */
	if (VATTR_IS_ACTIVE(va, va_name)) {
		const char *name;

		name = vnode_getname(ni->vn);
		if (name) {
			if (ni->mft_no != FILE_root || NInoAttr(ni)) {
				(void)strlcpy(va->va_name, name,
					      MAXPATHLEN - 1);
				VATTR_SET_SUPPORTED(va, va_name);
			}
			(void)vnode_putname(name);
		}
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_vnop_setattr -
 *
 */
static int ntfs_vnop_setattr(struct vnop_setattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	//ntfs_debug("Done (error %d).", (int)err);
	ntfs_error(NULL, "Setting vnode attributes is not supported yet, "
			"sorry.");
	return err;
}

/* Limit the internal i/o size so we can represent it in a 32-bit int. */
#define NTFS_MAX_IO_REQUEST_SIZE	(1024 * 1024 * 256)

/**
 * ntfs_vnop_read_compressed - read from a compressed attribute
 * @ni:		ntfs inode describing the compressed attribute to read
 * @uio:	destination in which to return the read data
 * @data_size:	data size of the compressed attribute
 * @ioflags:	flags further describing the read request (see ntfs_vnop_read())
 *
 * This is a helper function for ntfs_vnop_read() (see below).  It is called
 * when a read request for a compressed attribute is received by
 * ntfs_vnop_read().
 *
 * This function is somewhat similar to cluster_read() or to be more precise to
 * cluster_read_copy() in that it breaks up large i/os into smaller manageable
 * chunks, and for each chunk tries to get the data from the vm page cache and
 * return it in the destination buffer described by @uio and failing that, it
 * creates and maps a upl and causes it to be filled with data by calling
 * ntfs_read_compressed() which reads the compressed data via the raw inode and
 * decompresses it into our mapped upl and once that is done we now have the
 * data in the vm page cache and copy it into the destination buffer described
 * by @uio.
 *
 * Return 0 on success and errno on error.
 */
static inline int ntfs_vnop_read_compressed(ntfs_inode *ni, uio_t uio,
		const s64 data_size, int ioflags)
{
	s64 size;
	user_ssize_t start_count;
	off_t ofs;
	vnode_t vn = ni->vn;
	ntfs_inode *raw_ni;
	upl_t upl;
	upl_page_info_t *pl;
	kern_return_t kerr;
	int count, err, align_mask, cur_pg, last_pg;

	ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	ntfs_debug("Entering for compressed file inode 0x%llx, offset 0x%llx, "
			"count 0x%llx, ioflags 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)start_count, ioflags);
	/*
	 * We can only read from regular files and named streams that are
	 * compressed and non-resident.  We should never be called for anything
	 * else.
	 */
	if (ni->type != AT_DATA || !NInoCompressed(ni) ||
			!NInoNonResident(ni) || NInoEncrypted(ni) ||
			NInoRaw(ni))
		panic("%s(): Called for inappropriate inode.\n", __FUNCTION__);
	/* Get the raw inode. */
	err = ntfs_raw_inode_get(ni, &raw_ni);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get raw inode (error %d).",
				err);
		return err;
	}
	if (!NInoRaw(raw_ni))
		panic("%s(): Requested raw inode but got non-raw one.\n",
				__FUNCTION__);
	/*
	 * Protect against concurrent writers as the compressed data is invalid
	 * whilst a write is in progress.
	 */
	lck_rw_lock_shared(&raw_ni->lock);
	lck_spin_lock(&raw_ni->size_lock);
	size = ubc_getsize(raw_ni->vn);
	if (size != raw_ni->data_size)
		panic("%s(): size != raw_ni->data_size\n", __FUNCTION__);
	lck_spin_unlock(&raw_ni->size_lock);
	/*
	 * If nothing was requested or the request starts at or beyond the end
	 * of the attribute, we do not need to do anything.
	 */
	if (!start_count || ofs >= data_size) {
		err = 0;
		goto err;
	}
	/* Cannot read from a negative offset. */
	if (ofs < 0) {
		err = EINVAL;
		goto err;
	}
	if (vnode_isnocache(vn) || vnode_isnocache(raw_ni->vn))
		ioflags |= IO_NOCACHE;
	if (vnode_isnoreadahead(vn) || vnode_isnoreadahead(raw_ni->vn))
		ioflags |= IO_RAOFF;
	align_mask = ni->compression_block_size - 1;
	if (align_mask < PAGE_MASK)
		align_mask = PAGE_MASK;
	/*
	 * Loop until we have finished the whole request or reached the end of
	 * the attribute.
	 *
	 * FIXME: We do not bother with read-ahead on the uncompressed vnode
	 * for now except to the extent that we always decompress full
	 * compression blocks which may be larger than the current i/o request
	 * so the next i/o request will find the whole compression block
	 * decompressed in the vm page cache thus small reads will in effect
	 * experience a certain amount of read-ahead in this way.
	 */
	do {
		u8 *kaddr;
		int delta, next_pg, orig_count;

		size = data_size - ofs;
		if (size > start_count)
			size = start_count;
		count = size;
		/*
		 * Break up the i/o in chunks that fit into a 32-bit int so
		 * we can call cluster_copy_ubc_data(), etc.
		 */
		if (size > NTFS_MAX_IO_REQUEST_SIZE)
			count = NTFS_MAX_IO_REQUEST_SIZE;
		/*
		 * First of all, try to copy the data from the vm page cache.
		 * This will work on the second and all later reads so this is
		 * the hot path.  If the attribute has not been accessed at all
		 * before or its cached pages were dropped due to vm pressure
		 * this will fail to copy any data due to the lack of a valid
		 * page and we will drop into the slow path.
		 */
		if (!(ioflags & IO_NOCACHE)) {
			err = cluster_copy_ubc_data(vn, uio, &count, 0);
			if (err) {
				/*
				 * The copying (uiomove()) failed with an
				 * error, abort.
				 */
				ntfs_error(ni->vol->mp,
						"cluster_copy_ubc_data() "
						"failed (error %d).", err);
				goto err;
			}
			/*
			 * @count is now set to the number of bytes remaining
			 * to be transferred.  If it is zero, it means all the
			 * pages were in the vm page cache so we can skip onto
			 * the next part of the i/o.
			 */
			if (!count)
				continue;
			ofs = uio_offset(uio);
		}
		/*
		 * Only some or none of the pages were in the vm page cache or
		 * this is not a cached i/o.  First align this i/o request to
		 * compression block boundaries and to PAGE_SIZE boundaries and
		 * truncate it to the maximum upl size then create and map a
		 * page list so we can fill it with the data.
		 */
		delta = ofs & align_mask;
		ofs -= delta;
		orig_count = count;
		count += delta;
		count = (count + align_mask) & ~(off_t)align_mask;
		if (count > MAXBSIZE)
			count = MAXBSIZE;
		/*
		 * Do not exceed the attribute size except for a final partial
		 * page.
		 */
		size = (data_size - ofs + PAGE_MASK) & ~PAGE_MASK_64;
		if (count > size)
			count = size;
		start_count = count;
		kerr = ubc_create_upl(vn, ofs, count, &upl, &pl, UPL_SET_LITE);
		if (kerr != KERN_SUCCESS)
			panic("%s(): Failed to get page list (error %d).\n",
					__FUNCTION__, (int)kerr);
		kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
		if (kerr != KERN_SUCCESS) {
			ntfs_error(ni->vol->mp, "Failed to map page list "
					"(error %d).", (int)kerr);
			err = EIO;
			goto abort_err;
		}
		/*
		 * We know @ofs starts on both a compression block and a page
		 * boundary.  We read from the compressed raw vnode
		 * decompressing the data into our mapped page list.  Any
		 * already valid pages are automatically skipped.
		 */
		err = ntfs_read_compressed(ni, raw_ni, ofs, count, kaddr, pl,
				ioflags);
		if (err) {
			ntfs_error(ni->vol->mp, "Failed to decompress data "
					"(error %d).", err);
			goto unm_err;
		}
		/*
		 * We now have the entire page list filled with valid pages,
		 * thus we can now copy from the mapped page list into the
		 * destination buffer using uiomove().  We just need to make
		 * sure not to copy past the end of the attribute.
		 */
		ofs += delta;
		count -= delta;
		if (count > orig_count)
			count = orig_count;
		if (ofs + count > data_size)
			count = data_size - ofs;
		err = uiomove((void*)(kaddr + delta), count, uio);
		if (err) {
			ntfs_error(ni->vol->mp, "uiomove() failed (error %d).",
					err);
			goto unm_err;
		}
		kerr = ubc_upl_unmap(upl);
		if (kerr != KERN_SUCCESS) {
			ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed "
					"(error %d).", (int)kerr);
			err = EIO;
			goto abort_err;
		}
		/*
		 * We are done with the page list, commit and/or abort the
		 * pages.
		 */
		next_pg = 0;
		last_pg = start_count >> PAGE_SHIFT;
		do {
			int commit_flags;
			BOOL was_valid, was_dirty;

			cur_pg = next_pg;
			/* Determine the state of the current first page. */
			was_valid = upl_valid_page(pl, cur_pg);
			was_dirty = (was_valid && upl_dirty_page(pl, cur_pg));
			/* Find sequential pages of the same state. */
			for (next_pg = cur_pg + 1; next_pg < last_pg;
					next_pg++) {
				if (was_valid != upl_valid_page(pl, next_pg))
					break;
				if (was_valid) {
					if (was_dirty != upl_dirty_page(pl,
							next_pg))
						break;
				}
			}
			count = (next_pg - cur_pg) << PAGE_SHIFT;
			/*
			 * For a set of pages that were invalid and hence we
			 * just filled them with data we commit and clean them
			 * unless no caching is requested in which case we dump
			 * them.
			 *
			 * For a set of pages that were already valid and hence
			 * we did not touch we commit them taking care to
			 * preserve any dirty state unless the pages were clean
			 * and no caching is requested in which case we dump
			 * them.
			 */
			if (ioflags & IO_NOCACHE && !was_dirty) {
				ubc_upl_abort_range(upl, cur_pg << PAGE_SHIFT,
						count, UPL_ABORT_DUMP_PAGES |
						UPL_ABORT_FREE_ON_EMPTY);
				continue;
			}
			commit_flags = UPL_COMMIT_FREE_ON_EMPTY |
					UPL_COMMIT_INACTIVATE;
			if (!was_valid)
				commit_flags |= UPL_COMMIT_CLEAR_DIRTY;
			else if (was_dirty)
				commit_flags |= UPL_COMMIT_SET_DIRTY;
			ubc_upl_commit_range(upl, cur_pg << PAGE_SHIFT, count,
					commit_flags);
		} while (next_pg < last_pg);
	} while ((start_count = uio_resid(uio)) &&
			(ofs = uio_offset(uio)) < data_size);
	ntfs_debug("Done.");
err:
	lck_rw_unlock_shared(&raw_ni->lock);
	vnode_put(raw_ni->vn);
	return err;
unm_err:
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS)
		ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
abort_err:
	/*
	 * We handle each page independently for simplicity.  We do not care
	 * for performance given this is an error code path.
	 *
	 * For a page that was not valid, we dump it as it still does not
	 * contain valid data.  For a page that was valid, we release it
	 * without modification as we have not touched it unless no caching is
	 * requested and the page was clean in which case we dump it.
	 */
	last_pg = start_count >> PAGE_SHIFT;
	for (cur_pg = 0; cur_pg < last_pg; cur_pg++) {
		int abort_flags;

		abort_flags = UPL_ABORT_FREE_ON_EMPTY;
		if (!upl_valid_page(pl, cur_pg) || (ioflags & IO_NOCACHE &&
				!upl_dirty_page(pl, cur_pg)))
			abort_flags |= UPL_ABORT_DUMP_PAGES;
		ubc_upl_abort_range(upl, cur_pg << PAGE_SHIFT, PAGE_SIZE,
				abort_flags);
	}
	goto err;
}

/**
 * ntfs_vnop_read - read a number of bytes from a file into memory
 * @a:		arguments to read function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode of file whose data to read into memory
 *	uio_t a_uio;		destination in which to return the read data
 *	int a_ioflag;		flags further describing the read request
 *	vfs_context_t a_context;
 *
 * Read uio_resid(@a->a_uio) bytes from the vnode @a-a_vp, starting at byte
 * offset uio_offset(@a->a_uio) into the vnode into the destination buffer
 * pointed to by @uio.
 *
 * The flags in @a->a_ioflag further describe the read request.  The following
 * ioflags are currently defined in OSX kernel (a lot of them are not
 * applicable to VNOP_READ()) however:
 *	IO_UNIT		- Do i/o as atomic unit.
 *	IO_APPEND	- Append write to end.
 *	IO_SYNC		- Do i/o synchronously.
 *	IO_NODELOCKED	- Underlying node already locked.
 *	IO_NDELAY	- FNDELAY flag set in file table.
 *	IO_NOZEROFILL	- F_SETSIZE fcntl uses this to prevent zero filling.
 *	IO_TAILZEROFILL	- Zero fills at the tail of write.
 *	IO_HEADZEROFILL	- Zero fills at the head of write.
 *	IO_NOZEROVALID	- Do not zero fill if valid page.
 *	IO_NOZERODIRTY	- Do not zero fill if page is dirty.
 *	IO_CLOSE	- The i/o was issued from close path.
 *	IO_NOCACHE	- Same effect as VNOCACHE_DATA, but only for this i/o.
 *	IO_RAOFF	- Same effect as VRAOFF, but only for this i/o.
 *	IO_DEFWRITE	- Defer write if vfs.defwrite is set.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 *
 * For encrypted attributes we abort for now as we do not support them yet.
 *
 * For non-resident attributes we use cluster_read_ext() which deals with both
 * normal and multi sector transfer protected attributes and
 * ntfs_vnop_read_compressed() which deals with compressed attributes.
 *
 * For resident attributes we read the data from the vm page cache and if it is
 * not there we cause the vm page cache to be populated by reading the buffer
 * at offset 0 in the attribute.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_read(struct vnop_read_args *a)
{
	s64 size;
	user_ssize_t start_count;
	off_t ofs;
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	uio_t uio = a->a_uio;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	int err, count;

	ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	ntfs_debug("Entering for file inode 0x%llx, offset 0x%llx, count "
			"0x%llx, ioflags 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)start_count, a->a_ioflag);
	/*
	 * We can only read from regular files and named streams.
	 *
	 * Also, do not allow reading from system files or mst protected
	 * attributes.
	 */
	if (vnode_issystem(vn) || NInoMstProtected(ni) ||
			(!S_ISREG(ni->mode) && !(NInoAttr(ni) &&
			ni->type == AT_DATA))) {
		if (S_ISDIR(ni->mode))
			return EISDIR;
		return EPERM;
	}
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also.
	 */
	lck_rw_lock_shared(&ni->lock);
	/*
	 * TODO: This check may no longer be necessary now that we lock against
	 * changes in initialized size and thus truncation...  Revisit this
	 * issue when the write code has been written and remove the check if
	 * appropriate simply using ubc_getsize(vn); without the size_lock.
	 */
	lck_spin_lock(&ni->size_lock);
	size = ubc_getsize(vn);
	if (size > ni->data_size)
		size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If nothing was requested or the request starts at or beyond the end
	 * of the attribute, we do not need to do anything.
	 */
	if (!start_count || ofs >= size) {
		err = 0;
		goto err;
	}
	/* Cannot read from a negative offset. */
	if (ofs < 0) {
		err = EINVAL;
		goto err;
	}
	/* TODO: Deny access to encrypted attributes, just like NT4. */
	if (NInoEncrypted(ni)) {
		ntfs_warning(ni->vol->mp, "Denying access to encrypted "
				"attribute (EACCES).");
		err = EACCES;
		goto err;
	}
	if (NInoNonResident(ni)) {
		int (*callback)(buf_t, void *);

		if (NInoCompressed(ni) && !NInoRaw(ni)) {
			err = ntfs_vnop_read_compressed(ni, uio, size,
					a->a_ioflag);
			if (!err)
				ntfs_debug("Done (ntfs_vnop_read_compressed()"
						").");
			else
				ntfs_error(ni->vol->mp, "Failed ("
						"ntfs_vnop_read_compressed(), "
						"error %d).", err);
			goto err;
		}
		callback = NULL;
		if (NInoMstProtected(ni) || NInoEncrypted(ni))
			callback = ntfs_cluster_iodone;
		err = cluster_read_ext(vn, uio, size, a->a_ioflag, callback,
				NULL);
		if (!err)
			ntfs_debug("Done (cluster_read_ext()).");
		else
			ntfs_error(ni->vol->mp, "Failed for file inode "
					"0x%llx, start offset 0x%llx, start "
					"count 0x%llx, now offset 0x%llx, "
					"now count 0x%llx, ioflags 0x%x "
					"(cluster_read_ext(), error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned long long)ofs,
					(unsigned long long)start_count,
					(unsigned long long)uio_offset(uio),
					(unsigned long long)uio_resid(uio),
					a->a_ioflag, err);
		goto err;
	} /* else if (!NInoNonResident(ni)) */
	/*
	 * That attribute is resident thus we have to deal with it by
	 * ourselves.  First of all, try to copy the data from the vm page
	 * cache.  This will work on the second and all later reads so this is
	 * the hot path.  If the attribute has not been accessed at all before
	 * or its cached pages were dropped due to vm pressure this will fail
	 * to copy any data due to the lack of a valid page and we will drop
	 * into the slow path.
	 */
	size -= ofs;
	if (size > start_count)
		size = start_count;
	if (size > PAGE_SIZE) {
		ntfs_warning(ni->vol->mp, "Unexpected count 0x%llx > PAGE_SIZE "
				"0x%x, overriding it to PAGE_SIZE.",
				(unsigned long long)size, PAGE_SIZE);
		size = PAGE_SIZE;
	}
	count = size;
	err = cluster_copy_ubc_data(vn, uio, &count, 0);
	if (err) {
		/* The copying (uiomove()) failed with an error, abort. */
		ntfs_error(ni->vol->mp, "cluster_copy_ubc_data() failed "
				"(error %d).", err);
		goto err;
	}
	/*
	 * @count is now set to the number of bytes remaining to be
	 * transferred.  If it is zero, it means we are done.  Note it is
	 * possible that there is more data requested, i.e. uio_resid(uio) > 0,
	 * but that just means the request goes beyond the end of the
	 * attribute.
	 */
	if (!count) {
		ntfs_debug("Done (resident, cached, returned 0x%llx bytes).",
				(unsigned long long)size);
		goto err;
	}
	/*
	 * We failed to transfer everything.  That really means we failed to
	 * transfer anything at all as we are guaranteed that a resident
	 * attribute is smaller than a page thus either the page is there and
	 * valid and we transfer everything or it is not and we transfer
	 * nothing.
	 */
	if (count != size) {
		ntfs_warning(ni->vol->mp, "Unexpected partial transfer from "
				"cached page (size 0x%llx, count 0x%x).",
				(unsigned long long)size, count);
		ofs = uio_offset(uio);
	}
	/*
	 * The page is not in cache or is not valid.  We need to bring it into
	 * cache and make it valid so we can then copy the data out.  The
	 * easiest way to do this is to just map the page which will take care
	 * of everything for us.  We can than uiomove() straight out of the
	 * page into the @uio and then unmap the page again.
	 *
	 * Note this will take the inode lock again but this is ok as in both
	 * cases the lock is taken shared.
	 */
	err = ntfs_page_map(ni, 0, &upl, &pl, &kaddr, FALSE);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map page (error %d).", err);
		goto err;
	}
	err = uiomove((void*)(kaddr + ofs), count, uio);
	ntfs_page_unmap(ni, upl, pl, FALSE);
	if (!err)
		ntfs_debug("Done (resident, not cached, returned 0x%llx "
				"bytes).", (unsigned long long)size -
				uio_resid(uio));
	else
		ntfs_error(ni->vol->mp, "uiomove() failed (error %d).", err);
err:
	lck_rw_unlock_shared(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_write - write a number of bytes from a memory buffer into a file
 * @a:		arguments to write function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode of file to write to
 *	uio_t a_uio;		source containing the data to write
 *	int a_ioflag;		flags further describing the write request
 *	vfs_context_t a_context;
 *
 * Write uio_resid(@a->a_uio) bytes from the source buffer specified by
 * @a->a_uio to the vnode @a-a_vp, starting at byte offset
 * uio_offset(@a->a_uio) into the vnode.
 *
 * The flags in @a->a_ioflag further describe the write request.  The following
 * ioflags are currently defined in OSX kernel (not all of them are applicable
 * to VNOP_WRITE()) however:
 *	IO_UNIT		- Do i/o as atomic unit.
 *	IO_APPEND	- Append write to end.
 *	IO_SYNC		- Do i/o synchronously.
 *	IO_NODELOCKED	- Underlying node already locked.
 *	IO_NDELAY	- FNDELAY flag set in file table.
 *	IO_NOZEROFILL	- F_SETSIZE fcntl uses this to prevent zero filling.
 *	IO_TAILZEROFILL	- Zero fills at the tail of write.
 *	IO_HEADZEROFILL	- Zero fills at the head of write.
 *	IO_NOZEROVALID	- Do not zero fill if valid page.
 *	IO_NOZERODIRTY	- Do not zero fill if page is dirty.
 *	IO_CLOSE	- The i/o was issued from close path.
 *	IO_NOCACHE	- Same effect as VNOCACHE_DATA, but only for this i/o.
 *	IO_RAOFF	- Same effect as VRAOFF, but only for this i/o.
 *	IO_DEFWRITE	- Defer write if vfs.defwrite is set.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 *
 * For sparse, compressed, and encrypted attributes we abort for now as we do
 * not support them yet.
 *
 * For non-resident attributes we use cluster_write_ext() which deals with
 * normal attributes.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_write(struct vnop_write_args *a)
{
	s64 size, nr_truncated;
	user_ssize_t start_count;
	off_t ofs;
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	uio_t uio = a->a_uio;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	int ioflags, err, count;
	BOOL write_locked, partial_write;

	partial_write = FALSE;
	nr_truncated = 0;
	ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	ioflags = a->a_ioflag;
	ntfs_debug("Entering for file inode 0x%llx, offset 0x%llx, count "
			"0x%llx, ioflags 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)start_count, ioflags);
	/*
	 * We can only write to regular files and named streams.
	 *
	 * Also, do not allow writing to system files and mst protected
	 * attributes.
	 */
	if (vnode_issystem(vn) || NInoMstProtected(ni) ||
			(!S_ISREG(ni->mode) && !(NInoAttr(ni) &&
			ni->type == AT_DATA))) {
		if (S_ISDIR(ni->mode))
			return EISDIR;
		return EPERM;
	}
	/* If nothing to do return success. */
	if (!start_count)
		return 0;
	/* Cannot write to a negative offset. */
	if (ofs < 0)
		return EINVAL;
	/* TODO: Deny access to encrypted attributes, just like NT4. */
	if (NInoEncrypted(ni)) {
		ntfs_warning(ni->vol->mp, "Denying write to encrypted "
				"attribute (EACCES).");
		return EACCES;
	}
// See:
//	~/svk/xnu/bsd/vfs/hfs_readwrite.c
//	~/svk/msdosfs/msdosfs.kextproj/msdosfs.kmodproj/msdosfs_vnops.c
//	~/svk/rocky/kext/rocky_vnops.cpp
	/*
	 * We cannot change the file size yet so appending i/o definitely is a
	 * no go.  TODO: We will want to take the inode lock exclusive on
	 * append i/o because we know that we will be extending the file...
	 */
	if (ioflags & IO_APPEND) {
		ntfs_error(ni->vol->mp, "Extending files is not implemented "
				"yet.");
		return ENOTSUP;
	}
	// FIXME: Need to clip i/o at maximum file size of 2^63-1 bytes in case
	// someone creates a sparse file and is playing silly with seek + write
	// note we only need to check for this for sparse files as non-sparse
	// files can never reach 2^63-1 because that is also the maximum space
	// on the volume thus the write would simply get an ENOSPC when the
	// volume is full.  For now we do not need to do the check at all as we
	// do not support writing to sparse files.
	if (NInoSparse(ni)) {
		ntfs_error(ni->vol->mp, "Writing to sparse files is not "
				"implemented yet.");
		return ENOTSUP;
	}
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also.
	 */
	lck_rw_lock_shared(&ni->lock);
	write_locked = FALSE;
	/* TODO: Do we need to look at ubc_getsize() at all here? */
	lck_spin_lock(&ni->size_lock);
	size = ubc_getsize(vn);
	if (size > ni->initialized_size)
		size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * We cannot write past the end of the initialized size yet so truncate
	 * the write if it goes past the end of the initialized size.
	 */
	if (ofs + start_count > size) {
		if (ofs >= size || ioflags & IO_UNIT) {
			ntfs_error(ni->vol->mp, "Cannot write past the end of "
					"the file because extending files is "
					"not implemented yet.");
			err = EFBIG;
			nr_truncated = 0;
			goto err;
		}
		partial_write = TRUE;
		nr_truncated = start_count - (size - ofs);
		start_count = size - ofs;
		uio_setresid(uio, start_count);
		ntfs_warning(ni->vol->mp, "Truncating write because it "
				"exceeds the end of the file and extending "
				"files is not implemented yet.");
	}
	if (NInoNonResident(ni)) {
		int (*callback)(buf_t, void *);

		if (NInoCompressed(ni) && !NInoRaw(ni)) {
#if 0
			err = ntfs_vnop_write_compressed(ni, uio, size,
					ioflags);
			if (!err)
				ntfs_debug("Done (ntfs_vnop_write_compressed()"
						").");
			else
				ntfs_error(ni->vol->mp, "Failed ("
						"ntfs_vnop_write_compressed(), "
						"error %d).", err);
#endif
			ntfs_error(ni->vol->mp, "Writing to compressed files "
					"is not implemented yet, sorry.");
			err = ENOTSUP;
			goto err;
		}
		callback = NULL;
		if (NInoEncrypted(ni)) {
			callback = ntfs_cluster_iodone;
			ntfs_error(ni->vol->mp, "Writing to encrypted files "
					"is not implemented yet, sorry.");
			err = ENOTSUP;
			goto err;
		}
		/*
		 * Note the first size is the original file size and the second
		 * file size is the new file size when the write is complete.
		 * Note how rocky updates both the file size and ubc_setsize()
		 * before calling cluster_write().  We can do this, too by
		 * setting the initialized size to a low value thus
		 * guaranteeing data integrity.  We would then need to update
		 * the initialized size as we complete i/os or something or
		 * simply after the cluster_write() completes perhaps...
		 * Need to figure out what to do in this case exactly...
		 *
		 * The first zero is the offset at which to begin zeroing when
		 * IO_HEADZEROFILL is set and the second zero it the offset at
		 * which to begin zeroing if IO_TAILZEROFILL is set.
		 */
		err = cluster_write_ext(vn, uio, ubc_getsize(vn), size, 0, 0,
				ioflags | IO_NOZERODIRTY | IO_NOZEROVALID,
				callback, NULL);
		if (!err)
			ntfs_debug("Done (cluster_write_ext()).");
		else
			ntfs_error(ni->vol->mp, "Failed (cluster_write_ext(), "
					"error %d).", err);
		goto done;
	} /* else if (!NInoNonResident(ni)) */
#if 0
	/*
	 * Try to copy the data from the uio into the VM page cache
	 * using cluster_copy_ubc_data().  If that succeeds it means
	 * the page cache is now dealt with.  Need to use the residual
	 * bytes value to determine if it worked or not.  If not then
	 * it means the page is not in memory and we need to call
	 * ntfs_map_page() to get it, then do a uiomove() to copy the
	 * data into the page.  The only time this is inefficient is
	 * when the whole page/attribute size is being overwritten in
	 * which case we do not need to bring uptodate the page by
	 * reading it in.  We only need to create a upl and copy
	 * straight into it with cluster_copy_upl_data() and then
	 * commit the upl in this case.
	 * Then mark the page dirty as well as setting NInoDirtyData()
	 * to ensure the page will get written out when the inode is
	 * synced (which will happen at the very latest when the last
	 * reference to the vnode is released).
	 * Finally if synchronous i/o, call ntfs_vnop_pageout to flush
	 * out the data to the mft record and that to disk...
	 */
#endif
	/*
	 * That attribute is resident thus we have to deal with it by
	 * ourselves.  First of all, try to copy the data to the vm page cache.
	 * This will work on the second and all later writes so this is the hot
	 * path.  If the attribute has not been accessed at all before or its
	 * cached pages were dropped due to vm pressure this will fail to copy
	 * any data due to the lack of a valid page and we will drop into the
	 * slow path.
	 */
	size -= ofs;
	if (size > start_count)
		size = start_count;
	if (size > PAGE_SIZE) {
		ntfs_warning(ni->vol->mp, "Unexpected count 0x%llx > "
				"PAGE_SIZE 0x%x, overriding it to PAGE_SIZE.",
				(unsigned long long)size, PAGE_SIZE);
		size = PAGE_SIZE;
	}
	count = size;
	/*
	 * Note we pass mark_dirty = 1 (the last parameter) which means the
	 * pages that are written to will be marked dirty.
	 */
	err = cluster_copy_ubc_data(vn, uio, &count, 1);
	if (err) {
		/* The copying (uiomove()) failed with an error, abort. */
		ntfs_error(ni->vol->mp, "cluster_copy_ubc_data() failed "
				"(error %d).", err);
		goto err;
	}
	/*
	 * @count is now set to the number of bytes remaining to be
	 * transferred.  If it is zero, it means we are done.
	 *
	 * TODO: In the synchronous i/o case we need to cause the dirtied page
	 * to be written out now or we need to skip the cluster_copy_ubc_data()
	 * optimization altogether and fall through to the slow path (the
	 * latter is what the kernel does in cluster_write())...
	 */
	if (!count) {
		/* Record the fact that the inode has dirty pages. */
		NInoSetDirtyData(ni);
		ntfs_debug("Done (resident, cached, wrote 0x%llx bytes).",
				(unsigned long long)size);
		goto done;
	}
	/*
	 * We failed to transfer everything.  That really means we failed to
	 * transfer anything at all as we are guaranteed that a resident
	 * attribute is smaller than a page thus either the page is there and
	 * valid and we transfer everything or it is not and we transfer
	 * nothing.
	 */
	if (count != size) {
		ntfs_warning(ni->vol->mp, "Unexpected partial transfer to "
				"cached page (size 0x%llx, count 0x%x).",
				(unsigned long long)size, count);
		ofs = uio_offset(uio);
	}
	/*
	 * The page is not in cache or is not valid.  We need to bring it into
	 * cache and make it valid so we can then copy the data in.  The
	 * easiest way to do this is to just map the page which will take care
	 * of everything for us.  We can then uiomove() straight into the page
	 * from the @uio and then mark the page dirty and unmap it again.
	 *
	 * FIXME: Note this will take the inode lock again but this is ok at
	 * present as in both cases the lock is taken shared.
	 *
	 * TODO: If the write covers the whole existing attribute then making
	 * the page valid is a waste of time.  It is by far more efficient to
	 * implement a ntfs_page_grab() function (or do it by hand here) that
	 * just gets the page and does not make it valid.  Then we can copy
	 * straigt into the page, zero any area outside the attribute
	 * size/initialized size that is beyond the write and mark the page
	 * valid and dirty.  Having said that, the next time a write occurs,
	 * the page will already be valid and then ntfs_page_map() will be
	 * identical to ntfs_page_grab() thus is it worth our while to do all
	 * this extra work for the sake of a single first write special case?
	 */
	err = ntfs_page_map(ni, 0, &upl, &pl, &kaddr, TRUE);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map page (error %d).", err);
		goto err;
	}
	err = uiomove((void*)(kaddr + ofs), count, uio);
	ntfs_page_unmap(ni, upl, pl, TRUE);
	if (err) {
		ntfs_error(ni->vol->mp, "uiomove() failed (error %d).", err);
		goto err;
	}
	// TODO: In the synchronous i/o case we need to cause the dirtied page
	// to be written out now.  Note this must happen after the
	// ntfs_page_unmap() to maintain coherency with mmapped writes.  For
	// details see the big comment above the call to ubc_create_upl() in
	// xnu/bsd/vfs/vfs_cluster.c::cluster_push_now() and the comment
	// above the last ubc_upl_commit_range() in cluster_write_copy() in the
	// same file.
	// TODO: This is also where we need to throw out the pages if the vnode
	// has the VNOCACHE_DATA flag set or this i/o is IO_NOCACHE.
	/* Record the fact that the inode has dirty pages. */
	NInoSetDirtyData(ni);
	// FIXME: The "not cached" may be wrong if we fell through due to
	// synchronous i/o case.
	ntfs_debug("Done (resident, not cached, wrote 0x%llx bytes).",
			(unsigned long long)size - uio_resid(uio));
done:
	// TODO: Do ubc_setsize() if end offset (uio_offset()) is greater than
	// the end of the file (file size).
	// TODO: On successful write update ctime and mtime.
	// TODO: If we wrote anything at all we have to clear the setuid and
	// setgid bits as a precaution against tampering (see xnu/bsd/hfs/
	// hfs_readwrite.c::hfs_vnop_write()).
err:
	if (partial_write)
		uio_setresid(uio, uio_resid(uio) + nr_truncated);
	if (!write_locked)
		lck_rw_unlock_shared(&ni->lock);
	else
		lck_rw_unlock_exclusive(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_ioctl -
 *
 */
static int ntfs_vnop_ioctl(struct vnop_ioctl_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_select -
 *
 */
static int ntfs_vnop_select(struct vnop_select_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_exchange -
 *
 */
static int ntfs_vnop_exchange(struct vnop_exchange_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
#ifdef NTFS_RW
	// TODO:
	err = ENOTSUP;
#else
	err = EROFS;
#endif
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_kqfilt_add -
 *
 */
static int ntfs_vnop_kqfilt_add(struct vnop_kqfilt_add_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_kqfilt_remove -
 *
 */
static int ntfs_vnop_kqfilt_remove(struct vnop_kqfilt_remove_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_mmap - map a file (vnode) into memory
 * @a:		arguments to mmap function
 *
 * @a contains:
 *	vnode_t a_vp;			file vnode which to map into memory
 *	int a_fflags;			mapping flags for the vnode
 *	vfs_context_t a_context;
 *
 * Map the file vnode @a->a_vp into memory applying the mapping flags
 * @a->a_fflags which are a combination of one or more of PROT_READ,
 * PROT_WRITE, and PROT_EXEC.
 *
 * VNOP_MMAP() and hence ntfs_vnop_mmap() gets called from ubc_map() which in
 * turn gets called from the mmap() system call when a file is being mapped
 * into memory.
 *
 * The mmap() system call does the necessary permission checking and in fact
 * ignores the return value from ubc_map() and relies on things not working
 * later on for error handling.
 *
 * ubc_map() on the other hand does look at the return value of VNOP_MMAP() but
 * it only cares for one error code and that is EPERM.  All other errors are
 * ignored and not passed to its caller.  Thus for any return value not equal
 * to EPERM, ubc_map() takes an extra reference on the vnode and sets the flags
 * UI_ISMAPPED and UI_WASMAPPED in the ubc info of the vnode and for EPERM it
 * does not do anything and just returns EPERM to the caller.
 *
 * In effect neither class of return value (EPERM or not EPERM) actually has
 * any effect at all so we do not bother doing any checking here and defer all
 * checks to VNOP_PAGEIN() and hence ntfs_vnop_pagein().
 *
 * FIXME: This is a huge problem because it means that anyone can use mmap() on
 * a system file and then write rubbish into the mapped memory and then trash
 * the metadata in the mapped memory by calling msync() to write the rubbish
 * out into the system file on disk!  This will need to be fixed in the kernel
 * I think, i.e. the mmap() system call must fail if VNOP_MMAP() fails.  This
 * is because we have no way to tell who is causing a page{in,out} at
 * ntfs_vnop_page{in,out}() time and for what reason so we have to always
 * permit page{in,out} to be called.
 *
 * Return 0 on success and EPERM on error.
 */
static int ntfs_vnop_mmap(struct vnop_mmap_args *a)
{
#ifdef DEBUG
	ntfs_inode *ni = NTFS_I(a->a_vp);

	ntfs_debug("Mapping mft_no 0x%llx, type 0x%x, name_len 0x%x, mapping "
			"flags 0x%x.", (unsigned long long)ni->mft_no,
			le32_to_cpu(ni->type), (unsigned)ni->name_len,
			a->a_fflags);
#endif
	/* Nothing to do. */
	return 0;
}

/**
 * ntfs_vnop_mnomap - unmap a file (vnode) from memory
 * @a:		arguments to mnomap function
 *
 * @a contains:
 *	vnode_t a_vp;			file vnode which to unmap from memory
 *	vfs_context_t a_context;
 *
 * Remove the memory mapping of the file vnode @a->a_vp that was previously
 * established via ntfs_vnop_mmap().
 *
 * VNOP_MNOMAP() and hence ntfs_vnop_mnomap() gets called from ubc_unmap() when
 * a file is being unmapped from memory via the munmap() system call.
 *
 * ubc_unmap() only calls VNOP_MNOMAP() if the previous VNOP_MMAP() call did
 * not return EPERM.
 *
 * ubc_unmap() completely ignores the return value from VNOP_MNOMAP().
 *
 * Always return 0 as the return value is always ignored.
 */
static int ntfs_vnop_mnomap(struct vnop_mnomap_args *a)
{
#ifdef DEBUG
	ntfs_inode *ni = NTFS_I(a->a_vp);

	ntfs_debug("Unmapping mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
#endif
	/* Nothing to do. */
	return 0;
}

/**
 * ntfs_vnop_fsync - synchronize a vnode's in-code state with that on disk
 * @a:		arguments to fsync function
 *
 * @a contains:
 *	vnode_t a_vp;			vnode which to sync
 *	int a_waitfor;			if MNT_WAIT wait for i/o to complete
 *	vfs_context_t a_context;
 *
 * Write all dirty cached data belonging/related to the vnode @a->a_vp to disk.
 *
 * If @a->a_waitfor is MNT_WAIT, wait for all i/o to complete before returning.
 *
 * Return 0 on success and the error code on error.
 */
static int ntfs_vnop_fsync(struct vnop_fsync_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	int sync, err;

	sync = (a->a_waitfor == MNT_WAIT) ? IO_SYNC : 0;
	ntfs_debug("Entering for inode 0x%llx, waitfor 0x%x, %ssync i/o.",
			(unsigned long long)ni->mft_no, a->a_waitfor,
			(sync == IO_SYNC) ? "a" : "");
	if (!vnode_hasdirtyblks(vn) && !NInoDirty(ni) && !NInoDirtyData(ni)) {
		ntfs_debug("Done (nothing to do).");
		return 0;
	}
	err = ntfs_inode_sync(ni, sync);
	ntfs_debug("Done (error %d).", err);
	return err;
}

/**
 * ntfs_vnop_remove -
 *
 */
static int ntfs_vnop_remove(struct vnop_remove_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_link -
 *
 */
static int ntfs_vnop_link(struct vnop_link_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_rename -
 *
 */
static int ntfs_vnop_rename(struct vnop_rename_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_mkdir -
 *
 */
static int ntfs_vnop_mkdir(struct vnop_mkdir_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_rmdir -
 *
 */
static int ntfs_vnop_rmdir(struct vnop_rmdir_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_symlink -
 *
 */
static int ntfs_vnop_symlink(struct vnop_symlink_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_readdir - read directory entries into a supplied buffer
 * @a:		arguments to readdir function
 *
 * @a contains:
 *	vnode_t a_vp;		directory vnode to read directory entries from
 *	uio_t a_uio;		destination in which to return the entries
 *	int a_flags;		flags describing the entries to return
 *	int *a_eofflag;		return end of file status (can be NULL)
 *	int *a_numdirent;	return number of entries returned (can be NULL)
 *	vfs_context_t a_context;
 *
 * See ntfs_dir.c::ntfs_readdir() for a description of the implemented
 * features.  In addition to those described features VNOP_READDIR() should
 * also implement the below features.
 *
 * @a->a_flags can have the following bits set:
 *	VNODE_READDIR_EXTENDED		use extended directory entries
 *	VNODE_READDIR_REQSEEKOFF	requires seek offset (cookies)
 *	VNODE_READDIR_SEEKOFF32		seek offset values should be 32-bit
 *
 * When VNODE_READDIR_EXTENDED is set, the format of the returned directory
 * entry structures changes to the direntry structure which is defined as:
 *
 *	ino64_t	d_ino;			inode number of entry
 *	u64	d_seekoff;		seek offset (optional, used by servers)
 *	u16	d_reclen;		length of this record
 *	u16	d_namlen;		length of string in d_name
 *	u8	d_type;			inode type (one of DT_DIR, DT_REG, etc)
 *	char	d_name[MAXPATHLEN - 1];	null terminated filename
 *
 * If VNODE_READDIR_REQSEEKOFF is set, VNODE_READDIR_EXTENDED must also be set,
 * and it means that the seek offset (d_seekoff) in the direntry structure must
 * be set.  If VNODE_READDIR_REQSEEKOFF is not set, the seek offset can be set
 * to zero as the caller will ignore it.
 *
 * If VNODE_READDIR_SEEKOFF32 is set, both VNODE_READDIR_EXTENDED and
 * VNODE_READDIR_REQSEEKOFF must be set and it means that the seek offset must
 * be at most 32-bits, i.e. the most significant 32-bits of d_seekoff must be
 * zero.
 *
 * All the VNODE_READDIR_* flags are only ever set by the NFS server and given
 * we do not yet support NFS exporting of NTFS volumes we just abort if any of
 * them are set.
 *
 * If the directory is deleted-but-in-use, we do not synthesize entries for "."
 * and "..".
 *
 * Return 0 on success and the error code on error.
 */
static int ntfs_vnop_readdir(struct vnop_readdir_args *a)
{
	ntfs_inode *dir_ni = NTFS_I(a->a_vp);
	errno_t err;

	ntfs_debug("Entering for directory inode 0x%llx.",
			(unsigned long long)dir_ni->mft_no);
	/*
	 * FIXME: Is this check necessary?  Can we ever get here for
	 * non-directories?  All current callers (except the NFS server) ensure
	 * that @dir_ni is a directory.  We do not currently support NFS
	 * exporting so this should indeed definitely never trigger but leave
	 * it here as a kind of debug assertion.
	 */
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_error(dir_ni->vol->mp, "Not a directory!");
		return ENOTDIR;
	}
	if (a->a_flags) {
		ntfs_error(dir_ni->vol->mp, "None of the VNODE_READDIR_* "
				"flags are supported yet, sorry.");
		return ENOTSUP;
	}
	err = ntfs_readdir(dir_ni, a->a_uio, a->a_eofflag, a->a_numdirent);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_readdirattr -
 *
 */
static int ntfs_vnop_readdirattr(struct vnop_readdirattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	(void)nop_readdirattr(a);
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_readlink -
 *
 */
static int ntfs_vnop_readlink(struct vnop_readlink_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_inactive - the last reference to a vnode has been dropped
 * @a:		arguments to inactive function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode whose last reference has been dropped
 *	vfs_context_t a_context;
 *
 * Last reference to a vnode (io_count and use_count reached zero) or a forced
 * unmount is in progress.
 *
 * TODO: If dirty, write it or if hard link count is zero, delete it.
 *
 * TODO: If deleted, call vnode_recycle() when finished so the vnode can be
 * reused immediately.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_inactive(struct vnop_inactive_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	int err = 0;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	/* Last chance to commit dirty data to disk. */
	if (vnode_hasdirtyblks(vn) || NInoDirty(ni) || NInoDirtyData(ni))
		err = ntfs_inode_sync(ni, IO_SYNC | IO_CLOSE);
	ntfs_debug("Done (error %d).", err);
	return err;
}

/**
 * ntfs_vnop_reclaim - free ntfs specific parts of a vnode so it can be reused
 * @a:		arguments to reclaim function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to be reclaimed
 *	vfs_context_t a_context;
 *
 * Reclaim a vnode so it can be used for other purposes.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_reclaim(struct vnop_reclaim_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	vnode_removefsref(vn);
	err = ntfs_inode_reclaim(ni);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_pathconf - get configurable pathname variables
 * @a:		arguments to pathconf function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode for which to return pathconf information
 *	int a_name;		the pathconf variable to be queried
 *	register_t *a_retval;	destination for result of query
 *	vfs_context_t a_context;
 *
 * Return POSIX pathconf information applicable to ntfs file system.  Some
 * @a_name values are intercepted by the VFS in vn_pathconf (pathconf(2) ->
 * vn_pathconf() -> VNOP_PATHCONF() -> ntfs_vnop_pathconf()) so we do not
 * bother with them.
 *
 * Return 0 on success and EINVAL if an unsupported @a_name was queried for.
 */
static int ntfs_vnop_pathconf(struct vnop_pathconf_args *a)
{
	ntfs_inode *ni = NTFS_I(a->a_vp);
	errno_t err = 0;

	ntfs_debug("Entering for pathconf variable number %d.", a->a_name);
	switch (a->a_name) {
	case _PC_LINK_MAX:
		/*
		 * The maximum file link count.  For ntfs, the link count is
		 * stored in the mft record in the link_count field which is of
		 * type le16, thus 16 bits.  For attribute inodes and
		 * directories however, no hard links are allowed and thus the
		 * maximum link count is 1.
		 */
		*a->a_retval = 65535; /* 2^16 - 1 */
		if (NInoAttr(ni) || S_ISDIR(ni->mode))
			*a->a_retval = 1;
		break;
	case _PC_NAME_MAX:
		/*
		 * The maximum number of bytes in a filename.  For ntfs, this
		 * is stored in the attribute record in the name_length field
		 * which is of type u8, thus 8 bits.
		 */
		*a->a_retval = NTFS_MAX_NAME_LEN; /* 255 */
		break;
	case _PC_PATH_MAX:
		/*
		 * The maximum number of bytes in a path name.  Ntfs imposes no
		 * restrictions so use the system limit.
		 */
		*a->a_retval = PATH_MAX; /* 1024 */
		break;
	case _PC_PIPE_BUF:
		/*
		 * The maximum number of bytes which will be written atomically
		 * to a pipe, again ntfs imposes no restrictions so use the
		 * system limit.
		 */
		*a->a_retval = PIPE_BUF; /* 512 */
		break;
	case _PC_CHOWN_RESTRICTED:
		/*
		 * Non-zero if appropriate privileges are required for the
		 * chown(2) system call.  For ntfs, this is always the case.
		 */
		*a->a_retval = 200112; /* unistd.h: _POSIX_CHOWN_RESTRICTED */
		break;
	case _PC_NO_TRUNC:
		/*
		 * Non-zero if accessing filenames longer than _POSIX_NAME_MAX
		 * (which we specified above to be NTFS_MAX_NAME_LEN) generates
		 * an error.  For ntfs, this is always the case.
		 */
		*a->a_retval = 200112; /* unistd.h: _POSIX_NO_TRUNC */
		break;
	case _PC_NAME_CHARS_MAX:
		/*
		 * The maximum number of characters in a filename.  This is
		 * the same as _PC_NAME_MAX, above.
		 */
		*a->a_retval = NTFS_MAX_NAME_LEN; /* 255 */
		break;
	case _PC_CASE_SENSITIVE:
		/*
		 * Return 1 if case sensitive and 0 if not.  For ntfs, this
		 * depends on the mount options.
		 */
		*a->a_retval = NVolCaseSensitive(ni->vol);
		break;
	case _PC_CASE_PRESERVING:
		/*
		 * Return 1 if case preserving and 0 if not.  For ntfs, this is
		 * always 1, i.e. ntfs always preserves case.
		 */
		*a->a_retval = 1;
		break;
	case _PC_FILESIZEBITS:
		/*
		 * The number of bits to represent file size.  For ntfs, the
		 * file size is stored in the attribute record in the data_size
		 * field which is of type sle64, thus 63 bits.
		 */
		*a->a_retval = 63;
		break;
	default:
		err = EINVAL;
	}
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_allocate -
 *
 * Equivalent of truncate() on Linux.
 */
static int ntfs_vnop_allocate(struct vnop_allocate_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	(void)nop_allocate(a);
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
ntfs_error(NULL, "Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_pagein - read a range of pages into memory
 * @a:		arguments to pagein function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode whose data to read into the page range
 *	upl_t a_pl;		page list describing destination page range
 *	vm_offset_t a_pl_offset; byte offset into page list at which to start
 *	off_t a_f_offset;	byte offset in the vnode at which to start
 *	size_t a_size;		number of bytes to read from the vnode
 *	int a_flags;		flags further describing the pagein request
 *	vfs_context_t a_context;
 *
 * Read @a->a_size bytes from the vnode @a-a_vp, starting at byte offset
 * @a->a_f_offset into the vnode, into the range of pages specified by the page
 * list @a->a_pl, starting at byte offset @a->a_pl_offset into the page list.
 *
 * The flags in @a->a_flags further describe the pagein request.  The following
 * pagein flags are currently defined in OSX kernel:
 *	UPL_IOSYNC	- Perform synchronous i/o.
 *	UPL_NOCOMMIT	- Do not commit/abort the page range.
 *	UPL_NORDAHEAD	- Do not perform any speculative read-ahead.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 *
 * For encrypted attributes we abort for now as we do not support them yet.
 *
 * For non-resident, non-compressed attributes we use cluster_pagein_ext()
 * which deals with both normal and multi sector transfer protected attributes.
 *
 * For resident attributes and non-resident, compressed attributes we read the
 * data ourselves by mapping the page list, and in the resident case, mapping
 * the mft record, looking up the attribute in it, and copying the requested
 * data from the mapped attribute into the page list, then unmapping the mft
 * record, whilst for non-resident, compressed attributes, we get the raw inode
 * and use it with ntfs_read_compressed() to read and decompress the data into
 * our mapped page list.  We then unmap the page list and finally, if
 * UPL_NOCOMMIT is not specified, we commit (success) or abort (error) the page
 * range.
 *
 * Return 0 on success and errno on error.
 *
 * Note the pages in the page list are marked busy on entry and the busy bit is
 * cleared when we commit the page range.  Thus it is perfectly safe for us to
 * fill the pages with encrypted or mst protected data and to decrypt or mst
 * deprotect in place before committing the page range.
 *
 * Adapted from cluster_pagein_ext().
 */
static int ntfs_vnop_pagein(struct vnop_pagein_args *a)
{
	ntfs_inode *ni = NTFS_I(a->a_vp);
	int err;

	ntfs_debug("Entering for mft_no 0x%llx, offset 0x%llx, size 0x%llx, "
			"pagein flags 0x%x, page list offset 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)a->a_f_offset,
			(unsigned long long)a->a_size, a->a_flags,
			(unsigned long long)a->a_pl_offset);
	err = ntfs_pagein(ni, a->a_f_offset, a->a_size, a->a_pl,
			a->a_pl_offset, a->a_flags);
	return err;
}

// TODO: Move to ntfs_page.[hc].
static int ntfs_mst_pageout(ntfs_inode *ni, upl_t upl, vm_offset_t upl_ofs,
		unsigned size, s64 attr_ofs, s64 attr_size, int flags)
{
	u8 *kaddr;
	kern_return_t kerr;
	unsigned rec_size, rec_shift, nr_recs, i;
	int err;
	BOOL do_commit = !(flags & UPL_NOCOMMIT);

	ntfs_debug("Entering for mft_no 0x%llx, page list offset 0x%llx, size "
			"0x%x, offset 0x%llx, pageout flags 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)upl_ofs, size,
			(unsigned long long)attr_ofs, flags);
	if (attr_ofs < 0 || attr_ofs >= attr_size || attr_ofs & PAGE_MASK_64 ||
			size & PAGE_MASK || upl_ofs & PAGE_MASK) {
		err = EINVAL;
		goto err;
	}
	if (!NInoMstProtected(ni))
		panic("%s(): Called for non-mst protected attribute.\n",
				__FUNCTION__);
	if (!NInoNonResident(ni))
		panic("%s(): Resident mst protected attribute.\n",
				__FUNCTION__);
	rec_size = ni->block_size;
	if (attr_ofs & (rec_size - 1) || size & (rec_size - 1))
		panic("%s(): Write not aligned to NTFS record boundary.\n",
				__FUNCTION__);
	rec_shift = ni->block_size_shift;
	/* Clip the number of records to the size of the attribute. */
	nr_recs = size >> rec_shift;
	if (attr_ofs + size > attr_size) {
		unsigned to_write;

		/* Abort any pages outside the end of the attribute. */
		to_write = attr_size - attr_ofs;
		nr_recs = to_write >> rec_shift;
		to_write = (to_write + PAGE_MASK) & ~PAGE_MASK;
		if (size != to_write) {
			if (size < to_write)
				panic("%s(): size less than to_write.\n",
						__FUNCTION__);
			ntfs_debug("Truncating write past end of attribute.");
			if (do_commit)
				ubc_upl_abort_range(upl, upl_ofs + to_write,
						size - to_write,
						UPL_ABORT_FREE_ON_EMPTY);
			size = to_write;
		}
	}
	if (!nr_recs)
		panic("%s(): NTFS record size greater than write size.\n",
				__FUNCTION__);
	/*
	 * Need to apply the mst fixups and abort on errors.  To apply the
	 * fixups need to map the page list so we can access its contents.
	 */
	kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_map() failed (error %d).",
				(int)kerr);
		err = EIO;
		goto err;
	}
	/*
	 * Loop over the records in the page list and for each apply the mst
	 * fixups.  On any fixup errors, remove all the applied fixups and
	 * abort the write completely.
	 */
	for (i = 0; i < nr_recs; i++) {
		err = ntfs_mst_fixup_pre_write((NTFS_RECORD*)(kaddr +
				(i << rec_shift)), rec_size);
		if (err) {
			ntfs_error(ni->vol->mp, "Failed to apply mst fixups.");
			goto mst_err;
		}
	}
	/* Unmap the page list again so we can call cluster_pageout_ext(). */
	// FIXME: Can we leave the page list mapped throughout the
	// cluster_pageout_ext() call?  That would be a lot more efficient and
	// simplify error handling.
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
		err = EIO;
		goto mst_err;
	}
	/*
	 * We need the write to be synchronous so we do not leave the metadata
	 * with the fixups applied for too long.
	 *
	 * We also need to set the no commit flag so we can still recover from
	 * errors by removing the fixups.
	 */
	flags |= UPL_IOSYNC | UPL_NOCOMMIT;
	/*
	 * On success the fixups will have been removed by the
	 * ntfs_cluster_iodone callback.
	 */
	err = cluster_pageout_ext(ni->vn, upl, upl_ofs, attr_ofs, size,
			attr_size, flags, ntfs_cluster_iodone, NULL);
	if (!err) {
		if (do_commit) {
			/* Commit the page range we wrote out. */
			ubc_upl_commit_range(upl, upl_ofs, size,
					UPL_COMMIT_FREE_ON_EMPTY);
		}
		ntfs_debug("Done.");
		return err;
	}
	ntfs_error(ni->vol->mp, "Failed (cluster_pageout_ext() returned error "
			"%d).", err);
	/*
	 * We may have some records left with applied fixups thus remove them
	 * again.  It does not matter if it is done twice as this is an error
	 * code path and the only side effect is a little slow down.
	 */
	kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_map() failed (error %d), "
				"cannot remove mst fixups.  Unmount and run "
				"chkdsk.", (int)kerr);
		NVolSetErrors(ni->vol);
		goto err;
	}
mst_err:
	/* Remove the applied fixups, unmap the page list and abort. */
	while (i > 0)
		ntfs_mst_fixup_post_write((NTFS_RECORD*)
				(kaddr + (--i << rec_shift)));
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS)
		ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
err:
	if (do_commit) {
		int upl_flags = UPL_ABORT_FREE_ON_EMPTY;
		if (err == ENOMEM)
			upl_flags |= UPL_ABORT_RESTART;
		ubc_upl_abort_range(upl, upl_ofs, size, upl_flags);
	}
	return err;
}

/**
 * ntfs_vnop_pageout - write a range of pages to storage
 * @a:		arguments to pageout function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode whose data to write from the page range
 *	upl_t a_pl;		page list describing the source page range
 *	vm_offset_t a_pl_offset; byte offset into page list at which to start
 *	off_t a_f_offset;	byte offset in the vnode at which to start
 *	size_t a_size;		number of bytes to write to the vnode
 *	int a_flags;		flags further describing the pageout request
 *	vfs_context_t a_context;
 *
 * If UPL_NESTED_PAGEOUT is set in the flags (a->a_flags) we are called from
 * cluster_io() which is in turn called from cluster_write() which is in turn
 * called from ntfs_vnop_write() which means we are already holding the inode
 * lock (@ni->lock).  Alternatively cluster_io() can be called from
 * cluster_push() which can be called from various places in NTFS.
 *
 * Write @a->a_size bytes to the vnode @a-a_vp, starting at byte offset
 * @a->a_f_offset into the vnode, from the range of pages specified by the page
 * list @a->a_pl, starting at byte offset @a->a_pl_offset into the page list.
 *
 * The flags in @a->a_flags further describe the pageout request.  The
 * following pageout flags are currently defined in OSX kernel:
 *	UPL_IOSYNC	- Perform synchronous i/o.
 *	UPL_NOCOMMIT	- Do not commit/abort the page range.
 *	UPL_KEEPCACHED	- Data is already cached in memory, keep it cached.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 *
 * For encrypted attributes we abort for now as we do not support them yet.
 *
 * For non-resident, non-compressed attributes we use cluster_pageout_ext()
 * which deals with both normal and multi sector transfer protected attributes.
 *
 * In the case of multi sector transfer protected attributes we apply the
 * fixups and then submit the i/o synchronously by setting the UPL_IOSYNC flag.
 *
 * For resident attributes and non-resident, compressed attributes we write the
 * data ourselves by mapping the page list, and in the resident case, mapping
 * the mft record, looking up the attribute in it, and copying the data to the
 * mapped attribute from the page list, then unmapping the mft record, whilst
 * for non-resident, compressed attributes, we get the raw inode and use it
 * with ntfs_write_compressed() to compress and write the data from our mapped
 * page list.  We then unmap the page list and finally, if UPL_NOCOMMIT is not
 * specified, we commit (success) or abort (error) the page range.
 *
 * Return 0 on success and errno on error.
 *
 * Note the pages in the page list are marked busy on entry and the busy bit is
 * cleared when we commit the page range.  Thus it is perfectly safe for us to
 * apply the mst fixups and write out the data which will then also take away
 * the fixups again before committing the page range.
 *
 * Adapted from cluster_pageout_ext().
 */
static int ntfs_vnop_pageout(struct vnop_pageout_args *a)
{
	s64 attr_ofs, attr_size, bytes;
	ntfs_inode *ni = NTFS_I(a->a_vp);
	upl_t upl = a->a_pl;
	u8 *kaddr;
	vm_offset_t upl_ofs = a->a_pl_offset;
	kern_return_t kerr;
	unsigned size, to_write;
	int err, flags = a->a_flags;
	BOOL locked = FALSE;

	attr_ofs = a->a_f_offset;
	size = a->a_size;
	ntfs_debug("Entering for mft_no 0x%llx, offset 0x%llx, size 0x%x, "
			"pageout flags 0x%x, page list offset 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)attr_ofs, size, flags,
			(unsigned long long)upl_ofs);
	/*
	 * If the caller did not specify any i/o, then we are done.  We cannot
	 * issue an abort because we do not have a upl or we do not know its
	 * size.
	 */
	if (!upl || size <= 0) {
		ntfs_error(ni->vol->mp, "NULL page list passed in or request "
				"size is below zero (error EINVAL).");
		return EINVAL;
	}
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		err = EISDIR;
		goto err;
	}
	if (vnode_vfsisrdonly(a->a_vp)) {
		err = EROFS;
		goto err;
	}
	// FIXME: Need to clip i/o at maximum file size of 2^63-1 bytes in case
	// someone creates a sparse file and is playing silly with seek + write
	// note we only need to check for this for sparse files as non-sparse
	// files can never reach 2^63-1 because that is also the maximum space
	// on the volume thus the write would simply get an ENOSPC when the
	// volume is full.  For now we do not need to do the check at all as we
	// do not support writing to sparse files.
	if (NInoSparse(ni)) {
		ntfs_error(ni->vol->mp, "Writing to sparse files is not "
				"implemented yet.");
		err = ENOTSUP;
		goto err;
	}
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also but only if the VFS is not calling us after we (the
	 * NTFS driver) called it in which case we already would be holding the
	 * lock.
	 */
	if (!(flags & UPL_NESTED_PAGEOUT)) {
		lck_rw_lock_shared(&ni->lock);
		locked = TRUE;
	}
retry_pageout:
	/*
	 * TODO: This check may no longer be necessary now that we lock against
	 * changes in initialized size and thus truncation...  Revisit this
	 * issue when the write code has been written and remove the check if
	 * appropriate simply using ubc_getsize(vn); without the size_lock.
	 */
	lck_spin_lock(&ni->size_lock);
	attr_size = ubc_getsize(a->a_vp);
	if (attr_size > ni->data_size)
		attr_size = ni->data_size;
	// FIXME: For now abort writes beyond initialized size...
	if (attr_ofs + size > ni->initialized_size && ni->initialized_size !=
			ni->data_size) {
		lck_spin_unlock(&ni->size_lock);
		ntfs_error(ni->vol->mp, "Writing beyond the initialized size "
				"of a file is not implemented yet, sorry.");
		err = ENOTSUP;
		goto err;
	}
	lck_spin_unlock(&ni->size_lock);
	/*
	 * Only $DATA attributes can be encrypted/compressed.  Index root can
	 * have the flags set but this means to create compressed/encrypted
	 * files, not that the attribute is compressed/encrypted.  Note we need
	 * to check for AT_INDEX_ALLOCATION since this is the type of directory
	 * index inodes.
	 */
	if (ni->type != AT_INDEX_ALLOCATION) {
		/* TODO: Deny access to encrypted attributes, just like NT4. */
		if (NInoEncrypted(ni)) {
			if (ni->type != AT_DATA)
				panic("%s(): Encrypted non-data attribute.\n",
						__FUNCTION__);
			ntfs_warning(ni->vol->mp, "Denying write to encrypted "
					"attribute (EACCES).");
			err = EACCES;
			goto err;
		}
		/* Compressed data streams need special handling. */
		if (NInoNonResident(ni) && NInoCompressed(ni) && !NInoRaw(ni)) {
			if (ni->type != AT_DATA)
				panic("%s(): Compressed non-data attribute.\n",
						__FUNCTION__);
			goto compressed;
		}
	}
	/* NInoNonResident() == NInoIndexAllocPresent() */
	if (NInoNonResident(ni)) {
		if (NInoMstProtected(ni))
			err = ntfs_mst_pageout(ni, upl, upl_ofs, size,
					attr_ofs, attr_size, flags);
		else {
			err = cluster_pageout_ext(a->a_vp, upl, upl_ofs,
					attr_ofs, size, attr_size, flags, NULL,
					NULL);
			if (!err)
				ntfs_debug("Done (cluster_pageout_ext()).");
			else
				ntfs_error(ni->vol->mp, "Failed "
						"(cluster_pageout_ext(), "
						"error %d).", err);
		}
		// TODO: If we wrote anything at all we have to clear the
		// setuid and setgid bits as a precaution against tampering
		// (see xnu/bsd/hfs/hfs_readwrite.c::hfs_vnop_pageout()).
		if (locked)
			lck_rw_unlock_shared(&ni->lock);
		return err;
	}
compressed:
	/*
	 * The attribute is resident and/or compressed.
	 *
	 * Cannot pageout to a negative offset or if we are starting beyond
	 * the end of the attribute or if the attribute offset is not page
	 * aligned or the size requested is not a multiple of PAGE_SIZE.
	 */
	if (attr_ofs < 0 || attr_ofs >= attr_size || attr_ofs & PAGE_MASK_64 ||
			size & PAGE_MASK || upl_ofs & PAGE_MASK) {
		err = EINVAL;
		goto err;
	}
	to_write = size;
	bytes = attr_size - attr_ofs;
	if (to_write > bytes)
		to_write = bytes;
	/*
	 * Calculate the number of bytes available in the attribute starting at
	 * offset @attr_ofs up to a maximum of the number of bytes to be
	 * written rounded up to a multiple of the system page size.
	 */
	bytes = (to_write + PAGE_MASK) & ~PAGE_MASK;
	/* Abort any pages outside the end of the attribute. */
	if (size > bytes && !(flags & UPL_NOCOMMIT)) {
		ubc_upl_abort_range(upl, upl_ofs + bytes, size - bytes,
				UPL_ABORT_FREE_ON_EMPTY);
		/* Update @size. */
		size = bytes;
	}
	/* To access the page list contents, we need to map the page list. */
	kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_map() failed (error %d).",
				(int)kerr);
		err = EIO;
		goto err;
	}
	if (!NInoNonResident(ni)) {
		/*
		 * Write the data from the page list into the resident
		 * attribute in its mft record.
		 */
		err = ntfs_resident_attr_write(ni, kaddr + upl_ofs, to_write,
				attr_ofs);
		// TODO: If !err and synchronous i/o, write the mft record now.
		// This should probably happen in ntfs_resident_attr_write().
		if (err && err != EAGAIN)
			ntfs_error(ni->vol->mp, "ntfs_resident_attr_write() "
					"failed (error %d).", err);
	} else {
		ntfs_error(ni->vol->mp, "Writing to compressed files is not "
				"implemented yet, sorry.");
		err = ENOTSUP;
#if 0
		ntfs_inode *raw_ni;
		int ioflags;

		/* Get the raw inode. */
		err = ntfs_raw_inode_get(ni, &raw_ni);
		if (err)
			ntfs_error(ni->vol->mp, "Failed to get raw inode "
					"(error %d).", err);
		else {
			if (!NInoRaw(raw_ni))
				panic("%s(): Requested raw inode but got "
						"non-raw one.\n", __FUNCTION__);
			ioflags = 0;
			if (vnode_isnocache(ni->vn) ||
					vnode_isnocache(raw_ni->vn))
				ioflags |= IO_NOCACHE;
			if (vnode_isnoreadahead(ni->vn) ||
					vnode_isnoreadahead(raw_ni->vn))
				ioflags |= IO_RAOFF;
			err = ntfs_write_compressed(ni, raw_ni, attr_ofs, size,
					kaddr + upl_ofs, NULL, ioflags);
			if (err)
				ntfs_error(ni->vol->mp,
						"ntfs_write_compressed() "
						"failed (error %d).", err);
			(void)vnode_put(raw_ni->vn);
		}
#endif
	}
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
		if (!err)
			err = EIO;
	}
	if (!err) {
		if (!(flags & UPL_NOCOMMIT)) {
			/* Commit the page range we wrote out. */
			ubc_upl_commit_range(upl, upl_ofs, size,
					UPL_COMMIT_FREE_ON_EMPTY);
		}
		// TODO: If we wrote anything at all we have to clear the
		// setuid and setgid bits as a precaution against tampering
		// (see xnu/bsd/hfs/hfs_readwrite.c::hfs_vnop_pageout()).
		ntfs_debug("Done (%s).", !NInoNonResident(ni) ?
				"ntfs_resident_attr_write()" :
				"ntfs_write_compressed()");
	} else /* if (err) */ {
		if (!NInoNonResident(ni)) {
			/*
			 * If the attribute was converted to non-resident under
			 * our nose, retry the pageout.
			 *
			 * TODO: This may no longer be possible to happen now
			 * that we lock against changes in initialized size and
			 * thus truncation...  Revisit this issue when the
			 * write code has been written and remove the check +
			 * goto if appropriate.
			 */
			if (err == EAGAIN)
				goto retry_pageout;
		}
err:
		if (!(flags & UPL_NOCOMMIT)) {
			int upl_flags = UPL_ABORT_FREE_ON_EMPTY;
			if (err == ENOMEM)
				upl_flags |= UPL_ABORT_RESTART;
			ubc_upl_abort_range(upl, upl_ofs, size, upl_flags);
		}
		ntfs_error(ni->vol->mp, "Failed (error %d).", err);
	}
	if (locked)
		lck_rw_unlock_shared(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_searchfs -
 *
 */
static int ntfs_vnop_searchfs(struct vnop_searchfs_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = err_searchfs(a);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_getxattr -
 *
 */
static int ntfs_vnop_getxattr(struct vnop_getxattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_setxattr -
 *
 */
static int ntfs_vnop_setxattr(struct vnop_setxattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_removexattr -
 *
 */
static int ntfs_vnop_removexattr(struct vnop_removexattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_listxattr -
 *
 */
static int ntfs_vnop_listxattr(struct vnop_listxattr_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: Implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_blktooff - map a logical block number to its byte offset
 * @a:		arguments to blktooff function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to which the logical block number belongs
 *	daddr64_t a_lblkno;	logical block number to map
 *	off_t *a_offset;	destination for returning the result
 *
 * Map the logical block number @a->a_lblkno belonging to the vnode @a->a_vp to
 * the corresponding byte offset, i.e. the offset in the vnode in bytes and
 * return the result in @a->a_offset.
 *
 * Return 0 on success and EINVAL if no vnode was specified in @a->a_vp.
 */
static int ntfs_vnop_blktooff(struct vnop_blktooff_args *a)
{
	ntfs_inode *ni;

	if (!a->a_vp) {
		ntfs_warning(NULL, "Called with NULL vnode!");
		return EINVAL;
	}
	ni = NTFS_I(a->a_vp);
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	ntfs_debug("Entering for logical block 0x%llx, mft_no 0x%llx, type "
			"0x%x, name_len 0x%x.", (unsigned long long)a->a_lblkno,
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	*a->a_offset = a->a_lblkno << PAGE_SHIFT;
	ntfs_debug("Done (byte offset 0x%llx).",
			(unsigned long long)*a->a_offset);
	return 0;
}

/**
 * ntfs_vnop_offtoblk - map a byte offset to its logical block number
 * @a:		arguments to offtoblk function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to which the byte offset belongs
 *	off_t a_offset;		byte offset to map
 *	daddr64_t *a_lblkno;	destination for returning the result
 *
 * Map the byte offset @a->a_offset belonging to the vnode @a->a_vp to the
 * corresponding logical block number, i.e. the offset in the vnode in units of
 * the vnode block size and return the result in @a->a_lblkno.
 *
 * Return 0 on success and EINVAL if no vnode was specified in @a->a_vp.
 */
static int ntfs_vnop_offtoblk(struct vnop_offtoblk_args *a)
{
	ntfs_inode *ni;

	if (!a->a_vp) {
		ntfs_warning(NULL, "Called with NULL vnode.");
		return EINVAL;
	}
	ni = NTFS_I(a->a_vp);
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	ntfs_debug("Entering for byte offset 0x%llx, mft_no 0x%llx, type "
			"0x%x, name_len 0x%x.", (unsigned long long)a->a_offset,
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	*a->a_lblkno = a->a_offset >> PAGE_SHIFT;
	ntfs_debug("Done (logical block 0x%llx).",
			(unsigned long long)*a->a_lblkno);
	return 0;
}

/**
 * ntfs_vnop_blockmap - map a file offset to its physical block number
 * @a:		arguments to blockmap function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode to which the byte offset belongs
 *	off_t a_foffset;	starting byte offset to map
 *	size_t a_size;		number of bytes to map starting at @a_foffset
 *	daddr64_t *a_bpn;	destination for starting physical block number
 *	size_t *a_run;		destination for contiguous bytes from @a_bpn
 *	void *a_poff;		physical offset into @a_bpn
 *	int a_flags;		reason for map (VNODE_READ, VNODE_WRITE, or 0)
 *	vfs_context_t a_context;
 *
 * Map @a->a_size bytes starting at the file offset @a->a_foffset to the
 * corresponding physical block number and return the result in @a->a_bpn
 * (starting block number), @a->a_run (number of contiguous bytes starting at
 * @a->a_bpn), and @a->a_poff (byte offset into @a->a_bpn corresponding to the
 * file offset @a->a_foffset, this will be zero if @a_foffset is block aligned
 * and non-zero otherwise).
 *
 * FIXME: At present the osx kernel completely ignores @a->a_poff and in fact
 * it is always either NULL on entry or the returned value is ignored.  Thus,
 * for now, if @a->a_foffset is not aligned to the physical block size, we
 * always return error (EINVAL) unless @a->a_foffset equals the initialized
 * size in the ntfs inode in which case we return a block number of -1 in
 * @a->a_bpn thus alignment to the block and hence @a->a_poff are not relevant.
 * Thus we always return 0 in @a->a_poff.
 *
 * @a->a_flags is either VNODE_READ or VNODE_WRITE but can be 0 in certain call
 * paths such as the system call fcntl(F_LOG2PHYS) for example.
 *
 * Note, all the return pointers (@a->a_bpn, @a->a_run, @a->a_poff) are NULL in
 * some code paths in xnu (one or more of them at a time), thus all of them
 * need to be checked for being NULL before writing to them.  If @a->a_bpn is
 * NULL then there is nothing to do and success is returned immediately.
 *
 * For ntfs mapping to physical blocks is special because some attributes do
 * not have block aligned data.  This is the case for all resident attributes
 * as well as for all non-resident attributes which are compressed or
 * encrypted.  For all of those it would be logical to return an error however
 * this leads to a kernel panic in current xnu because a buf_bread() can cause
 * ntfs_vnop_blockmap() to be called when an uptodate page is in memory but no
 * buffer is in memory.  This can happen under memory pressure when the buffer
 * has been recycled for something else but the page has not been reused yet.
 * In that case ntfs_vnop_blockmap() is only called to recreate the physical
 * mapping of the buffer and is not actually used for anything as the data is
 * already present in the uptodate page.  Thus, instead of returning error, we
 * set the physical block @a->a_bpn to equal the logical block corresponding to
 * the byte offset @a->a_foffset and return success.  Doing this signals to the
 * VFS that the physical mapping cannot be cached in the buffer and all is
 * well.  Note this call path always has a non-zero @a->a_flags whilst other
 * "weird" code paths like fcntl(F_LOG2PHYS) set @a->a_flags to zero, thus we
 * can do the above workaround when @a->a_flags is not zero and return error
 * EINVAL when @a->a_flags is zero.
 *
 * If @a->a_foffset is beyond the end of the attribute, return error ERANGE.
 * HFS returns ERANGE in this case so we follow suit.  Although some other osx
 * file systems return EFBIG and some E2BIG instead so it does not seem to be
 * very standardized, so maybe we should return the IMHO more correct "invalid
 * seek" (ESPIPE), instead.  (-;
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_blockmap(struct vnop_blockmap_args *a)
{
	const s64 byte_offset = a->a_foffset;
	s64 data_size, init_size, clusters, bytes = 0;
	VCN vcn;
	LCN lcn;
	ntfs_inode *ni = NTFS_I(a->a_vp);
	ntfs_volume *vol = ni->vol;
	unsigned vcn_ofs;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"offset 0x%llx, size 0x%lx.",
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len,
			(unsigned long long)byte_offset,
			(unsigned long)a->a_size);
	if (S_ISDIR(ni->mode)) {
		ntfs_error(vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	// FIXME: Can this ever happen?  If not remove this check...
	if ((a->a_flags & VNODE_WRITE) && vfs_isrdonly(vol->mp)) {
		ntfs_warning(vol->mp, "Called for VNODE_WRITE but mount is "
				"read-only.");
		return EROFS;
	}
	if (!a->a_bpn) {
		ntfs_debug("Called with a_bpn == NULL, nothing to do.  "
				"Returning success (0).");
		return 0;
	}
	/*
	 * Note it does not matter if we are racing with truncate because that
	 * will be detected during the runlist lookup below.
	 */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	init_size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	// TODO: NTFS_RW implement!
	if (byte_offset >= data_size) {
eof:
		ntfs_error(vol->mp, "Called for inode 0x%llx, size 0x%lx, "
				"byte offset 0x%llx which is beyond the end "
				"of the inode data 0x%llx.  Returning error: "
				"ERANGE.", (unsigned long long)ni->mft_no,
				(unsigned long)a->a_size,
				(unsigned long long)byte_offset,
				(unsigned long long)data_size);
		return ERANGE;
	}
	if (byte_offset & vol->sector_size_mask && byte_offset != init_size) {
		ntfs_error(vol->mp, "Called for inode 0x%llx, byte offset "
				"0x%llx.  This is not a multiple of the "
				"physical block size %u thus the mapping "
				"cannot be performed.  Returning error: "
				"EINVAL.", (unsigned long long)ni->mft_no,
				(unsigned long long)byte_offset,
				(unsigned)vol->sector_size);
		return EINVAL;
	}
	// TODO: NTFS_RW implement! Update the initialized size...
	/*
	 * If the requested byte offset is at or beyond the initialized size
	 * simply return a hole.  We already checked for being at or beyond the
	 * data size so we know we are in an uninitialized region in this case
	 * rather than at or beyond the end of the attribute.
	 */
	if (byte_offset >= init_size) {
		*a->a_bpn = -1; /* -1 means hole. */
		/*
		 * Set the size of the block to the number of uninitialized
		 * bytes in the attribute starting at the requested byte offset
		 * @a->a_foffset.
		 */
		bytes = data_size - byte_offset;
		goto done;
	}
	/*
	 * Blockmap does not make sense for resident attributes and neither
	 * does it make sense for non-resident, compressed or encrypted
	 * attributes.  The only special case is for directory inodes because
	 * their flags are only defaults to be used when creating new files
	 * rather than having any meaning for their actual data contents.
	 */
	if (!NInoNonResident(ni) || (ni->type != AT_INDEX_ALLOCATION &&
			(NInoCompressed(ni) || NInoEncrypted(ni)) &&
			!NInoRaw(ni))) {
		if (!a->a_flags) {
			ntfs_error(vol->mp, "Called for inode 0x%llx, which "
					"is resident, compressed, or "
					"encrypted and VNOP_BLOCKMAP() does "
					"not make sense for such inodes.  "
					"Returning error: EINVAL.",
					(unsigned long long)ni->mft_no);
			return EINVAL;
		}
		*a->a_bpn = byte_offset >> PAGE_SHIFT;
		bytes = ni->block_size;
		ntfs_debug("Called for inode 0x%llx which is resident, "
				"compressed, or encrypted and VNOP_BLOCKMAP() "
				"does not make sense for such inodes.  "
				"Returning success and setting physical == "
				"logical block number to signal to VFS that "
				"the mapping cannot be cached in the buffer.",
				(unsigned long long)ni->mft_no);
		goto done;
	}
	/*
	 * All is ok, do the mapping.  First, work out the vcn and vcn offset
	 * corresponding to the @a->a_foffset.
	 */
	vcn = byte_offset >> vol->cluster_size_shift;
	vcn_ofs = (u32)byte_offset & vol->cluster_size_mask;
	/*
	 * Convert the vcn to the corresponding lcn and obtain the number of
	 * contiguous clusters starting at the vcn.
	 */
	lck_rw_lock_shared(&ni->rl.lock);
	lcn = ntfs_attr_vcn_to_lcn_nolock(ni, vcn, FALSE,
			a->a_run ? &clusters : 0);
	if (lcn < LCN_HOLE) {
		errno_t err;

		/* Error: deal with it. */
		lck_rw_unlock_shared(&ni->rl.lock);
		switch (lcn) {
		case LCN_ENOENT:
			/*
			 * Raced with a concurrent truncate which caused the
			 * byte offset @a->a_foffset to become outside the
			 * attribute size.
			 */
			goto eof;
		case LCN_ENOMEM:
			ntfs_error(vol->mp, "Not enough memory to complete "
					"mapping for inode 0x%llx.  "
					"Returning error: ENOMEM.",
					(unsigned long long)ni->mft_no);
			err = ENOMEM;
			break;
		default:
			ntfs_error(vol->mp, "Failed to complete mapping for "
					"inode 0x%llx.  Run chkdsk.  "
					"Returning error: EIO.",
					(unsigned long long)ni->mft_no);
			err = EIO;
			break;
		}
		return err;
	}
	if (lcn < 0) {
		/*
		 * It is a hole.  If the driver is compiled read-only, simply
		 * return the hole.  If the driver is compiled read-write, but
		 * this is not a VNODE_WRITE request, also return the hole.
		 * Only if the driver is compiled read-write and this is a
		 * VNODE_WRITE request, we can (but do not have to) fill the
		 * hole starting at @vcn for a length of min(@a->s_size +
		 * @vcn_ofs, contig length in bytes starting at @vcn).  If we
		 * do not fill the hole and the caller is the UBC, it will page
		 * out page by page and this will fill the hole in peaces.  If
		 * we do fill the hole we can do it all in one go, but we have
		 * to be careful about zeroing regions between @vcn and
		 * @a->a_foffset as well as the end of the contig region (or
		 * @a->a_foffset + @a->a_size) and the end of that cluster.
		 * Also, we need to deal with writes outside the initialized
		 * size, possibly need to check if calling process is allowed
		 * to do the write (or would we never have gotten here
		 * otherwise?), and we need to make sure that we either zero
		 * the allocated hole, or that the caller will definitely write
		 * to the hole and if that fails we would need to ensure that
		 * either the hole is recreated, or that zeroes are written to
		 * it, otherwise we risk exposing stale disk data which is bad.
		 */
		// TODO: NTFS_RW implement! Fill the hole in the write case...
#if 0 //#ifdef NTFS_RW
		if (!(a->a_flags & VNODE_WRITE))
#endif /* NTFS_RW */
		{
			/* Return the hole. */
			lck_rw_unlock_shared(&ni->rl.lock);
			*a->a_bpn = -1; /* -1 means hole. */
			if (a->a_run) {
				bytes = (clusters << vol->cluster_size_shift) -
						vcn_ofs;
				/*
				 * If the run overlaps the initialized size,
				 * extend the run length so it goes up to the
				 * data size thus merging the hole with the
				 * uninitialized region.
				 */
				if (byte_offset + bytes > init_size)
					bytes = data_size - byte_offset;
			}
			goto done;
		}
#if 0 //#ifdef NTFS_RW
		/* Fill the hole. */
		// TODO: Start by upgrading the runlist lock to exclusive and
		// if that fails, take the lock exclusive, and redo the mapping
		// to ensure no-one else filled the hole in the meantime.
		// Then fill the hole and deal with zeroing, etc and setup @lcn
		// lcn and @clusters appropriately.
		// Finally, drop the exclusive runlist lock and fall through.
#endif /* NTFS_RW */
	} else
		lck_rw_unlock_shared(&ni->rl.lock);
	/* The vcn was mapped successfully to a physical lcn, return it. */
	*a->a_bpn = ((lcn << vol->cluster_size_shift) + vcn_ofs) >>
			vol->sector_size_shift;
	if (a->a_run) {
		bytes = (clusters << vol->cluster_size_shift) - vcn_ofs;
		/*
		 * If the run overlaps the initialized size, truncate the run
		 * length so it only goes up to the initialized size.  The
		 * caller will then be able to access this region on disk
		 * directly and will then call us again with a byte offset
		 * equal to the initialized size and we will then return the
		 * entire initialized region as a hole.  Thus the caller does
		 * not need to know about the fact that NTFS has such a thing
		 * as the initialized_size.
		 *
		 * We already handled the case where the byte offset is beyond
		 * the initialized size so no need to check for that here.
		 *
		 * However do not do this if the initialized size is equal to
		 * the data size.  The caller is responsible for not returning
		 * data beyond the attribute size to user space.  If this is
		 * not done the last page of an attribute read is broken into
		 * two separate i/os, one with a read and one with a hole.
		 * cluster_io() will zero beyond the end of attribute in any
		 * case so it is faster to do it with a single call.
		 */
		if (byte_offset + bytes > init_size && init_size < data_size)
			bytes = init_size - byte_offset;
	}
done:
	if (a->a_run) {
		if (bytes > a->a_size)
			bytes = a->a_size;
		*a->a_run = bytes;
	}
	if (a->a_poff)
		*(int*)a->a_poff = 0;
	ntfs_debug("Done (a_bpn 0x%llx, a_run 0x%lx, a_poff 0x%x).",
			(unsigned long long)*a->a_bpn,
			a->a_run ? (unsigned long)*a->a_run : 0,
			a->a_poff ? *(int*)a->a_poff : 0);
	return 0;
}

/**
 * ntfs_vnop_getnamedstream -
 *
 */
static int ntfs_vnop_getnamedstream(struct vnop_getnamedstream_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: Implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_makenamedstream -
 *
 */
static int ntfs_vnop_makenamedstream(struct vnop_makenamedstream_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW: Implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_removenamedstream -
 *
 */
static int ntfs_vnop_removenamedstream(struct vnop_removenamedstream_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO: NTFS_RW: Implement!
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

static struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	/*
	 * Set vn_default_error() to be our default vnop, thus any vnops we do
	 * not specify (or specify as NULL) will be set to it and this function
	 * just returns ENOTSUP.
	 */
	{ &vnop_default_desc,		(vnop_t*)vn_default_error },
	{ &vnop_strategy_desc,		(vnop_t*)ntfs_vnop_strategy },
	/*
	 * vn_bwrite() is a simple wrapper for buf_bwrite() which in turn uses
	 * VNOP_STRATEGY() and hence ntfs_vnop_strategy() to do the i/o and the
	 * latter handles all NTFS specifics thus we can simply use the generic
	 * vn_bwrite() for our VNOP_BWRITE() method.
	 */
	{ &vnop_bwrite_desc,		(vnop_t*)vn_bwrite },
	{ &vnop_lookup_desc,		(vnop_t*)ntfs_vnop_lookup },
	{ &vnop_create_desc,		(vnop_t*)ntfs_vnop_create },
	{ &vnop_mknod_desc,		(vnop_t*)ntfs_vnop_mknod },
	{ &vnop_open_desc,		(vnop_t*)ntfs_vnop_open },
	{ &vnop_close_desc,		(vnop_t*)ntfs_vnop_close },
	{ &vnop_access_desc,		(vnop_t*)ntfs_vnop_access },
	{ &vnop_getattr_desc,		(vnop_t*)ntfs_vnop_getattr },
	{ &vnop_setattr_desc,		(vnop_t*)ntfs_vnop_setattr },
	{ &vnop_read_desc,		(vnop_t*)ntfs_vnop_read },
	{ &vnop_write_desc,		(vnop_t*)ntfs_vnop_write },
	{ &vnop_ioctl_desc,		(vnop_t*)ntfs_vnop_ioctl },
	{ &vnop_select_desc,		(vnop_t*)ntfs_vnop_select },
	{ &vnop_exchange_desc,		(vnop_t*)ntfs_vnop_exchange },
	{ &vnop_kqfilt_add_desc, 	(vnop_t*)ntfs_vnop_kqfilt_add },
	{ &vnop_kqfilt_remove_desc, 	(vnop_t*)ntfs_vnop_kqfilt_remove },
	/* Let the VFS deal with revoking a vnode. */
	{ &vnop_revoke_desc,		(vnop_t*)nop_revoke },
	{ &vnop_mmap_desc,		(vnop_t*)ntfs_vnop_mmap },
	{ &vnop_mnomap_desc,		(vnop_t*)ntfs_vnop_mnomap },
	{ &vnop_fsync_desc,		(vnop_t*)ntfs_vnop_fsync },
	{ &vnop_remove_desc,		(vnop_t*)ntfs_vnop_remove },
	{ &vnop_link_desc,		(vnop_t*)ntfs_vnop_link },
	{ &vnop_rename_desc,		(vnop_t*)ntfs_vnop_rename },
	{ &vnop_mkdir_desc,		(vnop_t*)ntfs_vnop_mkdir },
	{ &vnop_rmdir_desc,		(vnop_t*)ntfs_vnop_rmdir },
	{ &vnop_symlink_desc,		(vnop_t*)ntfs_vnop_symlink },
	{ &vnop_readdir_desc,		(vnop_t*)ntfs_vnop_readdir },
	{ &vnop_readdirattr_desc, 	(vnop_t*)ntfs_vnop_readdirattr },
	{ &vnop_readlink_desc,		(vnop_t*)ntfs_vnop_readlink },
	{ &vnop_inactive_desc,		(vnop_t*)ntfs_vnop_inactive },
	{ &vnop_reclaim_desc,		(vnop_t*)ntfs_vnop_reclaim },
	{ &vnop_pathconf_desc,		(vnop_t*)ntfs_vnop_pathconf },
	/*
	 * Let the VFS deal with advisory locking for us, so our advlock method
	 * should never get called and if it were to get called for some
	 * reason, we make sure to return error (ENOTSUP).
	 */
	{ &vnop_advlock_desc,		(vnop_t*)err_advlock },
	{ &vnop_allocate_desc,		(vnop_t*)ntfs_vnop_allocate },
	{ &vnop_pagein_desc,		(vnop_t*)ntfs_vnop_pagein },
	{ &vnop_pageout_desc,		(vnop_t*)ntfs_vnop_pageout },
	{ &vnop_searchfs_desc,		(vnop_t*)ntfs_vnop_searchfs },
	/*
	 * Nothing supports copyfile in current xnu and it is not documented so
	 * we do not support it either.
	 */
	{ &vnop_copyfile_desc,		(vnop_t*)err_copyfile },
	{ &vnop_getxattr_desc,		(vnop_t*)ntfs_vnop_getxattr },
	{ &vnop_setxattr_desc,		(vnop_t*)ntfs_vnop_setxattr },
	{ &vnop_removexattr_desc,	(vnop_t*)ntfs_vnop_removexattr },
	{ &vnop_listxattr_desc,		(vnop_t*)ntfs_vnop_listxattr },
	{ &vnop_blktooff_desc,		(vnop_t*)ntfs_vnop_blktooff },
	{ &vnop_offtoblk_desc,		(vnop_t*)ntfs_vnop_offtoblk },
	{ &vnop_blockmap_desc,		(vnop_t*)ntfs_vnop_blockmap },
	{ &vnop_getnamedstream_desc,	(vnop_t*)ntfs_vnop_getnamedstream },
	{ &vnop_makenamedstream_desc,	(vnop_t*)ntfs_vnop_makenamedstream },
	{ &vnop_removenamedstream_desc,	(vnop_t*)ntfs_vnop_removenamedstream },
	{ NULL,				(vnop_t*)NULL }
};

struct vnodeopv_desc ntfs_vnodeopv_desc = {
	&ntfs_vnodeop_p, ntfs_vnodeop_entries
};
