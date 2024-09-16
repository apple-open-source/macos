/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	vnode_pager.c
 *
 *	"Swap" pager that pages to/from vnodes.  Also
 *	handles demand paging from files.
 *
 */

#include <mach/boolean.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/vnode_internal.h>
#include <sys/namei.h>
#include <sys/mount_internal.h> /* needs internal due to fhandle_t */
#include <sys/ubc_internal.h>
#include <sys/lock.h>
#include <sys/disk.h>           /* For DKIOC calls */

#include <mach/mach_types.h>
#include <mach/memory_object_types.h>
#include <mach/vm_map.h>
#include <mach/mach_vm.h>
#include <mach/upl.h>
#include <mach/sdt.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <kern/zalloc.h>
#include <libkern/libkern.h>

#include <vm/vnode_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_ubc.h>

#include <kern/assert.h>
#include <sys/kdebug.h>
#include <nfs/nfs.h>

#include <vm/vm_protos_internal.h>

#include <sys/kdebug.h>
#include <sys/kdebug_triage.h>
#include <vfs/vfs_disk_conditioner.h>

void
vnode_pager_throttle(void)
{
	if (current_uthread()->uu_lowpri_window) {
		throttle_lowpri_io(1);
	}
}

boolean_t
vnode_pager_isSSD(vnode_t vp)
{
	return disk_conditioner_mount_is_ssd(vp->v_mount);
}

#if FBDP_DEBUG_OBJECT_NO_PAGER
bool
vnode_pager_forced_unmount(vnode_t vp)
{
	mount_t mnt;
	mnt = vnode_mount(vp);
	if (!mnt) {
		return false;
	}
	return vfs_isforce(mnt);
}
#endif /* FBDP_DEBUG_OBJECT_NO_PAGER */

#if CONFIG_IOSCHED
void
vnode_pager_issue_reprioritize_io(struct vnode *devvp, uint64_t blkno, uint32_t len, int priority)
{
	u_int32_t blocksize = 0;
	dk_extent_t extent;
	dk_set_tier_t set_tier;
	int error = 0;

	error = VNOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t)&blocksize, 0, vfs_context_kernel());
	if (error) {
		return;
	}

	memset(&extent, 0, sizeof(dk_extent_t));
	memset(&set_tier, 0, sizeof(dk_set_tier_t));

	extent.offset = blkno * (u_int64_t) blocksize;
	extent.length = len;

	set_tier.extents = &extent;
	set_tier.extentsCount = 1;
	set_tier.tier = (uint8_t)priority;

	error = VNOP_IOCTL(devvp, DKIOCSETTIER, (caddr_t)&set_tier, 0, vfs_context_kernel());
	return;
}
#endif

void
vnode_pager_was_dirtied(
	struct vnode            *vp,
	vm_object_offset_t      s_offset,
	vm_object_offset_t      e_offset)
{
	cluster_update_state(vp, s_offset, e_offset, TRUE);
}

uint32_t
vnode_pager_isinuse(struct vnode *vp)
{
	if (vp->v_usecount > vp->v_kusecount) {
		return 1;
	}
	return 0;
}

uint32_t
vnode_pager_return_throttle_io_limit(struct vnode *vp, uint32_t *limit)
{
	return cluster_throttle_io_limit(vp, limit);
}

vm_object_offset_t
vnode_pager_get_filesize(struct vnode *vp)
{
	return (vm_object_offset_t) ubc_getsize(vp);
}

extern int safe_getpath(struct vnode *dvp, char *leafname, char *path, int _len, int *truncated_path);

kern_return_t
vnode_pager_get_name(
	struct vnode    *vp,
	char            *pathname,
	vm_size_t       pathname_len,
	char            *filename,
	vm_size_t       filename_len,
	boolean_t       *truncated_path_p)
{
	*truncated_path_p = FALSE;
	if (pathname != NULL) {
		/* get the path name */
		safe_getpath(vp, NULL,
		    pathname, (int) pathname_len,
		    truncated_path_p);
	}
	if ((pathname == NULL || *truncated_path_p) &&
	    filename != NULL) {
		/* get the file name */
		const char *name;

		name = vnode_getname_printable(vp);
		strlcpy(filename, name, (size_t) filename_len);
		vnode_putname_printable(name);
	}
	return KERN_SUCCESS;
}

