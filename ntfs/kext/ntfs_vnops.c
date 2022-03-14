/*
 * ntfs_vnops.c - NTFS kernel vnode operations.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
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
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/xattr.h>

#include <string.h>

#include <mach/kern_return.h>
#include <mach/memory_object_types.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include <vfs/vfs_support.h>
#include <IOKit/IOLib.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_bitmap.h"
#include "ntfs_compress.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_mft.h"
#include "ntfs_mst.h"
#include "ntfs_page.h"
#include "ntfs_sfm.h"
#include "ntfs_time.h"
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
	errno_t err, err2;
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
		s64 ofs, data_size, init_size;
		u32 rec_size = ni->block_size;
		NTFS_RECORD_TYPE magic = 0;

		if (!is_read) {
			if (ni->type == AT_INDEX_ALLOCATION)
				magic = magic_INDX;
			else
				panic("%s(): Unknown mst protected inode "
						"0x%llx, type 0x%x, name_len "
						"0x%x.", __FUNCTION__,
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						(unsigned)ni->name_len);
		}
		/* The offset in the attribute at which this buffer begins. */
		ofs = (s64)buf_lblkno(cbp) << ni->block_size_shift;
		lck_spin_lock(&ni->size_lock);
		data_size = ni->data_size;
		init_size = ni->initialized_size;
		lck_spin_unlock(&ni->size_lock);
		/*
		 * Limit mst deprotection to the initialized size as beyond
		 * that the data is zero and deprotection will fail.  And worse
		 * in the write case it will lead to a kernel panic.
		 */
		if (ofs + size > init_size) {
			if (ofs > data_size) {
				ntfs_error(ni->vol->mp, "Buffer begins past "
						"the end of the data of the "
						"attribute (mft_no 0x%llx).",
						(unsigned long long)ni->mft_no);
				err = EINVAL;
				goto unm_err;
			}
			if (ofs > init_size) {
				ntfs_debug("Buffer begins past the end of the "
						"initialized data of the "
						"attribute (mft_no 0x%llx).",
						(unsigned long long)ni->mft_no);
				goto unm_err;
			}
			size = init_size - ofs;
			kend = kaddr + size;
		}
		/*
		 * Do the mst deprotection ignoring errors and make sure we do
		 * not go past the initialized size should an error somehow
		 * have caused the last record to straddle the initialized
		 * size.
		 */
		while (kaddr + rec_size <= kend) {
			if (is_read)
				(void)ntfs_mst_fixup_post_read(
						(NTFS_RECORD*)kaddr, rec_size);
			else if (__ntfs_is_magic(((NTFS_RECORD*)kaddr)->magic,
					magic))
				ntfs_mst_fixup_post_write((NTFS_RECORD*)kaddr);
			kaddr += rec_size;
		}
	} else if (NInoEncrypted(ni)) {
		// TODO: Need to decrypt the encrypted sectors here.  This
		// cannot happen at present as we deny opening/reading/writing/
		// paging encrypted vnodes.
		panic("%s(): Called for encrypted vnode.\n", __FUNCTION__);
	} else
		panic("%s(): Called for normal vnode.\n", __FUNCTION__);
unm_err:
	err2 = buf_unmap(cbp);
	if (err2) {
		if (!err)
			err = err2;
		ntfs_error(ni->vol->mp, "Failed to unmap buffer (error %d).",
				err2);
	}
err:
	return err;
}

/**
 * ntfs_buf_iodone - remove the MST fixups when i/o is complete on a buffer
 * @buf:	buffer for which to remove the MST fixups
 * @arg:	unused, always NULL
 *
 * ntfs_buf_iodone() is an i/o completion handler which is called when i/o is
 * completed on a buffer belonging to $MFT/$DATA.  It removes the MST fixups
 * and returns after which the buffer busy state (BL_BUSY flag) is cleared and
 * others can access the buffer again.
 *
 * ntfs_buf_iodone() is called both when the i/o was successful and when it
 * failed thus we have to deal with that as appropriate.
 *
 * Note that ntfs_buf_iodone() is called deep from within the driver stack and
 * thus there are limitations on what it is allowed to do.  In particular it is
 * not allowed to initiate new i/o operations nor to allocate/free memory.
 *
 * WARNING: This function can be called whilst an unmount is in progress and
 * thus it may not look up nor use the ntfs_volume structure to which the inode
 * belongs.
 */
static void ntfs_buf_iodone(buf_t buf, void *arg __unused)
{
	s64 ofs, data_size, init_size;
	vnode_t vn;
	mount_t mp;
	ntfs_inode *ni;
	unsigned size, recs_per_block, b_flags;
	errno_t err;

	vn = buf_vnode(buf);
	mp = vnode_mount(vn);
	ni = NTFS_I(vn);
	ntfs_debug("Entering for mft_no 0x%llx, lblkno 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)buf_lblkno(buf));
	if (!NInoMstProtected(ni) || ni->mft_no || NInoAttr(ni))
		panic("%s(): Called not for $MFT!\n", __FUNCTION__);
	/* The size and offset in the attribute at which this buffer begins. */
	size = buf_count(buf);
	recs_per_block = size >> ni->block_size_shift;
	if (!recs_per_block)
		panic("%s(): !recs_per_block\n", __FUNCTION__);
	ofs = (s64)buf_lblkno(buf) << ni->block_size_shift;
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	init_size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	b_flags = buf_flags(buf);
	/*
	 * Limit mst deprotection to the initialized size as beyond that the
	 * data is zero and deprotection will fail.  And worse in the write
	 * case it will lead to a kernel panic.
	 */
	if (ofs + size > init_size) {
		if (ofs > data_size) {
			ntfs_error(mp, "Buffer begins past the end of the "
					"data of the attribute (mft_no "
					"0x%llx).",
					(unsigned long long)ni->mft_no);
			err = EINVAL;
			goto err;
		}
		if (ofs > init_size) {
			ntfs_error(mp, "Buffer begins past the end of the "
					"initialized data of the attribute "
					"(mft_no 0x%llx).",
					(unsigned long long)ni->mft_no);
			err = EINVAL;
			goto err;
		}
	}
	/*
	 * Do not try to remove the fixups if a read failed as there will be
	 * nothing to remove.
	 */
	if (!buf_error(buf) || !(b_flags & B_READ)) {
		u8 *block_data;
		u32 recno;

		err = buf_map(buf, (caddr_t*)&block_data);
		if (err) {
			ntfs_error(mp, "Failed to map buffer (error %d).",
					err);
			goto err;
		}
		for (recno = 0; recno < recs_per_block; ++recno) {
			NTFS_RECORD *const rec = (NTFS_RECORD*)
				&block_data[recno * ni->block_size];
			if (b_flags & B_READ) {
				err = ntfs_mst_fixup_post_read(rec,
					ni->block_size);
				if (err) {
					ntfs_error(mp, "Multi sector transfer "
							"error detected in "
							"mft_no 0x%llx (error "
							"%d).  Run chkdsk",
							(unsigned long long)
							ni->mft_no,
							err);
					buf_seterror(buf, err);
				}
			} else if(__ntfs_is_magic(rec->magic, magic_FILE)) {
				ntfs_mst_fixup_post_write(rec);
			}
		}
		err = buf_unmap(buf);
		if (err) {
			ntfs_error(mp, "Failed to unmap buffer (error %d).",
					err);
			goto err;
		}
	}
	ntfs_debug("Done.");
	return;
err:
	if (!buf_error(buf))
		buf_seterror(buf, err);
	ntfs_debug("Failed.");
	return;
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
 * in (u8*)buf_dataptr(@a->a_bp).  The exception to this is i/o for $MFT/$DATA
 * and $MFTMirr/$DATA which is issued via buf_meta_bread(), etc, and thus does
 * not involve a page list at all.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_strategy(struct vnop_strategy_args *a)
{
	s64 ofs, max_end_io;
	daddr64_t lblkno;
	buf_t buf = a->a_bp;
	vnode_t vn = buf_vnode(buf);
	ntfs_inode *ni;
	ntfs_volume *vol;
	void (*old_iodone)(buf_t, void *);
	void *old_transact;
	unsigned b_flags;
	errno_t err, err2;
	BOOL do_fixup;

	/* Same checks as in buf_strategy(). */
	if (!vn || vnode_ischr(vn) || vnode_isblk(vn))
		panic("%s(): !vn || vnode_ischr(vn) || vnode_isblk(vn)\n",
				__FUNCTION__);
	ni = NTFS_I(vn);
	if (!ni) {
		err = EIO;
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		goto err;
	}
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"logical block 0x%llx.", (unsigned long long)ni->mft_no,
			le32_to_cpu(ni->type), (unsigned)ni->name_len,
			(unsigned long long)buf_lblkno(buf));
	if (S_ISDIR(ni->mode))
		panic("%s(): Called for directory vnode.\n", __FUNCTION__);
	vol = ni->vol;
	b_flags = buf_flags(buf);
	/*
	 * If we are called from cluster_io() then pass the request down to the
	 * underlying device containing the NTFS volume.  We have no KPI way of
	 * doing this directly so we invoke buf_strategy() and rely on the fact
	 * that it does not do anything other than associate the physical
	 * device with the buffer and then pass the buffer down to the device.
	 */
	if (b_flags & B_CLUSTER)
		goto done;
	/*
	 * If this i/o is for $MFTMirr/$DATA send it through straight without
	 * modifications.  This is because we keep the $MFTMirr/$DATA buffers
	 * in memory with the fixups applied for simplicity.
	 */
	if (ni->mft_no == FILE_MFTMirr && !NInoAttr(ni))
		goto done;
	/*
	 * Except for $MFT/$DATA we never do i/o via file system buffers thus
	 * we should never get here.
	 */
	if (ni->mft_no != FILE_MFT || NInoAttr(ni))
		panic("%s(): Called for non-cluster i/o buffer.\n",
				__FUNCTION__);
	/*
	 * We are reading/writing $MFT/$DATA.
	 *
	 * For reads, i/o is allowed up to the data_size whilst for writes, i/o
	 * is only allowed up to the initialized_size.
	 *
	 * Further when reading past the initialized size we do not need to do
	 * i/o at all as we can simply clear the buffer and return success.
	 */
	lblkno = buf_lblkno(buf);
	ofs = lblkno << ni->block_size_shift;
	lck_spin_lock(&ni->size_lock);
	max_end_io = ni->initialized_size;
	do_fixup = FALSE;
	if (b_flags & B_READ) {
		if (ofs >= max_end_io) {
			if (max_end_io > ni->data_size)
				panic("%s() initialized_size > data_size\n",
						__FUNCTION__);
			if (ofs < ni->data_size) {
				lck_spin_unlock(&ni->size_lock);
				buf_clear(buf);
				buf_biodone(buf);
				ntfs_debug("Read past initialized size.  "
						"Clearing buffer.");
				return 0;
			}
		}
		max_end_io = ni->data_size;
		do_fixup = TRUE;
	}
	lck_spin_unlock(&ni->size_lock);
	if (ofs >= max_end_io) {
		/* I/o is out of range.  This should never happen. */
		ntfs_error(vol->mp, "Trying to %s buffer for $MFT/$DATA which "
				"is out of range, aborting.",
				b_flags & B_READ ? "read" : "write");
		err = EIO;
		goto err;
	}
	/*
	 * For writes we need to apply the MST fixups before calling
	 * buf_strategy() which will perform the i/o and if the write is for an
	 * mft record that is also in the mft mirror we now need to write it to
	 * the mft mirror as well.
	 *
	 * Note B_WRITE is a pseudo flag and cannot be used for checking thus
	 * check that B_READ is not set which implies it is a write.
	 */
	if (!(b_flags & B_READ)) {
		const u32 recs_per_block =
			buf_count(buf) >> ni->block_size_shift;

		u8 *block_data;
		u32 recno;

		err = buf_map(buf, (caddr_t*)&block_data);
		if (err) {
			ntfs_error(vol->mp, "Failed to map buffer (error %d).",
					err);
			goto err;
		}
		if (!block_data)
			panic("%s(): buf_map() returned NULL.\n", __FUNCTION__);

		for (recno = 0; recno < recs_per_block; ++recno) {
			NTFS_RECORD *const rec = (NTFS_RECORD*)
				&block_data[recno << ni->block_size_shift];
			NTFS_RECORD_TYPE magic;
			BOOL need_mirr_sync;

#if 0
			need_mirr_sync = FALSE;
			if (ni->type == AT_INDEX_ALLOCATION)
				magic = magic_INDX;
			else if (ni == mft_ni || ni == vol->mftmirr_ni) {
				magic = magic_FILE;
				if (ni == mft_ni)
					need_mirr_sync =
						((lblkno + recno) <
						vol->mftmirr_size);
			} else
				panic("%s(): Unknown mst protected inode "
						"0x%llx, type 0x%x, name_len "
						"0x%x.", __FUNCTION__,
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						(unsigned)ni->name_len);
#else
			need_mirr_sync = ((lblkno + recno) < vol->mftmirr_size);
			magic = magic_FILE;
#endif
			/*
			 * Only apply fixups if the record has the correct
			 * magic.  We may have detected a multi sector transfer
			 * error and are thus now writing a BAAD record in which
			 * case we do not want to touch its contents.
			 *
			 * Further, if there is an error do not sync the record
			 * to the mft mirror as that may still be intact and we
			 * do not want to overwrite the correct data with
			 * corrupt data.
			 */
			if (__ntfs_is_magic(rec->magic, magic)) {
				err = ntfs_mst_fixup_pre_write(rec,
					ni->block_size);
				if (err) {
					/* The record is corrupt, do not write
					 * it. */
					ntfs_error(vol->mp, "Failed to apply "
							"mst fixups (mft_no "
							"0x%llx, type 0x%x, "
							"offset 0x%llx).",
							(unsigned long long)ni->
							mft_no,
							(unsigned)le32_to_cpu(
							ni->type),
							(unsigned long long)
							ofs);
					err = EIO;
					goto unm_err;
				}
				do_fixup = TRUE;
			}

			if (need_mirr_sync) {
				/*
				 * Note we continue despite an error as we may
				 * succeed to write the actual mft record.
				 */
				err = ntfs_mft_mirror_sync(vol, lblkno + recno,
						(MFT_RECORD*)rec,
						!(b_flags & B_ASYNC));
				if (err)
					ntfs_error(vol->mp, "Failed to sync "
							"mft mirror (error "
							"%d).  Run chkdsk.",
							err);
			}
		}
		err = buf_unmap(buf);
		if (err)
			ntfs_error(vol->mp, "Failed to unmap buffer (error "
					"%d).", err);
	}
	/*
	 * For both reads and writes we need to register our i/o completion
	 * handler which will be called after i/o is complete (including on i/o
	 * failure) and in which we will remove the MST fixups so the buffer in
	 * memory never has MST fixups applied unless it is under i/o in which
	 * case it is BL_BUSY and thus cannot be accessed by anyone so it is
	 * safe to have the MST fixups applied whilst i/o is in flight.
	 */
	if (do_fixup) {
		buf_setfilter(buf, ntfs_buf_iodone, NULL, &old_iodone,
				&old_transact);
		if (old_iodone || old_transact)
			panic("%s(): Buffer for $MFT/$DATA already had an i/o "
					"completion handler assigned!\n",
					__FUNCTION__);
	}
	/*
	 * Everything is set up.  Pass the i/o onto the buffer layer.
	 *
	 * When the i/o is done it will call our i/o completion handler which
	 * will remove the mst fixups.
	 */
done:
	return buf_strategy(vol->dev_vn, a);
unm_err:
	err2 = buf_unmap(buf);
	if (err2)
		ntfs_error(vol->mp, "Failed to unmap buffer in error code "
				"path (error %d).", err2);
err:
	buf_seterror(buf, err);
	buf_biodone(buf);
	return err;
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
	ntfs_inode *ni, *dir_ni = NTFS_I(a->a_dvp);
	vnode_t vn;
	ntfs_volume *vol;
	ntfschar *ntfs_name;
	ntfs_dir_lookup_name *name = NULL;
	u8 *utf8_name = NULL;
	size_t ntfs_name_size, utf8_size;
	signed ntfs_name_len;
	int err;
	/*
	 * This is rather gross but several other file systems do it so perhaps
	 * the large stack (16kiB I believe) in the OS X kernel is big enough.
	 * If we do not want to do the static allocation then simply set
	 * ntfs_name to NULL and utf8_to_ntfs() will allocate the memory for
	 * us.  (We then have to free it, see utf8_to_ntfs() description for
	 * details.)
	 */
	ntfschar ntfs_name_buf[NTFS_MAX_NAME_LEN];
	struct componentname cn_buf;
#ifdef DEBUG
	static const char *ops[4] = { "LOOKUP", "CREATE", "DELETE", "RENAME" };
#endif

	if (!dir_ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = dir_ni->vol;
	name_cn = cn = a->a_cnp;
	op = cn->cn_nameiop;
	ntfs_debug("Looking up %.*s in directory inode 0x%llx for %s, flags "
			"0x%lx.", (int)cn->cn_namelen, cn->cn_nameptr,
			(unsigned long long)dir_ni->mft_no,
			op < 4 ? ops[op] : "UNKNOWN",
			(unsigned long)cn->cn_flags);
	/*
	 * Ensure we are being called for a directory in case we are not being
	 * called from the VFS.
	 */
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_error(vol->mp, "Not a directory.");
		return ENOTDIR;
	}
	lck_rw_lock_shared(&dir_ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(dir_ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(dir_ni->vn);
		lck_rw_unlock_shared(&dir_ni->lock);
		ntfs_debug("Parent directory is deleted.");
		return ENOENT;
	}
	/*
	 * First, look for the name in the name cache.  cache_lookup() returns
	 * -1 if found and @vn is set to the vnode, ENOENT if found and it is a
	 * negative entry thus @vn is not set to anything, or 0 if the lookup
	 * failed in which case we need to do a file system based lookup.
	 *
	 * Note that if @op is CREATE and there is a negative entry in the name
	 * cache cache_lookup() will discard that name and return 0, i.e. the
	 * lookup failed.  In this case we will automatically fall through and
	 * do the right thing during the real lookup.
	 */
	err = cache_lookup(dir_ni->vn, &vn, cn);
	if (err) {
		if (err == -1) {
			ni = NTFS_I(vn);
			lck_rw_lock_shared(&ni->lock);
			/*
			 * Do not allow messing with the inode once it has been
			 * deleted.
			 */
			if (!NInoDeleted(ni)) {
				lck_rw_unlock_shared(&ni->lock);
				lck_rw_unlock_shared(&dir_ni->lock);
				*a->a_vpp = vn;
				ntfs_debug("Done (cached).");
				return 0;
			}
			lck_rw_unlock_shared(&ni->lock);
			/* Remove the inode from the name cache. */
			cache_purge(vn);
			vnode_put(vn);
			ntfs_warning(vol->mp, "Cached but deleted vnode "
					"found, purged from cache and doing "
					"real lookup.");
		} else {
			lck_rw_unlock_shared(&dir_ni->lock);
			if (err == ENOENT) {
				ntfs_debug("Done (cached, negative).");
				return err;
			}
			ntfs_error(vol->mp, "cache_lookup() failed (error "
					"%d).", err);
			return err;
		}
	}
	/* We special case "." and ".." as they are emulated on NTFS. */
	if (cn->cn_namelen == 1 && cn->cn_nameptr[0] == '.') {
		/* "." is not cached. */
		cn->cn_flags &= ~MAKEENTRY;
		if (op == RENAME) {
			lck_rw_unlock_shared(&dir_ni->lock);
			ntfs_debug("Op is RENAME but name is \".\", returning "
					"EISDIR.");
			return EISDIR;
		}
		err = vnode_get(dir_ni->vn);
		lck_rw_unlock_shared(&dir_ni->lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to get iocount reference "
					"on current directory (error %d).",
					err);
			return err;
		}
		ntfs_debug("Got \".\" directory 0x%llx.",
				(unsigned long long)dir_ni->mft_no);
		*a->a_vpp = dir_ni->vn;
		return 0;
	} else if (cn->cn_flags & ISDOTDOT) {
		/* ".." is not cached. */
		cn->cn_flags &= ~MAKEENTRY;
		vn = vnode_getparent(dir_ni->vn);
		if (vn) {
			lck_rw_unlock_shared(&dir_ni->lock);
			ntfs_debug("Got \"..\" directory 0x%llx of directory "
					"0x%llx.",
					(unsigned long long)NTFS_I(vn)->mft_no,
					(unsigned long long)dir_ni->mft_no);
			*a->a_vpp = vn;
			return 0;
		}
		/*
		 * Look up a filename attribute in the mft record of the
		 * directory @dir_ni and use its parent mft reference to run an
		 * ntfs_inode_get() on it to obtain an inode for "..".
		 */
		err = ntfs_inode_get_name_and_parent_mref(dir_ni, FALSE, &mref,
				NULL);
		lck_rw_unlock_shared(&dir_ni->lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to obtain parent mft "
					"reference for directory 0x%llx "
					"(error %d).",
					(unsigned long long)dir_ni->mft_no,
					err);
			return err;
		}
		mft_no = MREF(mref);
		err = ntfs_inode_get(vol, mft_no, FALSE, LCK_RW_TYPE_SHARED,
				&ni, NULL, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to obtain parent inode "
					"0x%llx for directory 0x%llx (error "
					"%d).", (unsigned long long)mft_no,
					(unsigned long long)dir_ni->mft_no,
					err);
			return err;
		}
		/* Consistency check. */
		if (MSEQNO(mref) != ni->seq_no) {
			lck_rw_unlock_shared(&ni->lock);
			(void)vnode_put(ni->vn);
			ntfs_error(vol->mp, "Found stale parent mft reference "
					"in filename of directory 0x%llx.  "
					"Volume is corrupt.  Run chkdsk.",
					(unsigned long long)dir_ni->mft_no);
			return EIO;
		}
		if (!S_ISDIR(ni->mode)) {
			lck_rw_unlock_shared(&ni->lock);
			(void)vnode_put(ni->vn);
			ntfs_error(vol->mp, "Found non-directory parent for "
					"filename of directory 0x%llx.  "
					"Volume is corrupt.  Run chkdsk.",
					(unsigned long long)dir_ni->mft_no);
			return EIO;
		}
		ntfs_debug("Got \"..\" directory 0x%llx of directory 0x%llx.",
				(unsigned long long)mft_no,
				(unsigned long long)dir_ni->mft_no);
		*a->a_vpp = ni->vn;
		lck_rw_unlock_shared(&ni->lock);
		return 0;
	}
	/* Convert the name from utf8 to Unicode. */
	ntfs_name = ntfs_name_buf;
	ntfs_name_size = sizeof(ntfs_name_buf);
	ntfs_name_len = utf8_to_ntfs(vol, (u8*)cn->cn_nameptr, cn->cn_namelen,
			&ntfs_name, &ntfs_name_size);
	if (ntfs_name_len < 0) {
		lck_rw_unlock_shared(&dir_ni->lock);
		err = -ntfs_name_len;
		if (err == ENAMETOOLONG)
			ntfs_debug("Failed (name is too long).");
		else
			ntfs_error(vol->mp, "Failed to convert name to "
					"Unicode (error %d).", err);
		return err;
	}
	/* Look up the converted name in the directory index. */
	err = ntfs_lookup_inode_by_name(dir_ni, ntfs_name, ntfs_name_len,
			&mref, &name);
	if (err) {
		lck_rw_unlock_shared(&dir_ni->lock);
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to find name in directory "
					"(error %d).", err);
			return err;
		}
not_found:
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
		 * this name is desired unless this is a create operation in
		 * which case we do not want to do that.
		 */
		if (cn->cn_flags & MAKEENTRY && op != CREATE)
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
	 * Remove all NTFS core system files from the name space so we do not
	 * need to worry about users damaging a volume by writing to them or
	 * deleting/renaming them and so that we can return fsRtParID (1) as
	 * the inode number of the parent of the volume root directory and
	 * fsRtDirID (2) as the inode number of the volume root directory which
	 * are both expected by Carbon and various applications.
	 */
	if (mft_no < FILE_first_user) {
		lck_rw_unlock_shared(&dir_ni->lock);
		if (name)
			IOFreeType(name, ntfs_dir_lookup_name);
		ntfs_debug("Removing core NTFS system file (mft_no 0x%x) "
				"from name space.", (unsigned)mft_no);
		err = ENOENT;
		goto not_found;
	}
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
			IOFreeType(name, ntfs_dir_lookup_name);
		} else {
			signed res_size;

			res_size = ntfs_to_utf8(vol, name->name, name->len <<
					NTFSCHAR_SIZE_SHIFT, &utf8_name,
					&utf8_size);
			IOFreeType(name, ntfs_dir_lookup_name);
			if (res_size < 0) {
				lck_rw_unlock_shared(&dir_ni->lock);
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
	 *
	 * Note we only drop the directory lock after obtaining the inode
	 * otherwise someone could delete it under our feet.
	 */
	err = ntfs_inode_get(vol, mft_no, FALSE, LCK_RW_TYPE_SHARED, &ni,
			dir_ni->vn, name_cn);
	lck_rw_unlock_shared(&dir_ni->lock);
	if (name_cn == &cn_buf) {
		/* Pick up any modifications to the cn_flags. */
		cn->cn_flags = cn_buf.cn_flags;
		IOFreeData(utf8_name, utf8_size);
	}
	if (!err) {
		/* Consistency check. */
		// FIXME: I cannot remember why we need the "mft_no !=
		// FILE_MFT" test...
		if (MSEQNO(mref) != ni->seq_no && mft_no != FILE_MFT) {
			lck_rw_unlock_shared(&ni->lock);
			(void)vnode_put(ni->vn);
			ntfs_debug("Inode was deleted and reused under our "
					"feet.");
			err = ENOENT;
			goto not_found;
		}
		/*
		 * We found it.  Before we can return it, we have to check if
		 * returning this inode is a valid response to the requested
		 * lookup.  To be more specific, if the lookup was for an
		 * intermediate path component and the inode is not a directory
		 * or symbolic link, it is not a valid response because it
		 * cannot be part of an intermediate path component.  In that
		 * case return an error.
		 */
		if (cn->cn_flags & ISLASTCN || S_ISDIR(ni->mode) ||
				S_ISLNK(ni->mode)) {
			/*
			 * Perfect WIN32/POSIX match or wrong case WIN32/POSIX
			 * match, i.e. cases 1 and 2, respectively.
			 */
			if (!name) {
				*a->a_vpp = ni->vn;
				ntfs_debug("Done (case %d).",
						name_cn == &cn_buf ? 2 : 1);
				lck_rw_unlock_shared(&ni->lock);
				return 0;
			}
			/*
			 * We are too indented.  Handle DOS matches further
			 * below.
			 */
			goto handle_dos_name;
		}
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_put(ni->vn);
		ntfs_debug("Done (intermediate path component requested but "
				"found inode is not a directory or symbolic "
				"link, returning ENOTDIR).");
		err = ENOTDIR;
	} else {
		if (err == ENOENT) {
			ntfs_debug("Inode was deleted under our feet.");
			goto not_found;
		}
		ntfs_error(vol->mp, "Failed to get inode 0x%llx (error %d).",
				(unsigned long long)mft_no, err);
	}
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

		err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0,
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
	IOFreeData(utf8_name, utf8_size);
	*a->a_vpp = ni->vn;
	lck_rw_unlock_shared(&ni->lock);
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
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(vn);
	return err;
   }
}

// TODO: Rename to ntfs_inode_create and move to ntfs_inode.[hc]?
/**
 * ntfs_create - create an inode on an ntfs volume
 * @dir_vn:	vnode of directory in which to create the new inode
 * @vn:		destination pointer for the vnode of the created inode
 * @cn:		componentname specifying name of the inode to create
 * @va:		vnode attributes to assign to the new inode
 * @lock:	if true the ntfs inode of the returned vnode *@vn is locked
 *
 * Create an inode with name as specified in @cn in the directory specified by
 * the vnode @dir_vn.  Assign the attributes @va to the created inode.  Finally
 * return the vnode of the created inode in *@vn.
 *
 * @va is used to determine which type of inode is to be created, i.e. if
 * @va->va_type if VDIR create a directory, etc.
 *
 * If @lock is true the ntfs inode of the returned vnode is locked for writing
 * (NTFS_I(@vn)->lock).
 *
 * Called by the various inode creation ntfs functions (ntfs_vnop_create(),
 * ntfs_vnop_mkdir(), ntfs_vnop_symlink(), ntfs_vnop_mknod(), etc) which are
 * called by the VFS.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create inode names in the POSIX namespace.
 */
static errno_t ntfs_create(vnode_t dir_vn, vnode_t *vn,
		struct componentname *cn, struct vnode_attr *va,
		const BOOL lock)
{
	ntfs_inode *ni, *dir_ni = NTFS_I(dir_vn);
	ntfs_volume *vol;
	FILENAME_ATTR *fn;
	ntfschar *ntfs_name;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	unsigned fn_alloc, fn_size;
	errno_t err, err2;

	if (!dir_ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = dir_ni->vol;
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_debug("Parent inode is not a directory, returning "
				"ENOTDIR.");
		return ENOTDIR;
	}
	if (dir_ni->file_attributes & FILE_ATTR_REPARSE_POINT) {
		ntfs_error(vol->mp, "Parent inode is a reparse point and not "
				"a regular directory, returning ENOTSUP.");
		return ENOTDIR;
	}
	/*
	 * Create a temporary copy of the filename attribute so we can release
	 * the mft record before we add the directory entry.  This is needed
	 * because when we hold the mft record for the newly created inode and
	 * we call ntfs_dir_entry_add() this would cause the mft record for the
	 * directory to be mapped which would result in a deadlock in the event
	 * that both mft records are in the same page.
	 */
	fn_alloc = sizeof(FILENAME_ATTR) + NTFS_MAX_NAME_LEN * sizeof(ntfschar);
	fn = IOMallocData(fn_alloc);
	if (!fn) {
		ntfs_error(vol->mp, "Failed to allocate memory for temporary "
				"filename attribute.");
		return ENOMEM;
	}
	bzero(fn, fn_alloc);
	/* Begin setting up the temporary filename attribute. */
	fn->parent_directory = MK_LE_MREF(dir_ni->mft_no, dir_ni->seq_no);
	/* FILENAME_POSIX is zero and the attribute is already zeroed. */
	/* fn->filename_type = FILENAME_POSIX; */
	/* Convert the name from utf8 to Unicode. */
	ntfs_name = fn->filename;
	ntfs_name_size = NTFS_MAX_NAME_LEN * sizeof(ntfschar);
	ntfs_name_len = utf8_to_ntfs(vol, (u8*)cn->cn_nameptr, cn->cn_namelen,
			&ntfs_name, &ntfs_name_size);
	if (ntfs_name_len < 0) {
		err = -ntfs_name_len;
		if (err == ENAMETOOLONG)
			ntfs_debug("Failed (name is too long).");
		else
			ntfs_error(vol->mp, "Failed to convert name to "
					"Unicode (error %d).", err);
		goto err;
	}
	/* Set the filename length in the temporary filename attribute. */
	fn->filename_length = ntfs_name_len;
	fn_size = sizeof(FILENAME_ATTR) + ntfs_name_len * sizeof(ntfschar);
	/* If no vnode type is specified default to VREG, i.e. regular file. */
	if (va->va_type == VNON)
		va->va_type = VREG;
	/*
	 * We support regular files, directories, symbolic links, sockets,
	 * fifos, and block and character device special filesr.
	 */
	switch (va->va_type) {
	case VBLK:
	case VCHR:
		if (!VATTR_IS_ACTIVE(va, va_rdev)) {
			ntfs_error(vol->mp, "va_type is %s but va_rdev is not "
					"specified!", va->va_type == VBLK ?
					"VBLK" : "VCHR");
			err = EINVAL;
			goto err;
		}
	case VREG:
	case VDIR:
	case VLNK:
	case VSOCK:
	case VFIFO:
		break;
	default:
		ntfs_error(vol->mp, "Tried to create inode of type 0x%x which "
				"is not supported at present.", va->va_type);
		err = ENOTSUP;
		goto err;
	}
	va->va_mode |= VTTOIF(va->va_type);
	/* If no create time is supplied default it to the current time. */
	if (!VATTR_IS_ACTIVE(va, va_create_time))
		nanotime(&va->va_create_time);
	/*
	 * Round the time down to the nearest 100-nano-second interval as
	 * needed for NTFS.
	 */
	va->va_create_time.tv_nsec -= va->va_create_time.tv_nsec % 100;
	/* Set the times in the temporary filename attribute. */
	fn->last_access_time = fn->last_mft_change_time =
			fn->last_data_change_time = fn->creation_time =
			utc2ntfs(va->va_create_time);
	/* Set the bits for all the supported fields at once. */
	va->va_supported |=
			VNODE_ATTR_BIT(va_mode) |
			VNODE_ATTR_BIT(va_flags) |
			VNODE_ATTR_BIT(va_create_time) |
			VNODE_ATTR_BIT(va_type);
again:
	/* Lock the target directory and check that it has not been deleted. */
	lck_rw_lock_exclusive(&dir_ni->lock);
	if (!dir_ni->link_count) {
		/* Remove the target directory from the name cache. */
		cache_purge(dir_vn);
		err = ENOENT;
		goto unl_err;
	}
	/* Allocate and map a new mft record. */
	err = ntfs_mft_record_alloc(vol, va, cn, dir_ni, &ni, &m, &a);
	if (err) {
		if (err != ENOSPC)
			ntfs_error(vol->mp, "Failed to allocate a new on-disk "
					"inode (error %d).", err);
		goto unl_err;
	}
	/*
	 * If requested by the caller, take the ntfs inode lock on the
	 * allocated ntfs inode for writing so no-one can start using it before
	 * it is ready.  For example if it is a symbolic link we cannot allow
	 * anyone to look at it until we have set the data size to the symbolic
	 * link target size otherwise a concurrent ntfs_vnop_readlink() would
	 * return EINVAL as it would see a target size of zero.
	 *
	 * Also, if the inode is a symbolic link we need to take the lock so
	 * that we can create the AFP_AfpInfo attribute when we have finished
	 * setting up the inode.
	 */
	if (lock || S_ISLNK(ni->mode))
		lck_rw_lock_exclusive(&ni->lock);
	/*
	 * @a now points to the location in the allocated mft record at which
	 * we need to insert the filename attribute so we can insert it without
	 * having to do a lookup first.
	 *
	 * Insert the filename attribute and initialize the value to zero.
	 * This cannot fail as we are dealing with a newly allocated mft record
	 * so there must be enough space for a filename attribute even if the
	 * filename is of the maximum allowed length.
	 */
	err = ntfs_resident_attr_record_insert_internal(m, a, AT_FILENAME,
			NULL, 0, fn_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/* Finish setting up the filename attribute value. */
	fn->file_attributes = ni->file_attributes;
	/*
	 * Directories need the FILE_ATTR_DUP_FILENAME_INDEX_PRESENT flag set
	 * in their filename attributes both in their mft records and in the
	 * index entries pointing to them but not in the standard information
	 * attribute which is why it is not set in @ni->file_attributes.
	 */
	if (va->va_type == VDIR)
		fn->file_attributes |= FILE_ATTR_DUP_FILENAME_INDEX_PRESENT;
	/*
	 * Update the data_size in the temporary filename attribute from the
	 * created ntfs inode.  This will not be zero for fifos and block and
	 * character device special files for example.
	 */
	fn->data_size = ni->data_size;
	/*
	 * Copy the created filename attribute into place in the attribute
	 * record.
	 */
	memcpy((u8*)a + le16_to_cpu(a->value_offset), fn, fn_size);
	/*
	 * Set the link count to one to indicate there is one filename
	 * attribute inside the mft record.
	 */
	m->link_count = const_cpu_to_le16(1);
	ni->link_count = 1;
	/*
	 * Ensure the mft record is written to disk.
	 *
	 * Note we do not set any of the NInoDirty*() flags because we have
	 * just created the inode thus all the fields are in sync between the
	 * ntfs_inode @ni and its mft record @m.
	 */
	NInoSetMrecNeedsDirtying(ni);
	/*
	 * Release the mft record.  It is safe to do so even though the
	 * directory entry has not been added yet because the inode is still
	 * locked and marked new thus it is not a candidate for syncing yet.
	 */
	ntfs_mft_record_unmap(ni);
	/*
	 * If the inode is a symbolic link now create the AFP_AfpInfo attribute
	 * with the Finder Info specifying that this is a symbolic link.
	 */
	if (S_ISLNK(ni->mode)) {
		err = ntfs_inode_afpinfo_write(ni);
		/*
		 * If the caller has not requested that the inode be returned
		 * locked unlock it now.
		 */
		if (!lock)
			lck_rw_unlock_exclusive(&ni->lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to create AFP_AfpInfo "
					"attribute in allocated inode 0x%llx "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
			goto rm_err;
		}
	}
	/* Add the created filename attribute to the parent directory index. */
	err = ntfs_dir_entry_add(dir_ni, fn, fn_size,
			MK_LE_MREF(ni->mft_no, ni->seq_no));
	if (!err) {
		/* Free the temporary filename attribute. */
		IOFreeData(fn, fn_alloc);
		/*
		 * Invalidate negative cache entries in the directory.  We need
		 * to do this because there may be negative cache entries
		 * which would match the name of the just created inode but in
		 * a different case.  Such negative cache entries would now be
		 * incorrect thus we need to throw away all negative cache
		 * entries to ensure there cannot be any incorrectly negative
		 * entries in the name cache.
		 */
		cache_purge_negatives(dir_vn);
		/*
		 * Add the inode to the name cache.  Note that
		 * ntfs_vnop_lookup() will have caused the name to not be
		 * cached because it will have cleared the MAKEENTRY flag.
		 */
		cache_enter(dir_ni->vn, ni->vn, cn);
		/* We are done with the directory so unlock it. */
		lck_rw_unlock_exclusive(&dir_ni->lock);
		/*
		 * We can finally unlock and unmark as new the new ntfs inode
		 * thus rendering the inode a full member of society.
		 */
		ntfs_inode_unlock_alloc(ni);
		ntfs_debug("Done (new mft_no 0x%llx).",
				(unsigned long long)ni->mft_no);
		*vn = ni->vn;
		return 0;
	}
	/*
	 * We failed to add the directory entry thus we have to effectively
	 * delete the created inode again.  To do this we need to map the mft
	 * record and mark it as no longer in use.
	 *
	 * We then also need to set the link count in the ntfs inode to zero to
	 * reflect that it is deleted and to ensure that the subsequent
	 * vnode_put() results in ntfs_delete_inode() being called (via
	 * VNOP_INACTIVE() and ntfs_vnop_inactive() respectively).
	 *
	 * But first, unlock the allocated ntfs inode if we locked it above.
	 * No-one can get to it now as it does not have a directory entry
	 * pointing to it.
	 */
rm_err:
	if (lock)
		lck_rw_unlock_exclusive(&ni->lock);
	err2 = ntfs_mft_record_map(ni, &m);
	if (err2) {
		ntfs_error(vol->mp, "Failed to map mft record in error code "
				"path (error %d).  Run chkdsk to recover the "
				"lost mft record.", err2);
		NVolSetErrors(vol);
	} else {
		m->flags &= ~MFT_RECORD_IN_USE;
		NInoSetMrecNeedsDirtying(ni);
		ntfs_mft_record_unmap(ni);
	}
	ni->link_count = 0;
	lck_rw_unlock_exclusive(&dir_ni->lock);
	ntfs_inode_unlock_alloc(ni);
	cache_purge(ni->vn);
	(void)vnode_put(ni->vn);
	if (err == EEXIST) {
		/*
		 * There are two possible reasons why the directory entry
		 * already exists.  Either someone created it under our feet in
		 * which case we try to look up the existing vnode and retrn
		 * that instead and failing that we try to create the inode
		 * again or the name really does exist but we have removed it
		 * from the name space thus ntfs_vnop_lookup() will always
		 * return ENOENT/EJUSTRETURN for it.  This is the case for the
		 * core system files for example.  This would cause an infinite
		 * loop thus we need to check for this case by checking that
		 * the name being created does not match one of the core system
		 * filenames and if it does we return EEXIST.
		 */
		if (dir_ni == vol->root_ni) {
			/* Catch the "." entry. */
			if (cn->cn_namelen == 1 && cn->cn_nameptr[0] == '.')
				goto is_system;
			/*
			 * Catch the core system files which all start with the
			 * '$' character.
			 */
			if (cn->cn_nameptr[0] == '$') {
				char *n = (char*)cn->cn_nameptr + 1;
				int l = cn->cn_namelen;

				if ((l == 4 && !strncmp(n, "MFT", 3)) ||
						(l == 5 && !strncmp(n, "Boot",
						4)) ||
						(l == 6 && !strncmp(n, "Quota",
						5)) ||
						(l == 7 && (
						!strncmp(n, "Volume", 6) ||
						!strncmp(n, "Bitmap", 6) ||
						!strncmp(n, "Secure", 6) ||
						!strncmp(n, "UpCase", 6) ||
						!strncmp(n, "Extend", 6))) ||
						(l == 8 && (
						!strncmp(n, "MFTMirr", 7) ||
						!strncmp(n, "LogFile", 7) ||
						!strncmp(n, "AttrDef", 7) ||
						!strncmp(n, "BadClus", 7))))
					goto is_system;
			}
		}
		ntfs_debug("Inode was created under our feet.");
		/*
		 * If the inode was created under our feet, we are creating a
		 * regular file, and the caller did not want an exclusive
		 * create, simply look up the inode and return that.
		 */
		if (va->va_type == VREG && !(va->va_vaflags & VA_EXCLUSIVE)) {
			struct vnop_lookup_args la;

			cn->cn_nameiop = LOOKUP;
			la = (struct vnop_lookup_args) {
				.a_desc = &vnop_lookup_desc,
				.a_dvp = dir_vn,
				.a_vpp = vn,
				.a_cnp = cn,
			};
			err = ntfs_vnop_lookup(&la);
			cn->cn_nameiop = CREATE;
			/*
			 * If the inode that was created under our feet was
			 * also deleted under our feet, repeat the whole
			 * process.
			 */
			if (err == ENOENT || err == EJUSTRETURN) {
				*vn = NULL;
				goto again;
			}
			/*
			 * Make sure the vnode we looked up is a regular file
			 * as we would not want to return a directory instead
			 * of a file for example.
			 */
			if (!err && vnode_vtype(*vn) != VREG) {
				(void)vnode_put(*vn);
				*vn = NULL;
				err = EEXIST;
			}
		}
	} else
		ntfs_error(vol->mp, "Failed to add directory entry (error "
				"%d).", err);
err:
	IOFreeData(fn, fn_alloc);
	return err;
unl_err:
	lck_rw_unlock_exclusive(&dir_ni->lock);
	goto err;
is_system:
	ntfs_error(vol->mp, "Cannot create inode with name %.*s in the volume "
			"root directory as the name clashes with the name of "
			"a core system file.  Returning EEXIST.",
			(int)cn->cn_namelen, cn->cn_nameptr);
	err = EEXIST;
	*vn = NULL;
	goto err;
}

/**
 * ntfs_vnop_create - create a regular file
 * @a:		arguments to create function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory in which to create the file
 *	vnode_t *a_vpp;			destination pointer for the created file
 *	struct componentname *a_cnp;	name of the file to create
 *	struct vnode_attr *a_vap;	attributes to set on the created file
 *	vfs_context_t a_context;
 *
 * Create a regular file with name as specified in @a->a_cnp in the directory
 * specified by the vnode @a->a_dvp.  Assign the attributes @a->a_vap to the
 * created file.  Finally return the vnode of the created file in *@a->a_vpp.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create filenames in the POSIX namespace.
 */
static int ntfs_vnop_create(struct vnop_create_args *a)
{
	errno_t err;
#ifdef DEBUG
	ntfs_inode *ni = NTFS_I(a->a_dvp);

	if (ni)
		ntfs_debug("Creating a file named %.*s in directory mft_no "
				"0x%llx.", (int)a->a_cnp->cn_namelen,
				a->a_cnp->cn_nameptr,
				(unsigned long long)ni->mft_no);
#endif
	err = ntfs_create(a->a_dvp, a->a_vpp, a->a_cnp, a->a_vap, FALSE);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_mknod - create a special file node
 * @a:		arguments to mknod function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory in which to create the file
 *	vnode_t *a_vpp;			destination pointer for the created file
 *	struct componentname *a_cnp;	name of the file to create
 *	struct vnode_attr *a_vap;	attributes to set on the created file
 *	vfs_context_t a_context;
 *
 * Create a special file node with name as specified in @a->a_cnp in the
 * directory specified by the vnode @a->a_dvp.  Assign the attributes @a->a_vap
 * to the created node.  Finally return the vnode of the created file in
 * *@a->a_vpp.
 *
 * The type of special file node to create is specified by the caller in
 * @a->a_vap->va_type and can be one of:
 *	VSOCK - create a socket
 *	VFIFO - create a fifo
 *	VBLK  - create a block special device
 *	VCHR  - create a character special device
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create filenames in the POSIX namespace.
 */
static int ntfs_vnop_mknod(struct vnop_mknod_args *a)
{
	errno_t err;
#ifdef DEBUG
	ntfs_inode *ni = NTFS_I(a->a_dvp);

	if (ni)
		ntfs_debug("Creating a special inode of type 0x%x named %.*s "
				"in directory mft_no 0x%llx.",
				a->a_vap->va_type, (int)a->a_cnp->cn_namelen,
				a->a_cnp->cn_nameptr,
				(unsigned long long)ni->mft_no);
#endif
	err = ntfs_create(a->a_dvp, a->a_vpp, a->a_cnp, a->a_vap, FALSE);
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
	ntfs_inode *base_ni, *ni = NTFS_I(a->a_vp);
	errno_t err = 0;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Entering for mft_no 0x%llx, mode 0x%x.",
			(unsigned long long)ni->mft_no, (unsigned)a->a_mode);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/*
	 * All the core system files cannot possibly be opened because they are
	 * removed from the name space thus it is impossible for a process to
	 * obtain a vnode to them thus VNOP_OPEN() can never be called for
	 * them.  The only exception is the root directory which we of course
	 * allow access to.
	 */
	if (ni->mft_no < FILE_first_user && ni != ni->vol->root_ni)
		panic("%s(): Called for a system inode.  This is not "
				"possible.\n", __FUNCTION__);
	lck_rw_lock_shared(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		lck_rw_unlock_shared(&ni->lock);
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Cannot open deleted mft_no 0x%llx, returning "
				"ENOENT.", (unsigned long long)ni->mft_no);
		return ENOENT;
	}
	/*
	 * Do not allow opening encrpyted files as we do not support reading,
	 * writing, nor mmap()ing them.
	 */
	if (NInoEncrypted(ni)) {
		lck_rw_unlock_shared(&ni->lock);
		ntfs_debug("Cannot open encrypted mft_no 0x%llx, returning "
				"EACCES.", (unsigned long long)ni->mft_no);
		return EACCES;
	}
	lck_rw_unlock_shared(&ni->lock);
	/*
	 * We keep track of how many times the base vnode has been opened and
	 * we count other vnodes towards the base vnode open count to ensure
	 * we do the right thing in ntfs_unlink().
	 */
	OSIncrementAtomic(&base_ni->nr_opens);
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
	vnode_t vn = a->a_vp;
	ntfs_inode *base_ni, *ni = NTFS_I(vn);

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return 0;
	}
	ntfs_debug("Entering for mft_no 0x%llx, fflag 0x%x.",
			(unsigned long long)ni->mft_no, a->a_fflag);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/*
	 * We keep track of how many times the base vnode has been opened and
	 * we count other vnodes towards the base vnode open count to ensure
	 * we do the right thing in ntfs_unlink().
	 */
	OSDecrementAtomic(&base_ni->nr_opens);
	/*
	 * If the vnode is still in use release any expired directory hints.
	 *
	 * If the vnode is no longer in use release all directory hints.
	 *
	 * Note we check for presence of directory hints outside the locks as
	 * an optimization.  It is not a disaster if we miss any as all will be
	 * released in ntfs_inode_free() before the inode is thrown away at the
	 * latest.
	 */
	if (ni != base_ni && ni->type == AT_INDEX_ALLOCATION &&
			ni->nr_dirhints) {
		int busy;

		busy = vnode_isinuse(vn, ni->nr_refs + 1);
		lck_rw_lock_exclusive(&ni->lock);
		ntfs_dirhints_put(ni, busy);
		lck_rw_unlock_exclusive(&ni->lock);
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_vnop_access -
 *
 */
static int ntfs_vnop_access(struct vnop_access_args *a __attribute__((unused)))
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
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
 * Return 0 on success and errno on error.
 *
 * TODO: Implement more attributes.
 */
static int ntfs_vnop_getattr(struct vnop_getattr_args *a)
{
	MFT_REF parent_mref;
	ino64_t mft_no;
	s64 on_disk_size;
	struct vnode_attr *va = a->a_vap;
	ntfs_inode *ni, *base_ni;
	ntfs_volume *vol;
	const char *name;
	FILE_ATTR_FLAGS file_attributes;
	unsigned flags;
	errno_t err;
	lck_rw_type_t lock;
	BOOL is_root, name_is_done, have_parent;

	ni = NTFS_I(a->a_vp);
	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return 0;
	}
	vol = ni->vol;
	mft_no = ni->mft_no;
	have_parent = name_is_done = is_root = FALSE;
	ntfs_debug("Entering for mft_no 0x%llx.", (unsigned long long)mft_no);
	base_ni = ni;
	if (NInoAttr(ni)) {
		base_ni = ni->base_ni;
		lck_rw_lock_shared(&base_ni->lock);
	}
	lck_rw_lock_shared(&ni->lock);
	lock = LCK_RW_TYPE_SHARED;
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * If this is the root directory, leave it to the VFS to get the name
	 * from the mountpoint (see below).
	 */
	if (base_ni == vol->root_ni)
		name_is_done = is_root = TRUE;
	/* For directories always return a link count of 1. */
	va->va_nlink = 1;
	if (!S_ISDIR(ni->mode))
		va->va_nlink = ni->link_count;
	va->va_rdev = (dev_t)0;
	switch (ni->mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		/*
		 * For block and character device special inodes return the
		 * device.
		 */
		va->va_rdev = ni->rdev;
	case S_IFIFO:
	case S_IFSOCK:
		/*
		 * For fifos, sockets, block and character device special files
		 * return all sizes set to zero.
		 */
		va->va_total_alloc = va->va_data_alloc = va->va_total_size =
				va->va_data_size = 0;
		break;
	default:
		lck_spin_lock(&ni->size_lock);
		/*
		 * We cheat for both the total size and the total allocated
		 * size and just return the attribute size rather than looping
		 * over all ($DATA?) attributes and adding up their sizes.
		 */
		va->va_total_size = va->va_data_size = ni->data_size;
		/*
		 * Resident attributes reside inside the on-disk inode and thus
		 * have no on-disk allocation because the on-disk inode itself
		 * is already accounted for in the allocated size of the $MFT
		 * system file which contains the table of on-disk inodes.
		 * Perhaps more importantly, if we delete a resident file no
		 * space would be freed up on the volume, thus we definitely
		 * need to return zero for the allocated size of such resident
		 * files.
		 */
		on_disk_size = 0;
		if (NInoNonResident(ni)) {
			if (ni->type == AT_DATA && (NInoCompressed(ni) ||
					NInoSparse(ni)))
				on_disk_size = ni->compressed_size;
			else
				on_disk_size = ni->allocated_size;
		}
		va->va_total_alloc = va->va_data_alloc = on_disk_size;
		lck_spin_unlock(&ni->size_lock);
	}
	va->va_iosize = ubc_upl_maxbufsize();
	va->va_uid = ni->uid;
	va->va_gid = ni->gid;
	va->va_mode = ni->mode;
	file_attributes = base_ni->file_attributes;
	/*
	 * Do not allow the volume root directory to be read-only or hidden and
	 * do not allow directories in general to be read-only as Windows uses
	 * the read-only bit on directories for completely different purposes
	 * like customized/specialized folder views which are lost when you
	 * clear the read-only bit.
	 */
	if (S_ISDIR(base_ni->mode)) {
		file_attributes &= ~FILE_ATTR_READONLY;
		if (is_root)
			file_attributes &= ~FILE_ATTR_HIDDEN;
	}
	flags = 0;
/*
 *	if (NInoCompressed(ni))
 *		flags |= SF_COMPRESSED;
 */
	if (file_attributes & FILE_ATTR_READONLY)
		flags |= UF_IMMUTABLE;
	if (file_attributes & FILE_ATTR_HIDDEN)
		flags |= UF_HIDDEN;
	/*
	 * Windows does not set the "needs archiving" bit on directories
	 * except for encrypted directories where it does set the bit.
	 */
	if ((!S_ISDIR(base_ni->mode) ||
			file_attributes & FILE_ATTR_ENCRYPTED) &&
			!(file_attributes & FILE_ATTR_ARCHIVE))
		flags |= SF_ARCHIVED;
	va->va_flags = flags;
	va->va_create_time = base_ni->creation_time;
	va->va_access_time = base_ni->last_access_time;
	va->va_modify_time = base_ni->last_data_change_time;
	va->va_change_time = base_ni->last_mft_change_time;
	/*
	 * NTFS does not distinguish between the inode and its hard links.
	 *
	 * We have to remap the root directory inode to inode number 2, i.e.
	 * fsRtDirID, for compatibility with Carbon.
	 */
	if (!is_root)
		va->va_fileid = mft_no;
	else
		va->va_fileid = 2;
	va->va_fsid = vol->dev;
	/* FIXME: What is the difference between the below two? */
	va->va_filerev = base_ni->seq_no;
	va->va_gen = base_ni->seq_no;
	va->va_encoding = 0x7e; /* = kTextEncodingMacUnicode */
	va->va_supported |=
			VNODE_ATTR_BIT(va_rdev) |
			VNODE_ATTR_BIT(va_nlink) |
			VNODE_ATTR_BIT(va_total_size) |
			VNODE_ATTR_BIT(va_total_alloc) |
			VNODE_ATTR_BIT(va_data_size) |
			VNODE_ATTR_BIT(va_data_alloc) |
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
			VNODE_ATTR_BIT(va_fsid) |
			VNODE_ATTR_BIT(va_filerev) |
			VNODE_ATTR_BIT(va_gen) |
			VNODE_ATTR_BIT(va_encoding) |
			0;
	/*
	 * Return va_parentid, i.e. the mft record number of the parent of the
	 * inode, if it was requested.
	 *
	 * We have to return 1, i.e. fsRtParID, for the parent inode number of
	 * the root directory inode for compatibility with Carbon.  Simillarly
	 * we have to return 2, i.e. fsRtDirID, if the parent inode is the root
	 * directory inode.
	 *
	 * For all other inodes we try to get the parent from the vnode and if
	 * it does not have the vnode cached then if the inode is an attribute
	 * inode we return the inode number of the base inode (in line with how
	 * named streams work on Mac OS X) and otherwise we obtain the parent
	 * mft reference by looking up a filename attribute record in the mft
	 * record of the inode and obtaining the parent mft record reference
	 * from there.
	 *
	 * There is one pitfall with this approach for files and that is that a
	 * file may have multiple parents and we are returning a random one but
	 * that is the best we can do.
	 *
	 * To make this a little better we get the name at the same time as we
	 * get the parent mft reference so we can at least return a parent id
	 * and name that match, i.e. the name is present in the parent id.
	 *
	 * And to make this even better, when the parent is requested and a
	 * name is cached in the vnode, we use the name in the vnode to find
	 * the parent that matches that name if it exists.  If it does not
	 * exist we revert to finding a random parent.
	 */
	if (VATTR_IS_ACTIVE(va, va_parentid)) {
		ino64_t parent_mft_no;
		vnode_t parent_vn;

		if (is_root && base_ni == ni)
			VATTR_RETURN(va, va_parentid, 1);
		else if ((parent_vn = vnode_getparent(ni->vn))) {
			parent_mft_no = NTFS_I(parent_vn)->mft_no;
			(void)vnode_put(parent_vn);
			have_parent = TRUE;
			if (parent_mft_no == FILE_root)
				parent_mft_no = 2;
			VATTR_RETURN(va, va_parentid, parent_mft_no);
		} else if (ni != base_ni) {
			parent_mft_no = base_ni->mft_no;
			if (parent_mft_no == FILE_root)
				parent_mft_no = 2;
			VATTR_RETURN(va, va_parentid, parent_mft_no);
		} else /* if (ni == base_ni) */ {
			name_is_done = TRUE;
			name = NULL;
			if (VATTR_IS_ACTIVE(va, va_name))
				name = va->va_name;
			err = ntfs_inode_get_name_and_parent_mref(base_ni,
					FALSE, &parent_mref, name);
			if (err) {
				ntfs_error(base_ni->vol->mp, "Failed to obtain "
						"parent mft reference for "
						"mft_no 0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				goto err;
			}
			parent_mft_no = MREF(parent_mref);
			if (parent_mft_no == FILE_root)
				parent_mft_no = 2;
			va->va_parentid = parent_mft_no;
			va->va_supported |= VNODE_ATTR_BIT(va_parentid) |
					(name ? VNODE_ATTR_BIT(va_name) : 0);
		}
	}
	/*
	 * Return va_name, i.e. the name of the inode, if it was requested.
	 *
	 * If this is the root directory of the volume, leave it to the VFS to
	 * find the mounted-on name, which is different from the real volume
	 * root directory name of "." (this is ensured by the fact that
	 * @name_is_done was set to TRUE for the root directory earlier).
	 *
	 * For all other inodes we try to get the name from the vnode and if it
	 * does not have the name cached we obtain the name by looking up a
	 * filename attribute record in the mft record of the inode and using
	 * that.
	 *
	 * Note we do not need to do anything if we dealt with the name as part
	 * of dealing with va_parentid above.  In this case @name_is_done will
	 * be set to true.
	 *
	 * Also we do not need to do anything if we tried to deal with
	 * va_parentid above and failed as we would only fail again here.  This
	 * means that if @err is not zero we skip the call to
	 * ntfs_inode_get_name_and_parent_mref().
	 *
	 * TODO: What do we return for attribute inodes?  Shall we exclude them
	 * from VNOP_GETATTR() altogether?  For now we simply do not return a
	 * name for them.
	 */
	if (!name_is_done && VATTR_IS_ACTIVE(va, va_name) && ni == base_ni) {
		name = vnode_getname(base_ni->vn);
		if (name) {
			(void)strlcpy(va->va_name, name, MAXPATHLEN - 1);
			VATTR_SET_SUPPORTED(va, va_name);
			(void)vnode_putname(name);
		} else {
			err = ntfs_inode_get_name_and_parent_mref(base_ni,
					have_parent, &parent_mref, va->va_name);
			if (err) {
				ntfs_error(base_ni->vol->mp, "Failed to obtain "
						"parent mft reference for "
						"mft_no 0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				goto err;
			}
			/*
			 * We forcibly overwrite the parent id with the
			 * possibly new parent id here to be consistent with
			 * the name, i.e. we want the name we return to
			 * actually exist in the returned parent.
			 *
			 * If we already had the parent id from before then
			 * ntfs_inode_get_name_and_parent_mref() will have
			 * found the name matching this parent id thus our
			 * setting of the parent id here will be a no-op.
			 */
			va->va_parentid = MREF(parent_mref);
			if (va->va_parentid == FILE_root)
				va->va_parentid = 2;
			va->va_supported |= VNODE_ATTR_BIT(va_parentid) |
					VNODE_ATTR_BIT(va_name);
		}
	}
	/*
	 * Unlock the attribute inode as we do not need it any more and so we
	 * cannot deadlock with converting the lock on the base inode to
	 * exclusive and with the call to ntfs_inode_afpinfo_read() below.
	 */
	if (ni != base_ni)
		lck_rw_unlock_shared(&ni->lock);
	if (VATTR_IS_ACTIVE(va, va_backup_time)) {
		if (!NInoValidBackupTime(base_ni)) {
			if (!lck_rw_lock_shared_to_exclusive(&base_ni->lock)) {
				lck_rw_lock_exclusive(&base_ni->lock);
				if (NInoDeleted(base_ni)) {
					cache_purge(base_ni->vn);
					lck_rw_unlock_exclusive(&base_ni->lock);
					return ENOENT;
				}
			}
			lock = LCK_RW_TYPE_EXCLUSIVE;
			/*
			 * Load the AFP_AfpInfo stream and initialize the
			 * backup time and Finder Info (if they are not already
			 * valid).
			 */
			err = ntfs_inode_afpinfo_read(base_ni);
			if (err) {
				ntfs_error(base_ni->vol->mp, "Failed to "
						"read AFP_AfpInfo attribute "
						"from inode 0x%llx (error "
						"%d).", (unsigned long long)
						base_ni->mft_no, err);
				lck_rw_unlock_exclusive(&base_ni->lock);
				return err;
			}
			if (!NInoValidBackupTime(base_ni))
				panic("%s(): !NInoValidBackupTime(base_ni)\n",
						__FUNCTION__);
		}
		VATTR_RETURN(va, va_backup_time, base_ni->backup_time);
	}
	if (lock == LCK_RW_TYPE_SHARED)
		lck_rw_unlock_shared(&base_ni->lock);
	else
		lck_rw_unlock_exclusive(&base_ni->lock);
	ntfs_debug("Done.");
	return 0;
err:
	lck_rw_unlock_shared(&ni->lock);
	if (ni != base_ni)
		lck_rw_unlock_shared(&base_ni->lock);
	return err;
}

/**
 * ntfs_vnop_setattr - set attributes of a vnode or of the mounted volume
 * @a:		arguments to setattr function
 *
 * @a contains:
 *	vnode_t a_vp;			vnode of which to set attributes
 *	struct vnode_attr *a_vap;	attributes to set and source
 *	vfs_context_t a_context;
 *
 * Set the attributes described by @a_vap in the vnode @a_vp.  Some attributes
 * are intercepted by the VFS in setattrlist() and setvolattrlist() so we do
 * not bother with them.
 *
 * At present we do not support all attributes.  We declare what we support to
 * the world in our VFS_GETATTR() function (ntfs_vfsops.c::ntfs_getattr()) so
 * do not forget to update that when support for further attributes is added
 * here.
 *
 * Return 0 on success and errno on error.
 *
 * TODO: Implement more attributes.
 */
static int ntfs_vnop_setattr(struct vnop_setattr_args *a)
{
	ntfs_inode *base_ni, *ni = NTFS_I(a->a_vp);
	ntfs_volume *vol;
	struct vnode_attr *va = a->a_vap;
	errno_t err = 0;
	BOOL dirty_times = FALSE;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	base_ni = ni;
	if (NInoAttr(ni)) {
		base_ni = ni->base_ni;
		lck_rw_lock_exclusive(&base_ni->lock);
	}
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		err = ENOENT;
		goto unl_err;
	}
	if (VATTR_IS_ACTIVE(va, va_data_size)) {
		ntfs_debug("Changing size for mft_no 0x%llx to 0x%llx.",
				(unsigned long long)ni->mft_no,
				(unsigned long long)va->va_data_size);
#if 1		// TODO: Remove this when sparse support is done...
		if (NInoSparse(ni)) {
			err = ENOTSUP;
			goto unl_err;
		}
#endif
		/*
		 * Do not allow calling for $MFT/$DATA as it would destroy the
		 * volume.
		 *
		 * Also only allow setting the size of VREG vnodes as that
		 * covers both regular files and named streams whilst excluding
		 * symbolic links for example.
		 */
		if (vnode_vtype(ni->vn) != VREG ||
				(!ni->mft_no && !NInoAttr(ni)))
			err = EPERM;
		else
			err = ntfs_attr_resize(ni, va->va_data_size,
					va->va_vaflags & 0xffff, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to set inode size (error "
					"%d).", err);
			goto unl_err;
		}
		VATTR_SET_SUPPORTED(va, va_data_size);
	}
	/*
	 * Unlock the attribute inode as we do not need it any more and so we
	 * cannot deadlock with the call to ntfs_inode_afpinfo_write() below.
	 */
	if (ni != base_ni)
		lck_rw_unlock_exclusive(&ni->lock);
	if (VATTR_IS_ACTIVE(va, va_flags)) {
		u32 flags = va->va_flags;
		BOOL dirty_flags = FALSE;

		/*
		 * Only allow changing of supported flags.  There are two
		 * exceptions and those are the archived flag and read-only bit
		 * on directories which are not supported on NTFS but we have
		 * to ignore them or too many things break such as "cp -pr"
		 * from a more sensible file system.
		 */
		if (flags & ~(SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE |
				UF_HIDDEN /* | SF_COMPRESSED */)) {
			ntfs_error(vol->mp, "Cannot set unsupported flags "
					"0x%x.",
					(unsigned)(flags & ~(SF_ARCHIVED |
					SF_IMMUTABLE | UF_IMMUTABLE |
					UF_HIDDEN)));
			err = EINVAL;
			goto err;
		}
		/*
		 * We do not allow modification for any of the core NTFS
		 * system files which we want to remain as they are except that
		 * we silently ignore changes to the root directory.
		 */
		if (base_ni->mft_no < FILE_first_user &&
				base_ni != vol->root_ni) {
			ntfs_error(vol->mp, "Refusing to change flags on core "
					"NTFS system file (mft_no 0x%llx).",
					(unsigned long long)base_ni->mft_no);
			err = EPERM;
			goto err;
		}
		/*
		 * We currently do not support changing the compression state
		 * of a vnode.
		 *
		 * Further, only the base inode may be compressed.
		 */
/*
 *		if (((flags & SF_COMPRESSED) && !NInoCompressed(ni)) ||
 *				(!(flags & SF_COMPRESSED) &&
 *				NInoCompressed(ni))) {
 *			if (ni != base_ni) {
 *				ntfs_error(vol->mp, "Only regular files and "
 *						"directories may be "
 *						"compressed, aborting.");
 *				err = EINVAL;
 *				goto err;
 *			}
 *			ntfs_warning(vol->mp, "Changing the compression state "
 *					"is not supported at present, "
 *					"returning ENOTSUP.");
 *			err = ENOTSUP;
 *			goto err;
 *		}
 */
		/*
		 * The root directory of a volume always has the hidden bit set
		 * but we pretend that it is not hidden to OS X and we do not
		 * allow this bit to be modified for the root directory.
		 */
		if (base_ni != vol->root_ni) {
			/*
			 * If the Finder info is valid need to update it as
			 * well.  Note setting or clearing the hidden flag in
			 * the Finder info does not cause the Finder info to
			 * become dirty as the hidden bit is not stored on disk
			 * in the Finder info.
			 */
			if (flags & UF_HIDDEN) {
				base_ni->file_attributes |= FILE_ATTR_HIDDEN;
				if (NInoValidFinderInfo(base_ni))
					base_ni->finder_info.attrs |=
							FINDER_ATTR_IS_HIDDEN;
			} else {
				base_ni->file_attributes &= ~FILE_ATTR_HIDDEN;
				if (NInoValidFinderInfo(base_ni))
					base_ni->finder_info.attrs &=
							~FINDER_ATTR_IS_HIDDEN;
			}
			dirty_flags = TRUE;
		}
		/*
		 * Windows does not allow users to set/clear the read-only bit
		 * on directories.  In fact Windows uses the read-only bit on a
		 * directory to signify that a customized or specialized folder
		 * view is in effect thus we do not allow setting/clearing the
		 * read-only bit on directories from OS X.
		 *
		 * Windows does not set the "needs archiving" bit on
		 * directories.
		 *
		 * The only exception are encrypted directories which do have
		 * the "needs archiving" bit set but we do not want to allow
		 * this bit to be cleared so ignore them, too.
		 */
		if (!S_ISDIR(base_ni->mode)) {
			if (flags & (SF_IMMUTABLE | UF_IMMUTABLE))
				base_ni->file_attributes |= FILE_ATTR_READONLY;
			else
				base_ni->file_attributes &= ~FILE_ATTR_READONLY;
			if (flags & SF_ARCHIVED)
				base_ni->file_attributes &= ~FILE_ATTR_ARCHIVE;
			else
				base_ni->file_attributes |= FILE_ATTR_ARCHIVE;
			dirty_flags = TRUE;
		}
		if (dirty_flags)
			NInoSetDirtyFileAttributes(base_ni);
		VATTR_SET_SUPPORTED(va, va_flags);
	}
	if (VATTR_IS_ACTIVE(va, va_create_time)) {
		base_ni->creation_time = va->va_create_time;
		VATTR_SET_SUPPORTED(va, va_create_time);
		dirty_times = TRUE;
	}
	if (VATTR_IS_ACTIVE(va, va_modify_time)) {
		base_ni->last_data_change_time = va->va_modify_time;
		VATTR_SET_SUPPORTED(va, va_modify_time);
		dirty_times = TRUE;
		/*
		 * The following comment came from the HFS code:
		 *
		 * <quote>The utimes system call can reset the modification
		 * time but it doesn't know about HFS create times.  So we need
		 * to ensure that the creation time is always at least as old
		 * as the modification time.</quote>
		 *
		 * SMB also follows this behaviour and it also adds the
		 * following comment:
		 *
		 * <quote>The HFS code also checks to make sure it was not the
		 * root vnode. Don Brady said that the SMB code should not use
		 * that part of the check.</quote>
		 *
		 * I assume the root vnode check is there in HFS as it does not
		 * support times on the root vnode at all so the check is
		 * needed for HFS only.
		 *
		 * The same applies for NTFS so follow the HFS/SMB behaviour.
		 *
		 * One salient point is that we only do the above if the
		 * creation time is not being explicitly set already.
		 */
		if (!VATTR_IS_ACTIVE(va, va_create_time) &&
				(va->va_modify_time.tv_sec <
				base_ni->creation_time.tv_sec ||
				(va->va_modify_time.tv_sec ==
				base_ni->creation_time.tv_sec &&
				va->va_modify_time.tv_nsec <
				base_ni->creation_time.tv_nsec)))
			base_ni->creation_time = va->va_modify_time;
	}
	if (VATTR_IS_ACTIVE(va, va_change_time)) {
		base_ni->last_mft_change_time = va->va_change_time;
		VATTR_SET_SUPPORTED(va, va_change_time);
		dirty_times = TRUE;
	}
	if (VATTR_IS_ACTIVE(va, va_access_time)) {
		base_ni->last_access_time = va->va_access_time;
		VATTR_SET_SUPPORTED(va, va_access_time);
		dirty_times = TRUE;
	}
	if (dirty_times)
		NInoSetDirtyTimes(base_ni);
	if (VATTR_IS_ACTIVE(va, va_backup_time)) {
		base_ni->backup_time = va->va_backup_time;
		NInoSetValidBackupTime(base_ni);
		NInoSetDirtyBackupTime(base_ni);
		/*
		 * Now write (if needed creating) the AFP_AfpInfo attribute
		 * with the specified backup time.
		 */
		err = ntfs_inode_afpinfo_write(base_ni);
		if (err) {
			ntfs_error(vol->mp, "Failed to write/create "
					"AFP_AfpInfo attribute in inode "
					"0x%llx (error %d).",
					(unsigned long long)base_ni->mft_no,
					err);
			goto err;
		}
		VATTR_SET_SUPPORTED(va, va_backup_time);
	}
	ntfs_debug("Done.");
err:
	lck_rw_unlock_exclusive(&base_ni->lock);
	return err;
unl_err:
	if (ni != base_ni)
		lck_rw_unlock_exclusive(&ni->lock);
	goto err;
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
	int max_upl_size = ubc_upl_maxbufsize();

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
	/*
	 * Get the raw inode.  We take the inode lock shared to protect against
	 * concurrent writers as the compressed data is invalid whilst a write
	 * is in progress.
	 */
	err = ntfs_raw_inode_get(ni, LCK_RW_TYPE_SHARED, &raw_ni);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get raw inode (error %d).",
				err);
		return err;
	}
	if (!NInoRaw(raw_ni))
		panic("%s(): Requested raw inode but got non-raw one.\n",
				__FUNCTION__);
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
		if (count > max_upl_size)
			count = max_upl_size;
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
		err = uiomove((caddr_t)(kaddr + delta), count, uio);
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
	(void)vnode_put(raw_ni->vn);
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

// TODO: Rename to ntfs_inode_read and move to ntfs_inode.[hc]?
/**
 * ntfs_read - read a number of bytes from an inode into memory
 * @ni:		ntfs inode whose data to read into memory
 * @uio:	destination in which to return the read data
 * @ioflags:	flags further describing the read request
 * @locked:	if true the ntfs inode lock is already taken for reading
 *
 * Read uio_resid(@uio) bytes from the ntfs inode @ni, starting at byte offset
 * uio_offset(@uio) into the inode into the destination buffer pointed to by
 * @uio.
 *
 * The flags in @ioflags further describe the read request.  The following
 * ioflags are currently defined in OS X kernel (a lot of them are not
 * applicable to VNOP_READ() however):
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
 *
 * Note it is up to the caller to verify that reading from the inode @ni makes
 * sense.  We cannot do the verification inside ntfs_read() as it is called
 * from various VNOPs which all have different requirements.  For example
 * VNOP_READLINK(), i.e. ntfs_vnop_readlink(), needs to only allow S_ISLNK()
 * inodes whilst VNOP_READ(), i.e. ntfs_vnop_read(), needs to not allow
 * S_ISLNK() but needs to allow S_IFREG() instead but only if it is not a
 * system file.
 */
static errno_t ntfs_read(ntfs_inode *ni, uio_t uio, const int ioflags,
		const BOOL locked)
{
	s64 size;
	user_ssize_t start_count;
	off_t ofs;
	vnode_t vn = ni->vn;
	ntfs_inode *base_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	int err, count;

	ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	ntfs_debug("Entering for file inode 0x%llx, offset 0x%llx, count "
			"0x%llx, ioflags 0x%x, locked is %s.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)start_count, ioflags,
			locked ? "true" : "false");
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also.
	 */
	if (!locked)
		lck_rw_lock_shared(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		if (!locked)
			lck_rw_unlock_shared(&ni->lock);
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		return ENOENT;
	}
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
			err = ntfs_vnop_read_compressed(ni, uio, size, ioflags);
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
		err = cluster_read_ext(vn, uio, size, ioflags, callback, NULL);
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
					ioflags, err);
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
	err = uiomove((caddr_t)(kaddr + ofs), count, uio);
	ntfs_page_unmap(ni, upl, pl, FALSE);
	if (!err)
		ntfs_debug("Done (resident, not cached, returned 0x%llx "
				"bytes).", (unsigned long long)size -
				uio_resid(uio));
	else
		ntfs_error(ni->vol->mp, "uiomove() failed (error %d).", err);
err:
	/*
	 * Update the last_access_time (atime) if something was read and this
	 * is the base ntfs inode or it is a named stream (this is what HFS+
	 * does, too).
	 *
	 * Skip the update if atime updates are disabled via the noatime mount
	 * option or the volume is read only or this is a symbolic link.
	 *
	 * Also, skip the core system files except for the root directory.
	 */
	if (uio_resid(uio) < start_count && !NVolReadOnly(ni->vol) &&
			!(vfs_flags(ni->vol->mp) & MNT_NOATIME) &&
			!S_ISLNK(base_ni->mode) &&
			(ni == base_ni || ni->type == AT_DATA)) {
		BOOL need_update_time;

		need_update_time = TRUE;
		if (ni->vol->major_ver > 1) {
			if (base_ni->mft_no <= FILE_Extend &&
					base_ni != ni->vol->root_ni)
				need_update_time = FALSE;
		} else {
			if (base_ni->mft_no <= FILE_UpCase &&
					base_ni != ni->vol->root_ni)
				need_update_time = FALSE;
		}
		if (need_update_time) {
			base_ni->last_access_time = ntfs_utc_current_time();
			NInoSetDirtyTimes(base_ni);
		}
	}
	if (!locked)
		lck_rw_unlock_shared(&ni->lock);
	return err;
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
 * ioflags are currently defined in OS X kernel (a lot of them are not
 * applicable to VNOP_READ() however):
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
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
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
	return (int)ntfs_read(ni, a->a_uio, a->a_ioflag, FALSE);
}

// TODO: Rename to ntfs_inode_write and move to ntfs_inode.[hc]?
/**
 * ntfs_write - write a number of bytes from a memory buffer into a file
 * @ni:			ntfs inode to write to
 * @uio:		source containing the data to write
 * @ioflags:		flags further describing the write request
 * @write_locked:	if true the ntfs inode lock is already taken for writing
 *
 * Write uio_resid(@uio) bytes from the source buffer specified by @uio to the
 * ntfs inode @ni, starting at byte offset uio_offset(@uio) into the inode.
 *
 * The flags in @ioflags further describe the write request.  The following
 * ioflags are currently defined in OS X kernel (not all of them are applicable
 * to VNOP_WRITE() however):
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
 * For compressed and encrypted attributes we abort for now as we do not
 * support them yet.
 *
 * For non-resident attributes we use cluster_write_ext() which deals with
 * normal attributes.
 *
 * Return 0 on success and errno on error.
 *
 * Note it is up to the caller to verify that writing to the inode @ni makes
 * sense.  We cannot do the verification inside ntfs_write() as it is called
 * from various VNOPs which all have different requirements.  For example
 * VNOP_SYMLINK(), i.e. ntfs_vnop_symlink(), needs to write to S_ISLNK() inodes
 * whilst VNOP_WRITE(), i.e. ntfs_vnop_write(), needs to not allow S_ISLNK()
 * but needs to allow S_IFREG() instead but only if it is not a system file.
 */
static errno_t ntfs_write(ntfs_inode *ni, uio_t uio, int ioflags,
		BOOL write_locked)
{
	s64 old_size, size, end, nr_truncated;
	user_ssize_t old_count, count;
	off_t old_ofs, ofs;
	vnode_t vn = ni->vn;
	ntfs_inode *base_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	int cnt;
	errno_t err;
	BOOL was_locked, need_uptodate;

	/* Do not allow writing if mounted read-only. */
	if (NVolReadOnly(ni->vol))
		return EROFS;
	nr_truncated = 0;
	ofs = old_ofs = uio_offset(uio);
	count = old_count = uio_resid(uio);
	ntfs_debug("Entering for file inode 0x%llx, offset 0x%llx, count "
			"0x%llx, ioflags 0x%x, write_locked is %s.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)count, ioflags,
			write_locked ? "true" : "false");
	/* If nothing to do return success. */
	if (!count)
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
	/* TODO: We do not support writing to compressed files. */
	if (NInoCompressed(ni)) {
		ntfs_error(ni->vol->mp, "Writing to compressed files is not "
				"implemented yet.  Sorry.");
		return ENOTSUP;
	}
#if 1	// TODO: Remove this when sparse support is done...
	if (NInoSparse(ni))
		return ENOTSUP;
#endif
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/* The first byte after the write. */
	end = ofs + count;
	/*
	 * If we are going to extend the initialized size take the inode lock
	 * for writing and take it for reading otherwise.
	 *
	 * Appending will always cause the initialized size to be extended thus
	 * always take the lock for writing.
	 *
	 * Writing into holes requires us to take the lock for writing thus if
	 * this is a sparse file take the lock for writing just in case.
	 */
	was_locked = write_locked;
	if (ioflags & IO_APPEND) {
		if (!was_locked) {
			lck_rw_lock_exclusive(&ni->lock);
			write_locked = TRUE;
		}
		/*
		 * Do not allow messing with the inode once it has been
		 * deleted.
		 */
		if (NInoDeleted(ni)) {
			if (!was_locked)
				lck_rw_unlock_exclusive(&ni->lock);
			/* Remove the inode from the name cache. */
			cache_purge(ni->vn);
			return ENOENT;
		}
		lck_spin_lock(&ni->size_lock);
		ofs = ni->data_size;
		lck_spin_unlock(&ni->size_lock);
		uio_setoffset(uio, ofs);
		ntfs_debug("Write to mft_no 0x%llx, IO_APPEND flag is set, "
				"setting uio_offset() to file size 0x%llx.",
				(unsigned long long)ni->mft_no,
				(unsigned long long)ofs);
		/* Update the first byte after the write with the new offset. */
		end = ofs + count;
	} else {
		if (!was_locked) {
			if (NInoSparse(ni)) {
				lck_rw_lock_exclusive(&ni->lock);
				write_locked = TRUE;
			} else {
				lck_rw_lock_shared(&ni->lock);
				write_locked = FALSE;
			}
		}
recheck_deleted:
		/*
		 * Do not allow messing with the inode once it has been
		 * deleted.
		 */
		if (NInoDeleted(ni)) {
			if (!was_locked) {
				if (write_locked)
					lck_rw_unlock_exclusive(&ni->lock);
				else
					lck_rw_unlock_shared(&ni->lock);
			}
			/* Remove the inode from the name cache. */
			cache_purge(ni->vn);
			return ENOENT;
		}
		lck_spin_lock(&ni->size_lock);
		size = ni->initialized_size;
		lck_spin_unlock(&ni->size_lock);
		if (!write_locked && end > size) {
			/* If we fail to convert the lock, take it. */
			if (!lck_rw_lock_shared_to_exclusive(&ni->lock))
				lck_rw_lock_exclusive(&ni->lock);
			write_locked = TRUE;
			goto recheck_deleted;
		}
		ntfs_debug("Mft_no 0x%llx, inode lock taken for %s.",
				(unsigned long long)ni->mft_no,
				write_locked ? "writing" : "reading");
	}
	/*
	 * We do not want any form of zero filling to happen at the starting
	 * offset of the write as we sort this out ourselves.
	 *
	 * Further, we never want to zero fill at the end of the write as this
	 * is pointless.  We automatically get zero filling at the end of the
	 * page when a page is read in and when the initialized size is
	 * extended.
	 */
	ioflags &= ~(IO_HEADZEROFILL | IO_TAILZEROFILL);
	/*
	 * We do not want to zero any valid/dirty pages as they could already
	 * have new data written via mmap() for example and we do not want to
	 * lose that.
	 */
	ioflags |= IO_NOZEROVALID | IO_NOZERODIRTY;
	lck_spin_lock(&ni->size_lock);
	old_size = ni->data_size;
	size = ni->allocated_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If this is a sparse attribute and the write overlaps the existing
	 * allocated size we need to fill any holes overlapping the write.  We
	 * can skip resident attributes as they cannot have sparse regions.
	 *
	 * As allocated size goes in units of clusters we need to round down
	 * the start offset to the nearest cluster boundary and we need to
	 * round up the end offset to the next cluster boundary.
	 */
	if (NInoSparse(ni) && NInoNonResident(ni) &&
			(ofs & ~ni->vol->cluster_size_mask) < size) {
		s64 aligned_end, new_end;

		if (!write_locked)
			panic("%s(): !write_locked\n", __FUNCTION__);
		aligned_end = (end + ni->vol->cluster_size_mask) &
				~ni->vol->cluster_size_mask;
		/*
		 * Only need to instantiate holes up to the allocated size
		 * itself.  Everything else is an extension and will be dealt
		 * with by ntfs_attr_extend_allocation() below.
		 */
		if (aligned_end > size)
			aligned_end = size;
		err = ntfs_attr_instantiate_holes(ni,
				ofs & ~ni->vol->cluster_size_mask, aligned_end,
				&new_end, ioflags & IO_UNIT);
		if (err) {
			ntfs_error(ni->vol->mp, "Cannot perform write to "
					"mft_no 0x%llx because instantiation "
					"of sparse regions failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			uio_setoffset(uio, old_ofs);
			uio_setresid(uio, old_count);
			if (!was_locked)
				lck_rw_unlock_exclusive(&ni->lock);
			return err;
		}
		/* If the instantiation was partial, truncate the write. */
		if (new_end < aligned_end) {
			s64 new_count;

			if (ioflags & IO_UNIT)
				panic("%s(): new_end < aligned_end && "
						"ioflags & IO_UNIT\n",
						__FUNCTION__);
			ntfs_debug("Truncating write to mft_no 0x%llx because "
					"instantiation of sparse regions was "
					"only partially completed.",
					(unsigned long long)ni->mft_no);
			if (new_end > end)
				panic("%s(): new_end > end\n", __FUNCTION__);
			end = new_end;
			new_count = new_end - ofs;
			if (new_count >= count)
				panic("%s(): new_count >= count\n",
						__FUNCTION__);
			nr_truncated += count - new_count;
			count = new_count;
			uio_setresid(uio, new_count);
		}
	}
	/*
	 * If the write goes beyond the allocated size, extend the allocation
	 * to cover the whole of the write, rounded up to the nearest cluster.
	 */
	if (end > size) {
		if (!write_locked)
			panic("%s(): !write_locked\n", __FUNCTION__);
		/* Extend the allocation without changing the data size. */
		err = ntfs_attr_extend_allocation(ni, end, -1, ofs, NULL,
				&size, ioflags & IO_UNIT);
		if (!err) {
			if (ofs >= size)
				panic("%s(): ofs >= size\n", __FUNCTION__);
			/* If the extension was partial truncate the write. */
			if (end > size) {
				s64 new_count;

				if (ioflags & IO_UNIT)
					panic("%s(): end > size && "
							"ioflags & IO_UNIT\n",
							__FUNCTION__);
				ntfs_debug("Truncating write to mft_no 0x%llx "
						"because the allocation was "
						"only partially extended.",
						(unsigned long long)ni->mft_no);
				end = size;
				new_count = size - ofs;
				if (new_count >= count)
					panic("%s(): new_count >= count\n",
							__FUNCTION__);
				nr_truncated += count - new_count;
				count = new_count;
				uio_setresid(uio, new_count);
			}
		} else /* if (err) */ {
			lck_spin_lock(&ni->size_lock);
			size = ni->allocated_size;
			lck_spin_unlock(&ni->size_lock);
			/* Perform a partial write if possible or fail. */
			if (ofs < size && !(ioflags & IO_UNIT)) {
				s64 new_count;
				
				ntfs_debug("Truncating write to mft_no 0x%llx "
						"because extending the "
						"allocation failed (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				end = size;
				new_count = size - ofs;
				if (new_count >= count)
					panic("%s(): new_count >= count\n",
							__FUNCTION__);
				nr_truncated += count - new_count;
				count = new_count;
				uio_setresid(uio, new_count);
			} else {
				ntfs_error(ni->vol->mp, "Cannot perform write "
						"to mft_no 0x%llx because "
						"extending the allocation "
						"failed (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				goto abort;
			}
		}
	}
	/*
	 * If the write starts beyond the initialized size, extend it up to the
	 * beginning of the write and initialize all non-sparse space between
	 * the old initialized size and the new one.  This automatically also
	 * increments the data size as well as the ubc size to keep it above or
	 * equal to the initialized size.
	 */
	lck_spin_lock(&ni->size_lock);
	size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	if (ofs > size) {
		if (!write_locked)
			panic("%s(): !write_locked 2\n", __FUNCTION__);
		err = ntfs_attr_extend_initialized(ni, ofs);
		if (err) {
			ntfs_error(ni->vol->mp, "Cannot perform write to "
					"mft_no 0x%llx because extending the "
					"initialized size failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			goto abort;
		}
		size = ofs;
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
			/*
			 * TODO: At present we should never get here for
			 * compressed files as this case is aborted at the
			 * start of the function.
			 */
			panic("%s(): NInoCompressed(ni) && !NInoRaw(ni)\n",
					__FUNCTION__);
		}
		callback = NULL;
		if (NInoEncrypted(ni)) {
			callback = ntfs_cluster_iodone;
			/*
			 * TODO: At present we should never get here for
			 * encrypted files as this case is aborted at the start
			 * of the function.
			 */
			panic("%s(): NInoEncrypted(ni)\n", __FUNCTION__);
		}
		/* Determine the new file size. */
		size = ubc_getsize(vn);
		if (end > size)
			size = end;
		/*
		 * Note the first size is the original file size and the second
		 * file size is the new file size when the write is complete.
		 */
		err = cluster_write_ext(vn, uio, ubc_getsize(vn), size, 0, 0,
				ioflags, callback, NULL);
		if (err) {
			/*
			 * There was an error.  We do not know where.  Ensure
			 * everything is set up as if the write never happened.
			 */
			ntfs_error(ni->vol->mp, "Failed (cluster_write_ext(), "
					"error %d).", err);
			goto abort;
		}
		goto done;
	}
	/*
	 * The attribute is resident thus we have to deal with it by ourselves.
	 * First of all, try to copy the data to the vm page cache.  This will
	 * work on the second and all later writes so this is the hot path.  If
	 * the attribute has not been accessed at all before or its cached
	 * pages were dropped due to vm pressure this will fail to copy any
	 * data due to the lack of a valid page and we will drop into the slow
	 * path.
	 */
	if (ofs > PAGE_SIZE)
		panic("%s(): ofs > PAGE_SIZE\n", __FUNCTION__);
	cnt = (int)count;
	if (count > PAGE_SIZE - ofs) {
		cnt = PAGE_SIZE - ofs;
		ntfs_warning(ni->vol->mp, "Unexpected count (0x%llx) > "
				"PAGE_SIZE - ofs (0x%x), overriding it to "
				"PAGE_SIZE - ofs.", (unsigned long long)count,
				cnt);
	}
	/*
	 * Note we pass mark_dirty = 1 (the last parameter) which means the
	 * pages that are written to will be marked dirty.
	 */
	err = cluster_copy_ubc_data(vn, uio, &cnt, 1);
	if (err) {
		/*
		 * The copying (uiomove()) failed with an error.  Ensure
		 * everything is set up as if the write never happened.
		 */
		ntfs_error(ni->vol->mp, "cluster_copy_ubc_data() failed "
				"(error %d).", err);
		goto abort;
	}
	/*
	 * @cnt is now set to the number of bytes remaining to be transferred.
	 * If it is zero, it means we are done.
	 */
	if (!cnt)
		goto done;
	/*
	 * We failed to transfer everything.  That really means we failed to
	 * transfer anything at all as we are guaranteed that a resident
	 * attribute is smaller than a page thus either the page is there and
	 * valid and we transfer everything or it is not and we transfer
	 * nothing.
	 */
	if (cnt != count) {
		ntfs_warning(ni->vol->mp, "Unexpected partial transfer to "
				"cached page (count 0x%llx, cnt 0x%x).",
				(unsigned long long)count, cnt);
		/* Ensure everything is as it was before. */
		uio_setoffset(uio, old_ofs);
		uio_setresid(uio, old_count - nr_truncated);
	}
	/*
	 * The page is not in cache or is not valid.  We need to bring it into
	 * cache and make it valid so we can then copy the data in.  The
	 * easiest way to do this is to just map the page which will take care
	 * of everything for us.  We can then uiomove() straight into the page
	 * from the @uio and then mark the page dirty and unmap it again.
	 *
	 * As an optimization, if the write covers the whole existing attribute
	 * we grab the page without bringing it uptodate if it is not valid
	 * already thus saving a pagein from disk.
	 */
	need_uptodate = (ofs || end < size);
	err = ntfs_page_map_ext(ni, 0, &upl, &pl, &kaddr, need_uptodate, TRUE);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map page (error %d).", err);
		goto abort;
	}
	err = uiomove((caddr_t)(kaddr + ofs), cnt, uio);
	if (err) {
		/*
		 * If we just caused the page to exist and did not bring it
		 * up-to-date or caching is disabled on the vnode or for this
		 * i/o, dump the page.  Otherwise release it back to the VM.
		 */
		if (upl_valid_page(pl, 0) || (need_uptodate &&
				!vnode_isnocache(vn) &&
				!(ioflags & IO_NOCACHE)))
			ntfs_page_unmap(ni, upl, pl, FALSE);
		else
			ntfs_page_dump(ni, upl, pl);
		/*
		 * The copying (uiomove()) failed with an error.  Ensure
		 * everything is set up as if the write never happened.
		 */
		ntfs_error(ni->vol->mp, "uiomove() failed (error %d).", err);
		goto abort;
	}
	/*
	 * If the page is not uptodate and we did not bring it up-to-date when
	 * mapping it, zero the remainder of the page now thus bringing it
	 * up-to-date.
	 */
	if (!need_uptodate && !upl_valid_page(pl, 0)) {
		const off_t cur_ofs = uio_offset(uio);
		if (cur_ofs > PAGE_SIZE)
			panic("%s(): cur_ofs > PAGE_SIZE\n", __FUNCTION__);
		bzero(kaddr + cur_ofs, PAGE_SIZE - cur_ofs);
	}
	/*
	 * Unmap the page marking it dirty.
	 *
	 * Note we leave the page cached even if no caching is requested for
	 * simplicity.  That way we do not need to touch the mft record at all
	 * and can instead rely on the next sync to propagate the dirty data
	 * from the page into the mft record and then to disk.  In the sync i/o
	 * case we will call ntfs_inode_sync() at the end of this function.
	 */
	ntfs_page_unmap(ni, upl, pl, TRUE);
done:
	/*
	 * If the write went past the end of the initialized size update it
	 * both in the ntfs inode and in the base attribute record.
	 *
	 * Also update the data size and the ubc size if the write went past
	 * the end of the data size.  Note this is automatically done by
	 * ntfs_attr_set_initialized_size() so we do not need to do it here.
	 */
	size = uio_offset(uio);
	lck_spin_lock(&ni->size_lock);
	if (size > ni->initialized_size) {
		lck_spin_unlock(&ni->size_lock);
		if (!write_locked)
			panic("%s(): !write_locked 3\n", __FUNCTION__);
		err = ntfs_attr_set_initialized_size(ni, size);
		if (err) {
			ntfs_error(ni->vol->mp, "Failed to update the "
					"initialized size of mft_no 0x%llx "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
			/*
			 * If the write was meant to be atomic, the write
			 * started beyond the end of the initialized size, or
			 * nothing was written ensure everything is set up as
			 * if the write never happened.
			 */
			lck_spin_lock(&ni->size_lock);
			size = ni->initialized_size;
			lck_spin_unlock(&ni->size_lock);
			if (ioflags & IO_UNIT || old_ofs >= size ||
					uio_resid(uio) >= old_count)
				goto abort;
			/*
			 * Something was written before the initialized size
			 * thus turn the error into a partial, successful write
			 * up to the initialized size.
			 */
			uio_setoffset(uio, size);
			uio_setresid(uio, size - old_ofs);
			err = 0;
		}
	} else
		lck_spin_unlock(&ni->size_lock);
	// TODO: If we wrote anything at all we have to clear the S_ISUID and
	// S_ISGID bits in the file mode as a precaution against tampering
	// (see xnu/bsd/hfs/hfs_readwrite.c::hfs_vnop_write()).
	/*
	 * Update the last_data_change_time (mtime) and last_mft_change_time
	 * (ctime) on the base ntfs inode @base_ni unless this is an attribute
	 * inode update in which case only update the ctime as named stream/
	 * extended attribute semantics expect on OS X.
	 */
	base_ni->last_mft_change_time = ntfs_utc_current_time();
	if (ni == base_ni)
		base_ni->last_data_change_time = base_ni->last_mft_change_time;
	NInoSetDirtyTimes(base_ni);
	/*
	 * If this is not a directory or it is an encrypted directory, set the
	 * needs archiving bit except for the core system files.
	 */
	if (!S_ISDIR(base_ni->mode) || NInoEncrypted(base_ni)) {
		BOOL need_set_archive_bit = TRUE;
		if (ni->vol->major_ver >= 2) {
			if (ni->mft_no <= FILE_Extend)
				need_set_archive_bit = FALSE;
		} else {
			if (ni->mft_no <= FILE_UpCase)
				need_set_archive_bit = FALSE;
		}
		if (need_set_archive_bit) {
			base_ni->file_attributes |= FILE_ATTR_ARCHIVE;
			NInoSetDirtyFileAttributes(base_ni);
		}
	}
	/*
	 * If we truncated the write add back the number of truncated bytes to
	 * the number of bytes remaining.
	 */
	if (nr_truncated > 0) {
		if (ioflags & IO_UNIT)
			panic("%s(): ioflags & IO_UNIT\n", __FUNCTION__);
		uio_setresid(uio, uio_resid(uio) + nr_truncated);
	}
	/*
	 * If the write was partial we need to trim off any extra allocated
	 * space by truncating the attribute to its old size.  We can only have
	 * extended the allocation if we hold the inode lock for writing so do
	 * not bother going through this code if we only hold the lock for
	 * reading.
	 *
	 * There is one exception and that is that if the write was meant to be
	 * atomic a partial write is not acceptable thus we need to abort the
	 * write completely in this case.
	 */
	size = uio_resid(uio);
	if (write_locked && size > nr_truncated) {
		s64 truncate_size;
		errno_t err2;
		int rflags;

		/*
		 * If the write was meant to be atomic or nothing was written
		 * reset everything as if the write never happened thus
		 * releasing any extra space we may have allocated.
		 */
		if (ioflags & IO_UNIT || size >= old_count) {
			if (size > old_count)
				panic("%s(): size > old_count\n", __FUNCTION__);
abort:
			uio_setoffset(uio, old_ofs);
			uio_setresid(uio, old_count);
			if (!write_locked) {
				if (!err)
					panic("%s(): !err\n", __FUNCTION__);
				goto skip_truncate;
			}
			truncate_size = old_size;
		} else /* if (uio_resid(uio) < old_count) */ {
			/*
			 * At least something was written.  Truncate the
			 * attribute to the successfully written size thus
			 * releasing any extra space we allocated but ensure we
			 * do not truncate to less than the old size.
			 */
			truncate_size = uio_offset(uio);
			if (truncate_size < old_size)
				truncate_size = old_size;
		}
		/*
		 * Truncate the attribute to @truncate_size.
		 *
		 * The truncate must be complete or no need to bother at all so
		 * set the IO_UNIT flag.  Also remove unwanted flags.
		 */
		rflags = (ioflags | IO_UNIT) & ~(IO_APPEND | IO_SYNC |
				IO_NOZEROFILL);
		err2 = ntfs_attr_resize(ni, truncate_size, rflags, NULL);
		if (err2) {
			BOOL is_dirty;

			/*
			 * If no other error has occured failing the truncate
			 * will at worst mean that we have too much allocated
			 * space which is not a disaster so carry on in this
			 * case.
			 *
			 * If another error has occured any of a number of
			 * things can now be wrong and in particular if the
			 * data size is not equal to @truncate_size this is
			 * very bad news so mark the volume dirty and warn the
			 * user about it.
			 */
			is_dirty = (err);
			if (is_dirty) {
				lck_spin_lock(&ni->size_lock);
				if (truncate_size == ni->data_size)
					is_dirty = FALSE;
				lck_spin_unlock(&ni->size_lock);
			}
			ntfs_error(ni->vol->mp, "Truncate failed (error %d).%s",
					err2, is_dirty ? "  Leaving "
					"inconsistent data on disk.  Unmount "
					"and run chkdsk." : "");
			if (is_dirty)
				NVolSetErrors(ni->vol);
		}
	}
skip_truncate:
	if (!was_locked) {
		if (!write_locked)
			lck_rw_unlock_shared(&ni->lock);
		else
			lck_rw_unlock_exclusive(&ni->lock);
		/*
		 * If the write was successful and synchronous i/o was
		 * requested, sync all changes to the backing store.  We
		 * dropped the inode lock already to be able to call
		 * ntfs_inode_sync() thus if it fails we cannot do anything
		 * about it so we just return the error even though the
		 * operation has otherwise been performed.
		 *
		 * Note we cannot do this if the inode was already locked or
		 * the call to ntfs_inode_sync() would cause a deadlock.
		 */
		if (!err && ioflags & IO_SYNC) {
			/* Mask out undersired @ioflags. */
			ioflags &= ~(IO_UNIT | IO_APPEND | IO_DEFWRITE);
			err = ntfs_inode_sync(ni, ioflags, FALSE);
		}
	}
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
 * ioflags are currently defined in OS X kernel (not all of them are applicable
 * to VNOP_WRITE() however):
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
 * For compressed and encrypted attributes we abort for now as we do not
 * support them yet.
 *
 * For non-resident attributes we use cluster_write_ext() which deals with
 * normal attributes.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_write(struct vnop_write_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
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
	return (int)ntfs_write(ni, a->a_uio, a->a_ioflag, FALSE);
}

/**
 * ntfs_vnop_ioctl -
 *
 */
static int ntfs_vnop_ioctl(struct vnop_ioctl_args *a __attribute__((unused)))
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
static int ntfs_vnop_select(struct vnop_select_args *a __attribute__((unused)))
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
static int ntfs_vnop_exchange(
		struct vnop_exchange_args *a __attribute__((unused)))
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
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

	if (ni)
		ntfs_debug("Mapping mft_no 0x%llx, type 0x%x, name_len 0x%x, "
				"mapping flags 0x%x.",
				(unsigned long long)ni->mft_no,
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

	if (ni)
		ntfs_debug("Unmapping mft_no 0x%llx, type 0x%x, name_len "
				"0x%x.", (unsigned long long)ni->mft_no,
				le32_to_cpu(ni->type), (unsigned)ni->name_len);
#endif
	/* Nothing to do. */
	return 0;
}

/**
 * ntfs_vnop_fsync - synchronize a vnode's in-core state with that on disk
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
 * Note: When called from reclaim, the vnode has a zero v_iocount and
 *	 v_usecount and vnode_isrecycled() is true.
 *
 * Return 0 on success and the error code on error.
 */
static int ntfs_vnop_fsync(struct vnop_fsync_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	int sync, err;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return 0;
	}
	/* If we are mounted read-only, we do not need to sync anything. */
	if (NVolReadOnly(ni->vol))
		return 0;
	sync = (a->a_waitfor == MNT_WAIT) ? IO_SYNC : 0;
	ntfs_debug("Entering for inode 0x%llx, waitfor 0x%x, %ssync i/o.",
			(unsigned long long)ni->mft_no, a->a_waitfor,
			(sync == IO_SYNC) ? "a" : "");
	/*
	 * We need to allow ENOENT errors since the unlink system call can call
	 * VNOP_FSYNC() during vclean().
	 */
	err = ntfs_inode_sync(ni, sync, FALSE);
	if (err == ENOENT)
		err = 0;
	ntfs_debug("Done (error %d).", err);
	return err;
}

/**
 * ntfs_unlink_internal - unlink and ntfs inode from its parent directory
 * @dir_ni:	directory ntfs inode from which to unlink the ntfs inode
 * @ni:		base ntfs inode to unlink
 * @name:	Unicode name of the inode to unlink
 * @name_len:	length of the name in Unicode characters
 * @name_type:	Namespace the name is in (i.e. FILENAME_{DOS,WIN32,POSIX,etc})
 * @is_rename:	if true ntfs_unlink_internal() is called for a rename operation
 *
 * Unlink an inode with the ntfs inode @ni and name @name with length @name_len
 * Unicode characters and of namespace @name_type from the directory with ntfs
 * inode @dir_ni.
 *
 * If @is_rename is true the caller was ntfs_vnop_rename() in which case the
 * link count of the inode to unlink @ni will be one higher than the link count
 * in the mft record.
 *
 * Return 0 on success and the error code on error.
 *
 * Note that if the name of the inode to be removed is in the WIN32 or DOS
 * namespaces, both the WIN32 and the corresponding DOS names are removed.
 *
 * Note that for a hard link this function simply removes the name and its
 * directory entry and decrements the hard link count whilst for the last name,
 * i.e. the last link to an inode, it only removes the directory entry, i.e. it
 * does not remove the name, however it does decrement the hard link count to
 * zero.  This is so that the inode can be undeleted and its original name
 * restored.  In any case, we do not actually delete the inode here as it may
 * still be open and UNIX semantics require an unlinked inode to be still
 * accessible through already opened file descriptors.  When the last file
 * descriptor is closed, we causes the inode to be deleted when the VFS
 * notifies us of the last close by calling VNOP_INACTIVE(), i.e.
 * ntfs_vnop_inactive().
 */
static errno_t ntfs_unlink_internal(ntfs_inode *dir_ni, ntfs_inode *ni,
		ntfschar *name, signed name_len, FILENAME_TYPE_FLAGS name_type,
		const BOOL is_rename)
{
	ntfs_volume *vol;
	ntfs_inode *objid_o_ni;
	ntfschar *ntfs_name;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *actx;
	ATTR_RECORD *a;
	ntfs_index_context *ictx;
	FILENAME_ATTR *fn, *tfn;
	signed ntfs_name_len;
	unsigned fn_count, tfn_alloc;
	errno_t err;
	BOOL seen_dos;
	FILENAME_TYPE_FLAGS seek_type, fn_type;

	vol = ni->vol;
	objid_o_ni = vol->objid_o_ni;
	ntfs_debug("Unlinking mft_no 0x%llx from directory mft_no 0x%llx, "
			"name type 0x%x.", (unsigned long long)ni->mft_no,
			(unsigned long long)dir_ni->mft_no,
			(unsigned)name_type);
	if (NInoAttr(ni))
		panic("%s(): Target inode is an attribute inode.\n",
				__FUNCTION__);
	/* Start the unlink by evicting the target from the name cache. */
	cache_purge(ni->vn);
	/*
	 * We now need to look up the target name in the target mft record.
	 *
	 * If @name_type is FILENAME_POSIX then @name and @name_len contain the
	 * correctly cased name and length in Unicode characters, respectively
	 * so we simply set @ntfs_name and @ntfs_name_len to @name and
	 * @name_len, respectively.
	 *
	 * If @name_type is anything else, i.e. FILENAME_WIN32, FILENAME_DOS,
	 * or FILENAME_WIN32_AND_DOS we simply need to look for that type of
	 * name in the target mft record as there can only be one filename
	 * attribute of this type thus the name is uniquely identified by type
	 * so the lookup can be optimized that way.
	 */
	seek_type = 0;
	if (name_type == FILENAME_POSIX) {
		ntfs_name = name;
		ntfs_name_len = name_len;
	} else {
		/*
		 * Set @ntfs_name to NULL so we know to do the look up based on
		 * the filename namespace @seek_type instead.
		 */
		ntfs_name = NULL;
		ntfs_name_len = 0;
		seek_type = name_type;
		/*
		 * If the target name is the WIN32 name we first need to delete
		 * the DOS name thus re-set @seek_type accordingly (see below
		 * for details).
		 */
		if (seek_type == FILENAME_WIN32)
			seek_type = FILENAME_DOS;
	}
	/*
	 * We know this is the base inode since we bailed out for attribute
	 * inodes above.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record 0x%llx (error "
				"%d).", (unsigned long long)ni->mft_no, err);
		goto err;
	}
	/*
	 * Sanity check that the inode link count is in step with the mft
	 * record link count.
	 */
	if ((!is_rename && ni->link_count != le16_to_cpu(m->link_count)) ||
			(is_rename && ni->link_count !=
			(unsigned)le16_to_cpu(m->link_count) + 1))
		panic("%s(): ni->link_count != le16_to_cpu(m->link_count)\n",
				__FUNCTION__);
	actx = ntfs_attr_search_ctx_get(ni, m);
	if (!actx) {
		err = ENOMEM;
		goto unm_err;
	}
	/*
	 * Find the name in the target mft record.
	 *
	 * If it is a name in the WIN32 or DOS namespace (but not both), we
	 * remove the DOS name from both the directory index it is in and from
	 * the mft record and we decrement the link count both in the base mft
	 * record and in the ntfs inode.  In the case of a WIN32 name, we find
	 * the corresponding DOS name first and proceed as described.
	 *
	 * If the removal of the DOS name from the directory index is
	 * successful, we change the namespace of the remaining WIN32 name to
	 * the POSIX namespace, thus if we fail to remove the remaining name
	 * after successfully removing the DOS name, we still have a consistent
	 * file system.  This also has the side effect of allowing undelete to
	 * work properly as otherwise the undelete would restore a WIN32 name
	 * without a corresponding DOS name which would result in an illegal
	 * inode.
	 *
	 * We thus reduce the problem to a normal single name unlink and we can
	 * now determine whether this unlink is just a hard link removal or the
	 * final name removal, i.e. the inode is being deleted.
	 */
	seen_dos = FALSE;
restart_name:
	/*
	 * Before looking for the last name and removing it from its directory
	 * index entry, i.e. before unlinking the inode and targeting it for
	 * deletion, we need to check if the inode has an object id and if so
	 * we need to remove it from the object id index on the volume (present
	 * in $O index of $Extend/$ObjId system file), so that the inode cannot
	 * be found via its object id any more either.  Also, when the deleted
	 * inode gets reused for different purposes, we do not want the old
	 * object id to still point at it.
	 *
	 * If the volume is pre-NTFS 3.0, i.e. it does not support object ids,
	 * @vol->objid_o_ni will be NULL.  It will also be NULL if the volume
	 * is NTFS 3.0+ but no object ids are present on the volume, thus we
	 * can make the check conditional on @objid_o_ni not being NULL.
	 *
	 * We do this before deleting the last directory entry so that we can
	 * abort the unlink if we fail to remove the object id from the index
	 * to ensure the volume does not become inconsistent.
	 */
	if (objid_o_ni && ni->link_count <= 1) {
		err = ntfs_attr_lookup(AT_OBJECT_ID, AT_UNNAMED, 0, 0, NULL, 0,
				actx);
		if (err) {
			if (err != ENOENT) {
				ntfs_error(vol->mp, "Failed to look up object "
						"id in mft_no 0x%llx (error "
						"%d).",
						(unsigned long long)ni->mft_no,
						err);
				goto put_err;
			}
			/*
			 * The object id was not found which is fine.  The
			 * inode simply does not have an object id assigned to
			 * it so there is nothing for us to do.
			 */
			ntfs_debug("Target mft_no 0x%llx does not have an "
					"object id assigned to it.",
					(unsigned long long)ni->mft_no);
		} else /* if (!err) */ {
			INDEX_ENTRY *ie;
			GUID object_id;

			/* The inode has an object id assigned to it. */
			ntfs_debug("Deleting object id from target mft_no "
					"0x%llx.",
					(unsigned long long)ni->mft_no);
			a = actx->a;
			/*
			 * We need to make a copy of the object id and release
			 * the mft record before looking up the object id in
			 * the $ObjID/$O index otherwise we could deadlock if
			 * the currently mapped mft record is in the same page
			 * as one of the mft records of $ObjId.
			 */
			memcpy(&object_id, &((OBJECT_ID_ATTR*)((u8*)a +
					le16_to_cpu(a->value_offset)))->
					object_id, sizeof(object_id));
			ntfs_attr_search_ctx_put(actx);
			ntfs_mft_record_unmap(ni);
			err = vnode_get(objid_o_ni->vn);
			if (err) {
				ntfs_error(vol->mp, "Failed to get index "
						"vnode for $ObjId/$O.");
				goto err;
			}
			lck_rw_lock_exclusive(&objid_o_ni->lock);
			ictx = ntfs_index_ctx_get(objid_o_ni);
			if (!ictx) {
				ntfs_error(vol->mp, "Failed to get index "
						"context.");
				err = ENOMEM;
				goto iput_err;
			}
restart_ictx:
			/* Get the index entry matching the object id. */
			err = ntfs_index_lookup(&object_id, sizeof(object_id),
					&ictx);
			if (err) {
				if (err == ENOENT) {
					ntfs_error(vol->mp, "Failed to delete "
							"object id of target "
							"inode 0x%llx from "
							"object id index "
							"because the object "
							"id was not found in "
							"the object id "
							"index.  Volume is "
							"corrupt.  Run "
							"chkdsk.",
							(unsigned long long)
							ni->mft_no);
					NVolSetErrors(vol);
					err = EIO;
				} else
					ntfs_error(vol->mp, "Failed to delete "
							"object id of target "
							"inode 0x%llx from "
							"object id index "
							"because looking up "
							"the object id in the "
							"object id index "
							"failed (error %d)." ,
							(unsigned long long)
							ni->mft_no, err);
				goto iput_err;
			}
			ie = ictx->entry;
			/* We now have the index entry, delete it. */
			err = ntfs_index_entry_delete(ictx);
			if (err) {
				if (err == -EAGAIN) {
					ntfs_debug("Restarting object id "
							"delete as tree was "
							"rearranged.");
					ntfs_index_ctx_reinit(ictx, objid_o_ni);
					goto restart_ictx;
				}
				ntfs_error(vol->mp, "Failed to delete object "
						"id of target inode 0x%llx "
						"from object id index (error "
						"%d).",
						(unsigned long long)ni->mft_no,
						err);
				goto iput_err;
			}
			ntfs_index_ctx_put(ictx);
			lck_rw_unlock_exclusive(&objid_o_ni->lock);
			(void)vnode_put(objid_o_ni->vn);
			/*
			 * Now get back the mft record so we can re-look up the
			 * object id attribute so we can delete it.
			 *
			 * This means we do not need to worry about
			 * inconsistencies to do with the object id in our
			 * error handling code paths later on.
			 */
			err = ntfs_mft_record_map(ni, &m);
			if (err) {
				ntfs_error(vol->mp, "Failed to re-map mft "
						"record 0x%llx (error %d).  "
						"Leaving inconstent "
						"metadata.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
				goto err;
			}
			actx = ntfs_attr_search_ctx_get(ni, m);
			if (!actx) {
				ntfs_error(vol->mp, "Failed to re-get "
						"attribute search context for "
						"mft record 0x%llx (error "
						"%d).  Leaving inconstent "
						"metadata.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
				err = ENOMEM;
				goto unm_err;
			}
			err = ntfs_attr_lookup(AT_OBJECT_ID, AT_UNNAMED, 0, 0,
					NULL, 0, actx);
			if (err) {
				ntfs_error(vol->mp, "Failed to re-look up "
						"object id in mft_no 0x%llx "
						"(error %d).  Leaving "
						"inconsistent metadata.  Run "
						"chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(ni->vol);
				err = EIO;
				goto put_err;
			}
			/*
			 * Remove the object id attribute from the mft record
			 * and mark the mft record dirty.
			 */
			err = ntfs_attr_record_delete(ni, actx);
			if (err) {
				ntfs_error(vol->mp, "Failed to delete object "
						"id in mft_no 0x%llx (error "
						"%d).  Leaving inconsistent "
						"metadata.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				goto put_err;
			}
		}
		/* Reinit the search context for the AT_FILENAME lookup. */
		ntfs_attr_search_ctx_reinit(actx);
	}
	/* Use label and goto instead of a loop to reduce indentation. */
	fn_count = 0;
next_name:
	/* Increment the filename attribute counter. */
	fn_count++;
	err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0, actx);
	if (err) {
		if (err == ENOENT) {
			/*
			 * If the name we are looking for is not found there is
			 * either some corruption or a bug given that a call to
			 * ntfs_lookup_inode_by_name() just found the name in
			 * the directory index.
			 */
			ntfs_error(vol->mp, "The target filename was not "
					"found in the mft record 0x%llx.  "
					"This is not possible.  This is "
					"either due to corruption or due to a "
					"driver bug.  Run chkdsk.",
					(unsigned long long)ni->mft_no);
			NVolSetErrors(vol);
			err = EIO;
		} else
			ntfs_error(vol->mp, "Failed to look up target "
					"filename in the mft record 0x%llx "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
		goto put_err;
	}
	a = actx->a;
	fn = (FILENAME_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
	fn_type = fn->filename_type;
	/*
	 * If this is a specific DOS or WIN32 or combined name lookup, no need
	 * to compare the actual name as there can only be one DOS and one
	 * WIN32 name or only one combined name in an inode.
	 */
	if (seek_type && seek_type != FILENAME_POSIX) {
		/*
		 * If this filename attribute does not match the target name
		 * try the next one.
		 */
		if (seek_type != fn_type)
			goto next_name;
		/* We found the filename attribute matching the target name. */
		if (fn_type == FILENAME_WIN32) {
			/*
			 * We were looking for the WIN32 name so we can remove
			 * it after having removed the DOS name.  We now found
			 * it, so switch it to the POSIX namespace as described
			 * above and then go ahead and delete it.
			 */
			ntfs_debug("Switching namespace of filename attribute "
					"from WIN32 to POSIX.");
			fn_type = fn->filename_type = FILENAME_POSIX;
			NInoSetMrecNeedsDirtying(actx->ni);
		}
		goto found_name;
	}
	/* If this is the DOS name, note that we have seen it. */
	if (fn_type == FILENAME_DOS)
		seen_dos = TRUE;
	/* If the names do not match, continue searching. */
	if (fn->filename_length != ntfs_name_len)
		goto next_name;
	if (MREF_LE(fn->parent_directory) != dir_ni->mft_no)
		goto next_name;
	if (bcmp(fn->filename, ntfs_name, ntfs_name_len * sizeof(ntfschar)))
		goto next_name;
	/* Found the matching name. */
	if (fn_type == FILENAME_WIN32) {
		/*
		 * Pure WIN32 name.  Repeat the lookup but for the DOS name
		 * this time so we can remove that first.
		 */
		seek_type = FILENAME_DOS;
		/*
		 * If @seen_dos is true, then restart the lookup from the
		 * beginning and if not then continue the lookup where we left
		 * off.
		 */
		if (seen_dos) {
			ntfs_attr_search_ctx_reinit(actx);
			fn_count = 0;
		}
		goto next_name;
	}
	if (fn_type == FILENAME_DOS) {
		/*
		 * This cannot happen as ntfs_lookup_inode_by_name() always
		 * returns @name for pure DOS names and hence we would have
		 * @seek_type == FILENAME_DOS and thus would have picked this
		 * filename attribute up above without ever doing a name based
		 * match.
		 */
		ntfs_error(vol->mp, "Filename is in DOS namespace.  This is "
				"not possible.  This is either due to "
				"corruption or due to a driver bug.  Run "
				"chkdsk.");
		NVolSetErrors(vol);
		err = EIO;
		goto put_err;
	}
found_name:
	/*
	 * We found the target filename attribute and can now remove it from
	 * the directory index.  But before we can do that we need to make a
	 * copy of the filename attribute value so we can release the mft
	 * record before we delete the directory index entry.  This is needed
	 * because when we hold the target mft record and we call
	 * ntfs_dir_entry_delete() this would cause the mft record for the
	 * directory to be mapped which could result in a deadlock in the event
	 * that both mft records are in the same page.
	 */
	tfn_alloc = le32_to_cpu(a->value_length);
	tfn = IOMallocData(tfn_alloc);
	if (!tfn) {
		/*
		 * TODO: If @seek_type == FILENAME_WIN32 &&
		 * @fn->filename_type == FILENAME_POSIX we need to update the
		 * directory entry filename_type to FILENAME_POSIX.  See below
		 * for how this is done for the error case in
		 * ntfs_dir_entry_delete().  Given a memory allocation just
		 * failed it is highly unlikely we would succeed in trying to
		 * look up the directory entry so that we could change the
		 * filename_type in it so at least for now just set the volume
		 * has errors flag instead.
		 */
		ntfs_error(vol->mp, "Failed to allocate memory for temporary "
				"filename attribute.  Leaving inconsistent "
				"metadata.  Run chkdsk.");
		NVolSetErrors(vol);
		err = EIO;
		goto put_err;
	}
	memcpy(tfn, fn, tfn_alloc);
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(ni);
	/*
	 * We copied the name and can now remove it from the directory index.
	 * If the name is in the POSIX namespace, we may have converted it from
	 * a pure WIN32 name after removing the corresponding DOS name, in
	 * which case we need to update the index entry to reflect the
	 * conversion should we fail to remove it from the directory index.
	 * ntfs_dir_entry_delete() takes care of this for us.
	 */
	err = ntfs_dir_entry_delete(dir_ni, ni, tfn, tfn_alloc);
	if (err) {
		ntfs_error(vol->mp, "Failed to delete directory index entry "
				"(error %d).", err);
		goto err;
	}
	/*
	 * Now get back the mft record.
	 *
	 * If getting back the mft record fails there is nothing we can do to
	 * recover and must bail out completely leaving inconsistent metadata.
	 *
	 * TODO: We could try to add the dir entry back again in an attempt to
	 * recover but as above we likely fail a memory allocation it is highly
	 * unlikely we would succeed in trying to do the lookup and addition of
	 * the directory entry.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to re-map mft record 0x%llx "
				"(error %d).  Leaving inconsistent metadata.  "
				"Run chkdsk.", (unsigned long long)ni->mft_no,
				err);
		NVolSetErrors(vol);
		goto err;
	}
	actx = ntfs_attr_search_ctx_get(ni, m);
	if (!actx) {
		ntfs_error(vol->mp, "Failed to re-get attribute search "
				"context for mft record 0x%llx (error %d).  "
				"Leaving inconsitent metadata.  Run chkdsk.",
				(unsigned long long)ni->mft_no, err);
		NVolSetErrors(vol);
		err = EIO;
		goto unm_err;
	}
	/*
	 * If the name is in the DOS namespace or this is not the last name we
	 * also need to remove the name from the mft record it is in and
	 * decrement the link count in the base mft record.
	 */
	if (fn_type == FILENAME_DOS || ni->link_count > 1) {
		/* Now need to re-lookup the target filename attribute. */
		while (fn_count > 0) {
			fn_count--;
			err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0,
					NULL, 0, actx);
			if (!err)
				continue;
			ntfs_error(vol->mp, "Failed to re-look up target "
					"filename in mft_no 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
			NVolSetErrors(vol);
			err = EIO;
			goto put_err;
		}
		a = actx->a;
		if (a->type != AT_FILENAME)
			panic("%s(): a->type (0x%x) != AT_FILENAME (0x30)\n",
					__FUNCTION__, le32_to_cpu(a->type));
		fn = (FILENAME_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
		if (fn_type != fn->filename_type)
			panic("%s(): fn_type != fn->filename_type\n",
					__FUNCTION__);
		/* Remove the filename from the mft record, too. */
		err = ntfs_attr_record_delete(ni, actx);
		if (err) {
			ntfs_error(vol->mp, "Failed to delete filename "
					"attribute from mft_no 0x%llx (error "
					"%d).", (unsigned long long)ni->mft_no,
					err);
			NVolSetErrors(vol);
			err = EIO;
			goto put_err;
		}
		/*
		 * Update the hard link count in the base mft record.  Note we
		 * subtract one from the inode link count if this is a rename
		 * as the link count has been elevated by one by the caller.
		 */
		m->link_count = cpu_to_le16(ni->link_count - 1 -
				(is_rename ? 1 : 0));
	} else /* if (fn_type != FILENAME_DOS && ni->link_count <= 1) */ {
		/*
		 * This is the last name, so we need to mark the mft record as
		 * unused in the mft record flags so no-one can open it by
		 * accident and so that, in case of a crash between now and the
		 * deletion of the inode, ntfsck will know that we meant to
		 * delete the inode rather than that we were in the process of
		 * allocating or renaming it so it will do the Right Thing(TM)
		 * and complete the deletion process.
		 */
		m->flags &= ~MFT_RECORD_IN_USE;
		/* Ensure the base mft record gets written out. */
		NInoSetMrecNeedsDirtying(ni);
	}
	/*
	 * We have either deleted the filename completely or we only removed
	 * the directory index entry if this is the last name.
	 * 
	 * In either case, we need to update the hard link count and the ctime
	 * in the ntfs inode (the ctime is the last_mft_change_time on NTFS).
	 */
	ni->link_count--;
	ni->last_mft_change_time = dir_ni->last_mft_change_time;
	NInoSetDirtyTimes(ni);
	/*
	 * If this is the DOS name, we now need to find the WIN32 name, so it
	 * can be deleted, too.  Otherwise we are done.
	 */
	if (fn_type == FILENAME_DOS) {
		seek_type = FILENAME_WIN32;
		/*
		 * We looked up the DOS name above thus we need to reinitialize
		 * the search context for the WIN32 name lookup.
		 */
		ntfs_attr_search_ctx_reinit(actx);
		fn_count = 0;
		goto restart_name;
	}
	/*
	 * If we removed a hard link but the inode is not deleted yet we need
	 * to remove the parent vnode from the vnode as this association may no
	 * longer exist.
	 *
	 * The same is true for the vnode name as we have just unlinked it.
	 *
	 * Note we skip this for the rename case because the subsequent call to
	 * ntfs_link_internal() is going to update the vnode identity with the
	 * new name and parent so no need to do wipe them here.
	 */
	if (ni->link_count > 0 && !is_rename)
		vnode_update_identity(ni->vn, NULL, NULL, 0, 0,
				VNODE_UPDATE_PARENT | VNODE_UPDATE_NAME);
	ntfs_debug("Done.");
put_err:
	ntfs_attr_search_ctx_put(actx);
unm_err:
	ntfs_mft_record_unmap(ni);
err:
	return err;
iput_err:
	if (ictx)
		ntfs_index_ctx_put(ictx);
	lck_rw_unlock_exclusive(&objid_o_ni->lock);
	(void)vnode_put(objid_o_ni->vn);
	return err;
}

/**
 * ntfs_unlink - unlink and ntfs inode from its parent directory
 * @dir_ni:	directory ntfs inode from which to unlink the ntfs inode
 * @ni:		base ntfs inode to unlink
 * @cn:		name of the inode to unlink
 * @flags:	flags describing the unlink request
 * @is_rmdir:	true if called from VNOP_RMDIR() and hence ntfs_vnop_rmdir()
 *
 * Unlink an inode with the ntfs inode @ni and name as specified in @cn from
 * the directory with ntfs inode @dir_ni.
 *
 * The flags in @flags further describe the unlink request.  The following
 * flags are currently defined in OS X kernel:
 *	VNODE_REMOVE_NODELETEBUSY	- Do not delete busy files, i.e. use
 *					  Carbon delete semantics).
 *
 * If @is_rmdir is true the caller is VNOP_RMDIR() and hence ntfs_vnop_rmdir()
 * and if @is_rmdir is false the caller is VNOP_REMOVE() and hence
 * ntfs_vnop_remove().  Note @flags is always zero if @is_rmdir is true.
 *
 * Return 0 on success and the error code on error.
 *
 * Note that if the name of the inode to be removed is in the WIN32 or DOS
 * namespaces, both the WIN32 and the corresponding DOS names are removed.
 *
 * Note that for a hard link this function simply removes the name and its
 * directory entry and decrements the hard link count whilst for the last name,
 * i.e. the last link to an inode, it only removes the directory entry, i.e. it
 * does not remove the name, however it does decrement the hard link count to
 * zero.  This is so that the inode can be undeleted and its original name
 * restored.  In any case, we do not actually delete the inode here as it may
 * still be open and UNIX semantics require an unlinked inode to be still
 * accessible through already opened file descriptors.  When the last file
 * descriptor is closed, we causes the inode to be deleted when the VFS
 * notifies us of the last close by calling VNOP_INACTIVE(), i.e.
 * ntfs_vnop_inactive().
 */
static errno_t ntfs_unlink(ntfs_inode *dir_ni, ntfs_inode *ni,
		struct componentname *cn, const int flags, const BOOL is_rmdir)
{
	MFT_REF mref;
	ntfs_volume *vol;
	ntfs_inode *objid_o_ni;
	ntfschar *ntfs_name;
	ntfs_dir_lookup_name *name = NULL;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	errno_t err;
	FILENAME_TYPE_FLAGS ntfs_name_type;
	ntfschar ntfs_name_buf[NTFS_MAX_NAME_LEN];

	vol = ni->vol;
	objid_o_ni = vol->objid_o_ni;
	ntfs_debug("Unlinking %s%.*s with mft_no 0x%llx from directory "
			"mft_no 0x%llx, flags 0x%x.",
			is_rmdir ? "directory " : "", (int)cn->cn_namelen,
			cn->cn_nameptr, (unsigned long long)ni->mft_no,
			(unsigned long long)dir_ni->mft_no, flags);
	/*
	 * Do not allow attribute inodes or raw inodes to be deleted.  Note
	 * raw inodes are always attribute inodes, too.
	 */
	if (NInoAttr(ni)) {
		ntfs_debug("Target %.*s, mft_no 0x%llx is a%s inode, "
				"returning EPERM.", (int)cn->cn_namelen,
				cn->cn_nameptr, (unsigned long long)ni->mft_no,
				NInoAttr(ni) ? "n attribute" : " raw");
		return EPERM;
	}
	/* The parent inode must be a directory. */
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_debug("Parent mft_no 0x%llx is not a directory, "
				"returning ENOTDIR.",
				(unsigned long long)dir_ni->mft_no);
		return ENOTDIR;
	}
	/* Check for "." removal. */
	if (ni == dir_ni) {
		ntfs_debug("Target %.*s, mft_no 0x%llx is the same as its "
				"parent directory, returning EINVAL.",
				(int)cn->cn_namelen, cn->cn_nameptr,
				(unsigned long long)ni->mft_no);
		return EINVAL;
	}
	/* Lock both the parent directory and the target inode for writing. */
	lck_rw_lock_exclusive(&dir_ni->lock);
	lck_rw_lock_exclusive(&ni->lock);
	/* Ensure the parent directory has not been deleted. */
	if (!dir_ni->link_count) {
		ntfs_debug("Parent directory mft_no 0x%llx has been deleted, "
				"returning ENOENT.",
				(unsigned long long)dir_ni->mft_no);
		/*
		 * If the directory is somehow still in the name cache remove
		 * it now.
		 */
		cache_purge(dir_ni->vn);
		err = ENOENT;
		goto err;
	}
	/* Ensure tha target has not been deleted by someone else already. */
	if (!ni->link_count) {
		ntfs_debug("Target %.*s, mft_no 0x%llx has been deleted, "
				"returning ENOENT.", (int)cn->cn_namelen,
				cn->cn_nameptr, (unsigned long long)ni->mft_no);
		/*
		 * If the target is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * If this is a directory removal, i.e. rmdir, need to check that the
	 * directory is empty.
	 *
	 * Note we already checked for "." removal and we do not need to check
	 * for ".." removal because that would fail the directory is empty
	 * check as the parent directory would at least have one entry and that
	 * is the current directory.
	 */
	if (is_rmdir) {
		err = ntfs_dir_is_empty(ni);
		if (err) {
			if (err == ENOTEMPTY)
				ntfs_debug("Target directory %.*s, mft_no "
						"0x%llx is not empty, "
						"returning ENOTEMPTY.",
						(int)cn->cn_namelen,
						cn->cn_nameptr,
						(unsigned long long)ni->mft_no);
			else
				ntfs_error(vol->mp, "Failed to determine if "
						"target directory %.*s, "
						"mft_no 0x%llx is empty "
						"(error %d).",
						(int)cn->cn_namelen,
						cn->cn_nameptr,
						(unsigned long long)ni->mft_no,
						err);
			goto err;
		}
	} else {
		/* Do not allow directories to be unlinked. */
		if (S_ISDIR(ni->mode)) {
			ntfs_debug("Target %.*s, mft_no 0x%llx is a "
					"directory, returning EPERM.",
					(int)cn->cn_namelen, cn->cn_nameptr,
					(unsigned long long)ni->mft_no);
			err = EPERM;
			goto err;
		}
	}
	/*
	 * Do not allow any of the system files to be deleted.
	 *
	 * For NTFS 3.0+ volumes do not allow any of the extended system files
	 * to be deleted, either.
	 *
	 * Note we specifically blacklist all system files that we make use of
	 * except for the transaction log $UsnJrnl as that is allowed to be
	 * deleted and its deletion means that transaction logging is disabled.
	 *
	 * Note that if the transaction log is present it will be held busy by
	 * the NTFS driver thus unlinking the $UsnJrnl will not actually delete
	 * it until the driver is unmounted.  FIXME: Should we leave it like
	 * this or should we detach the $UsnJrnl vnodes from the volume and
	 * release them so they can be deleted immediately?
	 *
	 * TODO: What about all the new metadata files introduced with Windows
	 * Vista?  We are currently ignoring them and allowing them to be
	 * deleted...
	 */
	if (ni->file_attributes & FILE_ATTR_SYSTEM) {
		BOOL is_system = FALSE;
		if (vol->major_ver <= 1) {
			if (ni->mft_no < FILE_Extend)
				is_system = TRUE;
		} else {
			if (ni->mft_no <= FILE_Extend)
				is_system = TRUE;
			if (dir_ni == vol->extend_ni) {
				if (ni == vol->objid_ni ||
						ni == vol->quota_ni)
					is_system = TRUE;
			}
		}
		if (is_system) {
			ntfs_debug("Target %.*s, mft_no 0x%llx is a%s system "
					"file, returning EPERM.",
					(int)cn->cn_namelen, cn->cn_nameptr,
					(unsigned long long)ni->mft_no,
					(dir_ni == vol->extend_ni) ?
					"n extended" : "");
			err = EPERM;
			goto err;
		}
	}
	/*
	 * Ensure the file is not read-only (the read-only bit is ignored for
	 * directories.
	 */
	if (!S_ISDIR(ni->mode) && ni->file_attributes & FILE_ATTR_READONLY) {
		ntfs_debug("Target %.*s, mft_no 0x%llx is marked read-only, "
				"returning EPERM.", (int)cn->cn_namelen,
				cn->cn_nameptr,
				(unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	/*
	 * If the inode is a reparse point or if the inode is offline we cannot
	 * remove a name from it yet.  TODO: Implement this.
	 */
	if (ni->file_attributes & (FILE_ATTR_REPARSE_POINT |
			FILE_ATTR_OFFLINE)) {
		ntfs_error(vol->mp, "Target %.*s, mft_no 0x%llx is %s.  "
				"Deleting names from such inodes is not "
				"supported yet, returning ENOTSUP.",
				(int)cn->cn_namelen, cn->cn_nameptr,
				(unsigned long long)ni->mft_no,
				ni->file_attributes & FILE_ATTR_REPARSE_POINT ?
				"a reparse point" : "offline");
		err = ENOTSUP;
		goto err;
	}
	/*
	 * If Carbon delete semantics are requested, do not allow busy files to
	 * be unlinked.  Note we do not use vnode_isinuse() as that accounts
	 * for open named streams/extended attributes as well which we do not
	 * care about.  We only care for actually opened files thus we keep
	 * track of them ourselves.
	 */
	if (flags & VNODE_REMOVE_NODELETEBUSY && ni->nr_opens) {
		ntfs_debug("Target %.*s, mft_no 0x%llx is busy (nr_opens "
				"0x%x) and Carbon delete semantics were "
				"requested, returning EBUSY.",
				(int)cn->cn_namelen, cn->cn_nameptr,
				(unsigned long long)ni->mft_no,
				(unsigned)ni->nr_opens);
		err = EBUSY;
		goto err;
	}
	/*
	 * We need to make sure the target still has the name specified in @cn
	 * that is being unlinked.  It could have been unlinked or renamed
	 * before we took the locks on the parent directory and the target.
	 *
	 * To do this, first convert the name of the target from utf8 to
	 * Unicode then look up the converted name in the directory index.
	 */
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
		goto err;
	}
	err = ntfs_lookup_inode_by_name(dir_ni, ntfs_name, ntfs_name_len,
			&mref, &name);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to find name in directory "
					"(error %d).", err);
			goto err;
		}
enoent:
		/*
		 * The name does not exist in the directory @dir_ni.
		 *
		 * This means someone renamed or deleted the name from the
		 * directory before we managed to take the locks.
		 */
		ntfs_debug("Target %.*s, mft_no 0x%llx has been renamed or "
				"deleted already, returning ENOENT.",
				(int)cn->cn_namelen, cn->cn_nameptr,
				(unsigned long long)ni->mft_no);
		/*
		 * If the target is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * We found the target name in the directory index but does it still
	 * point to the same mft record?  The sequence number check ensures the
	 * inode was not deleted and recreated with the same name and the same
	 * mft record number.
	 */
	if (mref != MK_MREF(ni->mft_no, ni->seq_no))
		goto enoent;
	/*
	 * We are going to go ahead with unlinking the target.
	 *
	 * There are several different types of outcome from the above lookup
	 * that need to be handled.
	 *
	 * If @name is NULL @ntfs_name contains the correctly cased name thus
	 * we can simply look for that.  In this case we set the name type to 0
	 * as we do not know which namespace the name is in.
	 *
	 * If @name is not NULL the correctly cased name is in @name->name thus
	 * we look for that.  In this case we do know which namespace the name
	 * is in as it is @name->type.
	 */
	ntfs_name_type = 0;
	if (name) {
		ntfs_name = name->name;
		ntfs_name_len = name->len;
		ntfs_name_type = name->type;
	}
	/* Now we can perform the actual unlink. */
	err = ntfs_unlink_internal(dir_ni, ni, ntfs_name, ntfs_name_len,
			ntfs_name_type, FALSE);
	if (err)
		ntfs_error(vol->mp, "Failed to unlink %.*s with mft_no 0x%llx "
				"from directory mft_no 0x%llx (error %d).",
				(int)cn->cn_namelen, cn->cn_nameptr,
				(unsigned long long)ni->mft_no,
				(unsigned long long)dir_ni->mft_no, err);
	else
		ntfs_debug("Done.");
err:
	if (name)
		IOFreeType(name, ntfs_dir_lookup_name);
	lck_rw_unlock_exclusive(&ni->lock);
	lck_rw_unlock_exclusive(&dir_ni->lock);
	return err;
}

/**
 * ntfs_vnop_remove - unlink a file
 * @a:		arguments to remove function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory from which to unlink the file
 *	vnode_t a_vp;			file to unlink
 *	struct componentname *a_cnp;	name of the file to unlink
 *	int a_flags;			flags describing the unlink request
 *	vfs_context_t a_context;
 *
 * Unlink a file with vnode @a->a_vp and name as specified in @a->a_cnp form
 * the directory with vnode @a->a_dvp.
 *
 * The flags in @a->a_flags further describe the unlink request.  The following
 * flags are currently defined in OS X kernel:
 *	VNODE_REMOVE_NODELETEBUSY	- Do not delete busy files, i.e. use
 *					  Carbon delete semantics).
 *
 * Return 0 on success and errno on error.
 *
 * Note that if the name of the inode to be removed is in the WIN32 or DOS
 * namespaces, both the WIN32 and the corresponding DOS names are removed.
 *
 * Note that for a hard link this function simply removes the name and its
 * directory entry and decrements the hard link count whilst for the last name,
 * i.e. the last link to an inode, it only removes the directory entry, i.e. it
 * does not remove the name, however it does decrement the hard link count to
 * zero.  This is so that the inode can be undeleted and its original name
 * restored.  In any case, we do not actually delete the inode here as it may
 * still be open and UNIX semantics require an unlinked inode to be still
 * accessible through already opened file descriptors.  When the last file
 * descriptor is closed, we causes the inode to be deleted when the VFS
 * notifies us of the last close by calling VNOP_INACTIVE(), i.e.
 * ntfs_vnop_inactive().
 */
static int ntfs_vnop_remove(struct vnop_remove_args *a)
{
	ntfs_inode *dir_ni = NTFS_I(a->a_dvp);
	ntfs_inode *ni = NTFS_I(a->a_vp);
	errno_t err;

	if (!dir_ni || !ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Entering.");
	err = ntfs_unlink(NTFS_I(a->a_dvp), NTFS_I(a->a_vp), a->a_cnp,
			a->a_flags, FALSE);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_link_internal - create a hard link to an inode
 * @ni:		base ntfs inode to create hard link to
 * @dir_ni:	directory ntfs inode in which to create the hard link
 * @cn:		componentname specifying name of the hard link to create
 * @is_rename:	if true ntfs_link_internal() is called for a rename
 * @name:	Unicode name of the inode to unlink
 * @name_len:	length of the name in Unicode characters
 *
 * Create a hard link to the ntfs inode @ni with name as specified in @cn in
 * the directory ntfs inode @dir_ni.
 *
 * If @is_rename is true the caller was ntfs_vnop_rename() in which case the
 * link count of the inode to link to will be one higher than the link count in
 * the mft record and @name and @name_len specify the Unicode name and length
 * in Unicode characters corresponding to @cn, respectively so we do not have
 * to convert @cn to Unicode in this case.
 *
 * If @is_rename is false then @name and @name_len are undefined.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create filenames in the POSIX namespace.
 */
static errno_t ntfs_link_internal(ntfs_inode *ni, ntfs_inode *dir_ni,
		struct componentname *cn, const BOOL is_rename,
		const ntfschar *name, const signed name_len)
{
	ntfs_volume *vol;
	FILENAME_ATTR *fn;
	ntfschar *ntfs_name;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	unsigned fn_alloc, fn_size;
	errno_t err, err2;
	BOOL is_dir;

	vol = ni->vol;
	ntfs_debug("Creating a hard link to mft_no 0x%llx, named %.*s in "
			"directory mft_no 0x%llx.",
			(unsigned long long)ni->mft_no, (int)cn->cn_namelen,
			cn->cn_nameptr, (unsigned long long)dir_ni->mft_no);
	if (NInoAttr(ni))
		panic("%s(): Inode to link to is an attribute/raw inode.\n",
				__FUNCTION__);
	is_dir = S_ISDIR(ni->mode);
	/*
	 * Create a temporary filename attribute so we can find the correct
	 * place to insert it into.  We also need a temporary copy so we can
	 * release the mft record before we add the directory entry.  This is
	 * needed because when we hold the mft record for the inode and we call
	 * ntfs_dir_entry_add() this would cause the mft record for the
	 * directory to be mapped which would result in a deadlock in the event
	 * that both mft records are in the same page.
	 */
	fn_alloc = sizeof(FILENAME_ATTR) + NTFS_MAX_NAME_LEN * sizeof(ntfschar);
	fn = IOMallocData(fn_alloc);
	if (!fn) {
		ntfs_error(vol->mp, "Failed to allocate memory for temporary "
				"filename attribute.");
		err = ENOMEM;
		goto err;
	}
	bzero(fn, fn_alloc);
	/* Begin setting up the temporary filename attribute. */
	fn->parent_directory = MK_LE_MREF(dir_ni->mft_no, dir_ni->seq_no);
	/* FILENAME_POSIX is zero and the attribute is already zeroed. */
	/* fn->filename_type = FILENAME_POSIX; */
	/*
	 * If this is not a rename then convert the name from utf8 to Unicode.
	 * If this is a rename on the other hand then we have the name in
	 * Unicode already so just copy that over.
	 */
	ntfs_name = fn->filename;
	ntfs_name_size = NTFS_MAX_NAME_LEN * sizeof(ntfschar);
	if (!is_rename) {
		ntfs_name_len = utf8_to_ntfs(vol, (u8*)cn->cn_nameptr,
				cn->cn_namelen, &ntfs_name, &ntfs_name_size);
		if (ntfs_name_len < 0) {
			err = -ntfs_name_len;
			if (err == ENAMETOOLONG)
				ntfs_debug("Failed (name is too long).");
			else
				ntfs_error(vol->mp, "Failed to convert name to "
						"Unicode (error %d).", err);
			goto err;
		}
	} else {
		memcpy(ntfs_name, name, name_len * sizeof(ntfschar));
		ntfs_name_len = name_len;
	}
	/* Set the filename length in the temporary filename attribute. */
	fn->filename_length = ntfs_name_len;
	fn_size = sizeof(FILENAME_ATTR) + ntfs_name_len * sizeof(ntfschar);
	/*
	 * Copy the times from the standard information attribute which we have
	 * cached in the ntfs inode.
	 */
	fn->creation_time = utc2ntfs(ni->creation_time);
	fn->last_data_change_time = utc2ntfs(ni->last_data_change_time);
	fn->last_mft_change_time = utc2ntfs(ni->last_mft_change_time);
	fn->last_access_time = utc2ntfs(ni->last_access_time);
	if (!is_dir) {
		lck_spin_lock(&ni->size_lock);
		fn->allocated_size = cpu_to_sle64(NInoNonResident(ni) &&
				(NInoSparse(ni) || NInoCompressed(ni)) ?
				ni->compressed_size : ni->allocated_size);
		fn->data_size = cpu_to_sle64(ni->data_size);
		lck_spin_unlock(&ni->size_lock);
	} else {
		/*
		 * Directories use 0 for the sizes in the filename attribute
		 * and the attribute is already zeroed.
		 */
		/* fn->data_size = fn->allocated_size = 0; */
	}
	/*
	 * If this is not a directory or it is an encrypted directory, set the
	 * needs archiving bit except for the core system files.
	 */
	fn->file_attributes = ni->file_attributes;
	if (!is_dir || NInoEncrypted(ni)) {
		BOOL need_set_archive_bit = TRUE;
		if (vol->major_ver >= 2) {
			if (ni->mft_no <= FILE_Extend)
				need_set_archive_bit = FALSE;
		} else {
			if (ni->mft_no <= FILE_UpCase)
				need_set_archive_bit = FALSE;
		}
		if (need_set_archive_bit) {
			ni->file_attributes |= FILE_ATTR_ARCHIVE;
			fn->file_attributes = ni->file_attributes;
			NInoSetDirtyFileAttributes(ni);
		}
	}
	/*
	 * Directories need the FILE_ATTR_DUP_FILENAME_INDEX_PRESENT flag set
	 * in their filename attributes both in their mft records and in the
	 * index entries pointing to them but not in the standard information
	 * attribute which is why it is not set in @ni->file_attributes.
	 */
	if (is_dir)
		fn->file_attributes |= FILE_ATTR_DUP_FILENAME_INDEX_PRESENT;
	/*
	 * TODO: We need to find out whether it is true that ea_length takes
	 * precedence over reparse_tag, i.e. we need to check that if both EAs
	 * are present and this is a reparse point, we need to set the
	 * ea_length rather than the reparse_tag.  So far I have not been able
	 * to create EAs on a reparse point and vice versa so perhaps the two
	 * are mutually exclusive in which case we are fine...
	 *
	 * The attribute is already zeroed so no need to set anything to zero.
	 */
#if 0
	if (ni->ea_length) {
		fn->ea_length = cpu_to_le16(ni->ea_length);
		/* fn->reserved = 0; */
	} else if (ni->file_attributes & FILE_ATTR_REPARSE_POINT) {
		// TODO: Instead of zero use actual value if/when we enable
		// creating hard links to reparse points...
		/* fn->reparse_tag = 0; */
	} else {
		/*
		 * We need to initialize the unused field to zero but as we
		 * have already zeroed the attribute we do not need to do
		 * anything now.
		 */
		/* fn->reparse_tag = 0; */
	}
#endif
	/*
	 * Add the created filename attribute to the parent directory index.
	 *
	 * We know @ni is the base inode since we bailed out for attribute
	 * inodes above so we can use it to generate the mft reference.
	 */
	err = ntfs_dir_entry_add(dir_ni, fn, fn_size,
			MK_LE_MREF(ni->mft_no, ni->seq_no));
	if (err)
		goto err;
	/*
	 * The ea_length and reparse_tag are only set in the directory index
	 * entries and not in filename attributes in the mft record so zero
	 * them here, before adding the filename attribute to the mft record.
	 */
	fn->reparse_tag = 0;
	/*
	 * Add the created filename attribute to the mft record as well.
	 *
	 * Again, we know @ni is the base inode.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record 0x%llx (error "
				"%d).", (unsigned long long)ni->mft_no, err);
		goto rm_err;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, fn, fn_size, ctx);
	if (err != ENOENT) {
		if (!err) {
			ntfs_debug("Failed (filename already present in "
					"inode.");
			err = EEXIST;
		} else
			ntfs_error(vol->mp, "Failed to add filename to mft_no "
					"0x%llx because looking up the "
					"filename in the mft record failed "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
		goto put_err;
	}
	/*
	 * The current implementation of ntfs_attr_lookup() will always return
	 * pointing into the base mft record when an attribute was not found.
	 */
	if (ni != ctx->ni)
		panic("%s(): ni != ctx->ni\n", __FUNCTION__);
	if (m != ctx->m)
		panic("%s(): m != ctx->m\n", __FUNCTION__);
	/*
	 * @ctx->a now points to the location in the mft record at which we
	 * need to insert the filename attribute, so insert it now.
	 *
	 * Note we ignore the case where @ctx->is_error is true because we do
	 * not need the attribute any more for anything after it has been
	 * inserted so we do not care that we failed to map its mft record.
	 */
	err = ntfs_resident_attr_record_insert(ni, ctx, AT_FILENAME, NULL, 0,
			fn, fn_size);
	if (err) {
		ntfs_error(vol->mp, "Failed to add filename to mft_no 0x%llx "
				"because inserting the filename attribute "
				"failed (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto put_err;
	}
	/*
	 * Update the hard link count in the mft record.  Note we subtract one
	 * from the inode link count if this is a rename as the link count has
	 * been elevated by one by the caller.
	 */
	ni->link_count++;
	m->link_count = cpu_to_le16(ni->link_count - (is_rename ? 1 : 0));
	/*
	 * Update the ctime in the inode by copying it from the target
	 * directory inode where it will have been updated by the above call to
	 * ntfs_dir_entry_add().
	 */
	ni->last_mft_change_time = dir_ni->last_mft_change_time;
	NInoSetDirtyTimes(ni);
	/*
	 * Invalidate negative cache entries in the directory.  We need to do
	 * this because there may be negative cache entries which would match
	 * the name of the just created inode but in a different case.  Such
	 * negative cache entries would now be incorrect thus we need to throw
	 * away all negative cache entries to ensure there cannot be any
	 * incorrectly negative entries in the name cache.
	 */
	cache_purge_negatives(dir_ni->vn);
	/*
	 * We should add the new hard link to the name cache.  Problem is that
	 * this is likely not to be a useful thing to do as the original name
	 * is likely in the name cache already and the OS X name cache only
	 * allows one name per vnode and cache_enter() simply returns without
	 * doing anything if a name is already present in the name cache for
	 * the vnode.  Thus we could use vnode_update_identity() instead to
	 * switch the cached name from the original name to the new hard link.
	 *
	 * FIXME: The question is whether this is a useful thing to do.  On the
	 * one hand people creating a hard link are likely to want to then
	 * access the inode via the new name but on the other hand hard links
	 * are often used in applications for locking purposes and in this case
	 * after the hard link is created the application is likely to unlink
	 * the original name thus it would be beneficial if that remains in the
	 * cache until this happens which will automatically remove the name
	 * from the name cache and the next lookup of the new name will insert
	 * the new one.  Thus it is best if we do nothing at all now.  If OS X
	 * ever allows multiple name links per vnode we can uncomment the below
	 * cache_enter() call.
	 *
	 * For the rename case we have just removed the original name, thus it
	 * makes sense to add the new name now and whilst at it also update the
	 * vnode identity with the new name and parent as the old ones are no
	 * longer valid.
	 */
	if (is_rename) {
		vnode_update_identity(ni->vn, dir_ni->vn, cn->cn_nameptr,
				cn->cn_namelen, cn->cn_hash,
				VNODE_UPDATE_PARENT | VNODE_UPDATE_NAME);
		cache_enter(dir_ni->vn, ni->vn, cn);
		cn->cn_flags &= ~MAKEENTRY;
	}
	/*
	 * Ensure the base mft record is written to disk.
	 *
	 * Note we do not set any of the NInoDirty*() flags because we have
	 * just created the inode thus all the fields are in sync between the
	 * ntfs_inode @ni and its mft record @m.
	 *
	 * Also note we defer the unmapping of the mft record to here so that
	 * we do not get racing time updates, etc during concurrent runs of
	 * link(2) and rename(2) where the source inode for the rename is the
	 * inode that has a new hardlink created to it at the same time.  This
	 * case can happen because we do not lock the source inode in
	 * ntfs_vnop_rename().
	 */
	NInoSetMrecNeedsDirtying(ni);
	/* We are done with the mft record. */
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	/* Free the temporary filename attribute. */
	IOFreeData(fn, fn_alloc);
	ntfs_debug("Done.");
	return 0;
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(ni);
rm_err:
#if 0
	if (ni->ea_length) {
		fn->ea_length = cpu_to_le16(ni->ea_length);
		/* fn->reserved = 0; */
	} else if (ni->file_attributes & FILE_ATTR_REPARSE_POINT) {
		// TODO: Instead of zero use actual value if/when we enable
		// creating hard links to reparse points...
		/* fn->reparse_tag = 0; */
	} else {
		/*
		 * We need to initialize the unused field to zero but as we
		 * have already zeroed the attribute we do not need to do
		 * anything now.
		 */
		/* fn->reparse_tag = 0; */
	}
#endif
	err2 = ntfs_dir_entry_delete(dir_ni, ni, fn, fn_size);
	if (err2) {
		ntfs_error(vol->mp, "Failed to rollback index entry creation "
				"in error handling code path (error %d).  "
				"Leaving inconsistent metadata.  Run chkdsk.",
				err2);
		NVolSetErrors(vol);
	}
err:
	if (fn)
		IOFreeData(fn, fn_alloc);
	if (err != EEXIST)
		ntfs_error(vol->mp, "Failed (error %d).", err);
	else
		ntfs_debug("Failed (error EEXIST).");
	return err;
}

/**
 * ntfs_vnop_link - create a hard link to an inode
 * @a:		arguments to link function
 *
 * @a contains:
 *	vnode_t a_vp;			vnode to create hard link to
 *	vnode_t a_tdvp;			destination directory for the hard link
 *	struct componentname *a_cnp;	name of the hard link to create
 *	vfs_context_t a_context;
 *
 * Create a hard link to the inode specified by the vnode @a->a_vp with name as
 * specified in @a->a_cnp in the directory specified by the vnode @a->a_tdvp.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create filenames in the POSIX namespace.
 */
static int ntfs_vnop_link(struct vnop_link_args *a)
{
	errno_t err;

	ntfs_inode *ni = NTFS_I(a->a_vp);
    if (!ni) {
        ntfs_debug("Entered with NULL ntfs_inode, aborting.");
        return EINVAL;
    }
	ntfs_inode *dir_ni = NTFS_I(a->a_tdvp);
	if (!dir_ni) {
		ntfs_debug("Entered with NULL dir ntfs_inode, aborting.");
		return EINVAL;
	}
    ntfs_volume *vol = ni->vol;
	struct componentname *cn = a->a_cnp;
	ntfs_debug("Creating a hard link to mft_no 0x%llx, named %.*s in "
			"directory mft_no 0x%llx.",
			(unsigned long long)ni->mft_no, (int)cn->cn_namelen,
			cn->cn_nameptr, (unsigned long long)dir_ni->mft_no);
	/* Do not allow attribute/raw inodes to be linked to. */
	if (NInoAttr(ni)) {
		ntfs_debug("Mft_no 0x%llx is a%s inode, returning EPERM.",
				(unsigned long long)ni->mft_no,
				NInoRaw(ni) ? " raw" : "n attribute");
		return EPERM;
	}
	/* The target inode must be a directory. */
	if (!S_ISDIR(dir_ni->mode)) {
		ntfs_debug("Target mft_no 0x%llx is not a directory, "
				"returning ENOTDIR.",
				(unsigned long long)dir_ni->mft_no);
		return ENOTDIR;
	}
	/* Lock the target directory inode for writing. */
	lck_rw_lock_exclusive(&dir_ni->lock);
	/* The inode being linked to must not be a directory. */
	if (S_ISDIR(ni->mode)) {
		lck_rw_unlock_exclusive(&dir_ni->lock);
		ntfs_debug("Mft_no 0x%llx to link to is a directory, cannot "
				"create hard link %.*s to it, returning "
				"EPERM.", (unsigned long long)ni->mft_no,
				(int)cn->cn_namelen, cn->cn_nameptr);
		return EPERM;
	}
	/* Lock the inode to link to for writing. */
	lck_rw_lock_exclusive(&ni->lock);
	/* Ensure the target directory has not been deleted. */
	if (!dir_ni->link_count) {
		ntfs_debug("Target directory mft_no 0x%llx has been deleted, "
				"returning ENOENT.",
				(unsigned long long)dir_ni->mft_no);
		/*
		 * If the directory is somehow still in the name cache remove
		 * it now.
		 */
		cache_purge(dir_ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * Ensure the inode has not been deleted.  Note we really should be
	 * checking that the source of the hard link has not been unlinked yet
	 * but we do not know what the source name was as the caller does not
	 * provide it to us and we do not know which name we were called for
	 * from just looking at the source vnode/inode.
	 */
	if (!ni->link_count) {
		ntfs_debug("Inode %.*s, mft_no 0x%llx has been deleted, "
				"returning ENOENT.", (int)cn->cn_namelen,
				cn->cn_nameptr, (unsigned long long)ni->mft_no);
		/*
		 * If the target is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * The inode being linked to must not be a directory or device special
	 * file.  TODO: Extend the checks when we support device special files.
	 */
	if (S_ISDIR(ni->mode)) {
		ntfs_debug("Mft_no 0x%llx to link to is a directory, cannot "
				"create hard link %.*s to it, returning "
				"EPERM.", (unsigned long long)ni->mft_no,
				(int)cn->cn_namelen, cn->cn_nameptr);
		err = EPERM;
		goto err;
	}
	/*
	 * Do not allow any of the system files to be linked to.
	 *
	 * For NTFS 3.0+ volumes do not allow any of the extended system files
	 * to be linked to, either.
	 *
	 * Note we specifically blacklist all system files that we make use of.
	 *
	 * TODO: What about all the new metadata files introduced with Windows
	 * Vista?  We are currently ignoring them and allowing them to be
	 * linked to...
	 */
	if (ni->file_attributes & FILE_ATTR_SYSTEM) {
		BOOL is_system = FALSE;
		if (vol->major_ver <= 1) {
			if (ni->mft_no < FILE_Extend)
				is_system = TRUE;
		} else {
			if (ni->mft_no <= FILE_Extend)
				is_system = TRUE;
			if (ni == vol->objid_ni || ni == vol->quota_ni ||
					ni == vol->usnjrnl_ni)
				is_system = TRUE;
		}
		if (is_system) {
			ntfs_debug("Mft_no 0x%llx is a%s system file, "
					"returning EPERM.",
					(unsigned long long)ni->mft_no,
					(ni->mft_no > FILE_Extend) ?
					"n extended" : "");
			err = EPERM;
			goto err;
		}
	}
	/*
	 * Ensure the inode to link to is not read-only (we already checked
	 * that @ni is not a directory).
	 */
	if (ni->file_attributes & FILE_ATTR_READONLY) {
		ntfs_debug("Mft_no 0x%llx is marked read-only, returning "
				"EPERM.", (unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	/*
	 * TODO: Test if Windows is happy with a reparse point having a hard
	 * link and if so remove this check and copy in the reparse point tag
	 * into the filename attribute below.  For mount point reparse points
	 * the reparse point is a directory so the link attempt would already
	 * have been aborted.
	 *
	 * TODO: Test if Windows is happy with an offline inode having a hard
	 * link and if so remove this check.
	 */
	if (ni->file_attributes & (FILE_ATTR_REPARSE_POINT |
			FILE_ATTR_OFFLINE)) {
		ntfs_debug("Mft_no 0x%llx is %s.  Creating hard links to such "
				"inodes is not allowed, returning EPERM.",
				(unsigned long long)ni->mft_no,
				(ni->file_attributes &
				FILE_ATTR_REPARSE_POINT) ?
				"a reparse point" : "offline");
		err = EPERM;
		goto err;
	}
	/* Check if the maximum link count is already reached. */
	if (ni->link_count >= NTFS_MAX_HARD_LINKS) {
		ntfs_debug("Cannot create hard link to mft_no 0x%llx because "
				"it already has too many hard links.",
				(unsigned long long)ni->mft_no);
		err = EMLINK;
		goto err;
	}
	/* Go ahead and create the hard link. */
	err = ntfs_link_internal(ni, dir_ni, cn, FALSE, NULL, 0);
	if (err) {
		if (err != EEXIST)
			ntfs_error(vol->mp, "Failed to create hard link to "
					"mft_no 0x%llx, named %.*s, in "
					"directory mft_no 0x%llx (error %d).",
					(unsigned long long)ni->mft_no,
					(int)cn->cn_namelen, cn->cn_nameptr,
					(unsigned long long)dir_ni->mft_no,
					err);
		else
			ntfs_debug("Failed to create hard link to mft_no "
					"0x%llx, named %.*s, in directory "
					"mft_no 0x%llx (error EEXIST).",
					(unsigned long long)ni->mft_no,
					(int)cn->cn_namelen, cn->cn_nameptr,
					(unsigned long long)dir_ni->mft_no);
	} else
		ntfs_debug("Done.");
err:
	/* We are done, unlock the inode and the target directory. */
	lck_rw_unlock_exclusive(&ni->lock);
	lck_rw_unlock_exclusive(&dir_ni->lock);
	return err;
}

/**
 * ntfs_vnop_rename - rename an inode (file/directory/symbolic link/etc)
 * @a:		arguments to rename function
 *
 * @a contains:
 *	vnode_t a_fdvp;			directory containing source inode
 *	vnode_t a_fvp;			source inode to be renamed
 *	struct componentname *a_fcnp;	name of the inode to rename
 *	vnode_t a_tdvp;			target directory to move the source to
 *	vnode_t a_tvp;			target inode to be deleted
 *	struct componentname *a_tcnp;	name of the inode to delete
 *	vfs_context_t a_context;
 *
 * Rename the inode @a_fvp with name as specified in @a->a_fcnp located in the
 * directory @a->a_fdvp to the new name specified in a->a_tcnp placing it in
 * the target directory @a->a_tdvp.
 *
 * If @a->a_tvp is not NULL it means that the rename target already exists
 * which means we have to delete the rename target before we can perform the
 * rename.  In this case @a->a_tvp is the existing target inode and its name is
 * the rename target name specified in @a->a_tcnp and it is located in the
 * target directory @a->a_tdvp.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create the target name @a->a_tcnp in the POSIX namespace.
 *
 * Rename is a complicated operation because there are several special cases
 * that need consideration:
 *
 * First of all unchecked renaming can create directory loops which are not
 * attached to the file system root, e.g. take the directory tree /a/b/c and
 * perform a rename of /a/b to /a/b/c/ which if allowed to proceed would create
 * /a and b/c/b where the latter is a loop in that b points back to c which
 * points back to b.  Also this loop no longer is attached to the file system
 * directory tree and there is no way to access it any more as there is no link
 * from /a to b or c any more.  Thus we have to check for this case and return
 * EINVAL error instead of doing the rename.  Also a concurrent rename could
 * reshape the tree after our check so that our case would result in a loop
 * after all thus all tree reshaping renames must be done under a rename lock.
 * Note the VFS already holds the mnt_renamelock mutex for some renames but it
 * does not hold it in all cases we need it to be held so we still need our own
 * NTFS rename lock.
 *
 * Further VNOP_RENAME() must observe the following rules:
 *
 * - Source and destination must either both be directories, or both not be
 *   directories.  If this is not the case return ENOTDIR if the target is not
 *   a directory and EISDIR if the target is a directory.
 *
 * - If the target is a directory, it must be empty.  Return ENOTEMPTY if not.
 *
 * - It is not allowed to rename "/", ".", or "..".  Return EINVAL if this is
 *   attempted.
 *
 * - If the source inode and the target inode are the same and the mount is
 *   case sensitive or the parent directories are also the same and the names
 *   are the same do not do anything at all and return success, i.e. 0.  Note
 *   this is a violation of POSIX but it is needed to allow renaming of files
 *   from one case to another, i.e. when a mount is not case sensitive but case
 *   preserving (this is the default for NTFS) and the source and target inodes
 *   and their parent directories match but the names do not match we want to
 *   perform the rename rather than just return success.  If we still find that
 *   the target exists as a hard link rather than this being a case changing
 *   rename we still need to abort and return success to comply with POSIX.
 *
 *   FIXME: There is a bug in the VFS in that it never calls VNOP_RENAME() at
 *   all when it is called with source and target strings being the same.  This
 *   is wrong when the string matches the name but does not have the same case,
 *   i.e. the rename would normally succeed switching the case to the new case.
 *   The VFS is currently forbidding this to happen.  <rdar://problem/5485782>
 */
static int ntfs_vnop_rename(struct vnop_rename_args *a)
{
	MFT_REF src_mref, dst_mref;
	ntfs_inode *src_dir_ni, *src_ni, *dst_dir_ni, *dst_ni;
	struct componentname *src_cn, *dst_cn;
	ntfs_volume *vol;
	ntfschar *ntfs_name_buf, *orig_ntfs_name, *dst_ntfs_name;
	ntfschar *src_ntfs_name, *target_ntfs_name;
	ntfs_dir_lookup_name *src_name, *dst_name;
	size_t orig_ntfs_name_size, dst_ntfs_name_size;
	signed orig_ntfs_name_len, dst_ntfs_name_len, src_ntfs_name_len;
	signed target_ntfs_name_len;
	errno_t err, err2;
	FILENAME_TYPE_FLAGS src_ntfs_name_type, target_ntfs_name_type;
	BOOL have_unlinked = FALSE;

	dst_name = src_name = NULL;
	src_dir_ni = NTFS_I(a->a_fdvp);
	src_ni = NTFS_I(a->a_fvp);
	src_cn = a->a_fcnp;
	dst_dir_ni = NTFS_I(a->a_tdvp);
	if (!src_dir_ni || !src_ni || !dst_dir_ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = src_dir_ni->vol;
	dst_cn = a->a_tcnp;
	if (a->a_tvp) {
		dst_ni = NTFS_I(a->a_tvp);
		if (!dst_ni) {
			ntfs_debug("Entered with NULL ntfs_inode, aborting.");
			return EINVAL;
		}
		ntfs_debug("Entering for source mft_no 0x%llx, name %.*s, "
				"parent directory mft_no 0x%llx and "
				"destination mft_no 0x%llx, name %.*s, parent "
				"directory mft_no 0x%llx.",
				(unsigned long long)src_ni->mft_no,
				(int)src_cn->cn_namelen, src_cn->cn_nameptr,
				(unsigned long long)src_dir_ni->mft_no,
				(unsigned long long)dst_ni->mft_no,
				(int)dst_cn->cn_namelen, dst_cn->cn_nameptr,
				(unsigned long long)dst_dir_ni->mft_no);
		if (src_ni == dst_ni && NVolCaseSensitive(vol)) {
			ntfs_debug("Source and destination inodes are the "
					"same and the volume is case "
					"sensitive.  Returning success "
					"without doing anything as required "
					"by POSIX.");
			return 0;
		}
	} else {
		dst_ni = NULL;
		ntfs_debug("Entering for source mft_no 0x%llx, name %.*s, "
				"parent directory mft_no 0x%llx and no "
				"destination mft_no, destination name %.*s, "
				"parent directory mft_no 0x%llx.",
				(unsigned long long)src_ni->mft_no,
				(int)src_cn->cn_namelen, src_cn->cn_nameptr,
				(unsigned long long)src_dir_ni->mft_no,
				(int)dst_cn->cn_namelen, dst_cn->cn_nameptr,
				(unsigned long long)dst_dir_ni->mft_no);
	}
	/*
	 * The source and target parent inodes must be directories which
	 * implies they are base inodes.
	 */
	if (!S_ISDIR(src_dir_ni->mode) || !S_ISDIR(dst_dir_ni->mode)) {
		ntfs_debug("%s parent inode 0x%llx is not a directory, "
				"returning ENOTDIR.",
				!S_ISDIR(src_dir_ni->mode) ?
				"Source" : "Destination", (unsigned long long)
				(!S_ISDIR(src_dir_ni->mode) ?
				src_dir_ni->mft_no : dst_dir_ni->mft_no));
		return ENOTDIR;
	}
	/*
	 * All inodes must be locked in parent -> child order so we need to
	 * check whether the source and target parent inodes have a
	 * parent/child relationship with each other.
	 *
	 * If both are the same we have the easiest case and we just lock the
	 * single directory inode.
	 *
	 * If the two are not the same we need to exclude all other tree
	 * reshaping renames from happening as they could change the
	 * relationship between the parent directory inodes under our feet.  To
	 * do this we use a per ntfs volume lock so we can then go on to
	 * determine their parent/child relationship.
	 *
	 * Once we have established if there is a parent/child relationship we
	 * lock the parent followed by the child and if the two are completely
	 * unrelated the order of locking does not matter so we just lock the
	 * destination followed by the source.
	 *
	 * Note that we take this opportunity of walking the directory tree up
	 * to the root starting from @dst_dir_ni to also check whether @src_ni
	 * is either equal to or a parent of @dst_dir_ni in which case a
	 * directory loop would be caused by the rename so we have to abort it
	 * with EINVAL error.
	 */
	if (src_dir_ni == dst_dir_ni)
		lck_rw_lock_exclusive(&src_dir_ni->lock);
	else {
		BOOL is_parent;

		lck_mtx_lock(&vol->rename_lock);
		err = ntfs_inode_is_parent(src_dir_ni, dst_dir_ni, &is_parent,
				src_ni);
		if (err) {
			lck_mtx_unlock(&vol->rename_lock);
			/*
			 * @err == EINVAL means @src_ni matches or is a parent
			 * of @dst_dir_ni.  This would create a directory
			 * loop so abort the rename but do not emit an error
			 * message as there is no error as such.
			 */
			if (err != EINVAL)
				ntfs_error(vol->mp, "Failed to determine "
						"whether source directory "
						"mft_no 0x%llx is a parent of "
						"destination directory mft_no "
						"0x%llx (error %d).",
						(unsigned long long)
						src_dir_ni->mft_no,
						(unsigned long long)
						dst_dir_ni->mft_no, err);
			return err;
		}
		/*
		 * If @src_dir_ni is a parent of @dst_dir_ni, lock @src_dir_ni
		 * followed by @dst_dir_ni.
		 *
		 * Otherwise either @dst_dir_ni is a parent of @src_dir_ni, in
		 * which case we have to lock @dst_dir_ni followed by
		 * @src_dir_ni, or they are unrelated in which case lock
		 * ordering does not matter thus we do not need to distinguish
		 * those two cases and can simply lock @dst_dir_ni followed by
		 * @src_dir_ni.
		 */
		if (is_parent) {
			lck_rw_lock_exclusive(&src_dir_ni->lock);
			lck_rw_lock_exclusive(&dst_dir_ni->lock);
		} else {
			lck_rw_lock_exclusive(&dst_dir_ni->lock);
			lck_rw_lock_exclusive(&src_dir_ni->lock);
		}
	}
	/*
	 * The source cannot be the source directory and the destination cannot
	 * be the destination directory.  Also as we are about to lock the
	 * target ensure it does not equal the source directory either.  We
	 * have already checked for the source being equal to the target
	 * directory above so no need to check again.
	 */
	if (dst_ni && dst_ni == src_dir_ni) {
		ntfs_debug("The source parent directory equals the target, "
				"returning ENOTEMPTY.");
		err = ENOTEMPTY;
		/* Set @dst_ni to NULL so we do not try to unlock it. */
		dst_ni = NULL;
		goto err;
	}
	if (src_ni == src_dir_ni || (dst_ni && dst_ni == dst_dir_ni)) {
		ntfs_debug("The source and/or the target is/are equal to "
				"their parent directories, returning EINVAL.");
		err = EINVAL;
		/* Set @dst_ni to NULL so we do not try to unlock it. */
		dst_ni = NULL;
		goto err;
	}
	/*
	 * If the destination inode exists lock it so it can be unlinked
	 * safely.  For example if it is a directory we need to ensure that it
	 * is empty and that no-one creates an entry in it whilst the delete is
	 * in progress which requires us to hold an exclusive lock on it.
	 */
	if (dst_ni)
		lck_rw_lock_exclusive(&dst_ni->lock);
	/*
	 * Because we have locked the parent inode of the source inode there is
	 * no need to lock the source inode itself.  We are not going to unlink
	 * it completely, just move it from one location/name to another name
	 * and/or place in the directory tree and the mft record will be mapped
	 * and thus locked for exclusive access whenever we modify the inode
	 * which will serialize any potential concurrent operations on the
	 * inode.  The only concurrent operation to watch out for is when the
	 * source inode is a directory and someone calls VNOP_REMOVE() or
	 * VNOP_RMDIR() on any of its child inodes.  This can end up in the
	 * situation where the index root node is locked in
	 * ntfs_index_entry_delete() and hence the mft record is mapped whilst
	 * the free space in the mft record is evaluated but then before this
	 * information is used the mft record is unmapped and then mapped again
	 * as part of a call to ntfs_index_entry_lock_two() and if our
	 * VNOP_RENAME() manages to map the mft record whilst it is temporarily
	 * unmapped during the ntfs_index_entry_lock_two() we can cause the
	 * free space in the mft record to decrease and thus the
	 * ntfs_index_entry_delete() may then encounter an out of space
	 * condition when it thought it had determined the amount of free space
	 * already and thus assume something has gone wrong and panic().  We
	 * overcome this problem inside ntfs_index_entry_delete() by rechecking
	 * the free space after reacquiring the lock and dealing with it as
	 * appropriate.
	 *
	 * First, ensure the parent directories have not been deleted.
	 */
	if (!src_dir_ni->link_count || !dst_dir_ni->link_count) {
		ntfs_debug("One or both of the parent directories mft_no "
				"0x%llx and mft_no 0x%llx has/have been "
				"deleted, returning ENOENT.",
				(unsigned long long)src_dir_ni->mft_no,
				(unsigned long long)dst_dir_ni->mft_no);
		/*
		 * If the directory is somehow still in the name cache remove
		 * it now.
		 */
		if (!src_dir_ni->link_count)
			cache_purge(src_dir_ni->vn);
		if (!dst_dir_ni->link_count)
			cache_purge(dst_dir_ni->vn);
		err = ENOENT;
		goto err;
	}
	/* Rename is not allowed on attribute/raw inodes. */
	if (NInoAttr(src_ni) || (dst_ni && NInoAttr(dst_ni))) {
		ntfs_debug("Source and/or target inode is/are attribute/raw "
				"inodes, returning EPERM.");
		err = EPERM;
		goto err;
	}
	/* Ensure the source has not been deleted by someone else already. */
	if (!src_ni->link_count) {
		ntfs_debug("Source %.*s, mft_no 0x%llx has been deleted, "
				"returning ENOENT.", (int)src_cn->cn_namelen,
				src_cn->cn_nameptr,
				(unsigned long long)src_ni->mft_no);
		/*
		 * If the source is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(src_ni->vn);
		err = ENOENT;
		goto err;
	}
	/*
	 * Ensure the target has not been deleted by someone else already.  If
	 * it has been deleted pretend the caller did not specify a target.
	 * This is what HFS+ does, too.
	 */
	if (dst_ni && !dst_ni->link_count) {
		ntfs_debug("Target %.*s, mft_no 0x%llx has been deleted, "
				"pretending no target was specified.",
				(int)dst_cn->cn_namelen, dst_cn->cn_nameptr,
				(unsigned long long)dst_ni->mft_no);
		/*
		 * If the target is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(dst_ni->vn);
		lck_rw_unlock_exclusive(&dst_ni->lock);
		dst_ni = NULL;
	}
	/*
	 * If the destination exists need to ensure that it is a directory if
	 * the source is a directory or that it is not a directory if the
	 * source is not a directory.
	 *
	 * Also, need to ensure the target directory is empty.
	 *
	 * If the source and destination are the same none of these checks
	 * apply so skip them.
	 */
	if (dst_ni && src_ni != dst_ni) {
		if (S_ISDIR(src_ni->mode)) {
			if (!S_ISDIR(dst_ni->mode)) {
				ntfs_debug("Source is a directory but "
						"destination is not, "
						"returning ENOTDIR");
				err = ENOTDIR;
				goto err;
			}
			/* The target is a directory, but is it empty? */
			err = ntfs_dir_is_empty(dst_ni);
			if (err) {
				if (err == ENOTEMPTY)
					ntfs_debug("Target directory %.*s, "
							"mft_no 0x%llx is not "
							"empty, returning "
							"ENOTEMPTY.",
							(int)dst_cn->cn_namelen,
							dst_cn->cn_nameptr,
							(unsigned long long)
							dst_ni->mft_no);
				else {
					ntfs_error(vol->mp, "Failed to "
							"determine if target "
							"directory %.*s, "
							"mft_no 0x%llx is "
							"empty (error %d).",
							(int)dst_cn->cn_namelen,
							dst_cn->cn_nameptr,
							(unsigned long long)
							dst_ni->mft_no, err);
					err = EIO;
				}
				goto err;
			}
		} else /* if (!S_ISDIR(src_ni->mode)) */ {
			if (S_ISDIR(dst_ni->mode)) {
				ntfs_debug("Source is not a directory but "
						"destination is, returning "
						"EISDIR");
				err = EISDIR;
				goto err;
			}
		}
	}
	/* Ensure none of the inodes are read-only. */
	if ((!S_ISDIR(src_ni->mode) &&
			src_ni->file_attributes & FILE_ATTR_READONLY) ||
			(dst_ni && !S_ISDIR(dst_ni->mode) &&
			dst_ni->file_attributes & FILE_ATTR_READONLY)) {
		ntfs_debug("One of the inodes involved in the rename is "
				"read-only, returning EPERM.");
		err = EPERM;
		goto err;
	}
	/*
	 * Do not allow any of the system files to be renamed/deleted.
	 *
	 * For NTFS 3.0+ volumes do not allow any of the extended system files
	 * to be renamed/deleted, either.
	 *
	 * Note we specifically blacklist all system files that we make use of.
	 *
	 * TODO: What about all the new metadata files introduced with Windows
	 * Vista?  We are currently ignoring them and allowing them to be
	 * renamed/deleted...
	 */
	if (src_ni->file_attributes & FILE_ATTR_SYSTEM || (dst_ni &&
			dst_ni->file_attributes & FILE_ATTR_SYSTEM)) {
		BOOL is_system = FALSE;
		if (vol->major_ver <= 1) {
			if (src_ni->mft_no < FILE_Extend || (dst_ni &&
					dst_ni->mft_no < FILE_Extend))
				is_system = TRUE;
		} else {
			if (src_ni->mft_no <= FILE_Extend || (dst_ni &&
					dst_ni->mft_no <= FILE_Extend))
				is_system = TRUE;
			if (src_dir_ni == vol->extend_ni) {
				if (src_ni == vol->objid_ni ||
						src_ni == vol->quota_ni ||
						src_ni == vol->usnjrnl_ni)
					is_system = TRUE;
			}
			if (dst_dir_ni == vol->extend_ni) {
				if (dst_ni == vol->objid_ni ||
						dst_ni == vol->quota_ni ||
						dst_ni == vol->usnjrnl_ni)
					is_system = TRUE;
			}
		}
		if (is_system) {
			ntfs_debug("Source and/or target inode is a system "
					"file, returning EPERM.");
			err = EPERM;
			goto err;
		}
	}
	/*
	 * If the source/target inodes are reparse points or if they are
	 * offline we cannot rename/delete them yet.  TODO: Implement this.
	 */
	if (src_ni->file_attributes & (FILE_ATTR_REPARSE_POINT |
			FILE_ATTR_OFFLINE) || (dst_ni &&
			dst_ni->file_attributes & (FILE_ATTR_REPARSE_POINT |
			FILE_ATTR_OFFLINE))) {
		ntfs_error(vol->mp, "Source or target inode is a reparse "
				"point or offline, renaming such indoes is "
				"notsupported yet, returning ENOTSUP.");
		err = ENOTSUP;
		goto err;
	}
	/*
	 * To proceed further we need to convert both the source and target
	 * names from utf8 to Unicode.  This is a good time to do both as the
	 * conversion also checks for invalid names, too long names, etc.
	 *
	 * Note we allocate both source and target names with a single buffer
	 * so we only have to call once into the allocator.
	 */
	ntfs_name_buf = IOMallocData(NTFS_MAX_NAME_LEN * 2);
	if (!ntfs_name_buf) {
		ntfs_debug("Not enough memory to allocate name buffer.");
		err = ENOMEM;
		goto err;
	}
	orig_ntfs_name = ntfs_name_buf;
	dst_ntfs_name = (ntfschar*)((u8*)ntfs_name_buf + NTFS_MAX_NAME_LEN);
	dst_ntfs_name_size = orig_ntfs_name_size = NTFS_MAX_NAME_LEN;
	orig_ntfs_name_len = utf8_to_ntfs(vol, (u8*)src_cn->cn_nameptr,
			src_cn->cn_namelen, &orig_ntfs_name,
			&orig_ntfs_name_size);
	if (orig_ntfs_name_len < 0) {
		err = -orig_ntfs_name_len;
		if (err == ENAMETOOLONG)
			ntfs_debug("Failed (source name is too long).");
		else
			ntfs_error(vol->mp, "Failed to convert name to "
					"Unicode (error %d).", err);
		goto free_err;
	}
	dst_ntfs_name_len = utf8_to_ntfs(vol, (u8*)dst_cn->cn_nameptr,
			dst_cn->cn_namelen, &dst_ntfs_name,
			&dst_ntfs_name_size);
	if (dst_ntfs_name_len < 0) {
		err = -dst_ntfs_name_len;
		if (err == ENAMETOOLONG)
			ntfs_debug("Failed (target name is too long).");
		else
			ntfs_error(vol->mp, "Failed to convert target name to "
					"Unicode (error %d).", err);
		goto free_err;
	}
	/*
	 * We need to make sure the source still has the name specified in
	 * @src_cn.  It could have been unlinked or renamed before we took the
	 * lock on the parent directory.
	 *
	 * To do this, look up the converted source name in the source parent
	 * directory index.
	 */
	err = ntfs_lookup_inode_by_name(src_dir_ni, orig_ntfs_name,
			orig_ntfs_name_len, &src_mref, &src_name);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to find source name in "
					"directory (error %d).", err);
			goto free_err;
		}
src_enoent:
		/*
		 * The source name does not exist in the source parent
		 * directory.
		 *
		 * This means someone renamed or deleted the name from the
		 * directory before we managed to take the locks.
		 */
		ntfs_debug("Source has been renamed or deleted already, "
				"returning ENOENT.");
		/*
		 * If the source is somehow still in the name cache remove it
		 * now.
		 */
		cache_purge(src_ni->vn);
		err = ENOENT;
		goto free_err;
	}
	/*
	 * We found the source name in the directory index but does it still
	 * point to the same mft record?  The sequence number check ensures the
	 * inode was not deleted and recreated with the same name and the same
	 * mft record number.
	 */
	if (src_mref != MK_MREF(src_ni->mft_no, src_ni->seq_no))
		goto src_enoent;
	/*
	 * We now have verified everything to do with the source.  Set the
	 * source name to be the correctly cased name (unless it was correctly
	 * cased already in which case @src_name will be NULL and
	 * @orig_ntfs_name contains the correcly cased name).
	 */
	if (src_name) {
		src_ntfs_name = src_name->name;
		src_ntfs_name_len = src_name->len;
		src_ntfs_name_type = src_name->type;
	} else {
		src_ntfs_name = orig_ntfs_name;
		src_ntfs_name_len = orig_ntfs_name_len;
		src_ntfs_name_type = 0;
	}
	/*
	 * Now we need to verify the target.  In an ideal world, either it has
	 * to be specified in @dst_ni in which case it also has to exist in the
	 * destination parent directory @dst_dir_ni, or @dst_ni has to be NULL
	 * in which case the target name must not exist in the destination
	 * parent directory.
	 *
	 * But because the VFS obtains the target before we take the necessary
	 * locks it is possible for the above ideal not to be true.  There are
	 * several possible cases:
	 *
	 * - Target was specified but deleted.  We have detected this case
	 *   above and have set @dst_ni to NULL thus we do not need to worry
	 *   about this case any more.
	 * - Target was not specified but another inode was created with the
	 *   same name.  In this case we return EEXIST which is what HFS+ does,
	 *   too.
	 * - Target was specified but renamed.  This means we may or may not
	 *   find a directory entry of the same name.  If we do not find a
	 *   matching directory entry we know the target has been renamed thus
	 *   we can simply set @dst_ni to NULL and pretend it does not exist.
	 *   If we do find a directory entry that matches in name but does not
	 *   point to the same mft reference we know the target was renamed and
	 *   another inode was created with the same name.  In this case we
	 *   return EEXIST which is what HFS+ does, too.
	 */
	err = ntfs_lookup_inode_by_name(dst_dir_ni, dst_ntfs_name,
			dst_ntfs_name_len, &dst_mref, &dst_name);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to find target name in "
					"directory (error %d).", err);
			goto free_err;
		}
		/*
		 * The destination name does not exist in the destination
		 * parent directory which means that the target must have been
		 * renamed to something else before we took the locks.  We
		 * treat this the same as if had been deleted, i.e. we pretend
		 * the caller did not specify a target.
		 */
		if (dst_ni) {
			ntfs_debug("Target %.*s, mft_no 0x%llx has been "
					"renamed, pretending no target was "
					"specified.", (int)dst_cn->cn_namelen,
					dst_cn->cn_nameptr,
					(unsigned long long)dst_ni->mft_no);
			lck_rw_unlock_exclusive(&dst_ni->lock);
			dst_ni = NULL;
		}
	} else /* if (!err) */ {
		/*
		 * The destination name exists in the directory index.
		 *
		 * If the caller did not specify it in @dst_ni or the
		 * destination inode has been deleted (in which case we set
		 * @dst_ni to NULL above) or the target was renamed and another
		 * inode was created with the same name return error EEXIST
		 * which is what HFS+ does, too.
		 *
		 * FIXME: Technically it would probably be more correct to get
		 * the new target ntfs inode and restart the function but at
		 * least for now stick with the same behaviour as HFS+.
		 */
		if (!dst_ni || dst_mref != MK_MREF(dst_ni->mft_no,
				dst_ni->seq_no)) {
			ntfs_debug("Target name %.*s exists but %s, returning "
					"EEXIST.", (int)dst_cn->cn_namelen,
					dst_cn->cn_nameptr, !dst_ni ?
					"target inode was not specified or it "
					"was already deleted" :
					"does not match specified target "
					"inode (it must have been renamed and "
					"a new inode created with the same "
					"name)");
			err = EEXIST;
			goto free_err;
		}
		/*
		 * We still need the destination name thus use a new variable
		 * to store the correctly cased target name.
		 */
		if (!dst_name) {
			target_ntfs_name = dst_ntfs_name;
			target_ntfs_name_len = dst_ntfs_name_len;
			target_ntfs_name_type = 0;
		} else {
			target_ntfs_name = dst_name->name;
			target_ntfs_name_len = dst_name->len;
			target_ntfs_name_type = dst_name->type;
		}
		/*
		 * We have verified everything to do with the target.  We now
		 * need to unlink it unless the source and the target are the
		 * same, i.e. we are changing the case of an existing filename.
		 * We need to distinguish two cases.  If the volume is mounted
		 * case sensitive or it is not case sensitive and the source
		 * and destination names do not match (i.e. they are different
		 * hard links to the same inode) we do not proceed and return
		 * success (this is required by POSIX).  Otherwise the volume
		 * is not case sensitive and the source and destination names
		 * match (i.e. they are the same hard link) and we can either
		 * return success when the source and destination names are
		 * identical (same case) or we can proceed with the rename when
		 * the case differs.
		 *
		 * Note we have caught the case of the inodes being equal and
		 * the volume being mounted case sensitive earlier on so we now
		 * know that the volume is not mounted case sensitive.
		 */
		if (src_ni == dst_ni) {
			/*
			 * If the two names are not the same hardlink return
			 * success not doing anything as required by POSIX.
			 *
			 * Note we do not need to care about case when
			 * comparing because we are comparing the correctly
			 * cased names.
			 */
			if (src_ntfs_name_len != target_ntfs_name_len ||
					bcmp(src_ntfs_name, target_ntfs_name,
					src_ntfs_name_len * sizeof(ntfschar))) {
				ntfs_debug("Source and target inodes are the "
						"same but the source and "
						"target names are different "
						"hard links.  Returning "
						"success without doing "
						"anything as required by "
						"POSIX.");
				goto done;
			}
			/*
			 * The names are the same hard link.  If the existing
			 * name is the same as the destination name (i.e. the
			 * target name before case correction) there is
			 * nothing to do and we can return success.
			 */
			if (src_ntfs_name_len == dst_ntfs_name_len &&
					!bcmp(src_ntfs_name, dst_ntfs_name,
					src_ntfs_name_len * sizeof(ntfschar))) {
				ntfs_debug("Source and destination are "
						"identical so no need to do "
						"anything.  Returning "
						"success.");
				goto done;
			}
			/*
			 * The names are the same hard link but they differ in
			 * case thus there is no target to be removed as it
			 * will be removed as part of the actual rename when
			 * the source name is removed.
			 */
		} else /* if (dst_ni && src_ni != dst_ni) */ {
			/*
			 * The source and the target are not the same thus now
			 * unlink the target.  We can do this atomically before
			 * adding the new entry because both the parent
			 * directory inode and the target inode are locked for
			 * writing thus no-one can access either until we have
			 * finished.  FIXME: The only pitfal is what happens if
			 * the rename fails after we have removed the target?
			 * We just ignore this problem for now and let the
			 * target disappear.  This is what HFS does also so at
			 * least we are not the only non-POSIX conformant file
			 * system on OS X...  In fact as long as we return EIO
			 * on error once we have unlinked the target POSIX
			 * still considers this ok.  (This is what HFS does,
			 * too.)
			 *
			 * Note we do not set @is_rename to true here as this
			 * is just a normal unlink operation.
			 */
			err = ntfs_unlink_internal(dst_dir_ni, dst_ni,
					target_ntfs_name, target_ntfs_name_len,
					target_ntfs_name_type, FALSE);
			if (err) {
				ntfs_error(vol->mp, "Rename failed because "
						"the target mft_no 0x%llx "
						"could not be removed from "
						"directory mft_no 0x%llx "
						"(error %d).",
						(unsigned long long)
						dst_ni->mft_no,
						(unsigned long long)
						dst_dir_ni->mft_no, err);
				goto free_err;
			}
			/*
			 * Set @have_unlinked to true so that we know that we
			 * have to return error EIO from now on if we fail to
			 * complete the rename.
			 */
			have_unlinked = TRUE;
		}
		/*
		 * Release the lock on the destination inode and set it to NULL
		 * so we assume it does not exist from now on.
		 */
		lck_rw_unlock_exclusive(&dst_ni->lock);
		dst_ni = NULL;
	}
	/*
	 * We dealt with the target if there was one thus now we can begin the
	 * actual rename.
	 *
	 * To start with we lock the source inode for writing which allows us
	 * to split the removal of the source name and the addition of the
	 * destination name into two events.
	 *
	 * Note we cheat a little and set @dst_ni to @src_ni so that @src_ni is
	 * unlocked at the end of the function/on error.
	 */
	if (dst_ni)
		panic("%s(): dst_ni\n", __FUNCTION__);
	dst_ni = src_ni;
	lck_rw_lock_exclusive(&src_ni->lock);
	/*
	 * As the source inode is now locked for writing we can perform the
	 * rename in two stages.  First we remove the source name and then we
	 * add the destination name both to the mft record of the inode and to
	 * the parent directory indexes.  We can do this atomically because
	 * both the parent directory and the source inode are locked for
	 * writing thus no-one can access either until we are finished.
	 *
	 * As removal of the source name can leave the source inode with a zero
	 * link count we artificially increment the link count here to ensure
	 * it cannot reach zero.  This is required to guarantee that the unlink
	 * of the source name will remove the filename attribute and to ensure
	 * that the object id is not deleted.  Finally, this also ensures
	 * no-one can ever see the inode in a deleted state (although this
	 * should never happen anyway as we have the inode locked for writing).
	 *
	 * Note the link count in the ntfs inode is unsigned int type, i.e. at
	 * least 32-bit, to allow us to overflow 16-bits here if needed.  In
	 * this way we do not need to worry about the link count overflowing
	 * here which makes the code simpler.
	 *
	 * We set @is_rename to true as we have elevated the link count by one.
	 */
	src_ni->link_count++;
	err = ntfs_unlink_internal(src_dir_ni, src_ni, src_ntfs_name,
			src_ntfs_name_len, src_ntfs_name_type, TRUE);
	if (err) {
		ntfs_error(vol->mp, "Rename failed because the source name, "
				"%.*s mft_no 0x%llx could not be removed from "
				"directory mft_no 0x%llx (error %d).",
				(int)src_cn->cn_namelen, src_cn->cn_nameptr,
				(unsigned long long)src_ni->mft_no,
				(unsigned long long)src_dir_ni->mft_no, err);
		goto dec_err;
	}
	/*
	 * The source name is now removed both from the source parent directory
	 * index and from the mft record of the source inode.
	 *
	 * Now add the destination name as a hard link to the mft record of the
	 * source inode and to the destination parent directory index.
	 *
	 * Calling ntfs_link_internal() also sets the "needs to be archived"
	 * bit on the ntfs inode unless we are renaming an unencrypted
	 * directory inode so we do not need to worry about setting it
	 * ourselves.
	 */
	err = ntfs_link_internal(src_ni, dst_dir_ni, dst_cn, TRUE,
			dst_ntfs_name, dst_ntfs_name_len);
	if (err)
		goto link_err;
	/* We are done, decrement the link count back to its correct value. */
	src_ni->link_count--;
done:
	if (src_name)
		IOFreeType(src_name, ntfs_dir_lookup_name);
	if (dst_name)
		IOFreeType(dst_name, ntfs_dir_lookup_name);
	IOFreeData(ntfs_name_buf, NTFS_MAX_NAME_LEN * 2);
err:
	/* If the destination inode existed we locked it so unlock it now. */
	if (dst_ni)
		lck_rw_unlock_exclusive(&dst_ni->lock);
	/* Drop the source and destination parent directory inode locks. */
	lck_rw_unlock_exclusive(&src_dir_ni->lock);
	if (src_dir_ni != dst_dir_ni) {
		lck_rw_unlock_exclusive(&dst_dir_ni->lock);
		lck_mtx_unlock(&vol->rename_lock);
	}
	ntfs_debug("Done (error %d).", (int)err);
	return err;
link_err:
	ntfs_error(vol->mp, "Rename failed because the destination name %.*s, "
			"mft_ni 0x%llx could not be added to directory mft_no "
			"0x%llx (error %d).", (int)dst_cn->cn_namelen,
			dst_cn->cn_nameptr, (unsigned long long)src_ni->mft_no,
			(unsigned long long)dst_dir_ni->mft_no, err);
	/*
	 * Try to roll back the unlink of the source by creating a new hard
	 * link with the old name.
	 */
	err2 = ntfs_link_internal(src_ni, src_dir_ni, src_cn, TRUE,
			orig_ntfs_name, orig_ntfs_name_len);
	if (err2) {
		ntfs_error(vol->mp, "Failed to roll back partially completed "
				"rename (error %d).  Leaving corrupt "
				"metadata and returning EIO.  Unmount and run "
				"chkdsk.", err2);
		NVolSetErrors(vol);
		err = EIO;
	} else
		ntfs_debug("Re-linking of source name succeeded.");
dec_err:
	src_ni->link_count--;
free_err:
	if (have_unlinked) {
		/* We unlinked an existing target, need to re-link it now. */
		ntfs_debug("Rename failed but the target was already unlinked "
				"and relinking it is not implemented (yet), "
				"returning EIO.  (Given you were renaming "
				"over it chances are you did not care about "
				"the target anyway.)");
		err = EIO;
	}
	goto done;
}

/**
 * ntfs_vnop_mkdir - create a directory
 * @a:		arguments to mkdir function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory in which to create the dir
 *	vnode_t *a_vpp;			destination pointer for the created dir
 *	struct componentname *a_cnp;	name of the directory to create
 *	struct vnode_attr *a_vap;	attributes to set on the created dir
 *	vfs_context_t a_context;
 *
 * Create a directory with name as specified in @a->a_cnp in the directory
 * specified by the vnode @a->a_dvp.  Assign the attributes @a->a_vap to the
 * created directory.  Finally return the vnode of the created directory in
 * *@a->a_vpp.
 *
 * Return 0 on success and errno on error.
 *
 * Note we always create directory names in the POSIX namespace.
 */
static int ntfs_vnop_mkdir(struct vnop_mkdir_args *a)
{
	errno_t err;
#ifdef DEBUG
	ntfs_inode *ni = NTFS_I(a->a_dvp);

	if (ni)
		ntfs_debug("Creating a directory named %.*s in directory "
				"mft_no 0x%llx.", (int)a->a_cnp->cn_namelen,
				a->a_cnp->cn_nameptr,
				(unsigned long long)ni->mft_no);
#endif
	err = ntfs_create(a->a_dvp, a->a_vpp, a->a_cnp, a->a_vap, FALSE);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_rmdir - remove an empty directory
 * @a:		arguments to rmdir function
 *
 * @a contains:
 *	vnode_t a_dvp;			parent directory remove from
 *	vnode_t a_vp;			directory to remove
 *	struct componentname *a_cnp;	name of the dircetory to remove
 *	vfs_context_t a_context;
 *
 * Make sure that the directory with vnode @a->a_vp and name as specified in
 * @a->a_cnp is empty and if so remove it from its parent directory with vnode
 * @a->a_dvp.
 *
 * Return 0 on success and errno on error.
 *
 * Note that if the name of the directory to be removed is in the WIN32 or DOS
 * namespaces, both the WIN32 and the corresponding DOS names are removed.
 *
 * Note that this function only removes the directory entry, i.e. it does not
 * remove the name, however it does decrement the hard link count to zero.
 * This is so that the directory can be undeleted and its original name
 * restored.  In any case, we do not actually delete the inode here as it may
 * still be open and UNIX semantics require an unlinked inode to be still
 * accessible through already opened file descriptors.  When the last file
 * descriptor is closed, we causes the inode to be deleted when the VFS
 * notifies us of the last close by calling VNOP_INACTIVE(), i.e.
 * ntfs_vnop_inactive().
 */
static int ntfs_vnop_rmdir(struct vnop_rmdir_args *a)
{
	ntfs_inode *dir_ni = NTFS_I(a->a_dvp);
	ntfs_inode *ni = NTFS_I(a->a_vp);
	errno_t err;

	ntfs_debug("Entering.");
	if (!dir_ni || !ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	err = ntfs_unlink(dir_ni, ni, a->a_cnp, 0, TRUE);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_symlink - create a symbolic link
 * @a:		arguments to symlink function
 *
 * @a contains:
 *	vnode_t a_dvp;			directory to create the symlink in
 *	vnode_t *a_vpp;			destination pointer for the new symlink
 *	struct componentname *a_cnp;	name of the symlink to create
 *	struct vnode_attr *a_vap;	attributes to set on the new symlink
 *	char *a_target;			path to point the created symlink at
 *	vfs_context_t a_context;
 *
 * Create a symbolic link to the path string @a->a_target with name as
 * specified in @a->a_cnp in directory specified by the vnode @a->a_dvp.
 * Assign the attributes @a->a_vap to the created symlink.  Finally return the
 * vnode of the created symlink in *@a->a_vpp.
 *
 * We implement symbolic links the same way as SFM, i.e. a symbolic link is a
 * regular file as far as NTFS is concerned with an AFP_AfpInfo named stream
 * containing the finder info with the type set to 'slnk' and the creator set
 * to 'rhap'.  This is basically how HFS+ stores symbolic links, too.
 *
 * Return 0 on success and errno on error.
 *
 * Note, since IEEE Std 1003.1-2001 does not require any association of file
 * times with symbolic links, there is no requirement that file times be
 * updated by symlink(). - This is what POSIX says about updating times in
 * symlink() thus we do not update any of the times except as an indirect
 * result of calling ntfs_write() on the symbolic link inode.
 */
static int ntfs_vnop_symlink(struct vnop_symlink_args *a)
{
	uio_t uio;
	ntfs_inode *dir_ni, *ni, *raw_ni;
	int err, err2;
	unsigned len;

	dir_ni = NTFS_I(a->a_dvp);
	if (!dir_ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Creating a symbolic link named %.*s in directory mft_no "
			"0x%llx and pointing it at path \"%s\".",
			(int)a->a_cnp->cn_namelen, a->a_cnp->cn_nameptr,
			(unsigned long long)dir_ni->mft_no, a->a_target);
	len = strlen(a->a_target);
	/* Zero length symbolic links are not allowed. */
	if (!len || len > MAXPATHLEN) {
		err = EINVAL;
		if (len)
			err = ENAMETOOLONG;
		ntfs_error(dir_ni->vol->mp, "Invalid symbolic link target "
				"length %d, returning %s.", len,
				len ? "ENAMETOOLONG" : "EINVAL");
		return err;
	}
retry:
	/* Create the symbolic link inode. */
	err = ntfs_create(dir_ni->vn, a->a_vpp, a->a_cnp, a->a_vap, TRUE);
	if (err) {
		if (err != EEXIST)
			ntfs_error(dir_ni->vol->mp, "Failed to create "
					"symbolic link named %.*s in "
					"directory mft_no 0x%llx and pointing "
					"to path \"%s\" (error %d).",
					(int)a->a_cnp->cn_namelen,
					a->a_cnp->cn_nameptr,
					(unsigned long long)dir_ni->mft_no,
					a->a_target, err);
		else
			ntfs_debug("Failed to create symbolic link named %.*s "
					"in directory mft_no 0x%llx and "
					"pointing to path \"%s\" (error "
					"EEXIST).", (int)a->a_cnp->cn_namelen,
					a->a_cnp->cn_nameptr,
					(unsigned long long)dir_ni->mft_no,
					a->a_target);
		return err;
	}
	/* Note the ntfs inode @ni is locked for writing. */
	ni = NTFS_I(*a->a_vpp);
	/* Make sure no-one deleted it under our feet. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		/* Release the vnode and try the create again. */
		lck_rw_unlock_exclusive(&ni->lock);
		vnode_put(ni->vn);
		goto retry;
	}
	/*
	 * Create a uio and attach the target path to it so we can use
	 * ntfs_write() to do the work.
	 */
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
	if (!uio) {
		err = ENOMEM;
		ntfs_error(dir_ni->vol->mp, "Failed to allocate UIO.");
		goto err;
	}
	err = uio_addiov(uio, (uintptr_t)a->a_target, len);
	if (err)
		panic("%s(): Failed to attach target path buffer to UIO "
				"(error %d).", __FUNCTION__, err);
	/*
	 * FIXME: At present the kernel does not allow VLNK vnodes to use the
	 * UBC (<rdar://problem/5794900>) thus we need to use a shadow VREG
	 * vnode to do the actual write of the symbolic link data.  Fortunately
	 * we already implemented this functionality for compressed files where
	 * we need to read the compressed data using a shadow vnode so we use
	 * the same implementation here, thus our shadow vnode is a raw inode.
	 */
	err = ntfs_raw_inode_get(ni, LCK_RW_TYPE_EXCLUSIVE, &raw_ni);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get raw inode (error %d).",
				err);
		goto err;
	}
	if (!NInoRaw(raw_ni))
		panic("%s(): Requested raw inode but got non-raw one.\n",
				__FUNCTION__);
	/*
	 * Write the symbolic link target to the created inode.  We pass in
	 * IO_UNIT as we want an atomic i/o operation.
	 *
	 * FIXME: ntfs_write() does not always honour the IO_UNIT flag so we
	 * still have to test for partial writes.
	 */
	err = ntfs_write(raw_ni, uio, IO_UNIT, TRUE);
	/*
	 * Update the sizes in the base inode.  Note there is no need to lock
	 * @raw_ni->size_lock as the values cannot change at present as we are
	 * holding the inode lock @raw_ni->lock for write.
	 */
	lck_spin_lock(&ni->size_lock);
	ni->initialized_size = raw_ni->initialized_size;
	ni->data_size = raw_ni->data_size;
	ni->allocated_size = raw_ni->allocated_size;
	ni->compressed_size = raw_ni->compressed_size;
	lck_spin_unlock(&ni->size_lock);
	if (NInoNonResident(raw_ni))
		NInoSetNonResident(ni);
	lck_rw_unlock_exclusive(&raw_ni->lock);
	vnode_put(raw_ni->vn);
	/* Check for write errors. */
	if (uio_resid(uio) && !err)
		err = EIO;
	/* We no longer need the uio. */
	uio_free(uio);
	if (!err) {
		lck_rw_unlock_exclusive(&ni->lock);
		ntfs_debug("Done.");
		return 0;
	}
	/* Write failed or was partial, unlink the created symbolic link. */
	ntfs_error(dir_ni->vol->mp, "Failed to write target path to symbolic "
			"link inode (error %d).", err);
err:
	lck_rw_unlock_exclusive(&ni->lock);
	err2 = ntfs_unlink(dir_ni, ni, a->a_cnp, 0, FALSE);
	if (err2) {
		ntfs_error(dir_ni->vol->mp, "Failed to unlink symbolic link "
				"inode in error code path (error %d).  Run "
				"chkdsk.", err2);
		NVolSetErrors(dir_ni->vol);
	}
	vnode_put(ni->vn);
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
 *	u64 d_ino;			inode number of entry
 *	u64 d_seekoff;			seek offset (optional, used by servers)
 *	u16 d_reclen;			length of this record
 *	u16 d_namlen;			length of string in d_name
 *	u8 d_type;			inode type (one of DT_DIR, DT_REG, etc)
 *	char d_name[MAXPATHLEN];	null terminated filename
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
	user_ssize_t start_count;
	ntfs_inode *dir_ni = NTFS_I(a->a_vp);
	errno_t err;

	if (!dir_ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
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
		ntfs_debug("Not a directory, returning ENOTDIR.");
		return ENOTDIR;
	}
	if (a->a_flags) {
		ntfs_error(dir_ni->vol->mp, "None of the VNODE_READDIR_* "
				"flags are supported yet, sorry.");
		return ENOTSUP;
	}
	lck_rw_lock_shared(&dir_ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(dir_ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(dir_ni->vn);
		lck_rw_unlock_shared(&dir_ni->lock);
		ntfs_debug("Directory is deleted.");
		return ENOENT;
	}
	start_count = uio_resid(a->a_uio);
	err = ntfs_readdir(dir_ni, a->a_uio, a->a_eofflag, a->a_numdirent);
	/*
	 * Update the last_access_time (atime) if something was read.
	 *
	 * Skip the update if atime updates are disabled via the noatime mount
	 * option or the volume is read only.
	 */
	if (uio_resid(a->a_uio) < start_count && !NVolReadOnly(dir_ni->vol) &&
			!(vfs_flags(dir_ni->vol->mp) & MNT_NOATIME)) {
		dir_ni->last_access_time = ntfs_utc_current_time();
		NInoSetDirtyTimes(dir_ni);
	}
	lck_rw_unlock_shared(&dir_ni->lock);
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
 * ntfs_vnop_readlink - read the contents of a symbolic link
 * @a:		arguments to readlink function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode of symbolic link whose data to read
 *	uio_t *a_uio;		destination in which to return the read data
 *	vfs_context_t a_context;
 *
 * Read the path stored in the symbolic link vnode @a->a_vp and return it in
 * the destination buffer pointed to by @a->a_uio.
 *
 * uio_resid(@a->a_uio) is the maximum number of bytes to read and
 * uio_offset(@a->a_uio) must be zero.
 *
 * We implement symbolic links the same way as SFM, i.e. a symbolic link is a
 * regular file as far as NTFS is concerned with an AFP_AfpInfo named stream
 * containing the finder info with the type set to 'slnk' and the creator set
 * to 'rhap'.  This is basically how HFS+ stores symbolic links, too.
 *
 * Thus obtaining the symbolic link target is a simple matter of calling
 * ntfs_read() on the symbolic link inode.
 *
 * TODO: We may wish to add support for other symbolic link types found on NTFS
 * volumes such as the methods used by:
 *	- Windows Services for Unix (SFU) and the userspace ntfsmount driver,
 *	- SMB/Samba (when run on a file system without native symbolic links)
 *	- Cygwin
 *
 * It may also be worth supporting reparse point based symbolic links but those
 * are a lot trickier if at all possible as they contain information that
 * cannot be resolved without access to the Windows registry and potentially
 * without access to the Windows Domain/Active Directory.
 *
 * Return 0 on success and errno on error.
 *
 * Note, since IEEE Std 1003.1-2001 does not require any association of file
 * times with symbolic links, there is no requirement that file times be
 * updated by readlink().
 */
static int ntfs_vnop_readlink(struct vnop_readlink_args *a)
{
	s64 size;
	user_ssize_t start_count;
	ntfs_inode *ni, *raw_ni;
	uio_t uio = a->a_uio;
	errno_t err;

	ni = NTFS_I(a->a_vp);
	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also and against deletion/rename.
	 */
	lck_rw_lock_shared(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (!ni->link_count || NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
	if (!S_ISLNK(ni->mode)) {
		ntfs_debug("Not a symbolic link, returning EINVAL.");
		err = EINVAL;
		goto err;
	}
	if (uio_offset(uio)) {
		ntfs_error(ni->vol->mp, "uio_offset(uio) is not zero, "
				"returning EINVAL.");
		err = EINVAL;
		goto err;
	}
	/*
	 * FIXME: At present the kernel does not allow VLNK vnodes to use the
	 * UBC (<rdar://problem/5794900>) thus we need to use a shadow VREG
	 * vnode to do the actual read of the symbolic link data.  Fortunately
	 * we already implemented this functionality for compressed files where
	 * we need to read the compressed data using a shadow vnode so we use
	 * the same implementation here, thus our shadow vnode is a raw inode.
	 *
	 * Doing this has the unfortunate consequence that if the symbolic link
	 * inode is compressed or encrypted we cannot read it as we are already
	 * using the raw inode and we can only have one raw inode.
	 */
	lck_spin_lock(&ni->size_lock);
	size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	/* Zero length symbolic links are not allowed. */
	if (!size || size > MAXPATHLEN) {
		ntfs_error(ni->vol->mp, "Invalid symbolic link size %lld in "
				"mft_no 0x%llx, returning EINVAL.",
				(long long)size,
				(unsigned long long)ni->mft_no);
		err = EINVAL;
		goto err;
	}
	start_count = uio_resid(uio);
	err = ntfs_raw_inode_get(ni, LCK_RW_TYPE_SHARED, &raw_ni);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get raw inode (error %d).",
				err);
		goto err;
	}
	if (!NInoRaw(raw_ni))
		panic("%s(): Requested raw inode but got non-raw one.\n",
				__FUNCTION__);
	lck_spin_lock(&raw_ni->size_lock);
	if (size > ubc_getsize(raw_ni->vn) || size != raw_ni->data_size)
		panic("%s(): size (0x%llx) > ubc_getsize(raw_ni->vn, 0x%llx) "
				"|| size != raw_ni->data_size (0x%llx)\n",
				__FUNCTION__, (unsigned long long)size,
				(unsigned long long)ubc_getsize(raw_ni->vn),
				(unsigned long long)raw_ni->data_size);
	lck_spin_unlock(&raw_ni->size_lock);
	/* Perform the actual read of the symbolic link data into the uio. */
	err = ntfs_read(raw_ni, uio, 0, TRUE);
	lck_rw_unlock_shared(&raw_ni->lock);
	vnode_put(raw_ni->vn);
	/*
	 * If the read was partial, reset @uio pretending that the read never
	 * happened unless we used up all the space in the uio and it was
	 * simply not big enough to hold the entire symbolic link data in which
	 * case we return a truncated result.
	 */
	if (err || (uio_resid(uio) && start_count - uio_resid(uio) != size)) {
		/*
		 * FIXME: Should we be trying to continue a partial read in
		 * case we can complete it with multiple calls to ntfs_read()?
		 */
		if (!err) {
			ntfs_debug("ntfs_read() returned a partial read, "
					"pretending the read never happened.");
			err = EIO;
		}
		uio_setoffset(uio, 0);
		uio_setresid(uio, start_count);
		if (err)
			ntfs_error(ni->vol->mp, "Failed to read symbolic link "
					"data (error %d).", err);
	}
	ntfs_debug("Done (error %d).", (int)err);
err:
	lck_rw_unlock_shared(&ni->lock);
	return err;
}

/**
 * ntfs_mft_record_free_all - free clusters referenced by an mft record
 * @base_ni:	base ntfs inode to which the (extent) inode @ni and @m belong
 * @ni:		ntfs inode for which to free all clusters
 * @m:		mft record for which to free all clusters
 *
 * For the ntfs inode @ni and its mft record @m, iterate over all attributes in
 * the mft record and free all clusters referenced by the attributes.  @base_ni
 * is the base ntfs inode to which @ni and @m belong.
 *
 * Also, mark the mft record as not in use, increment its sequence number and
 * mark it dirty to ensure it gets written out later.
 *
 * When any operations fail this function notifies the user about it and marks
 * the volume dirty but does not return an error code as the caller can proceed
 * regardless without caring if some clusters failed to be freed.  A later
 * chkdsk will find them and free them and in the mean time they just waste
 * some space on the volume.
 */
static void ntfs_mft_record_free_all(ntfs_inode *base_ni, ntfs_inode *ni,
		MFT_RECORD *m)
{
	ntfs_volume *vol = base_ni->vol;
	ATTR_RECORD *a;
	errno_t err;
	ntfs_runlist rl;

	for (a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
			a->type != AT_END;
			a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length))) {
		if ((u8*)a < (u8*)m || (u8*)a > (u8*)m +
				le32_to_cpu(m->bytes_in_use) ||
				le32_to_cpu(m->bytes_in_use) >
				le32_to_cpu(m->bytes_allocated) ||
				!a->length) {
			ntfs_warning(vol->mp, "Found corrupt attribute whilst "
					"releasing deleted mft_no 0x%llx.  "
					"Run chkdsk to recover lost space and "
					"fix any other inconsistencies.",
					(unsigned long long)ni->mft_no);
			NVolSetErrors(vol);
			break;
		}
		/*
		 * For most resident attribute records, there is nothing we
		 * need to do as they do not reference any clusters outside the
		 * mft record itself.
		 */
		if (!a->non_resident) {
			STANDARD_INFORMATION *si;

			/*
			 * We only need to deal with the standard information
			 * attribute.
			 */
			if (a->type != AT_STANDARD_INFORMATION)
				continue;
			/*
			 * We need to update the {a,m,c}times from the ntfs
			 * inode into the corresponding times in the standard
			 * information attribute.  The inode ctime, i.e. the
			 * last_mft_change_time in the standard information
			 * attribute, gives us a de facto deleted time that can
			 * be used by ntfsck and ntfsundelete for example.
			 */
			si = (STANDARD_INFORMATION*)((u8*)a +
					le16_to_cpu(a->value_offset));
			si->last_data_change_time = utc2ntfs(
					base_ni->last_data_change_time);
			si->last_mft_change_time = utc2ntfs(
					base_ni->last_mft_change_time);
			si->last_access_time = utc2ntfs(
					base_ni->last_access_time);
			/* Whilst here also update the file attributes. */
			si->file_attributes = base_ni->file_attributes;
			/*
			 * We need to take care to handle NTFS 1.x style
			 * standard information attributes on NTFS 3.0+ volumes
			 * as they are lazily updated on write after a volume
			 * has been upgraded from 1.x and after a volume has
			 * been accessed by an older NTFS driver such as the
			 * one in Windows NT4.
			 */
#if 0
			if (vol->major_ver <= 3 ||
					le32_to_cpu(a->value_length) <
					sizeof(STANDARD_INFORMATION))
				continue;
#endif
			/*
			 * We have an NTFS 3.0+ style, extended standard
			 * information attribute.
			 */
			/*
			 * TODO: When we implement support for $UsnJrnl, we
			 * will need to journal the delete event and update the
			 * usn field in the standard information attribute.
			 * For now this is not needed as we stamp the
			 * transaction log thus telling applications querying
			 * the transaction log that it does not contain
			 * uptodate information.  We cannot do this at unlink
			 * time because there may still be writes and truncates
			 * happening due to existing open file descriptors and
			 * the delete event has to come last.
			 */
			/*
			 * TODO: When we implement support for quotas, we will
			 * need to update the quota control entry belonging to
			 * the user_id specified in the owner_id field in the
			 * standard information attribute by updating its
			 * change_time field to the current time and
			 * decrementing its bytes_used field by the amount
			 * specified in the quota_charged field in the standard
			 * information attribute as well as setting the
			 * exceeded_time to 0 if we go from over the soft quota
			 * specified in the limit of the quota control entry.
			 * For now this is not needed as we mark all quotas as
			 * invalid when we mount a volume read-write.  We
			 * cannot do the quota update at unlink time because
			 * there may still be writes and truncates happening
			 * due to existing open file descriptors which will
			 * affect the quota related fields.
			 */
			continue;
		}
		/*
		 * For non-resident attribute records, we need to free all the
		 * clusters specified in their mapping pairs array.
		 *
		 * If this is the base extent, we only need to do this if the
		 * allocated size is not zero.  If this is not the base extent
		 * then by definition the allocated size cannot be zero and
		 * more importantly an extent mft rceord does not have the
		 * allocated_size field set thus it is always zero.
		 */
		if (!a->lowest_vcn && !a->allocated_size)
			continue;
		rl.rl = NULL;
		rl.alloc_count = rl.elements = 0;
		err = ntfs_mapping_pairs_decompress(vol, a, &rl);
		if (!err) {
			VCN lowest_vcn;

			/*
			 * We need to supply the correct start and count values
			 * otherwise freeing the clusters fails when an
			 * attribute has multiple extent records because the
			 * runlist contains unmapped elements.
			 */
			lowest_vcn = sle64_to_cpu(a->lowest_vcn);
			err = ntfs_cluster_free_from_rl(vol, rl.rl, lowest_vcn,
					sle64_to_cpu(a->highest_vcn) + 1 -
					lowest_vcn, NULL);
			if (err) {
				ntfs_warning(vol->mp, "Failed to free some "
						"allocated clusters belonging "
						"to mft_no 0x%llx (error "
						"%d).  Run chkdsk to recover "
						"the lost space.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
			}
			IODeleteData(rl.rl, ntfs_rl_element, rl.alloc_count);
		} else {
			ntfs_error(vol->mp, "Cannot free some allocated space "
					"belonging to mft_no 0x%llx because "
					"the decompression of the mapping "
					"pairs array failed (error %d).  Run "
					"chkdsk to recover the lost space.",
					(unsigned long long)ni->mft_no, err);
			NVolSetErrors(vol);
		}
	}
	/*
	 * We have processed all attributes in the base mft record thus we can
	 * mark it as not in use, increment its sequence number, and mark it
	 * dirty for later writeout.
	 */
	m->flags &= ~MFT_RECORD_IN_USE;
	if (m->sequence_number != const_cpu_to_le16(0xffff))
		m->sequence_number = cpu_to_le16(
				le16_to_cpu(m->sequence_number) + 1);
	else
		m->sequence_number = const_cpu_to_le16(1);
	ni->seq_no = le16_to_cpu(m->sequence_number);
	NInoSetMrecNeedsDirtying(ni);
}

/**
 * ntfs_vnop_inactive - the last reference to a vnode has been dropped
 * @args:	arguments to inactive function
 *
 * @args contains:
 *	vnode_t a_vp;		vnode whose last reference has been dropped
 *	vfs_context_t a_context;
 *
 * Last reference to a vnode has been dropped or a forced unmount is in
 * progress.
 *
 * Note: When called from reclaim, the vnode has a zero v_iocount and
 *	 v_usecount and vnode_isrecycled() is true.
 *
 * Return 0 on success and errno on error.
 *
 * Note the current OS X VFS ignores the return value from VNOP_INACTIVE() and
 * hence ntfs_vnop_inactive().
 */
static int ntfs_vnop_inactive(struct vnop_inactive_args *args)
{
	leMFT_REF mref;
	vnode_t vn = args->a_vp;
	ntfs_inode *base_ni, *mftbmp_ni, *ni = NTFS_I(vn);
	ntfs_volume *vol;
	MFT_RECORD *m;
	leMFT_REF *mrefs;
	unsigned nr_mrefs;
	errno_t err;
	BOOL is_delete;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return 0;
	}
	is_delete = !ni->link_count;
	vol = ni->vol;
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x%s.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type), (unsigned)ni->name_len,
			is_delete ? ", is delete" : "");
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/*
	 * This is the last close thus remove any directory hints.
	 *
	 * Note we check for presence of directory hints outside the locks as
	 * an optimization.  It is not a disaster if we miss any as all will be
	 * released in ntfs_inode_free() before the inode is thrown away at the
	 * latest.
	 */
	if (ni != base_ni && ni->type == AT_INDEX_ALLOCATION &&
			ni->nr_dirhints) {
		lck_rw_lock_exclusive(&ni->lock);
		ntfs_dirhints_put(ni, 0);
		lck_rw_unlock_exclusive(&ni->lock);
	}
	/*
	 * If the inode is not being deleted or this is a raw inode sync it and
	 * we are done.
	 */
	if (!is_delete || NInoRaw(ni)) {
sync:
		/*
		 * Commit dirty data to disk unless mounted read-only.
		 *
		 * WARNING: Please see <rdar://problem/7202356> why this causes
		 * stack exhaustion and kernel panics by creating a loop where
		 * the VNOP_INACTIVE() calls ntfs_inode_sync() which ends up
		 * doing ntfs_inode_get() which in turn triggers another
		 * VNOP_INACTIVE() which in turn calls ntfs_inode_sync() and
		 * thus ntfs_inode_get() which in turns calls VNOP_INACTIVE()
		 * and so on until the stack overflows.
		 */
		err = 0;
		if (!NVolReadOnly(vol))
			err = ntfs_inode_sync(ni, IO_SYNC | IO_CLOSE, FALSE);
		if (!err)
			ntfs_debug("Done.");
		else
			ntfs_error(vol->mp, "Failed to sync mft_no 0x%llx, "
					"type 0x%x, name_len 0x%x (error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type),
					(unsigned)ni->name_len, err);
		return err;
	}
	if (ni != base_ni)
		lck_rw_lock_exclusive(&base_ni->lock);
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(vn);
		lck_rw_unlock_exclusive(&ni->lock);
		if (ni != base_ni)
			lck_rw_unlock_exclusive(&base_ni->lock);
		ntfs_debug("Done (was already deleted).");
		return 0;
	}
	/*
	 * If someone else re-instantiated the inode whilst we were waiting for
	 * the inode lock sync the inode instead of deleting it.
	 */
	if (ni->link_count) {
		lck_rw_unlock_exclusive(&ni->lock);
		if (ni != base_ni)
			lck_rw_unlock_exclusive(&base_ni->lock);
		ntfs_debug("Someone re-instantiated the inode.");
		goto sync;
	}
	/*
	 * The inode has been unlinked, delete it now freeing all allocated
	 * space on disk as well as all related resources on disk.  Note we
	 * proceed on errors because there is not much we can do about them.
	 * We have to carry on regardless as the inode is about to be
	 * terminated in any case.
	 *
	 * On a metadata affecting error, we mark the volume dirty and leave it
	 * to a subsequent chkdsk to clean up after us.  This is not a disaster
	 * since there are no directory entries pointing to the inode @ni any
	 * more, thus us failing just means that we will keep some on disk
	 * resources allocated so chkdsk will just find this file and delete
	 * it.
	 *
	 * First, remove the inode from the inode cache so it cannot be found
	 * any more.
	 */
	lck_mtx_lock(&ntfs_inode_hash_lock);
	/*
	 * Mark the inode as having been deleted so we do not try to remove it
	 * from the ntfs inode hash again in ntfs_inode_reclaim().
	 */
	NInoSetDeleted(ni);
	/*
	 * Remove the ntfs_inode from the inode hash so it cannot be looked up
	 * any more.
	 */
	ntfs_inode_hash_rm_nolock(ni);
	lck_mtx_unlock(&ntfs_inode_hash_lock);
	/* Remove the inode from the name cache if it is still in it. */
	cache_purge(vn);
	/*
	 * The inode/vnode are no longer reachable at all so drop the inode
	 * lock.  Anyone waiting on the lock should test for NInoDeleted() and
	 * abort once they have taken the lock.
	 */
	lck_rw_unlock_exclusive(&ni->lock);
	/* In case someone is waiting on the inode do a wakeup. */
	ntfs_inode_wakeup(ni);
	/* Invalidate all buffers to do with the vnode. */
	err = buf_invalidateblks(vn, 0, 0, 0);
	if (err)
		ntfs_error(vol->mp, "Failed to invalidate cached buffers "
				"(error %d).", err);
	/*
	 * Invalidate all cached pages in the VM.
	 *
	 * This will fail for non-regular (VREG) nodes as they do not have UBC
	 * info attached to them and ubc_msync() returns error in this case.
	 */
	if (vnode_isreg(vn)) {
		err = ubc_msync(vn, 0, ubc_getsize(vn), NULL, UBC_INVALIDATE);
		if (err)
			ntfs_error(vol->mp, "Failed to invalidate cached "
					"pages (error %d).", err);
	}
	/*
	 * Cause the vnode to be reused immediately when we return rather than
	 * sitting around in the vnode cache.
	 */
	vnode_recycle(vn);
	/*
	 * ntfs_unlink() and ntfs_vnop_rename() bail out for attribute inodes
	 * so we cannot get here with an attribute inode unless something has
	 * gone badly wrong.
	 *
	 * When a named stream is deleted via VNOP_REMOVENAMEDSTREAM() its
	 * link_count is set to zero so we get here on the last close.  We have
	 * to perform the actual freeing of allocated space if the attribute is
	 * non-resident as well as the removal of the attribute record here.
	 */
	if (ni != base_ni) {
		ntfs_attr_search_ctx *ctx;

		if (ni->type != AT_DATA || !ni->name_len)
			panic("%s(): ni != base_ni && (ni->type != AT_DATA || "
					"!ni->name_len)\n", __FUNCTION__);
		/*
		 * For simplicity, if the attribute is non-resident, we
		 * truncate the attribute to zero size first as that causes
		 * both the allocated clusters to be freed as well as all
		 * extent attribute records to be deleted.
		 *
		 * We then only need to remove the base attribute record and we
		 * are done.
		 */
		if (NInoNonResident(ni)) {
			err = ntfs_attr_resize(ni, 0, 0, NULL);
			if (err) {
				ntfs_error(vol->mp, "Cannot delete named "
						"stream from mft_no 0x%llx "
						"because truncating the "
						"stream inode to zero size "
						"failed (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				goto err;
			}
		}
		/* Remove the named stream. */
		err = ntfs_mft_record_map(base_ni, &m);
		if (err) {
			ntfs_error(vol->mp, "Failed to delete named stream "
					"because mapping the mft record "
					"0x%llx failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			goto err;
		}
		ctx = ntfs_attr_search_ctx_get(base_ni, m);
		if (!ctx) {
			ntfs_error(vol->mp, "Failed to delete named stream "
					"because allocating an attribute "
					"search context failed.");
			goto unm_err;
		}
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, ctx);
		if (err) {
			ntfs_error(vol->mp, "Failed to delete named stream "
					"because looking up the named $DATA "
					"attribute in the mft record 0x%llx "
					"failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			goto put_err;
		}
		err = ntfs_attr_record_delete(base_ni, ctx);
		if (err) {
			ntfs_error(vol->mp, "Failed to delete named stream "
					"because deleting the named $DATA "
					"attribute from its mft record 0x%llx "
					"failed (error %d).",
					(unsigned long long)ctx->ni->mft_no,
					err);
			goto put_err;
		}
		ntfs_debug("Done (deleted attribute inode).");
put_err:
		ntfs_attr_search_ctx_put(ctx);
unm_err:
		ntfs_mft_record_unmap(base_ni);
err:
		lck_rw_unlock_exclusive(&base_ni->lock);
		return err;
	}
	/*
	 * We only need to be concerned with the allocated space on disk which
	 * we need to deallocate and any related resources on disk, which we
	 * also need to deallocate and/or mark unused.  To do this, we map the
	 * base mft record and iterate over all its attributes and deal with
	 * each of them in sequence.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_warning(vol->mp, "Cannot release deleted mft_no 0x%llx "
				"because the mapping of the base mft record "
				"failed (error %d).  Run chkdsk to recover "
				"lost resources.",
				(unsigned long long)ni->mft_no, err);
		NVolSetErrors(vol);
		return 0;
	}
	/*
	 * Make sure the mft record was marked as not in use in
	 * ntfs_unlink_internal().
	 */
	if (m->flags & MFT_RECORD_IN_USE)
		panic("%s(): m->flags & MFT_RECORD_IN_USE\n", __FUNCTION__);
	/*
	 * We will need the mft reference of the base mft record below but we
	 * are about to change it thus make a note of the old one now.
	 */
	mref = MK_LE_MREF(ni->mft_no, ni->seq_no);
	/*
	 * Release all clusters allocated to attribute records located in the
	 * extent mft record.
	 */
	ntfs_mft_record_free_all(ni, ni, m);
	/*
	 * We are finished with the base mft record, if there is an attribute
	 * list attribute, we iterate over its entries and each time we
	 * encounter an extent mft record that we have not done yet, we map it
	 * and iterate over all its attributes as we did above for the base mft
	 * record, followed by marking the extent mft record as not in use,
	 * incrementing its sequence number, and marking it dirty, again as we
	 * did above for the base mft record.  Finally, we add it to our list
	 * of mft records to deallocate from the $MFT/$BITMAP attribute.
	 *
	 * As an optimization, we reuse the attribute list buffer as our list
	 * of mft records to deallocate from the $MFT/$BITMAP attribute.  This
	 * works because each ATTR_LIST_ENTRY record in the attribute list
	 * attribute is at least 24 bytes long and we only need to store 8
	 * bytes for each mft reference in our list of mft records to
	 * deallocate so we are guaranteed to have enough space in the buffer
	 * for our needs and we are also guaranteed that we will never
	 * overwrite part of the attribute list attribute data that we have not
	 * dealt with yet.
	 */
	nr_mrefs = 1;
	mrefs = &mref;
	if (NInoAttrList(ni)) {
		ATTR_LIST_ENTRY *entry, *next_entry, *end;
		ntfs_inode *eni;

		if (!ni->attr_list || ni->attr_list_size < sizeof(leMFT_REF) ||
				!ni->attr_list_alloc)
			panic("%s(): !ni->attr_list || !ni->attr_list_size || "
					"!ni->attr_list_alloc\n", __FUNCTION__);
		entry = (ATTR_LIST_ENTRY*)ni->attr_list;
		mrefs = (leMFT_REF*)entry;
		next_entry = (ATTR_LIST_ENTRY*)((u8*)entry +
				le16_to_cpu(entry->length));
		end = (ATTR_LIST_ENTRY*)(ni->attr_list + ni->attr_list_size);
		/*
		 * Add the mft reference of the base mft record as the first
		 * element in our list as we have already dealt with it.
		 */
		*mrefs = mref;
		while (entry < end) {
			unsigned i;

			mref = entry->mft_reference;
			for (i = 0; i < nr_mrefs; i++) {
				if (mref == mrefs[i])
					goto do_next;
			}
			/*
			 * This mft reference has not been encountered before.
			 * Add it to the list of mft references and free all
			 * disk storage associated with all the attribute
			 * records stored in the mft record with this mft
			 * reference.
			 */
			mrefs[nr_mrefs++] = mref;
			err = ntfs_extent_mft_record_map(ni, le64_to_cpu(mref),
					&eni, &m);
			if (!err) {
				/*
				 * Release all clusters allocated to attribute
				 * records located in the extent mft record and
				 * mark the mft record as not in use.
				 *
				 * We need to ensure the mft record is marked
				 * as in use.  It can happen that it is not
				 * marked in use after a system crash occurs
				 * whilst a file is being extended.
				 */
				if (m->flags & MFT_RECORD_IN_USE)
					ntfs_mft_record_free_all(ni, eni, m);
				else {
					ntfs_warning(vol->mp, "Extent mft_no "
							"0x%llx, base mft_no "
							"0x%llx is marked as "
							"not in use.  Cannot "
							"release allocated "
							"clusters.  Unmount "
							"and run chkdsk to "
							"recover the lost "
							"clusters.",
							(unsigned long long)
							MREF_LE(mref),
							(unsigned long long)
							ni->mft_no);
					NVolSetErrors(vol);
				}
				/* Unmap the mft record again. */
				ntfs_extent_mft_record_unmap(eni);
			} else {
			     ntfs_warning(vol->mp, "Failed to release "
					     "allocated clusters because "
					     "mapping extent mft_no 0x%llx, "
					     "base mft_no 0x%llx failed "
					     "(error %d).  Unmount and run "
					     "chkdsk to recover the lost "
					     "clusters.",
					     (unsigned long long)MREF_LE(mref),
					     (unsigned long long)ni->mft_no,
					     err);
			     NVolSetErrors(vol);
			}
do_next:
			entry = next_entry;
			next_entry = (ATTR_LIST_ENTRY*)((u8*)entry +
					le16_to_cpu(entry->length));
		}
	}
	ntfs_mft_record_unmap(ni);
	/*
	 * Mark the base mft record and all extent mft records (if any) as
	 * unused in the mft bitmap.
	 *
	 * Note that this means that ntfs_inode_reclaim() may run when someone
	 * else has already reused one of the mft records we are freeing now.
	 * This is ok because all ntfs_inode_reclaim() does is to do some
	 * memory freeing.  And we have already removed the inode from the
	 * inode cache thus there are no problems from that point of view
	 * either.
	 */
	lck_rw_lock_exclusive(&vol->mftbmp_lock);
	mftbmp_ni = vol->mftbmp_ni;
	err = vnode_get(mftbmp_ni->vn);
	if (err)
		ntfs_warning(vol->mp, "Failed to get vnode for $MFT/$BITMAP "
				"(error %d) thus cannot release mft "
				"record(s).  Run chkdsk to recover the lost "
				"mft record(s).", err);
	else {
		lck_rw_lock_shared(&mftbmp_ni->lock);
		while (nr_mrefs > 0) {
			nr_mrefs--;
			err = ntfs_bitmap_clear_bit(mftbmp_ni,
					MREF_LE(mrefs[nr_mrefs]));
			if (!err) {
				/*
				 * We cleared a bit in the mft bitmap thus we
				 * need to reflect this in the cached number of
				 * free mft records.
				 */
				vol->nr_free_mft_records++;
				if (vol->nr_free_mft_records >=
						vol->nr_mft_records)
					panic("%s(): vol->nr_free_mft_records "
							"> vol->nr_mft_records"
							"\n", __FUNCTION__);
			} else {
				ntfs_error(vol->mp, "Failed to free mft_no "
						"0x%llx (error %d).  Run "
						"chkdsk to recover the lost "
						"mft record.",
						(unsigned long long)
						MREF_LE(mrefs[nr_mrefs]), err);
				NVolSetErrors(vol);
			}
		}
		lck_rw_unlock_shared(&mftbmp_ni->lock);
		(void)vnode_put(mftbmp_ni->vn);
	}
	lck_rw_unlock_exclusive(&vol->mftbmp_lock);
	ntfs_debug("Done (deleted base inode).");
	return 0;
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
 * Note: This is called from reclaim.  The vnode has a zero v_iocount and
 *	 v_usecount and vnode_isrecycled() is true.
 *
 * Return 0 on success and errno on error.
 *
 * Note the current OS X VFS panic()s the machine if VNOP_RECLAIM() and hence
 * ntfs_vnop_reclaim() returns an error.
 */
static int ntfs_vnop_reclaim(struct vnop_reclaim_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *ni = NTFS_I(vn);
	errno_t err;

	/* Do not dereference @ni if it is NULL. */
#ifdef DEBUG
	if (ni)
		ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len "
				"0x%x.", (unsigned long long)ni->mft_no,
				le32_to_cpu(ni->type), (unsigned)ni->name_len);
	else
		ntfs_debug("Entering for already reclaimed vnode!");
#endif
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
	ntfs_volume *vol = NTFS_MP(vnode_mount(a->a_vp));
	errno_t err = 0;

	ntfs_debug("Entering for pathconf variable number %d.", a->a_name);
	if (ni) {
		lck_rw_lock_shared(&ni->lock);
		/*
		 * Do not allow messing with the inode once it has been
		 * deleted.
		 */
		if (NInoDeleted(ni)) {
			/* Remove the inode from the name cache. */
			cache_purge(ni->vn);
			lck_rw_unlock_shared(&ni->lock);
			ntfs_debug("Directory is deleted.");
			return ENOENT;
		}
	}
	switch (a->a_name) {
	case _PC_LINK_MAX:
		/*
		 * The maximum file link count.  For ntfs, the link count is
		 * stored in the mft record in the link_count field which is of
		 * type le16, thus 16 bits.  For attribute inodes and
		 * directories however, no hard links are allowed and thus the
		 * maximum link count is 1.
		 */
		if (!ni) {
			ntfs_debug("Entered with NULL ntfs_inode, aborting.");
			return EINVAL;
		}
		*a->a_retval = NTFS_MAX_HARD_LINKS;
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
		if (vol)
			*a->a_retval = (NVolCaseSensitive(vol) ? 1 : 0);
		else
			err = EINVAL;
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
	if (ni)
		lck_rw_unlock_shared(&ni->lock);
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_allocate -
 */
static int ntfs_vnop_allocate(struct vnop_allocate_args *a)
{
	errno_t err;

	ntfs_debug("Entering.");
	// TODO:
	(void)nop_allocate(a);
	err = ENOTSUP;
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_vnop_pagein - read a range of pages into memory
 * @a:		arguments to pagein function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode whose data to read into the page range
 *	upl_t a_pl;		page list describing destination page range
 *	upl_offset_t a_pl_offset; byte offset into page list at which to start
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
 * pagein flags are currently defined in OS X kernel:
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
	ntfs_inode *base_ni, *ni = NTFS_I(a->a_vp);
	int err;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		if (!(a->a_flags & UPL_NOCOMMIT) && a->a_pl)
			ubc_upl_abort_range(a->a_pl, a->a_pl_offset, a->a_size,
					UPL_ABORT_FREE_ON_EMPTY |
					UPL_ABORT_ERROR);
		return EINVAL;
	}
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	ntfs_debug("Entering for mft_no 0x%llx, offset 0x%llx, size 0x%llx, "
			"pagein flags 0x%x, page list offset 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)a->a_f_offset,
			(unsigned long long)a->a_size, a->a_flags,
			(unsigned long long)a->a_pl_offset);
	err = ntfs_pagein(ni, a->a_f_offset, a->a_size, a->a_pl,
			a->a_pl_offset, a->a_flags);
	/*
	 * Update the last_access_time (atime) if something was read and this
	 * is the base ntfs inode or it is a named stream (this is what HFS+
	 * does, too).
	 *
	 * Skip the update if atime updates are disabled via the noatime mount
	 * option or the volume is read only or this is a symbolic link.
	 *
	 * Also, skip the core system files except for the root directory.
	 */
	if (!err && !NVolReadOnly(ni->vol) &&
			!(vfs_flags(ni->vol->mp) & MNT_NOATIME) &&
			!S_ISLNK(base_ni->mode) &&
			(ni == base_ni || ni->type == AT_DATA)) {
		BOOL need_update_time;

		need_update_time = TRUE;
		if (ni->vol->major_ver > 1) {
			if (base_ni->mft_no <= FILE_Extend &&
					base_ni != ni->vol->root_ni)
				need_update_time = FALSE;
		} else {
			if (base_ni->mft_no <= FILE_UpCase &&
					base_ni != ni->vol->root_ni)
				need_update_time = FALSE;
		}
		if (need_update_time) {
			base_ni->last_access_time = ntfs_utc_current_time();
			NInoSetDirtyTimes(base_ni);
		}
	}
	return err;
}

// TODO: Move to ntfs_page.[hc].
static int ntfs_mst_pageout(ntfs_inode *ni, upl_t upl, upl_offset_t upl_ofs,
		unsigned size, s64 attr_ofs, s64 attr_size, int flags)
{
	ntfs_volume *vol = ni->vol;
	u8 *kaddr;
	kern_return_t kerr;
	unsigned rec_size, rec_shift, nr_recs, i;
	int err;
	NTFS_RECORD_TYPE magic = 0;
	BOOL do_commit;

	do_commit = !(flags & UPL_NOCOMMIT);
	if (ni->type == AT_INDEX_ALLOCATION)
		magic = magic_INDX;
	else
		panic("%s(): Unknown mst protected inode 0x%llx, type 0x%x, "
				"name_len 0x%x.", __FUNCTION__,
				(unsigned long long)ni->mft_no,
				(unsigned)le32_to_cpu(ni->type),
				(unsigned)ni->name_len);
	ntfs_debug("Entering for mft_no 0x%llx, page list offset 0x%llx, size "
			"0x%x, offset 0x%llx, pageout flags 0x%x, magic is "
			"0x%x.", (unsigned long long)ni->mft_no,
			(unsigned long long)upl_ofs, size,
			(unsigned long long)attr_ofs, flags,
			(unsigned)le32_to_cpu(magic));
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
		ntfs_error(vol->mp, "ubc_upl_map() failed (error %d).",
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
		NTFS_RECORD *rec = (NTFS_RECORD*)(kaddr + (i << rec_shift));
		if (__ntfs_is_magic(rec->magic, magic)) {
			err = ntfs_mst_fixup_pre_write(rec, rec_size);
			if (err) {
				ntfs_error(vol->mp, "Failed to apply mst "
						"fixups (mft_no 0x%llx, type "
						"0x%x, offset 0x%llx).",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						(unsigned long long)attr_ofs +
						(i << rec_shift));
				goto mst_err;
			}
		}
	}
	/* Unmap the page list again so we can call cluster_pageout_ext(). */
	// FIXME: Can we leave the page list mapped throughout the
	// cluster_pageout_ext() call?  That would be a lot more efficient and
	// simplify error handling.
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(vol->mp, "ubc_upl_unmap() failed (error %d).",
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
	 * ntfs_cluster_iodone() callback.
	 */
	err = cluster_pageout_ext(ni->vn, upl, upl_ofs, attr_ofs, size,
			attr_size, flags, ntfs_cluster_iodone, NULL);
	if (!err) {
		if (do_commit) {
			/* Commit the page range we wrote out. */
			ubc_upl_commit_range(upl, upl_ofs, size,
					UPL_COMMIT_FREE_ON_EMPTY |
					UPL_COMMIT_CLEAR_DIRTY);
		}
		ntfs_debug("Done.");
		return err;
	}
	ntfs_error(vol->mp, "Failed (cluster_pageout_ext() returned error "
			"%d).", err);
	/*
	 * We may have some records left with applied fixups thus remove them
	 * again.  It does not matter if it is done twice as this is an error
	 * code path and the only side effect is a little slow down.
	 */
	kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(vol->mp, "ubc_upl_map() failed (error %d), cannot "
				"remove mst fixups.  Unmount and run chkdsk.",
				(int)kerr);
		NVolSetErrors(vol);
		goto err;
	}
mst_err:
	/* Remove the applied fixups, unmap the page list and abort. */
	while (i > 0) {
		NTFS_RECORD *rec = (NTFS_RECORD*)(kaddr + (--i << rec_shift));
		if (__ntfs_is_magic(rec->magic, magic))
			ntfs_mst_fixup_post_write(rec);
	}
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS)
		ntfs_error(vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
err:
	if (do_commit)
		ubc_upl_abort_range(upl, upl_ofs, size,
				UPL_ABORT_FREE_ON_EMPTY);
	return err;
}

/**
 * ntfs_vnop_pageout - write a range of pages to storage
 * @a:		arguments to pageout function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode whose data to write from the page range
 *	upl_t a_pl;		page list describing the source page range
 *	upl_offset_t a_pl_offset; byte offset into page list at which to start
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
 * following pageout flags are currently defined in OS X kernel:
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
	s64 attr_ofs, attr_size, alloc_size, bytes;
	ntfs_inode *base_ni, *ni = NTFS_I(a->a_vp);
	upl_t upl = a->a_pl;
	ntfs_volume *vol;
	u8 *kaddr;
	upl_offset_t upl_ofs = a->a_pl_offset;
	kern_return_t kerr;
	unsigned to_write, size = a->a_size;
	int err, flags = a->a_flags;
	lck_rw_type_t lock_type = LCK_RW_TYPE_SHARED;
	BOOL locked = FALSE;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		if (!(flags & UPL_NOCOMMIT) && upl)
			ubc_upl_abort_range(upl, upl_ofs, size,
					UPL_ABORT_FREE_ON_EMPTY);
		return EINVAL;
	}
	vol = ni->vol;
	attr_ofs = a->a_f_offset;
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
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
		ntfs_error(vol->mp, "NULL page list passed in or request size "
				"is below zero (error EINVAL).");
		return EINVAL;
	}
	if (S_ISDIR(ni->mode)) {
		ntfs_error(vol->mp, "Called for directory vnode.");
		err = EISDIR;
		goto err;
	}
	if (NVolReadOnly(vol)) {
		err = EROFS;
		goto err;
	}
	/*
	 * Need to clip i/o at maximum file size of 2^63-1 bytes in case
	 * someone creates a sparse file and is playing silly with seek + write
	 * note we only need to check for this for sparse files as non-sparse
	 * files can never reach 2^63-1 because that is also the maximum space
	 * on the volume thus the write would simply get an ENOSPC when the
	 * volume is full.
	 */
	if (NInoSparse(ni) && (u64)attr_ofs + size > NTFS_MAX_ATTRIBUTE_SIZE) {
		err = EFBIG;
		goto err;
	}
#if 1	// TODO: Remove this when sparse support is done...
	if (NInoSparse(ni)) {
		err = ENOTSUP;
		goto err;
	}
#endif
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also but only if the VFS is not calling back into the
	 * NTFS driver after the NTFS driver called it in which case we are
	 * already holding the lock.
	 *
	 * There is a complication in that the UPL is already created by the
	 * caller thus us taking the lock here is a case of lock reversal wrt
	 * the UPL keeping the pages locked for exclusive access thus we can
	 * deadlock with a concurrent file create for example when it holds the
	 * ntfs inode lock @ni->lock for exclusive access on the index vnode of
	 * the parent directory and then calls ntfs_page_map() to map a page
	 * from the index as we already hold the same UPL that ntfs_page_map()
	 * will try to get thus if we go to sleep on the ntfs inode lock that
	 * is held exclusive by the create code path we would now deadlock.
	 *
	 * To avoid the deadlock, we do a try-lock for the ntfs inode lock and
	 * if that fails we simply abort the pages returning them to the VM
	 * without modification thus they should remain dirty and they should
	 * be paged out at a later point in time.
	 *
	 * We then return ENXIO to indicate that this is a temporary failure to
	 * the caller.
	 *
	 * FIXME: There is a complication and that is that we really need to
	 * hole the inode lock for writing if we are writing to a hole and/or
	 * writing past the initialized size as we would then be modifying the
	 * initialized_size.  But if UPL_NESTED_PAGEOUT is set we have no idea
	 * whether the caller is holding the lock for write or not and we
	 * cannot safely drop/retake the lock in any case...  For now we ignore
	 * the problem and just emit a warning in this case.
	 */
	if (!(flags & UPL_NESTED_PAGEOUT)) {
		if (NInoSparse(ni))
			lock_type = LCK_RW_TYPE_EXCLUSIVE;
		if (!lck_rw_try_lock(&ni->lock, lock_type)) {
			ntfs_debug("Failed to take ni->lock for %s for mft_no "
					"0x%llx, type 0x%x.  Aborting with "
					"ENXIO to avoid deadlock.",
					(lock_type == LCK_RW_TYPE_SHARED) ?
					"reading" : "writing",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type));
			if (!(flags & UPL_NOCOMMIT))
				ubc_upl_abort_range(upl, upl_ofs, size,
						UPL_ABORT_FREE_ON_EMPTY);
			return ENXIO;
		}
		locked = TRUE;
	} else {
		if (NInoSparse(ni))
			ntfs_warning(vol->mp, "flags & UPL_NESTED_PAGEOUT && "
					"NINoSparse(ni), need inode lock "
					"exclusive but caller holds the lock "
					"so we do not know if it is exclusive "
					"or not.");
	}
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
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
	/*
	 * Cannot pageout to a negative offset or if we are starting beyond the
	 * end of the attribute or if the attribute offset is not page aligned
	 * or the size requested is not a multiple of PAGE_SIZE.
	 */
	if (attr_ofs < 0 || attr_ofs >= attr_size || attr_ofs & PAGE_MASK_64 ||
			size & PAGE_MASK || upl_ofs & PAGE_MASK) {
		lck_spin_unlock(&ni->size_lock);
		err = EINVAL;
		goto err;
	}
// TODO: HERE:
	// FIXME: For now abort writes beyond initialized size...
	// TODO: This causes a problem and that is in ntfs_vnop_write() we only
	// update the initialized size after calling cluster_write() which
	// means we cannot zero up to the initialized size here or we could
	// trample over data that has just been written out.  Also this causes
	// our check here to trigger even though we are not really outside the
	// initialized size at all and in fact this page out may be part of the
	// write itself so it has to succeed.  But on the other hand if this is
	// a genuine mmap()-based write we do need to do the zeroing.  We need
	// to somehow be able to tell the difference between the two...
	// If the initialized size equals attr_ofs then we can safely perform
	// the write and then update the initialized size to attr_ofs + size
	// but need to be careful to update the data size appropriately and
	// also need to make sure not to exceed the end of the write otherwise
	// we would cause a file extension here when we should not do so.  In
	// fact if this is not part of an extending write then we should not
	// modify the data size and only the initialized size instead.
	if (attr_ofs + size > ni->initialized_size && ni->initialized_size !=
			ni->data_size) {
		lck_spin_unlock(&ni->size_lock);
		ntfs_error(vol->mp, "Writing beyond the initialized size of "
				"an attribute is not implemented yet.");
		err = ENOTSUP;
		goto err;
	}
	alloc_size = ni->allocated_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If this is a sparse attribute we need to fill any holes overlapping
	 * the write.  We can skip resident attributes as they cannot have
	 * sparse regions.
	 *
	 * As allocated size goes in units of clusters we need to round down
	 * the start offset to the nearest cluster boundary and we need to
	 * round up the end offset to the next cluster boundary.
	 */
	if (NInoSparse(ni) && NInoNonResident(ni) && ni->type == AT_DATA) {
		s64 aligned_end, new_end;

		aligned_end = (attr_ofs + size + vol->cluster_size_mask) &
				~vol->cluster_size_mask;
		/*
		 * Only need to instantiate holes up to the allocated size
		 * itself.  Everything else would be an extension which is not
		 * allowed from VNOP_PAGEOUT().
		 */
		if (aligned_end > alloc_size)
			aligned_end = alloc_size;
		err = ntfs_attr_instantiate_holes(ni,
				attr_ofs & ~vol->cluster_size_mask,
				aligned_end, &new_end, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Cannot perform pageout of mft_no "
					"0x%llx because instantiation of "
					"sparse regions failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			goto err;
		}
		/* The instantiation may not be partial. */
		if (new_end < aligned_end)
			panic("%s(): new_end < aligned_end\n", __FUNCTION__);
	}
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
			ntfs_warning(vol->mp, "Denying write to encrypted "
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
				ntfs_error(vol->mp, "Failed "
						"(cluster_pageout_ext(), "
						"error %d).", err);
		}
		goto done;
	}
compressed:
	/* The attribute is resident and/or compressed. */
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
		ntfs_error(vol->mp, "ubc_upl_map() failed (error %d).",
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
			ntfs_error(vol->mp, "ntfs_resident_attr_write() "
					"failed (error %d).", err);
	} else if (NInoCompressed(ni)) {
		ntfs_error(vol->mp, "Writing to compressed files is not "
				"implemented yet, sorry.");
		err = ENOTSUP;
#if 0
		ntfs_inode *raw_ni;
		int ioflags;

		/*
		 * Get the raw inode and lock it for writing to protect against
		 * concurrent readers and writers as the compressed data is
		 * invalid whilst a write is in progress.
		 */
		err = ntfs_raw_inode_get(ni, LCK_RW_TYPE_EXCLUSIVE, &raw_ni);
		if (err)
			ntfs_error(vol->mp, "Failed to get raw inode (error "
					"%d).", err);
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
				ntfs_error(vol->mp, "ntfs_write_compressed() "
						"failed (error %d).", err);
			lck_rw_unlock_exclusive(&raw_ni->lock);
			(void)vnode_put(raw_ni->vn);
		}
#endif
	} else {
		/*
		 * The attribute was converted to non-resident under our nose
		 * we need to retry the pageout.
		 *
		 * TODO: This may no longer be possible to happen now that we
		 * lock against changes in initialized size and thus
		 * truncation...  Revisit this issue when the write code has
		 * been finished and replace this with a panic().
		 */
		err = EAGAIN;
	}
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(vol->mp, "ubc_upl_unmap() failed (error %d).",
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
		/*
		 * If the attribute was converted to non-resident under our
		 * nose, retry the pageout.
		 *
		 * TODO: This may no longer be possible to happen now that we
		 * lock against changes in initialized size and thus
		 * truncation...  Revisit this issue when the write code has
		 * been finished and remove the check and goto if appropriate.
		 */
		if (err == EAGAIN)
			goto retry_pageout;
err:
		if (!(flags & UPL_NOCOMMIT))
			ubc_upl_abort_range(upl, upl_ofs, size,
					UPL_ABORT_FREE_ON_EMPTY);
		ntfs_error(vol->mp, "Failed (error %d).", err);
	}
done:
	// TODO: If we wrote anything at all we have to clear the setuid and
	// setgid bits as a precaution against tampering (see
	// xnu/bsd/hfs/hfs_readwrite.c::hfs_vnop_pageout()).
	/*
	 * If this is not a directory or it is an encrypted directory, set the
	 * needs archiving bit except for the core system files.
	 */
	if (!err && (!S_ISDIR(base_ni->mode) || NInoEncrypted(base_ni))) {
		BOOL need_set_archive_bit = TRUE;
		if (vol->major_ver > 1) {
			if (base_ni->mft_no <= FILE_Extend)
				need_set_archive_bit = FALSE;
		} else {
			if (base_ni->mft_no <= FILE_UpCase)
				need_set_archive_bit = FALSE;
		}
		if (need_set_archive_bit) {
			base_ni->file_attributes |= FILE_ATTR_ARCHIVE;
			NInoSetDirtyFileAttributes(base_ni);
		}
	}
	/*
	 * Update the last_data_change_time (mtime) and last_mft_change_time
	 * (ctime) on the base ntfs inode @base_ni but not on the core system
	 * files.  However do set it on the root directory.
	 *
	 * Do not update the times on symbolic links.
	 */
	if (!err && !S_ISLNK(base_ni->mode)) {
		BOOL need_update_time = TRUE;
		if (vol->major_ver > 1) {
			if (base_ni->mft_no <= FILE_Extend &&
					base_ni != vol->root_ni)
				need_update_time = FALSE;
		} else {
			if (base_ni->mft_no <= FILE_UpCase &&
					base_ni != vol->root_ni)
				need_update_time = FALSE;
		}
		if (need_update_time) {
			base_ni->last_mft_change_time =
					base_ni->last_data_change_time =
					ntfs_utc_current_time();
			NInoSetDirtyTimes(base_ni);
		}
	}
	if (locked) {
		if (lock_type == LCK_RW_TYPE_SHARED)
			lck_rw_unlock_shared(&ni->lock);
		else
			lck_rw_unlock_exclusive(&ni->lock);
	}
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
 * ntfs_vnop_getxattr - get the data of an extended attribute of an ntfs inode
 * @a:		arguments to getxattr function
 *
 * @a contains:
 *	vnode_t a_vp;	vnode whose extended attribute to get
 *	char *a_name;	name of extented attribute to get in utf8
 *	uio_t a_uio;	destination in which to return the exteneded attribute
 *	size_t *a_size;	size of the extended attribute in bytes
 *	int a_options;	flags controlling how the attribute is obtained
 *	vfs_context_t a_context;
 *
 * Get the named stream with the name @a->a_name (we map named streams 1:1 with
 * extended attributes for NTFS as the NTFS native EAs are useless) contained
 * in the vnode @a->a_vp and return its data in the destination specified by
 * @a->a_uio.
 * 
 * If there was not enough space to return the whole extended attribute in the
 * destination @a->a_uio we return error ERANGE.  The only exception to this is
 * the resource fork (@a->a_name is XATTR_RESOURCEFORK_NAME) for which we just
 * return up to uio_resid(@a->a_uio) bytes (or up to the end of the resource
 * fork if that is smaller).
 *
 * Note that uio_offset(@a->a_uio) must be zero except for the resource fork
 * where it can specify the offset into the resource fork at which to begin
 * returning the data.
 *
 * If @a->a_uio is NULL, do not return the data of the attribute and instead
 * return the current data size of the named stream in *@a->a_size.  Note that
 * when @a->a_uio is not NULL @a->a_size is ignored as the size of the named
 * stream is implicitly returned in the @a->a_uio and it can be obtained by
 * taking the original buffer size and subtracting uio_resid(@a->a_uio) from
 * it.
 *
 * The flags in @a->a_options control how the attribute is obtained.  The
 * following flags are currently defined in OS X kernel:
 *	XATTR_NOFOLLOW	- Do not follow symbolic links.
 *	XATTR_CREATE	- Set the value, fail if already exists (setxattr only).
 *	XATTR_REPLACE	- Set the value, fail if does not exist (setxattr only).
 *	XATTR_NOSECURITY- Bypass authorization checking.
 *	XATTR_NODEFAULT	- Bypass default extended attribute file ('._' file).
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_getxattr(struct vnop_getxattr_args *a)
{
	s64 size;
	user_ssize_t start_count;
	off_t start_ofs;
	ntfs_inode *ani, *ni = NTFS_I(a->a_vp);
	const char *name = a->a_name;
	uio_t uio = a->a_uio;
	ntfs_volume *vol;
	ntfschar *ntfs_name;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	errno_t err;
	ntfschar ntfs_name_buf[NTFS_MAX_ATTR_NAME_LEN];

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	/* Check for invalid names. */
	if (!name || name[0] == '\0')
		return EINVAL;
	start_ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	ntfs_debug("Entering for mft_no 0x%llx, extended attribute name %s, "
			"offset 0x%llx, size 0x%llx, options 0x%x.",
			(unsigned long long)ni->mft_no, name, start_ofs,
			start_count, a->a_options);
	lck_rw_lock_shared(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	/*
	 * Only regular files, directories, and symbolic links can have
	 * extended attributes.  (Specifically named streams cannot have them.)
	 *
	 * Thus the check is for attribute inodes as all base inodes are
	 * allowed.  Raw inodes are also attribute inodes so they are excluded
	 * automatically, too.
	 */
	if (NInoAttr(ni)) {
		ntfs_debug("Mft_no 0x%llx is an attribute inode.",
				(unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	/*
	 * First of all deal with requests for the Finder info as that is
	 * special because we cache it in the base ntfs inode @ni and we only
	 * want to return it if the Finder info is non-zero.  This is what HFS
	 * does, too.
	 *
	 * Thus we need to check the status of the cache in the ntfs inode
	 * first and if that it valid we can use it to check the content of the
	 * Finder info for being zero.  And if it is not valid then we need to
	 * read it into the cache in the ntfs inode and then we can check the
	 * Finder info in the cache for being zero.  In fact we do this the
	 * other way round, i.e. if the Finder info cache is not valid we read
	 * the Finder info into the cache first and then the cache is
	 * definitely valid thus we can check the Finder info for being
	 * non-zero and the Finder info data if so.
	 *
	 * A further complication is in the event of symbolic links where we do
	 * not return the type and creator and instead return zero for them as
	 * that is what HFS+ does, too.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME))) {
		FINDER_INFO fi;

		if (!NInoValidFinderInfo(ni)) {
			if (!lck_rw_lock_shared_to_exclusive(&ni->lock)) {
				lck_rw_lock_exclusive(&ni->lock);
				if (NInoDeleted(ni)) {
					cache_purge(ni->vn);
					lck_rw_unlock_exclusive(&ni->lock);
					ntfs_debug("Mft_no 0x%llx is deleted.",
							(unsigned long long)
							ni->mft_no);
					return ENOENT;
				}
			}
			/*
			 * Load the AFP_AfpInfo stream and initialize the
			 * backup time and Finder info (if they are not already
			 * valid).
			 */
			err = ntfs_inode_afpinfo_read(ni);
			if (err) {
				ntfs_error(vol->mp, "Failed to obtain AfpInfo "
						"for mft_no 0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				lck_rw_unlock_exclusive(&ni->lock);
				return err;
			}
			lck_rw_lock_exclusive_to_shared(&ni->lock);
			if (!NInoValidFinderInfo(ni))
				panic("%s(): !NInoValidFinderInfo(ni)\n",
						__FUNCTION__);
		}
		/*
		 * Make a copy of the Finder info and mask out the hidden bit
		 * if this is the root directory and the type and creator if
		 * this is a symbolic link.
		 */
		memcpy(&fi, &ni->finder_info, sizeof(fi));
		if (ni == vol->root_ni)
			fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
		if (S_ISLNK(ni->mode)) {
			fi.type = 0;
			fi.creator = 0;
		}
		/* If the Finder info is zero, pretend it does not exist. */
		if (!bcmp(&fi, &ntfs_empty_finder_info,
				sizeof(ni->finder_info))) {
			ntfs_debug("Mft_no 0x%llx has zero Finder info, "
					"returning ENOATTR.",
					(unsigned long long)ni->mft_no);
			err = ENOATTR;
			goto err;
		}
		/* The Finder info is not zero, return it. */
		if (!uio) {
			*a->a_size = sizeof(FINDER_INFO);
			err = 0;
		} else if (start_ofs)
			err = EINVAL;
		else if (uio_resid(uio) < (user_ssize_t)sizeof(FINDER_INFO))
			err = ERANGE;
		else {
			err = uiomove((caddr_t)&fi, sizeof(fi), uio);
			if (err)
				ntfs_error(vol->mp, "uiomove() failed (error "
						"%d).", err);
		}
		goto err;
	}
	/*
	 * Now deal with requests for the resource fork as that is special
	 * because on one hand we need to translate its name from
	 * XATTR_RESOURCEFORK_NAME to AFP_Resource so we do not need to convert
	 * the utf8 name @name to Unicode and on the other hand the offset
	 * @start_ofs may be non-zero and the read may be only from a partial
	 * region of the resource fork.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_name = NTFS_SFM_RESOURCEFORK_NAME;
		ntfs_name_len = 12;
	} else {
		/*
		 * The request is not for the resource fork (nor for the Finder
		 * info).  This means that the offset @start_ofs must be zero.
		 */
		if (start_ofs) {
			err = EINVAL;
			goto err;
		}
		/* Convert the requested name from utf8 to Unicode. */
		ntfs_name = ntfs_name_buf;
		ntfs_name_size = sizeof(ntfs_name_buf);
		ntfs_name_len = utf8_to_ntfs(vol, (const u8*)name, strlen(name),
				&ntfs_name, &ntfs_name_size);
		if (ntfs_name_len < 0) {
			err = -ntfs_name_len;
			if (err == ENAMETOOLONG)
				ntfs_debug("Failed (name is too long).");
			else
				ntfs_error(vol->mp, "Failed to convert name to "
						"Unicode (error %d).", err);
			goto err;
		}
		/*
		 * If this is one of the SFM named streams, skip it, as they
		 * contain effectively metadata information so should not be
		 * exposed directly.
		 */
		if (ntfs_is_sfm_name(vol, ntfs_name, ntfs_name_len)) {
			ntfs_debug("Not allowing access to protected SFM name "
					"(returning EINVAL).");
			err = EINVAL;
			goto err;
		}
	}
	/*
	 * We now have the name of the requested attribute in @ntfs_name and it
	 * is @ntfs_name_len characters long and we have verified that the
	 * start offset is zero (unless this is the resource fork in which case
	 * a non-zero start offset is fine).
	 *
	 * Start by getting the ntfs inode for the $DATA:@ntfs_name attribute.
	 */
	err = ntfs_attr_inode_get(ni, AT_DATA, ntfs_name, ntfs_name_len, FALSE,
			LCK_RW_TYPE_SHARED, &ani);
	if (err) {
		if (err == ENOENT)
			err = ENOATTR;
		else if (err != ENOATTR)
			ntfs_error(vol->mp, "Failed to get $DATA/%s attribute "
					"inode mft_no 0x%llx (error %d).", name,
					(unsigned long long)ni->mft_no, err);
		goto err;
	}
	/*
	 * TODO: This check may no longer be necessary now that we lock against
	 * changes in initialized size and thus truncation...  Revisit this
	 * issue when the write code has been written and remove the check if
	 * appropriate simply using ubc_getsize(ni->vn); without the size_lock.
	 */
	lck_spin_lock(&ani->size_lock);
	size = ubc_getsize(ani->vn);
	if (size > ani->data_size)
		size = ani->data_size;
	lck_spin_unlock(&ani->size_lock);
	if (!uio)
		*a->a_size = size;
	else if (ntfs_name != NTFS_SFM_RESOURCEFORK_NAME &&
			start_count < size) {
		/* Partial reads are only allowed for the resource fork. */
		err = ERANGE;
	} else {
		/*
		 * Perform the actual read from the attribute inode.  We pass
		 * in IO_UNIT as we want an atomic i/o operation.
		 *
		 * FIXME: ntfs_read() currently ignores the IO_UNIT flag so we
		 * still have to test for partial reads.
		 */
		err = ntfs_read(ani, uio, IO_UNIT, TRUE);
		/*
		 * If the read was partial, reset @uio pretending that the read
		 * never happened.  This is because extended attribute i/o is
		 * meant to be atomic, i.e. either we get it all or we do not
		 * get anything.
		 *
		 * Note we also accept the case where uio_resid() has gone to
		 * zero as this covers the exception of the resource fork for
		 * which we do not need to return the whole resource fork in
		 * one go.
		 */
		if (uio_resid(uio) && start_count - uio_resid(uio) !=
				size - start_ofs) {
			/*
			 * FIXME: Should we be trying to continue a partial
			 * read in case we can complete it with multiple calls
			 * to ntfs_read()?  If we do that we could also drop
			 * the IO_UNIT flag above.
			 */
			if (!err) {
				ntfs_debug("ntfs_read() returned a partial "
						"read, pretending the read "
						"never happened.");
				err = EIO;
			}
			uio_setoffset(uio, start_ofs);
			uio_setresid(uio, start_count);
		}
	}
	lck_rw_unlock_shared(&ani->lock);
	(void)vnode_put(ani->vn);
err:
	lck_rw_unlock_shared(&ni->lock);
	ntfs_debug("Done (error %d).", err);
	return err;
}

/**
 * ntfs_vnop_setxattr - set the data of an extended attribute of an ntfs inode
 * @a:		arguments to setxattr function
 *
 * @a contains:
 *	vnode_t a_vp;	vnode whose extended attribute to set
 *	char *a_name;	name of extented attribute to set in utf8
 *	uio_t a_uio;	source data to which to set the exteneded attribute
 *	int a_options;	flags controlling how the attribute is set
 *	vfs_context_t a_context;
 *
 * Get the named stream with the name @a->a_name (we map named streams 1:1 with
 * extended attributes for NTFS as the NTFS native EAs are useless) contained
 * in the vnode @a->a_vp and set its data to the source specified by @a->a_uio.
 *
 * If @a->a_options does not specify XATTR_CREATE nor XATTR_REPLACE the
 * attribute will be created if it does not exist already and if it exists
 * already the old value will be replaced with the new one, i.e. if the old
 * value does not have the same size as the new value the attribute is
 * truncated to the new size. 
 *
 * If @a->a_options specifies XATTR_CREATE the call will fail if the attribute
 * already exists, i.e. the existing attribute will not be replaced.
 *
 * If @a->a_options specifies XATTR_REPLACE the call will fail if the attribute
 * does not exist, i.e. the new attribute will not be created.
 *
 * An exception is the resource fork (@a->a_name is XATTR_RESOURCEFORK_NAME)
 * for which we do not replace the existing attribute and instead we write over
 * the existing attribute starting at offset uio_offset(@a->a_uio) and writing
 * uio_resid(@a->a_uio) bytes.  Writing past the end of the resource fork will
 * cause the resource fork to be extended just like a regular file write would
 * do but a write to any existing part of the attribute will not cause the
 * attribute to be shrunk.
 *
 * Simillar to other extended attributes, if @a->a_options specifies
 * XATTR_CREATE the call will fail if the resource fork already exists, i.e.
 * the write to the existing resource fork will be denied and if @a->a_options
 * specified XATTR_REPLACE the call will fail if the resource fork does not yet
 * exist, i.e. the new resource fork will not be created.
 *
 * Note that uio_offset(@a->a_uio) must be zero except for the resource fork
 * where it can specify the offset into the resource fork at which to begin
 * writing the data.
 *
 * The flags in @a->a_options control how the attribute is set.  The following
 * flags are currently defined in OS X kernel:
 *	XATTR_NOFOLLOW	- Do not follow symbolic links.
 *	XATTR_CREATE	- Set the value, fail if already exists (setxattr only).
 *	XATTR_REPLACE	- Set the value, fail if does not exist (setxattr only).
 *	XATTR_NOSECURITY- Bypass authorization checking.
 *	XATTR_NODEFAULT	- Bypass default extended attribute file ('._' file).
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_setxattr(struct vnop_setxattr_args *a)
{
	s64 size;
	user_ssize_t start_count;
	off_t start_ofs;
	ntfs_inode *ani, *ni = NTFS_I(a->a_vp);
	ntfs_volume *vol;
	const char *name = a->a_name;
	uio_t uio = a->a_uio;
	ntfschar *ntfs_name;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	const int options = a->a_options;
	errno_t err;
	ntfschar ntfs_name_buf[NTFS_MAX_ATTR_NAME_LEN];

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	/* Check for invalid names. */
	if (!name || name[0] == '\0')
		return EINVAL;
	start_ofs = uio_offset(uio);
	start_count = uio_resid(uio);
	ntfs_debug("Entering for mft_no 0x%llx, extended attribute name %s, "
			"offset 0x%llx, size 0x%llx, options 0x%x.",
			(unsigned long long)ni->mft_no, name, start_ofs,
			start_count, options);
	/*
	 * Access to extended attributes must be atomic which we ensure by
	 * locking the base ntfs inode for writing.
	 */
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	/*
	 * Only regular files, directories, and symbolic links can have
	 * extended attributes.  (Specifically named streams cannot have them.)
	 *
	 * Thus the check is for attribute inodes as all base inodes are
	 * allowed.  Raw inodes are also attribute inodes so they are excluded
	 * automatically, too.
	 */
	if (NInoAttr(ni)) {
		ntfs_debug("Mft_no 0x%llx is an attribute inode.",
				(unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	/*
	 * XATTR_CREATE and XATTR_REPLACE may not be specified at the same time
	 * or weird things would happen so test for and abort this case here.
	 */
	if ((options & (XATTR_CREATE | XATTR_REPLACE)) ==
			(XATTR_CREATE | XATTR_REPLACE)) {
		ntfs_debug("Either XATTR_CREATE or XATTR_REPLACE but not both "
				"may be specified.");
		err = EINVAL;
		goto err;
	}
	/*
	 * First of all deal with requests to set the Finder info as that is
	 * special because we cache it in the base ntfs inode @ni thus we need
	 * to copy the new Finder info into the cache and then write the
	 * changes out to the AFP_AfpInfo attribute (creating it if it did not
	 * exist before).
	 *
	 * The only exception to the above description is when the XATTR_CREATE
	 * or XATTR_REPLACE flags are set in @options in which case we need to
	 * know whether the Finder info extists already or not and thus if the
	 * Finder info cache is not valid we need to make it valid first and
	 * then we can check it against being zero to determine whether the
	 * Finder info exists already or not and then we know whether or not to
	 * proceed with setting the Finder info.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME))) {
		FINDER_INFO fi;

		if (start_count != sizeof(ni->finder_info)) {
			ntfs_debug("Number of bytes to write (%lld) does not "
					"equal Finder info size (%ld), "
					"returning ERANGE.",
					(unsigned long long)start_count,
					sizeof(ni->finder_info));
			err = ERANGE;
			goto err;
		}
		/*
		 * If @options does not specify XATTR_CREATE nor XATTR_REPLACE
		 * there is no need to bring the Finder info up-to-date before
		 * the write.
		 */
		if (options & (XATTR_CREATE | XATTR_REPLACE)) {
			if (!NInoValidFinderInfo(ni)) {
				/*
				 * Load the AFP_AfpInfo stream and initialize
				 * the backup time and Finder info (at least
				 * the Finder info is not yet valid).
				 */
				err = ntfs_inode_afpinfo_read(ni);
				if (err) {
					ntfs_error(vol->mp, "Failed to obtain "
							"AfpInfo for mft_no "
							"0x%llx (error %d).",
							(unsigned long long)
							ni->mft_no, err);
					goto err;
				}
				if (!NInoValidFinderInfo(ni))
					panic("%s(): !NInoValidFinderInfo(ni)"
							"\n", __FUNCTION__);
			}
			/*
			 * Make a copy of the Finder info and mask out the
			 * hidden bit if this is the root directory and the
			 * type and creator if this is a symbolic link.
			 */
			memcpy(&fi, &ni->finder_info, sizeof(fi));
			if (ni == vol->root_ni)
				fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
			if (S_ISLNK(ni->mode)) {
				fi.type = 0;
				fi.creator = 0;
			}
			if (bcmp(&ni->finder_info, &ntfs_empty_finder_info,
					sizeof(ni->finder_info))) {
				/*
				 * Finder info is non-zero, i.e. it exists, and
				 * XATTR_CREATE was specified.
				 */
				if (options & XATTR_CREATE) {
					ntfs_debug("Mft_no 0x%llx has "
							"non-zero Finder info "
							"and XATTR_CREATE was "
							"specified, returning "
							"EEXIST.",
							(unsigned long long)
							ni->mft_no);
					err = EEXIST;
					goto err;
				}
			} else {
				/*
				 * Finder info is zero, i.e. it does not exist,
				 * and XATTR_REPLACE was specified.
				 */
				if (options & XATTR_REPLACE) {
					ntfs_debug("Mft_no 0x%llx has zero "
							"Finder info and "
							"XATTR_REPLACE was "
							"specified, returning "
							"ENOATTR.",
							(unsigned long long)
							ni->mft_no);
					err = ENOATTR;
					goto err;
				}
			}
		}
		/* Copy the new Finder info value to our buffer. */
		err = uiomove((caddr_t)&fi, sizeof(fi), uio);
		if (!err) {
			/*
			 * Set the Finder info to the new value after masking
			 * out the hidden bit if this is the root directory and
			 * enforcing the type and creator if this is a symbolic
			 * link to be our private values for symbolic links.
			 */
			if (ni == vol->root_ni)
				fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
			if (S_ISLNK(ni->mode)) {
				fi.type = FINDER_TYPE_SYMBOLIC_LINK;
				fi.creator = FINDER_CREATOR_SYMBOLIC_LINK;
			}
			memcpy((u8*)&ni->finder_info, (u8*)&fi, sizeof(fi));
			NInoSetValidFinderInfo(ni);
			NInoSetDirtyFinderInfo(ni);
			/*
			 * If the file is not hidden but the Finder info hidden
			 * bit is being set, we need to cause the file to be
			 * hidden, i.e. we need to set the FILE_ATTR_HIDDEN bit
			 * in the file_attributes of the $STANDARD_INFORMATION
			 * attribute.
			 */
			if (fi.attrs & FINDER_ATTR_IS_HIDDEN &&
					!(ni->file_attributes &
					FILE_ATTR_HIDDEN)) {
				ni->file_attributes |= FILE_ATTR_HIDDEN;
				NInoSetDirtyFileAttributes(ni);
			}
			/*
			 * Updating the Finder info causes both the
			 * last_data_change_time (mtime) and
			 * last_mft_change_time (ctime) to be updated.
			 */
			ni->last_mft_change_time = ni->last_data_change_time =
					ntfs_utc_current_time();
			NInoSetDirtyTimes(ni);
			/*
			 * Now write (if needed creating) the AFP_AfpInfo
			 * attribute with the specified Finder Info.
			 */
			err = ntfs_inode_afpinfo_write(ni);
			if (err)
				ntfs_error(vol->mp, "Failed to write/create "
						"AFP_AfpInfo attribute in "
						"inode 0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
		} else
			ntfs_error(vol->mp, "uiomove() failed (error %d).",
					err);
		goto err;
	}
	/*
	 * Now deal with requests to write to the resource fork as that is
	 * special because on one hand we need to translate its name from
	 * XATTR_RESOURCEFORK_NAME to AFP_Resource so we do not need to convert
	 * the utf8 name @name to Unicode and on the other hand the offset
	 * @start_ofs may be non-zero, the write may be only to a partial
	 * region of the resource fork, and the write may not shrink the
	 * resource fork though it may extend it.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_name = NTFS_SFM_RESOURCEFORK_NAME;
		ntfs_name_len = 12;
	} else {
		/*
		 * The request is not for the resource fork (nor for the Finder
		 * info).  This means that the offset @start_ofs must be zero.
		 */
		if (start_ofs) {
			err = EINVAL;
			goto err;
		}
		/* Convert the requested name from utf8 to Unicode. */
		ntfs_name = ntfs_name_buf;
		ntfs_name_size = sizeof(ntfs_name_buf);
		ntfs_name_len = utf8_to_ntfs(vol, (const u8*)name, strlen(name),
				&ntfs_name, &ntfs_name_size);
		if (ntfs_name_len < 0) {
			err = -ntfs_name_len;
			if (err == ENAMETOOLONG)
				ntfs_debug("Failed (name is too long).");
			else
				ntfs_error(vol->mp, "Failed to convert name to "
						"Unicode (error %d).", err);
			goto err;
		}
		/*
		 * If this is one of the SFM named streams, skip it, as they
		 * contain effectively metadata information so should not be
		 * exposed directly.
		 */
		if (ntfs_is_sfm_name(vol, ntfs_name, ntfs_name_len)) {
			ntfs_debug("Not allowing access to protected SFM name "
					"(returning EINVAL).");
			err = EINVAL;
			goto err;
		}
	}
	/*
	 * We now have the name of the requested attribute in @ntfs_name and it
	 * is @ntfs_name_len characters long and we have verified that the
	 * start offset is zero (unless this is the resource fork in which case
	 * a non-zero start offset is fine).
	 *
	 * Get the ntfs attribute inode of the $DATA:@ntfs_name attribute
	 * (unless XATTR_CREATE is specified in @options) and if it does not
	 * exist create it first (unless XATTR_REPLACE is specified in
	 * @options).
	 */
	err = ntfs_attr_inode_get_or_create(ni, AT_DATA, ntfs_name,
			ntfs_name_len, FALSE, FALSE, options,
			LCK_RW_TYPE_EXCLUSIVE, &ani);
	if (err) {
		if (err == ENOENT)
			err = ENOATTR;
		else if (err != ENOATTR && err != EEXIST)
			ntfs_error(vol->mp, "Failed to get or create $DATA/%s "
					"attribute inode mft_no 0x%llx (error "
					"%d).", name,
					(unsigned long long)ni->mft_no, err);
		goto err;
	}
	/*
	 * TODO: This check may no longer be necessary now that we lock against
	 * changes in initialized size and thus truncation...  Revisit this
	 * issue when the write code has been written and remove the check if
	 * appropriate simply using ubc_getsize(ni->vn); without the size_lock.
	 */
	lck_spin_lock(&ani->size_lock);
	size = ubc_getsize(ani->vn);
	if (size > ani->data_size)
		size = ani->data_size;
	lck_spin_unlock(&ani->size_lock);
	/*
	 * Perform the actual write to the attribute inode.  We pass in IO_UNIT
	 * as we want an atomic i/o operation.
	 *
	 * FIXME: ntfs_write() does not always honour the IO_UNIT flag so we
	 * still have to test for partial writes.
	 */
	err = ntfs_write(ani, uio, IO_UNIT, TRUE);
	/*
	 * If the write was successful, need to shrink the attribute if the new
	 * size is smaller than the old size.
	 *
	 * If the write was partial or failed, reset @uio pretending that the
	 * write never happened.  This is because extended attribute i/o is
	 * meant to be atomic, i.e. either we get it all or we do not get
	 * anything.
	 *
	 * In the partial/failed case, if @options specifies XATTR_REPLACE we
	 * know the extended attribute existed already thus we truncate it to
	 * zero size to simulate that the old value has been replaced.  And if
	 * @options specifies XATTR_CREATE we know we created the extended
	 * attribute thus we delete it again.  And if @options does not specify
	 * XATTR_REPLACE nor XATTR_CREATE then we do not know whether we
	 * created it or not and in this case we assume the caller does not
	 * care so we delete it to conserve disk space.
	 */
	if (!err && !uio_resid(uio)) {
		/*
		 * Shrink the attribute if the new value is smaller than the
		 * old value.  We do not do this for the resource fork as that
		 * is a special case.
		 */
		if (ntfs_name != NTFS_SFM_RESOURCEFORK_NAME) {
			if (size > start_count) {
				err = ntfs_attr_resize(ani, start_count, 0,
						NULL);
				if (err) {
					ntfs_error(vol->mp, "Failed to resize "
							"extended attribute "
							"to its new size "
							"(error %d).", err);
					goto undo_err;
				}
			}
		}
	} else {
		/*
		 * FIXME: Should we be trying to continue a partial write in
		 * case we can complete it with multiple calls to ntfs_write()?
		 */
		if (!err) {
			ntfs_debug("ntfs_write() returned a partial write, "
					"pretending the write never happened "
					"and removing or truncating to zero "
					"size the old attribute value.");
			err = EIO;
		}
undo_err:
		uio_setoffset(uio, start_ofs);
		uio_setresid(uio, start_count);
		if (options & XATTR_REPLACE) {
			errno_t err2;

			err2 = ntfs_attr_resize(ani, 0, 0, NULL);
			if (err2) {
				ntfs_error(vol->mp, "Failed to truncate "
						"extended attribute to zero "
						"size in error code path "
						"(error %d), attempting to "
						"delete it instead.", err2);
				goto rm_err;
			}
		} else {
rm_err:
			/*
			 * Unlink the named stream.  The last close will cause
			 * the VFS to call ntfs_vnop_inactive() which will do
			 * the actual removal.
			 */
			ani->link_count = 0;
			/*
			 * Update the last_mft_change_time (ctime) in the inode
			 * as named stream/extended attribute semantics expect
			 * on OS X.
			 */
			ni->last_mft_change_time = ntfs_utc_current_time();
			NInoSetDirtyTimes(ni);
			/*
			 * If this is not a directory or it is an encrypted
			 * directory, set the needs archiving bit except for
			 * the core system files.
			 */
			if (!S_ISDIR(ni->mode) || NInoEncrypted(ni)) {
				BOOL need_set_archive_bit = TRUE;
				if (ni->vol->major_ver >= 2) {
					if (ni->mft_no <= FILE_Extend)
						need_set_archive_bit = FALSE;
				} else {
					if (ni->mft_no <= FILE_UpCase)
						need_set_archive_bit = FALSE;
				}
				if (need_set_archive_bit) {
					ni->file_attributes |=
							FILE_ATTR_ARCHIVE;
					NInoSetDirtyFileAttributes(ni);
				}
			}
		}
	}
	lck_rw_unlock_exclusive(&ani->lock);
	(void)vnode_put(ani->vn);
err:
	lck_rw_unlock_exclusive(&ni->lock);
	ntfs_debug("Done (error %d).", err);
	return err;
}

/**
 * ntfs_vnop_removexattr - remove an extended attribute from an ntfs inode
 * @a:		arguments to removexattr function
 *
 * @a contains:
 *	vnode_t a_vp;	vnode whose extended attribute to remove
 *	char *a_name;	name of extented attribute to remove in utf8
 *	int a_options;	flags controlling how the attribute is removed
 *	vfs_context_t a_context;
 *
 * Remove the named stream with the name @a->a_name (we map named streams 1:1
 * with extended attributes for NTFS as the NTFS native EAs are useless) from
 * the vnode @a->a_vp.
 *
 * The flags in @a->a_options control how the attribute is set.  The following
 * flags are currently defined in OS X kernel:
 *	XATTR_NOFOLLOW	- Do not follow symbolic links.
 *	XATTR_CREATE	- Set the value, fail if already exists (setxattr only).
 *	XATTR_REPLACE	- Set the value, fail if does not exist (setxattr only).
 *	XATTR_NOSECURITY- Bypass authorization checking.
 *	XATTR_NODEFAULT	- Bypass default extended attribute file ('._' file).
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_removexattr(struct vnop_removexattr_args *a)
{
	ntfs_inode *ani, *ni = NTFS_I(a->a_vp);
	const char *name = a->a_name;
	ntfs_volume *vol;
	ntfschar *ntfs_name;
	size_t ntfs_name_size;
	signed ntfs_name_len;
	errno_t err;
	ntfschar ntfs_name_buf[NTFS_MAX_ATTR_NAME_LEN];

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	/* Check for invalid names. */
	if (!name || name[0] == '\0')
		return EINVAL;
	ntfs_debug("Entering for mft_no 0x%llx, extended attribute name %s, "
			"options 0x%x.", (unsigned long long)ni->mft_no, name,
			a->a_options);
	/*
	 * Access to extended attributes must be atomic which we ensure by
	 * locking the base ntfs inode for writing.
	 */
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	/*
	 * Only regular files, directories, and symbolic links can have
	 * extended attributes.  (Specifically named streams cannot have them.)
	 *
	 * Thus the check is for attribute inodes as all base inodes are
	 * allowed.  Raw inodes are also attribute inodes so they are excluded
	 * automatically, too.
	 */
	if (NInoAttr(ni)) {
		ntfs_debug("Mft_no 0x%llx is an attribute inode.",
				(unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	/*
	 * First of all deal with requests to remove the Finder info as that is
	 * special because we cache it in the base ntfs inode @ni thus we need
	 * to zero the cached Finder info and then write the changes out to the
	 * AFP_AfpInfo attribute (deleting it if it is no longer needed).  This
	 * is sufficient as a zero Finder info is treated the same as
	 * non-existent Finder info and vice versa.
	 *
	 * Note if the Finder info is already zero it does not exist thus we
	 * need to return ENOATTR instead thus we may need to load the Finder
	 * info first to find out whether it is zero or not.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME))) {
		FINDER_INFO fi;

		if (!NInoValidFinderInfo(ni)) {
			/*
			 * Load the AFP_AfpInfo stream and initialize the
			 * backup time and Finder info (at least the Finder
			 * info is not yet valid).
			 */
			err = ntfs_inode_afpinfo_read(ni);
			if (err) {
				ntfs_error(vol->mp, "Failed to obtain AfpInfo "
						"for mft_no 0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				goto err;
			}
			if (!NInoValidFinderInfo(ni))
				panic("%s(): !NInoValidFinderInfo(ni)\n",
						__FUNCTION__);
		}
		/*
		 * Make a copy of the Finder info and mask out the hidden bit
		 * if this is the root directory and the type and creator if
		 * this is a symbolic link.
		 */
		memcpy(&fi, &ni->finder_info, sizeof(fi));
		if (ni == vol->root_ni)
			fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
		if (S_ISLNK(ni->mode)) {
			fi.type = 0;
			fi.creator = 0;
		}
		if (!bcmp(&fi, &ntfs_empty_finder_info, sizeof(fi))) {
			/* Finder info is zero, i.e. it does not exist. */
			ntfs_debug("Mft_no 0x%llx has zero Finder info, "
					"returning ENOATTR.",
					(unsigned long long)ni->mft_no);
			err = ENOATTR;
			goto err;
		}
		/* Zero the Finder info. */
		bzero(&ni->finder_info, sizeof(ni->finder_info));
		/*
		 * If the file is hidden, we need to reflect this fact in the
		 * Finder info, too.
		 */
		if (ni->file_attributes & FILE_ATTR_HIDDEN)
			ni->finder_info.attrs |= FINDER_ATTR_IS_HIDDEN;
		/*
		 * Also, enforce the type and creator if this is a symbolic
		 * link to be our private values for symbolic links.  This in
		 * fact causes the Finder info not to be deleted on disk and we
		 * cannot allow that to happen as we would then no longer know
		 * that this is a symbolic link.
		 */
		if (S_ISLNK(ni->mode)) {
			ni->finder_info.type = FINDER_TYPE_SYMBOLIC_LINK;
			ni->finder_info.creator = FINDER_CREATOR_SYMBOLIC_LINK;
		}
		NInoSetValidFinderInfo(ni);
		NInoSetDirtyFinderInfo(ni);
		/*
		 * Updating the Finder info causes both the
		 * last_data_change_time (mtime) and last_mft_change_time
		 * (ctime) to be updated.
		 */
		ni->last_mft_change_time = ni->last_data_change_time =
				ntfs_utc_current_time();
		NInoSetDirtyTimes(ni);
		/* Now write (if needed deleting) the AFP_AfpInfo attribute. */
		err = ntfs_inode_afpinfo_write(ni);
		if (!err)
			ntfs_debug("Deleted Finder info from mft_no 0x%llx.",
					(unsigned long long)ni->mft_no);
		else
			ntfs_error(vol->mp, "Failed to write/delete "
					"AFP_AfpInfo attribute in inode "
					"0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
		goto err;
	}
	/*
	 * Now deal with requests to remove the resource fork as that is
	 * special because we need to translate its name from
	 * XATTR_RESOURCEFORK_NAME to AFP_Resource so we do not need to convert
	 * the utf8 name @name to Unicode.
	 *
	 * FIXME: This comparison is case sensitive.
	 */
	if (!bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_name = NTFS_SFM_RESOURCEFORK_NAME;
		ntfs_name_len = 12;
	} else {
		/*
		 * The request is not for the resource fork (nor for the Finder
		 * info).
		 *
		 * Convert the requested name from utf8 to Unicode.
		 */
		ntfs_name = ntfs_name_buf;
		ntfs_name_size = sizeof(ntfs_name_buf);
		ntfs_name_len = utf8_to_ntfs(vol, (const u8*)name, strlen(name),
				&ntfs_name, &ntfs_name_size);
		if (ntfs_name_len < 0) {
			err = -ntfs_name_len;
			if (err == ENAMETOOLONG)
				ntfs_debug("Failed (name is too long).");
			else
				ntfs_error(vol->mp, "Failed to convert name to "
						"Unicode (error %d).", err);
			goto err;
		}
		/*
		 * If this is one of the SFM named streams, skip it, as they
		 * contain effectively metadata information so should not be
		 * exposed directly.
		 */
		if (ntfs_is_sfm_name(vol, ntfs_name, ntfs_name_len)) {
			ntfs_debug("Not allowing access to protected SFM name "
					"%s in mft_no 0x%llx (returning "
					"EINVAL).", name,
					(unsigned long long)ni->mft_no);
			err = EINVAL;
			goto err;
		}
	}
	/*
	 * We now have the name of the requested attribute in @ntfs_name and it
	 * is @ntfs_name_len characters long.
	 *
	 * Get the ntfs attribute inode of the $DATA:@ntfs_name attribute.
	 */
	err = ntfs_attr_inode_get(ni, AT_DATA, ntfs_name, ntfs_name_len, FALSE,
			LCK_RW_TYPE_EXCLUSIVE, &ani);
	if (err) {
		if (err == ENOENT)
			err = ENOATTR;
		else if (err != ENOATTR)
			ntfs_error(vol->mp, "Failed to get $DATA/%s attribute "
					"inode mft_no 0x%llx (error %d).",
					name, (unsigned long long)ni->mft_no,
					err);
		goto err;
	}
	/*
	 * Unlink the named stream.  The last close will cause the VFS to call
	 * ntfs_vnop_inactive() which will do the actual removal.
	 */
	ani->link_count = 0;
	/*
	 * Update the last_mft_change_time (ctime) in the inode as named
	 * stream/extended attribute semantics expect on OS X.
	 */
	ni->last_mft_change_time = ntfs_utc_current_time();
	NInoSetDirtyTimes(ni);
	/*
	 * If this is not a directory or it is an encrypted directory, set the
	 * needs archiving bit except for the core system files.
	 */
	if (!S_ISDIR(ni->mode) || NInoEncrypted(ni)) {
		BOOL need_set_archive_bit = TRUE;
		if (ni->vol->major_ver >= 2) {
			if (ni->mft_no <= FILE_Extend)
				need_set_archive_bit = FALSE;
		} else {
			if (ni->mft_no <= FILE_UpCase)
				need_set_archive_bit = FALSE;
		}
		if (need_set_archive_bit) {
			ni->file_attributes |= FILE_ATTR_ARCHIVE;
			NInoSetDirtyFileAttributes(ni);
		}
	}
	ntfs_debug("Done.");
	lck_rw_unlock_exclusive(&ani->lock);
	(void)vnode_put(ani->vn);
err:
	lck_rw_unlock_exclusive(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_listxattr - list the names of the extended attributes of an inode
 * @args:		arguments to listxattr function
 *
 * @args contains:
 *	vnode_t a_vp;	vnode whose extended attributes to list
 *	uio_t a_uio;	destination in which to return the list
 *	size_t *a_size;	size of the list of extended attributes in bytes
 *	int a_options;	flags controlling how the attribute list is generated
 *	vfs_context_t a_context;
 *
 * Iterate over the list of named streams (which we map 1:1 with extended
 * attributes for NTFS as the NTFS native EAs are useless) in the vnode
 * @args->a_vp and for each encountered stream copy its name (converted to an
 * NULL-terminated utf8 string) to the destination as specified by
 * @args->a_uio.
 *
 * If @args->a_uio is NULL, do not copy anything and simply iterate over all
 * named streams and add up the number of bytes needed to create a full list of
 * their names and return that in *@args->a_size.  Note that when @args->a_uio
 * is not NULL @args->a_size is ignored as the number of bytes is implicitly
 * returned in the @args->a_uio and it can be obtained by taking the original
 * buffer size and subtracting uio_resid(@args->a_uio) from it.
 *
 * The flags in @args->a_options control how the attribute list is generated.
 * The following flags are currently defined in OS X kernel:
 *	XATTR_NOFOLLOW	- Do not follow symbolic links.
 *	XATTR_CREATE	- Set the value, fail if already exists (setxattr only).
 *	XATTR_REPLACE	- Set the value, fail if does not exist (setxattr only).
 *	XATTR_NOSECURITY- Bypass authorization checking.
 *	XATTR_NODEFAULT	- Bypass default extended attribute file ('._' file).
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_listxattr(struct vnop_listxattr_args *args)
{
	ntfs_inode *ni = NTFS_I(args->a_vp);
	uio_t uio = args->a_uio;
	ntfs_volume *vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	u8 *utf8_name;
	ntfschar *upcase;
	unsigned upcase_len;
	size_t size, utf8_size;
	errno_t err;
	BOOL case_sensitive;
	FINDER_INFO fi;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	upcase = vol->upcase;
	upcase_len = vol->upcase_len;
	case_sensitive = NVolCaseSensitive(vol);
	ntfs_debug("Entering.");
	lck_rw_lock_shared(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	/*
	 * Only regular files, directories, and symbolic links can have
	 * extended attributes.  (Specifically named streams cannot have them.)
	 *
	 * Thus the check is for attribute inodes as all base inodes are
	 * allowed.  Raw inodes are also attribute inodes so they are excluded
	 * automatically, too.
	 */
	if (NInoAttr(ni)) {
		ntfs_debug("Mft_no 0x%llx is an attribute inode.",
				(unsigned long long)ni->mft_no);
		err = EPERM;
		goto err;
	}
	size = 0;
	/*
	 * First of all deal with the Finder info as that is special because we
	 * cache it in the base ntfs inode @ni and we only want to export the
	 * name for the Finder info, XATTR_FINDERINFO_NAME, if the Finder info
	 * is non-zero.  This is what HFS does, too.
	 *
	 * Thus we need to check the status of the cache in the ntfs inode
	 * first and if that it valid we can use it to check the content of the
	 * Finder info for being zero.  And if it is not valid then it must be
	 * non-resident in which case we need to read it into the cache in the
	 * ntfs inode and then we can check the Finder info in the cache for
	 * being zero.  In fact we do this the other way round, i.e. if the
	 * Finder info cache is not valid we read the Finder info into the
	 * cache first and then the cache is definitely valid thus we can check
	 * the Finder info for being non-zero and export XATTR_FINDERINFO_NAME
	 * if so.
	 */
	if (!NInoValidFinderInfo(ni)) {
		if (!lck_rw_lock_shared_to_exclusive(&ni->lock)) {
			lck_rw_lock_exclusive(&ni->lock);
			if (NInoDeleted(ni)) {
				cache_purge(ni->vn);
				lck_rw_unlock_exclusive(&ni->lock);
				ntfs_debug("Mft_no 0x%llx is deleted.",
						(unsigned long long)ni->mft_no);
				return ENOENT;
			}
		}
		/*
		 * Load the AFP_AfpInfo stream and initialize the backup time
		 * and Finder info (if they are not already valid).
		 */
		err = ntfs_inode_afpinfo_read(ni);
		if (err) {
			ntfs_error(vol->mp, "Failed to obtain AfpInfo for "
					"mft_no 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
			lck_rw_unlock_exclusive(&ni->lock);
			return err;
		}
		if (!NInoValidFinderInfo(ni))
			panic("%s(): !NInoValidFinderInfo(ni)\n", __FUNCTION__);
		lck_rw_lock_exclusive_to_shared(&ni->lock);
	}
	/*
	 * Make a copy of the Finder info and mask out the hidden bit if this
	 * is the root directory and the type and creator if this is a symbolic
	 * link.
	 */
	memcpy(&fi, &ni->finder_info, sizeof(fi));
	if (ni == vol->root_ni)
		fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
	if (S_ISLNK(ni->mode)) {
		fi.type = 0;
		fi.creator = 0;
	}
	if (bcmp(&fi, &ntfs_empty_finder_info, sizeof(fi))) {
		if (!uio)
			size += sizeof(XATTR_FINDERINFO_NAME);
		else if (uio_resid(uio) <
				(user_ssize_t)sizeof(XATTR_FINDERINFO_NAME)) {
			err = ERANGE;
			goto err;
		} else {
			err = uiomove((caddr_t)XATTR_FINDERINFO_NAME,
					sizeof(XATTR_FINDERINFO_NAME), uio);
			if (err) {
				ntfs_error(vol->mp, "uiomove() failed (error "
						"%d).", err);
				goto err;
			}
		}
		ntfs_debug("Exporting Finder info name %s.",
				XATTR_FINDERINFO_NAME);
	}
	/* Iterate over all the named $DATA attributes. */
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
	/*
	 * Allocate a buffer we can use when converting the names of the named
	 * $DATA attributes to utf8.  We want enough space to definitely be
	 * able to convert the name as well as a byte for the NULL terminator.
	 */
	utf8_size = NTFS_MAX_ATTR_NAME_LEN * 4 + 1;
	utf8_name = IOMallocData(utf8_size);
	if (!utf8_name) {
		ntfs_error(vol->mp, "Failed to allocate name buffer.");
		err = ENOMEM;
		goto put_err;
	}
	do {
		ntfs_inode *ani;
		ATTR_RECORD *a;
		ntfschar *name;
		unsigned name_len;
		signed utf8_len;

		/* Get the next $DATA attribute. */
		err = ntfs_attr_lookup(AT_DATA, NULL, 0, 0, NULL, 0, ctx);
		if (err) {
			if (err == ENOENT) {
				err = 0;
				break;
			}
			ntfs_error(vol->mp, "Failed to iterate over named "
					"$DATA attributes (error %d).", err);
			goto free_err;
		}
		/* Got the next attribute, deal with it. */
		a = ctx->a;
		/* If this is the unnamed $DATA attribute, skip it. */
		if (!a->name_length) {
			ntfs_debug("Skipping unnamed $DATA attribute.");
			continue;
		}
		name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		name_len = a->name_length;
		if ((u8*)name < (u8*)a || (u8*)name + name_len > (u8*)a +
				le32_to_cpu(a->length)) {
			ntfs_error(vol->mp, "Found corrupt named $DATA "
					"attribute.  Run chkdsk.");
			NVolSetErrors(vol);
			err = EIO;
			goto free_err;
		}
		/*
		 * Check if this attribute currently has a cached inode/vnode
		 * and if so check if it has been unlinked/deleted and if so
		 * skip it.
		 */
		err = ntfs_attr_inode_lookup(ni, a->type, name, name_len,
				FALSE, &ani);
		if (err != ENOENT) {
			BOOL skip_it;

			if (err)
				panic("%s() inode lookup failed (error %d).\n",
						__FUNCTION__, err);
			/* Got the cached attribute inode. */
			skip_it = FALSE;
			if (NInoDeleted(ani) || !ani->link_count ||
					(ntfs_are_names_equal(name, name_len,
					NTFS_SFM_RESOURCEFORK_NAME, 12,
					case_sensitive, upcase, upcase_len) &&
					!ubc_getsize(ani->vn)))
				skip_it = TRUE;
			if (skip_it) {
				if (NInoDeleted(ani) || !ani->link_count)
					ntfs_debug("Skipping deleted/unlinked "
							"attribute.");
				else
					ntfs_debug("Mft_no 0x%llx has zero "
							"size resource fork, "
							"pretending it does "
							"not exist.",
							(unsigned long long)
							ani->mft_no);
				(void)vnode_put(ani->vn);
				continue;
			}
			(void)vnode_put(ani->vn);
		}
		/*
		 * If AFP_Resource named stream exists, i.e. the resource fork
		 * is present, and it is non-empty export the name
		 * XATTR_RESOURCEFORK_NAME.  This is what HFS does, too.
		 */
		if (ntfs_are_names_equal(name, name_len,
				NTFS_SFM_RESOURCEFORK_NAME, 12, case_sensitive,
				upcase, upcase_len)) {
			if (!ntfs_attr_size(a)) {
				ntfs_debug("Skipping empty resource fork "
						"name %s.",
						XATTR_RESOURCEFORK_NAME);
				continue;
			}
			if (!uio)
				size += sizeof(XATTR_RESOURCEFORK_NAME);
			else if (uio_resid(uio) < (user_ssize_t)sizeof(
					XATTR_RESOURCEFORK_NAME)) {
				err = ERANGE;
				goto free_err;
			} else {
				err = uiomove((caddr_t)XATTR_RESOURCEFORK_NAME,
						sizeof(XATTR_RESOURCEFORK_NAME),
						uio);
				if (err) {
					ntfs_error(vol->mp, "uiomove() failed "
							"(error %d).", err);
					goto free_err;
				}
			}
			ntfs_debug("Exporting resource fork name %s.",
					XATTR_RESOURCEFORK_NAME);
			continue;
		}
		/*
		 * If this is one of the SFM named streams, skip it, as they
		 * contain effectively metadata information so should not be
		 * exposed directly.
		 */
		if (ntfs_is_sfm_name(vol, name, name_len)) {
			ntfs_debug("Skipping protected SFM name.");
			continue;
		}
		/* Convert the name to utf8. */
		utf8_len = ntfs_to_utf8(vol, name, name_len <<
				NTFSCHAR_SIZE_SHIFT, &utf8_name, &utf8_size);
		if (utf8_len < 0) {
			ntfs_warning(vol->mp, "Skipping unrepresentable name "
					"in mft_no 0x%llx (error %d).",
					(unsigned long long)ni->mft_no,
					-utf8_len);
			continue;
		}
		/*
		 * If this is a protected attribute, skip it.
		 *
		 * FIXME: xattr_protected() is case sensitive so it does not
		 * exclude protected attributes when they are not correctly
		 * cased on disk.
		 *
		 * However we do call it to be consistent with HFS and SMB but
		 * it is pointless as anyone can call getxattr() for a case
		 * variant and the getxattr() system call would use
		 * xattr_protected() which would not filter it out so the
		 * VNOP_GETXATTR() call would happen and we would return the
		 * attribute just fine.  Simillarly anyone could set and remove
		 * such "protected" attributes by just calling the system call
		 * with a case variant even when they are correctly filtered
		 * out here.
		 */
		if (xattr_protected((char*)utf8_name)) {
			ntfs_debug("Skipping protected name %.*s.", utf8_len,
					utf8_name);
			continue;
		}
		/*
		 * Increment the length of the name by one for the NULL
		 * terminator.
		 */
		utf8_len++;
		/* Export the utf8_name. */
		if (!uio)
			size += utf8_len;
		else if (uio_resid(uio) < utf8_len) {
			err = ERANGE;
			goto free_err;
		} else {
			err = uiomove((caddr_t)utf8_name, utf8_len, uio);
			if (err) {
				ntfs_error(vol->mp, "uiomove() failed (error "
						"%d).", err);
				goto free_err;
			}
		}
		ntfs_debug("Exporting name %.*s.", utf8_len, utf8_name);
		/* Continue to the next name. */
	} while (1);
	if (!uio)
		*args->a_size = size;
	ntfs_debug("Done.");
free_err:
	IOFreeData(utf8_name, utf8_size);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(ni);
err:
	lck_rw_unlock_shared(&ni->lock);
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
	ntfs_volume *vol;
	unsigned block_size_shift;

	if (!a->a_vp) {
		ntfs_warning(NULL, "Called with NULL vnode!");
		return EINVAL;
	}
	ni = NTFS_I(a->a_vp);
	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	ntfs_debug("Entering for logical block 0x%llx, mft_no 0x%llx, type "
			"0x%x, name_len 0x%x.", (unsigned long long)a->a_lblkno,
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	vol = ni->vol;
	block_size_shift = PAGE_SHIFT;
	/*
	 * For $MFT/$DATA and $MFTMirr/$DATA the logical block number is the
	 * mft record number and the block size is the mft record size which is
	 * also in @ni->block_size{,_shift}.
	 */
	if (ni == vol->mft_ni || ni == vol->mftmirr_ni)
		block_size_shift = ni->block_size_shift;
	*a->a_offset = a->a_lblkno << block_size_shift;
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
	ntfs_volume *vol;
	unsigned block_size_shift;

	if (!a->a_vp) {
		ntfs_warning(NULL, "Called with NULL vnode.");
		return EINVAL;
	}
	ni = NTFS_I(a->a_vp);
	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	ntfs_debug("Entering for byte offset 0x%llx, mft_no 0x%llx, type "
			"0x%x, name_len 0x%x.", (unsigned long long)a->a_offset,
			(unsigned long long)ni->mft_no, le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	vol = ni->vol;
	block_size_shift = PAGE_SHIFT;
	/*
	 * For $MFT/$DATA and $MFTMirr/$DATA the logical block number is the
	 * mft record number and the block size is the mft record size which is
	 * also in @ni->block_size{,_shift}.
	 */
	if (ni == vol->mft_ni || ni == vol->mftmirr_ni)
		block_size_shift = ni->block_size_shift;
	*a->a_lblkno = a->a_offset >> block_size_shift;
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
 * FIXME: At present the OS X kernel completely ignores @a->a_poff and in fact
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
 * In the read case and when @a->a_flags is zero, if @a->a_foffset is beyond
 * the end of the attribute, return error ERANGE.  HFS returns ERANGE in this
 * case so we follow suit.  Although some other OS X file systems return EFBIG
 * and some E2BIG instead so it does not seem to be very standardized, so maybe
 * we should return the IMHO more correct "invalid seek" (ESPIPE), instead. (-;
 *
 * In the write case we need to allow the mapping of blocks beyond the end of
 * the attribute as we will already have extended the allocated size but not
 * yet the data size nor the initialized size.  Thus in this case we only
 * return ERANGE if the requested @a->a_foffset is beyond the end of the
 * allocated size.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_vnop_blockmap(struct vnop_blockmap_args *a)
{
	const s64 byte_offset = a->a_foffset;
	const s64 byte_size = a->a_size;
	s64 max_size, data_size, init_size, clusters, bytes = 0;
	VCN vcn;
	LCN lcn;
	ntfs_inode *ni = NTFS_I(a->a_vp);
	ntfs_volume *vol;
	unsigned vcn_ofs;
	BOOL is_write = (a->a_flags & VNODE_WRITE);

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vol = ni->vol;
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"offset 0x%llx, size 0x%llx, for %s operation.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned)ni->name_len,
			(unsigned long long)byte_offset,
			(unsigned long long)byte_size,
			a->a_flags ? (is_write ? "write" : "read") :
			"unspecified");
	if (S_ISDIR(ni->mode)) {
		ntfs_error(vol->mp, "Called for directory vnode.");
		return EINVAL;
	}
	if (is_write && NVolReadOnly(vol)) {
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
	 * We cannot take the inode lock as it may be held already so we just
	 * check the deleted bit and abort if it is set which is better than
	 * nothing.
	 */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		ntfs_debug("Inode has been deleted.");
		return ENOENT;
	}
	/*
	 * Note it does not matter if we are racing with truncate because that
	 * will be detected during the runlist lookup below.
	 */
	lck_spin_lock(&ni->size_lock);
	if (is_write)
		max_size = ni->allocated_size;
	else
		max_size = ni->data_size;
	data_size = ni->data_size;
	init_size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	if (byte_offset >= max_size) {
eof:
		ntfs_error(vol->mp, "Called for inode 0x%llx, size 0x%llx, "
				"byte offset 0x%llx, for %s operation, which "
				"is beyond the end of the inode %s size "
				"0x%llx.  Returning error: ERANGE.",
				(unsigned long long)ni->mft_no,
				(unsigned long long)byte_size,
				(unsigned long long)byte_offset, a->a_flags ?
				(is_write ? "write" : "read") : "unspecified",
				is_write ? "allocated" : "data",
				(unsigned long long)max_size);
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
	/*
	 * In the read case, if the requested byte offset is at or beyond the
	 * initialized size simply return a hole.  We already checked for being
	 * at or beyond the data size so we know we are in an uninitialized
	 * region in this case rather than at or beyond the end of the
	 * attribute.
	 */
	if (!is_write && byte_offset >= init_size) {
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
		 * It is a hole, return it.  If this is a VNODE_WRITE request,
		 * output a warning as this should never happen.  Both
		 * VNOP_WRITE() and VNOP_PAGEOUT() should have instantiated the
		 * hole before performing the write.
		 *
		 * Note we could potentially fill the hole here in the write
		 * case.  However this is quite hard to do as the caller will
		 * likely have pages around the hole locked in UBC UPLs thus we
		 * would have difficulties zeroing the surrounding regions when
		 * the cluster size is larger than the page size.  Also a
		 * problem is what happens if the write fails for some reason
		 * but we have instantiated the hole here and not zeroed it
		 * completely (because we are expecting the write to go into
		 * the allocated clusters).  We would have no way of fixing up
		 * in this case and we would end up exposing stale data.  This
		 * all is why we choose not to fill the hole here but to do it
		 * in advance in ntfs_vnop_write() and ntfs_vnop_pageout().
		 *
		 * The only thing that will happen when we return a hole in the
		 * write case is that when the caller is cluster_io(), it will
		 * page out page by page and this will fill the hole in pieces
		 * which will degrade performance.
		 */
		if (is_write)
			ntfs_warning(vol->mp, "Returning hole but flags "
					"specify VNODE_WRITE.  This causes "
					"very inefficient allocation and I/O "
					"patterns.");
		/* Return the hole. */
		lck_rw_unlock_shared(&ni->rl.lock);
		*a->a_bpn = -1; /* -1 means hole. */
		if (a->a_run) {
			bytes = (clusters << vol->cluster_size_shift) - vcn_ofs;
			/*
			 * If the run overlaps the initialized size, extend the
			 * run length so it goes up to the data size thus
			 * merging the hole with the uninitialized region.
			 *
			 * Note, do not do this in the write case as we want to
			 * return the real clusters even beyond the initialized
			 * size as the initialized size will only be updated
			 * after the write has completed.
			 */
			if (!is_write && byte_offset + bytes > init_size)
				bytes = data_size - byte_offset;
		}
		goto done;
	} else
		lck_rw_unlock_shared(&ni->rl.lock);
	/* The vcn was mapped successfully to a physical lcn, return it. */
	*a->a_bpn = ((lcn << vol->cluster_size_shift) + vcn_ofs) >>
			vol->sector_size_shift;
	if (a->a_run) {
		bytes = (clusters << vol->cluster_size_shift) - vcn_ofs;
		/*
		 * In the read case, if the run overlaps the initialized size,
		 * truncate the run length so it only goes up to the
		 * initialized size.  The caller will then be able to access
		 * this region on disk directly and will then call us again
		 * with a byte offset equal to the initialized size and we will
		 * then return the entire initialized region as a hole.  Thus
		 * the caller does not need to know about the fact that NTFS
		 * has such a thing as the initialized_size.
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
		if (!is_write && byte_offset + bytes > init_size &&
				init_size < data_size)
			bytes = init_size - byte_offset;
	}
done:
	if (a->a_run) {
		if (bytes > byte_size)
			bytes = byte_size;
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
 * ntfs_vnop_getnamedstream - find a named stream in an inode given its name
 * @a:		arguments to getnamedstream function
 *
 * @a contains:
 *	vnode_t a_vp;			vnode containing the named stream
 *	vnode_t *a_svpp;		destination for the named stream vnode
 *	const char *a_name;		name of the named stream to get
 *	enum nsoperation a_operation;	reason for getnamedstream
 *	int a_flags;			flags describing the request
 *	vfs_context_t a_context;
 *
 * Find the named stream with name @a->a_name in the vnode @a->a_vp and return
 * the vnode of the named stream in *@a->a_svpp if it was found.
 *
 * @a->a_operation specifies the reason for the lookup of the named stream.
 * The following operations are currently defined in OS X kernel:
 *	NS_OPEN	  - Want to open the named stream for access.
 *	NS_CREATE - Want to create the named stream so checking it does not
 *		    exist already.
 *	NS_DELETE - Want to delete the named stream so making sure it exists.
 *
 * The flags in @a->a_flags further describe the getnamedstream request.  At
 * present no flags are defined in OS X kernel.
 *
 * Note that at present Mac OS X only supports the "com.apple.ResourceFork"
 * stream so we follow suit.
 *
 * Return 0 on success and the error code on error.  A return value of ENOATTR
 * does not signify an error as such but merely the fact that the named stream
 * @name is not present in the vnode @a->a_vp.
 */
static int ntfs_vnop_getnamedstream(struct vnop_getnamedstream_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *sni, *ni = NTFS_I(vn);
	const char *name = a->a_name;
	int options;
	const enum nsoperation op = a->a_operation;
	errno_t err;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Entering for mft_no 0x%llx, stream name %s, operation %s "
			"(0x%x), flags 0x%x.", (unsigned long long)ni->mft_no,
			name, op == NS_OPEN ? "NS_OPEN" :
			(op == NS_CREATE ? "NS_CREATE" :
			(op == NS_DELETE ? "NS_DELETE" : "unknown")), op,
			a->a_flags);
	/*
	 * Mac OS X only supports the resource fork stream.
	 * Note that this comparison is case sensitive.
	 */
	if (bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_warning(ni->vol->mp, "Unsupported named stream %s "
				"specified, only the resource fork named "
				"stream (%s) is supported at present.  "
				"Returning ENOATTR.", name,
				XATTR_RESOURCEFORK_NAME);
		return ENOATTR;
	}
	/* Only regular files may have a resource fork stream. */
	if (!S_ISREG(ni->mode)) {
		ntfs_warning(ni->vol->mp, "The resource fork may only be "
				"attached to regular files and mft_no 0x%llx "
				"is not a regular file.  Returning EPERM.",
				(unsigned long long)ni->mft_no);
		return EPERM;
	}
	/*
	 * Attempt to get the inode for the named stream.  For the resource
	 * fork we need to return it even if it is zero size if the caller has
	 * specified @op == NS_OPEN so we set @options to zero in this case.
	 * Otherwise we want to treat a zero size resource fork as a
	 * non-existent resource fork se we set @options to XATTR_REPLACE which
	 * is the behaviour of ntfs_attr_inode_get().
	 */
	if (op == NS_OPEN) {
		options = 0;
		lck_rw_lock_exclusive(&ni->lock);
	} else {
		options = XATTR_REPLACE;
		lck_rw_lock_shared(&ni->lock);
	}
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(vn);
		if (op == NS_OPEN)
			lck_rw_unlock_exclusive(&ni->lock);
		else
			lck_rw_unlock_shared(&ni->lock);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		return ENOENT;
	}
	err = ntfs_attr_inode_get_or_create(ni, AT_DATA,
			NTFS_SFM_RESOURCEFORK_NAME, 12, FALSE, FALSE, options,
			LCK_RW_TYPE_SHARED, &sni);
	if (!err) {
		/* We have successfully opened the named stream. */
		*a->a_svpp = sni->vn;
		lck_rw_unlock_shared(&sni->lock);
		ntfs_debug("Done.");
	} else {
		if (err == ENOENT) {
			err = ENOATTR;
			ntfs_debug("Done (named stream %s does not exist in "
					"mft_no 0x%llx.", name,
					(unsigned long long)ni->mft_no);
		} else
			ntfs_error(ni->vol->mp, "Failed to get named stream "
					"%s, mft_no 0x%llx (error %d).", name,
					(unsigned long long)ni->mft_no, err);
	}
	if (op == NS_OPEN)
		lck_rw_unlock_exclusive(&ni->lock);
	else
		lck_rw_unlock_shared(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_makenamedstream - create a named stream in an ntfs inode
 * @a:		arguments to makenamedstream function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode in which to create the named stream
 *	vnode_t *a_svpp;	destination for the named stream vnode
 *	const char *a_name;	name of the named stream to create
 *	int a_flags;		flags describing the request
 *	vfs_context_t a_context;
 *
 * Create the named stream with name @a->a_name in the vnode @a->a_vp and
 * return the created vnode of the named stream in *@a->a_svpp.  If the named
 * stream already exists than it is obtained instead, i.e. if the named stream
 * already exists then ntfs_vnop_makenamedstream() does exactly the same thing
 * as ntfs_vnop_getnamedstream().
 *
 * The flags in @a->a_flags further describe the makenamedstream request.  At
 * present no flags are defined in OS X kernel.
 *
 * Note that at present Mac OS X only supports the "com.apple.ResourceFork"
 * stream so we follow suit.
 *
 * Return 0 on success and the error code on error.
 */
static int ntfs_vnop_makenamedstream(struct vnop_makenamedstream_args *a)
{
	vnode_t vn = a->a_vp;
	ntfs_inode *sni, *ni = NTFS_I(vn);
	const char *name = a->a_name;
	errno_t err;

	if (!ni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	ntfs_debug("Entering for mft_no 0x%llx, stream name %s, flags 0x%x.",
			(unsigned long long)ni->mft_no, name, a->a_flags);
	/*
	 * Mac OS X only supports the resource fork stream.
	 * Note that this comparison is case sensitive.
	 */
	if (bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_warning(ni->vol->mp, "Unsupported named stream %s "
				"specified, only the resource fork named "
				"stream (%s) is supported at present.  "
				"Returning ENOATTR.", name,
				XATTR_RESOURCEFORK_NAME);
		return ENOATTR;
	}
	/* Only regular files may have a resource fork stream. */
	if (!S_ISREG(ni->mode)) {
		ntfs_warning(ni->vol->mp, "The resource fork may only be "
				"attached to regular files and mft_no 0x%llx "
				"is not a regular file.  Returning EPERM.",
				(unsigned long long)ni->mft_no);
		return EPERM;
	}
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(vn);
		lck_rw_unlock_exclusive(&ni->lock);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		return ENOENT;
	}
	/*
	 * Attempt to create the named stream.
	 *
	 * HFS allows an existing resource fork to be opened.  We want to
	 * follow suit so we specify 0 for @options when calling
	 * ntfs_attr_inode_get_or_create().
	 *
	 * FIXME: I think this is actually wrong behaviour.  If I am right and
	 * this is one day fixed in HFS, then we can trivially fix the
	 * behaviour here by setting @options to XATTR_CREATE.
	 */
	err = ntfs_attr_inode_get_or_create(ni, AT_DATA,
			NTFS_SFM_RESOURCEFORK_NAME, 12, FALSE, FALSE, 0,
			LCK_RW_TYPE_SHARED, &sni);
	if (!err) {
		/* We have successfully opened the (created) named stream. */
		*a->a_svpp = sni->vn;
		lck_rw_unlock_shared(&sni->lock);
		ntfs_debug("Done.");
	} else {
		if (err == EEXIST)
			ntfs_debug("Named stream %s already exists in mft_no "
					"0x%llx.", name,
					(unsigned long long)ni->mft_no);
		else
			ntfs_error(ni->vol->mp, "Failed to create named "
					"stream %s in mft_no 0x%llx (error "
					"%d).", name,
					(unsigned long long)ni->mft_no, err);
	}
	lck_rw_unlock_exclusive(&ni->lock);
	return err;
}

/**
 * ntfs_vnop_removenamedstream - remove a named stream from an ntfs inode
 * @a:		arguments to removenamedstream function
 *
 * @a contains:
 *	vnode_t a_vp;		vnode from which to remove the named stream
 *	vnode_t a_svp;		vnode of named stream to remove
 *	const char *a_name;	name of the named stream to remove
 *	int a_flags;		flags describing the request
 *	vfs_context_t a_context;
 *
 * Delete the named stream described by the vnode @a->a_svp with name
 * @a->a_name from the vnode @a->a_vp.
 *
 * The flags in @a->a_flags further describe the removenamedstream request.  At
 * present no flags are defined in OS X kernel.
 *
 * Note we obey POSIX open unlink semantics thus an open named stream will
 * remain accessible for read/write/lseek purproses until the last open
 * instance is closed when the VFS will call ntfs_vnop_inactive() which will in
 * turn actually remove the named stream.
 *
 * Note that at present Mac OS X only supports the "com.apple.ResourceFork"
 * stream so we follow suit.
 *
 * Return 0 on success and the error code on error.  A return value of ENOATTR
 * does not signify an error as such but merely the fact that the named stream
 * @name is not present in the vnode @a->a_vp.
 */
static int ntfs_vnop_removenamedstream(struct vnop_removenamedstream_args *a)
{
	vnode_t svn, vn = a->a_vp;
	ntfs_inode *sni, *ni = NTFS_I(vn);
	const char *vname, *name = a->a_name;

	svn = a->a_svp;
	sni = NTFS_I(svn);
	if (!ni || !sni) {
		ntfs_debug("Entered with NULL ntfs_inode, aborting.");
		return EINVAL;
	}
	vname = vnode_getname(svn);
	ntfs_debug("Entering for mft_no 0x%llx, stream mft_no 0x%llx, stream "
			"name %s, flags 0x%x, stream vnode name %s.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)sni->mft_no, name, a->a_flags,
			vname ? vname : "not present");
	if (vname)
		(void)vnode_putname(vname);
	/*
	 * Mac OS X only supports the resource fork stream.
	 * Note that this comparison is case sensitive.
	 */
	if (bcmp(name, XATTR_RESOURCEFORK_NAME,
			sizeof(XATTR_RESOURCEFORK_NAME))) {
		ntfs_warning(ni->vol->mp, "Unsupported named stream %s "
				"specified, only the resource fork named "
				"stream (%s) is supported at present.  "
				"Returning ENOATTR.", name,
				XATTR_RESOURCEFORK_NAME);
		return ENOATTR;
	}
	/* Only regular files may have a resource fork stream. */
	if (!S_ISREG(ni->mode)) {
		ntfs_warning(ni->vol->mp, "The resource fork may only be "
				"attached to regular files and mft_no 0x%llx "
				"is not a regular file.  Returning EPERM.",
				(unsigned long long)ni->mft_no);
		return EPERM;
	}
	lck_rw_lock_exclusive(&ni->lock);
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(vn);
		lck_rw_unlock_exclusive(&ni->lock);
		ntfs_debug("Mft_no 0x%llx is deleted.",
				(unsigned long long)ni->mft_no);
		return ENOATTR;
	}
	lck_rw_lock_exclusive(&sni->lock);
	/* Do not allow messing with the stream once it has been deleted. */
	if (NInoDeleted(sni)) {
		/* Remove the inode from the name cache. */
		cache_purge(svn);
		lck_rw_unlock_exclusive(&sni->lock);
		lck_rw_unlock_exclusive(&ni->lock);
		ntfs_debug("Stream mft_no 0x%llx, name %s is deleted.",
				(unsigned long long)sni->mft_no, name);
		return ENOATTR;
	}
	/*
	 * The base inode of the stream inode must be the same as the parent
	 * inode specified by the caller.
	 */
	if (!NInoAttr(sni) || sni->base_ni != ni)
		panic("%s(): !NInoAttr(sni) || sni->base_ni != ni\n",
				__FUNCTION__);
	/*
	 * Unlink the named stream.  The last close will cause the VFS to call
	 * ntfs_vnop_inactive() which will do the actual removal.
	 *
	 * And if the named stream is already unlinked there is nothing to do.
	 * This is what HFS does so we follow suit.
	 */
	if (sni->link_count) {
		sni->link_count = 0;
		/*
		 * Update the last_mft_change_time (ctime) in the inode as
		 * named stream/extended attribute semantics expect on OS X.
		 */
		ni->last_mft_change_time = ntfs_utc_current_time();
		NInoSetDirtyTimes(ni);
		/*
		 * If this is not a directory or it is an encrypted directory,
		 * set the needs archiving bit except for the core system
		 * files.
		 */
		if (!S_ISDIR(ni->mode) || NInoEncrypted(ni)) {
			BOOL need_set_archive_bit = TRUE;
			if (ni->vol->major_ver >= 2) {
				if (ni->mft_no <= FILE_Extend)
					need_set_archive_bit = FALSE;
			} else {
				if (ni->mft_no <= FILE_UpCase)
					need_set_archive_bit = FALSE;
			}
			if (need_set_archive_bit) {
				ni->file_attributes |= FILE_ATTR_ARCHIVE;
				NInoSetDirtyFileAttributes(ni);
			}
		}
		ntfs_debug("Done.");
	} else
		ntfs_debug("$DATA/%s attribute has already been unlinked from "
				"mft_no 0x%llx.", name,
				(unsigned long long)sni->mft_no);
	lck_rw_unlock_exclusive(&sni->lock);
	lck_rw_unlock_exclusive(&ni->lock);
	return 0;
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