kern_return_t
vnode_pager_get_mtime(
	struct vnode    *vp,
	struct timespec *current_mtime,
	struct timespec *cs_mtime)
{
	vnode_mtime(vp, current_mtime, vfs_context_current());
	if (cs_mtime != NULL) {
		ubc_get_cs_mtime(vp, cs_mtime);
	}
	return KERN_SUCCESS;
}

kern_return_t
vnode_pager_get_cs_blobs(
	struct vnode    *vp,
	void            **blobs)
{
	*blobs = ubc_get_cs_blobs(vp);
	return KERN_SUCCESS;
}

/*
 * vnode_trim:
 * Used to call the DKIOCUNMAP ioctl on the underlying disk device for the specified vnode.
 * Trims the region at offset bytes into the file, for length bytes.
 *
 * Care must be taken to ensure that the vnode is sufficiently reference counted at the time this
 * function is called; no iocounts or usecounts are taken on the vnode.
 * This function is non-idempotent in error cases;  We cannot un-discard the blocks if only some of them
 * are successfully discarded.
 */
u_int32_t
vnode_trim(
	struct vnode *vp,
	off_t offset,
	size_t length)
{
	daddr64_t io_blockno;    /* Block number corresponding to the start of the extent */
	size_t io_bytecount;    /* Number of bytes in current extent for the specified range */
	size_t trimmed = 0;
	off_t current_offset = offset;
	size_t remaining_length = length;
	int error = 0;
	u_int32_t blocksize = 0;
	struct vnode *devvp;
	dk_extent_t extent;
	dk_unmap_t unmap;


	/* Get the underlying device vnode */
	devvp = vp->v_mount->mnt_devvp;

	/* Figure out the underlying device block size */
	error  = VNOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t)&blocksize, 0, vfs_context_kernel());
	if (error) {
		goto trim_exit;
	}

	/*
	 * We may not get the entire range from offset -> offset+length in a single
	 * extent from the blockmap call.  Keep looping/going until we are sure we've hit
	 * the whole range or if we encounter an error.
	 */
	while (trimmed < length) {
		/*
		 * VNOP_BLOCKMAP will tell us the logical to physical block number mapping for the
		 * specified offset.  It returns blocks in contiguous chunks, so if the logical range is
		 * broken into multiple extents, it must be called multiple times, increasing the offset
		 * in each call to ensure that the entire range is covered.
		 */
		error = VNOP_BLOCKMAP(vp, current_offset, remaining_length,
		    &io_blockno, &io_bytecount, NULL, VNODE_READ | VNODE_BLOCKMAP_NO_TRACK, NULL);

		if (error) {
			goto trim_exit;
		}
		/*
		 * We have a contiguous run.  Prepare & issue the ioctl for the device.
		 * the DKIOCUNMAP ioctl takes offset in bytes from the start of the device.
		 */
		memset(&extent, 0, sizeof(dk_extent_t));
		memset(&unmap, 0, sizeof(dk_unmap_t));
		extent.offset = (uint64_t) io_blockno * (u_int64_t) blocksize;
		extent.length = io_bytecount;
		unmap.extents = &extent;
		unmap.extentsCount = 1;
		error = VNOP_IOCTL(devvp, DKIOCUNMAP, (caddr_t)&unmap, 0, vfs_context_kernel());

		if (error) {
			goto trim_exit;
		}
		remaining_length = remaining_length - io_bytecount;
		trimmed = trimmed + io_bytecount;
		current_offset = current_offset + io_bytecount;
	}
trim_exit:

	return error;
}

pager_return_t
vnode_pageout(struct vnode *vp,
    upl_t                   upl,
    upl_offset_t            upl_offset,
    vm_object_offset_t      f_offset,
    upl_size_t              size,
    int                     flags,
    int                     *errorp)
{
	int             result = PAGER_SUCCESS;
	int             error = 0;
	int             error_ret = 0;
	daddr64_t blkno;
	int isize;
	int pg_index;
	int base_index;
	upl_offset_t offset;
	upl_page_info_t *pl;
	vfs_context_t ctx = vfs_context_current();      /* pager context */

	isize = (int)size;

	/*
	 * This call is non-blocking and does not ever fail but it can
	 * only be made when there is other explicit synchronization
	 * with reclaiming of the vnode which, in this path, is provided
	 * by the paging in progress counter.
	 *
	 * In addition, this may also be entered via explicit ubc_msync
	 * calls or vm_swapfile_io where the existing iocount provides
	 * the necessary synchronization. Ideally we would not take an
	 * additional iocount here in the cases where an explcit iocount
	 * has already been taken but this call doesn't cause a deadlock
	 * as other forms of vnode_get* might if this thread has already
	 * taken an iocount.
	 */
	error = vnode_getalways_from_pager(vp);
	if (error != 0) {
		/* This can't happen */
		panic("vnode_getalways returned %d for vp %p", error, vp);
	}

	if (isize <= 0) {
		result    = PAGER_ERROR;
		error_ret = EINVAL;
		goto out;
	}

	if (UBCINFOEXISTS(vp) == 0) {
		result    = PAGER_ERROR;
		error_ret = EINVAL;

		if (upl && !(flags & UPL_NOCOMMIT)) {
			ubc_upl_abort_range(upl, upl_offset, size, UPL_ABORT_FREE_ON_EMPTY);
		}
		goto out;
	}
	if (!(flags & UPL_VNODE_PAGER)) {
		/*
		 * This is a pageout from the default pager,
		 * just go ahead and call vnop_pageout since
		 * it has already sorted out the dirty ranges
		 */
		KDBG_RELEASE(
			VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_START,
			size, 1);

		if ((error_ret = VNOP_PAGEOUT(vp, upl, upl_offset, (off_t)f_offset,
		    (size_t)size, flags, ctx))) {
			result = PAGER_ERROR;
		}

		KDBG_RELEASE(
			VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_END,
			size, 1);

		goto out;
	}
	if (upl == NULL) {
		int                     request_flags;

		if (vp->v_mount->mnt_vtable->vfc_vfsflags & VFC_VFSVNOP_PAGEOUTV2) {
			/*
			 * filesystem has requested the new form of VNOP_PAGEOUT for file
			 * backed objects... we will not grab the UPL befofe calling VNOP_PAGEOUT...
			 * it is the fileystem's responsibility to grab the range we're denoting
			 * via 'f_offset' and 'size' into a UPL... this allows the filesystem to first
			 * take any locks it needs, before effectively locking the pages into a UPL...
			 */
			KDBG_RELEASE(VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_START,
			    size, (int)f_offset);

			if ((error_ret = VNOP_PAGEOUT(vp, NULL, upl_offset, (off_t)f_offset,
			    size, flags, ctx))) {
				result = PAGER_ERROR;
			}
			KDBG_RELEASE(VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_END,
			    size);

			goto out;
		}
		if (flags & UPL_MSYNC) {
			request_flags = UPL_UBC_MSYNC | UPL_RET_ONLY_DIRTY;
		} else {
			request_flags = UPL_UBC_PAGEOUT | UPL_RET_ONLY_DIRTY;
		}

		if (ubc_create_upl_kernel(vp, f_offset, size, &upl, &pl, request_flags, VM_KERN_MEMORY_FILE) != KERN_SUCCESS) {
			result    = PAGER_ERROR;
			error_ret = EINVAL;
			goto out;
		}
		upl_offset = 0;
	} else {
		pl = ubc_upl_pageinfo(upl);
	}

	/*
	 * Ignore any non-present pages at the end of the
	 * UPL so that we aren't looking at a upl that
	 * may already have been freed by the preceeding
	 * aborts/completions.
	 */
	base_index = upl_offset / PAGE_SIZE;

	for (pg_index = (upl_offset + isize) / PAGE_SIZE; pg_index > base_index;) {
		if (upl_page_present(pl, --pg_index)) {
			break;
		}
		if (pg_index == base_index) {
			/*
			 * no pages were returned, so release
			 * our hold on the upl and leave
			 */
			if (!(flags & UPL_NOCOMMIT)) {
				ubc_upl_abort_range(upl, upl_offset, isize, UPL_ABORT_FREE_ON_EMPTY);
			}

			goto out;
		}
	}
	isize = ((pg_index + 1) - base_index) * PAGE_SIZE;

	/*
	 * we come here for pageouts to 'real' files and
	 * for msyncs...  the upl may not contain any
	 * dirty pages.. it's our responsibility to sort
	 * through it and find the 'runs' of dirty pages
	 * to call VNOP_PAGEOUT on...
	 */

	if (ubc_getsize(vp) == 0) {
		/*
		 * if the file has been effectively deleted, then
		 * we need to go through the UPL and invalidate any
		 * buffer headers we might have that reference any
		 * of it's pages
		 */
		for (offset = upl_offset; isize; isize -= PAGE_SIZE, offset += PAGE_SIZE) {
			if (vp->v_tag == VT_NFS) {
				/* check with nfs if page is OK to drop */
				error = nfs_buf_page_inval(vp, (off_t)f_offset);
			} else {
				blkno = ubc_offtoblk(vp, (off_t)f_offset);
				error = buf_invalblkno(vp, blkno, 0);
			}
			if (error) {
				if (!(flags & UPL_NOCOMMIT)) {
					ubc_upl_abort_range(upl, offset, PAGE_SIZE, UPL_ABORT_FREE_ON_EMPTY);
				}
				if (error_ret == 0) {
					error_ret = error;
				}
				result = PAGER_ERROR;
			} else if (!(flags & UPL_NOCOMMIT)) {
				ubc_upl_commit_range(upl, offset, PAGE_SIZE, UPL_COMMIT_FREE_ON_EMPTY);
			}
			f_offset += PAGE_SIZE;
		}
		goto out;
	}

	offset = upl_offset;
	pg_index = base_index;

	while (isize) {
		int  xsize;
		int  num_of_pages;

		if (!upl_page_present(pl, pg_index)) {
			/*
			 * we asked for RET_ONLY_DIRTY, so it's possible
			 * to get back empty slots in the UPL
			 * just skip over them
			 */
			f_offset += PAGE_SIZE;
			offset   += PAGE_SIZE;
			isize    -= PAGE_SIZE;
			pg_index++;

			continue;
		}
		if (!upl_dirty_page(pl, pg_index)) {
			/*
			 * if the page is not dirty and reached here it is
			 * marked precious or it is due to invalidation in
			 * memory_object_lock request as part of truncation
			 * We also get here from vm_object_terminate()
			 * So all you need to do in these
			 * cases is to invalidate incore buffer if it is there
			 * Note we must not sleep here if the buffer is busy - that is
			 * a lock inversion which causes deadlock.
			 */
			if (vp->v_tag == VT_NFS) {
				/* check with nfs if page is OK to drop */
				error = nfs_buf_page_inval(vp, (off_t)f_offset);
			} else {
				blkno = ubc_offtoblk(vp, (off_t)f_offset);
				error = buf_invalblkno(vp, blkno, 0);
			}
			if (error) {
				if (!(flags & UPL_NOCOMMIT)) {
					ubc_upl_abort_range(upl, offset, PAGE_SIZE, UPL_ABORT_FREE_ON_EMPTY);
				}
				if (error_ret == 0) {
					error_ret = error;
				}
				result = PAGER_ERROR;
			} else if (!(flags & UPL_NOCOMMIT)) {
				ubc_upl_commit_range(upl, offset, PAGE_SIZE, UPL_COMMIT_FREE_ON_EMPTY);
			}
			f_offset += PAGE_SIZE;
			offset   += PAGE_SIZE;
			isize    -= PAGE_SIZE;
			pg_index++;

			continue;
		}
		num_of_pages = 1;
		xsize = isize - PAGE_SIZE;

		while (xsize) {
			if (!upl_dirty_page(pl, pg_index + num_of_pages)) {
				break;
			}
			num_of_pages++;
			xsize -= PAGE_SIZE;
		}
		xsize = num_of_pages * PAGE_SIZE;

		KDBG_RELEASE(VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_START,
		    xsize, (int)f_offset);

		if ((error = VNOP_PAGEOUT(vp, upl, offset, (off_t)f_offset,
		    xsize, flags, ctx))) {
			if (error_ret == 0) {
				error_ret = error;
			}
			result = PAGER_ERROR;
		}
		KDBG_RELEASE(VMDBG_CODE(DBG_VM_VNODE_PAGEOUT) | DBG_FUNC_END,
		    xsize, error);

		f_offset += xsize;
		offset   += xsize;
		isize    -= xsize;
		pg_index += num_of_pages;
	}
out:
	vnode_put_from_pager(vp);

	if (errorp) {
		*errorp = error_ret;
	}

	return result;
}


pager_return_t
vnode_pagein(
	struct vnode            *vp,
	upl_t                   upl,
	upl_offset_t            upl_offset,
	vm_object_offset_t      f_offset,
	upl_size_t              size,
	int                     flags,
	int                     *errorp)
{
	upl_page_info_t *pl;
	int             result = PAGER_SUCCESS;
	int             error = 0;
	int             pages_in_upl;
	int             start_pg;
	int             last_pg;
	int             first_pg;
	int             xsize;
	int             must_commit = 1;
	int             ignore_valid_page_check = 0;

	if (flags & UPL_NOCOMMIT) {
		must_commit = 0;
	}

	if (flags & UPL_IGNORE_VALID_PAGE_CHECK) {
		ignore_valid_page_check = 1;
	}

	/*
	 * This call is non-blocking and does not ever fail but it can
	 * only be made when there is other explicit synchronization
	 * with reclaiming of the vnode which, in this path, is provided
	 * by the paging in progress counter.
	 *
	 * In addition, this may also be entered via vm_swapfile_io
	 * where the existing iocount provides the necessary synchronization.
	 * Ideally we would not take an additional iocount here in the cases
	 * where an explcit iocount has already been taken but this call
	 * doesn't cause a deadlock as other forms of vnode_get* might if
	 * this thread has already taken an iocount.
	 */
	error = vnode_getalways_from_pager(vp);
	if (error != 0) {
		/* This can't happen */
		panic("vnode_getalways returned %d for vp %p", error, vp);
	}

	if (UBCINFOEXISTS(vp) == 0) {
		result = PAGER_ERROR;
		error  = PAGER_ERROR;

		if (upl && must_commit) {
			ubc_upl_abort_range(upl, upl_offset, size, UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_ERROR);
		}

		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_VNODEPAGEIN_NO_UBCINFO), 0 /* arg */);
		goto out;
	}
	if (upl == (upl_t)NULL) {
		flags &= ~UPL_NOCOMMIT;

		if (size > MAX_UPL_SIZE_BYTES) {
			result = PAGER_ERROR;
			error  = PAGER_ERROR;
			goto out;
		}
		if (vp->v_mount->mnt_vtable->vfc_vfsflags & VFC_VFSVNOP_PAGEINV2) {
			/*
			 * filesystem has requested the new form of VNOP_PAGEIN for file
			 * backed objects... we will not grab the UPL befofe calling VNOP_PAGEIN...
			 * it is the fileystem's responsibility to grab the range we're denoting
			 * via 'f_offset' and 'size' into a UPL... this allows the filesystem to first
			 * take any locks it needs, before effectively locking the pages into a UPL...
			 * so we pass a NULL into the filesystem instead of a UPL pointer... the 'upl_offset'
			 * is used to identify the "must have" page in the extent... the filesystem is free
			 * to clip the extent to better fit the underlying FS blocksize if it desires as
			 * long as it continues to include the "must have" page... 'f_offset' + 'upl_offset'
			 * identifies that page
			 */
			if ((error = VNOP_PAGEIN(vp, NULL, upl_offset, (off_t)f_offset,
			    size, flags, vfs_context_current()))) {
				set_thread_pagein_error(current_thread(), error);
				result = PAGER_ERROR;
				error  = PAGER_ERROR;
				ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_VNODEPAGEIN_FSPAGEIN_FAIL), 0 /* arg */);
			}
			goto out;
		}
		ubc_create_upl_kernel(vp, f_offset, size, &upl, &pl, UPL_UBC_PAGEIN | UPL_RET_ONLY_ABSENT, VM_KERN_MEMORY_FILE);

		if (upl == (upl_t)NULL) {
			result =  PAGER_ABSENT;
			error = PAGER_ABSENT;
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_VNODEPAGEIN_NO_UPL), 0 /* arg */);
			goto out;
		}
		ubc_upl_range_needed(upl, upl_offset / PAGE_SIZE, 1);

		upl_offset = 0;
		first_pg = 0;

		/*
		 * if we get here, we've created the upl and
		 * are responsible for commiting/aborting it
		 * regardless of what the caller has passed in
		 */
		must_commit = 1;
	} else {
		pl = ubc_upl_pageinfo(upl);
		first_pg = upl_offset / PAGE_SIZE;
	}
	pages_in_upl = size / PAGE_SIZE;
	DTRACE_VM2(pgpgin, int, pages_in_upl, (uint64_t *), NULL);

	/*
	 * before we start marching forward, we must make sure we end on
	 * a present page, otherwise we will be working with a freed
	 * upl
	 */
	for (last_pg = pages_in_upl - 1; last_pg >= first_pg; last_pg--) {
		if (upl_page_present(pl, last_pg)) {
			break;
		}
		if (last_pg == first_pg) {
			/*
			 * empty UPL, no pages are present
			 */
			if (must_commit) {
				ubc_upl_abort_range(upl, upl_offset, size, UPL_ABORT_FREE_ON_EMPTY);
			}
			goto out;
		}
	}
	pages_in_upl = last_pg + 1;
	last_pg = first_pg;

	while (last_pg < pages_in_upl) {
		/*
		 * skip over missing pages...
		 */
		for (; last_pg < pages_in_upl; last_pg++) {
			if (upl_page_present(pl, last_pg)) {
				break;
			}
		}

		if (ignore_valid_page_check == 1) {
			start_pg = last_pg;
		} else {
			/*
			 * skip over 'valid' pages... we don't want to issue I/O for these
			 */
			for (start_pg = last_pg; last_pg < pages_in_upl; last_pg++) {
				if (!upl_valid_page(pl, last_pg)) {
					break;
				}
			}
		}

		if (last_pg > start_pg) {
			/*
			 * we've found a range of valid pages
			 * if we've got COMMIT responsibility
			 * commit this range of pages back to the
			 * cache unchanged
			 */
			xsize = (last_pg - start_pg) * PAGE_SIZE;

			if (must_commit) {
				ubc_upl_abort_range(upl, start_pg * PAGE_SIZE, xsize, UPL_ABORT_FREE_ON_EMPTY);
			}
		}
		if (last_pg == pages_in_upl) {
			/*
			 * we're done... all pages that were present
			 * have either had I/O issued on them or
			 * were aborted unchanged...
			 */
			break;
		}

		if (!upl_page_present(pl, last_pg)) {
			/*
			 * we found a range of valid pages
			 * terminated by a missing page...
			 * bump index to the next page and continue on
			 */
			last_pg++;
			continue;
		}
		/*
		 * scan from the found invalid page looking for a valid
		 * or non-present page before the end of the upl is reached, if we
		 * find one, then it will be the last page of the request to
		 * 'cluster_io'
		 */
		for (start_pg = last_pg; last_pg < pages_in_upl; last_pg++) {
			if ((!ignore_valid_page_check && upl_valid_page(pl, last_pg)) || !upl_page_present(pl, last_pg)) {
				break;
			}
		}
		if (last_pg > start_pg) {
			int xoff;
			xsize = (last_pg - start_pg) * PAGE_SIZE;
			xoff  = start_pg * PAGE_SIZE;

			if ((error = VNOP_PAGEIN(vp, upl, (upl_offset_t) xoff,
			    (off_t)f_offset + xoff,
			    xsize, flags, vfs_context_current()))) {
				/*
				 * Usually this UPL will be aborted/committed by the lower cluster layer.
				 *
				 * a)	In the case of decmpfs, however, we may return an error (EAGAIN) to avoid
				 *	a deadlock with another thread already inflating the file.
				 *
				 * b)	In the case of content protection, EPERM is a valid error and we should respect it.
				 *
				 * In those cases, we must take care of our UPL at this layer itself.
				 */
				if (must_commit) {
					if (error == EAGAIN) {
						ubc_upl_abort_range(upl, (upl_offset_t) xoff, xsize, UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_RESTART);
					}
					if (error == EPERM) {
						ubc_upl_abort_range(upl, (upl_offset_t) xoff, xsize, UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_ERROR);
					}
				}
				set_thread_pagein_error(current_thread(), error);
				result = PAGER_ERROR;
				error  = PAGER_ERROR;
				ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_VNODEPAGEIN_FSPAGEIN_FAIL), 0 /* arg */);
			}
		}
	}
out:
	vnode_put_from_pager(vp);

	if (errorp) {
		*errorp = result;
	}

	return error;
}
