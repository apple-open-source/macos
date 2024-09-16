/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *    @(#)nfs_bio.c    8.9 (Berkeley) 3/30/95
 * FreeBSD-Id: nfs_bio.c,v 1.44 1997/09/10 19:52:25 phk Exp $
 */

#include <vm/vm_kern.h>
#include <libkern/libkern.h>

#include "nfsnode.h"
#include "nfs_kdebug.h"

/*
 * Helper function to post a ktrace with string to a fsrw_code like
 * DEVICE_NAME_INFO_START(mp <-> URL) or VNODE_PATH_INFO_START(vp <-> file path)
 */
static void
nfs_kdebug_add_string(vm_offset_t identifier, char *buf, size_t buflen, const uint32_t fsrw_code)
{
	size_t args_to_write = 0;
	size_t buflen64 = buflen % sizeof(uint64_t) == 0 ? buflen / sizeof(uint64_t) : (buflen / sizeof(uint64_t)) + 1;
	uint64_t *buf64 = (uint64_t *)buf;

	/*
	 * We are limited per ktrace event to just 4 uint64_t to post the entire
	 * string so may have to post multiple ktrace events to get entire string.
	 *
	 * 1. Start with DBG_FUNC_START in code, arg1 is identifier, 24 char bytes
	 * 2. Keep posting 32 char bytes at a time
	 * 3. At end of string, set DBG_FUNC_END in code
	 */
	for (size_t i = 0; i < buflen64; i += args_to_write) {
		int code = DBG_FUNC_NONE;
		size_t args_left = buflen64 - i;
		uint64_t arg1, arg2, arg3, arg4;
		if (i == 0) {
			/* Post beginning of string (up to 24 bytes) along with identifier */
			code |= DBG_FUNC_START;
			args_to_write = MIN(3, args_left);

			arg1 = (uintptr_t)identifier;
			arg2 = args_to_write >= 1 ? (uint64_t)buf64[i + 0] : 0;
			arg3 = args_to_write >= 2 ? (uint64_t)buf64[i + 1] : 0;
			arg4 = args_to_write >= 3 ? (uint64_t)buf64[i + 2] : 0;
		} else {
			/* Keep posting string up to 32 bytes at a time */
			args_to_write = MIN(4, args_left);

			arg1 = args_to_write >= 1 ? (uint64_t)buf64[i + 0] : 0;
			arg2 = args_to_write >= 2 ? (uint64_t)buf64[i + 1] : 0;
			arg3 = args_to_write >= 3 ? (uint64_t)buf64[i + 2] : 0;
			arg4 = args_to_write >= 4 ? (uint64_t)buf64[i + 3] : 0;
		}

		if (i + args_to_write == buflen64) {
			/* Done with string, mark the end of the string */
			code |= DBG_FUNC_END;
		}

		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_COMMON, (FSDBG_CODE(DBG_FSRW, fsrw_code)) | code, arg1, arg2, arg3, arg4, 0);
	}
}

/*
 * Helper function to get the "mount from" URL to display in Ariadne in the
 * FileSystem I/O section instead of just showing "Device 0x/0x". If we fail
 * to create the URL, then Ariadne will show the "Device 0x/0x" instead.
 */
void
nfs_kdebug_device_url(mount_t mp)
{
	char buf[PATH_MAX] = {};
	size_t buflen = sizeof(buf);
	static const uint32_t DEVICE_NAME_INFO_START = 0x26;
	struct vfsstatfs *stats;
	vm_offset_t mp_perm;

	if (mp == NULL) {
		printf("%s Got NULL mount\n", __FUNCTION__);
		return;
	}
	if ((stats = vfs_statfs(mp)) == NULL) {
		printf("%s vfs_statfs returns NULL for mount 0x%lx\n", __FUNCTION__, nfs_kernel_hideaddr(mp));
		return;
	}

	/*
	 * Get the "mount from" string which is in format of "server:/path",
	 * so just need to add the "nfs://" to the front of the string.
	 */
	if ((buflen = snprintf(buf, sizeof(buf), "nfs://%s", stats->f_mntfromname)) <= 0) {
		printf("%s failed to build mount URL for mount 0x%lx\n", __FUNCTION__, nfs_kernel_hideaddr(mp));
		return;
	}

	/* Expose the mount point address to user space */
	vm_kernel_addrperm_external((vm_offset_t)mp, &mp_perm);

	/* Associate this URL with this mount point */
	nfs_kdebug_add_string(mp_perm, buf, buflen, DEVICE_NAME_INFO_START);
}

/*
 * Helper function to get the path to the vnode and then post it to ktrace
 */
void
nfs_kdebug_vnode_path(vnode_t vp)
{
	int err;
	char buf[PATH_MAX] = {};
	size_t buflen = sizeof(buf);
	static const uint32_t VNODE_PATH_INFO_START = 0x24;
	vm_offset_t vp_perm;

	if (vp == NULL) {
		printf("%s Got NULL vnode\n", __FUNCTION__);
		return;
	}
	if ((err = vn_getpath_ext(vp, NULL, buf, &buflen, BUILDPATH_VOLUME_RELATIVE)) != 0) {
		printf("%s failed to get vnode path for vnode 0x%lx, err %d\n", __FUNCTION__, nfs_kernel_hideaddr(vp), err);
		return;
	}

	/* Expose the vnode address to user space */
	vm_kernel_addrperm_external((vm_offset_t)vp, &vp_perm);

	/* Associate this path with this vnode */
	nfs_kdebug_add_string(vp_perm, buf, buflen, VNODE_PATH_INFO_START);
}

/*
 * Log the start of an IO and also register the NFS mount from URL with this
 * mount point so Ariadne can display it under FileSystem IO row. The start IO
 * and end IO events both log an IO ID and that is how Ariadne matches them
 * together.
 */
void
nfs_kdebug_io_start(mount_t mp, void *trace_id, size_t blocknu, uint32_t nb_flags, int64_t resid)
{
	NFS_KDBG_FILE_IO_RETURN

	int code = 0;
	vm_offset_t trace_id_perm, mp_perm;

	if (ISSET(nb_flags, NB_READ)) {
		SET(code, DKIO_READ);
	}
	if (ISSET(nb_flags, NB_ASYNC)) {
		SET(code, DKIO_ASYNC);
	}
	if (ISSET(nb_flags, NB_META)) {
		SET(code, DKIO_META);
	}
	if (ISSET(nb_flags, NB_NOCACHE)) {
		SET(code, DKIO_NOCACHE);
	}
	if (ISSET(nb_flags, NB_PAGING)) {
		SET(code, DKIO_PAGING);
	}

	/*
	 * Try to associate the mountpoint to a NFS URL to show in Ariadne.
	 *
	 * Note that we only have to do this once per mount on its first IO, but
	 * since kernel tracing can be enabled/disabled at any time, we currently
	 * dont know if we have done this already or not so we are lazy and do it
	 * on every IO. A minor future enhancement would be to have a per mount
	 * flag to indicate whether we have done this for this mount and if so,
	 * skip doing it again.
	 */
	nfs_kdebug_device_url(mp);

	/*
	 * Ignore any errors from nfs_kdebug_device_url as that is to just
	 * have a nice URL showing instead showing just a device 0x/0x
	 */

	/*
	 * Expose the address of trace_id to user space which is used to match the
	 * start and end IO events. This address must be unique to this specific IO.
	 */
	vm_kernel_addrperm_external((vm_offset_t)trace_id, &trace_id_perm);

	/*
	 * Expose the mountpoint address to user space which is used to match to
	 * the corrert NFS URL for this mountpoint
	 */
	vm_kernel_addrperm_external((vm_offset_t)mp, &mp_perm);

	/*
	 * Notify start of IO with trace id, mountpoint, block number and length
	 */
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_COMMON, (FSDBG_CODE(DBG_DKRW, code)) | DBG_FUNC_NONE, trace_id_perm, mp_perm, blocknu, resid, 0);
}

/*
 * Log the end of an IO along with the path to file being accessed
 */
void
nfs_kdebug_io_end(vnode_t vp, void *trace_id, int64_t resid, int error)
{
	NFS_KDBG_FILE_IO_RETURN

	int code = DKIO_DONE;
	vm_offset_t trace_id_perm, vp_perm;

	/*
	 * Try to associate the vnode to a file path
	 *
	 * Note that we only have to do this once per vnode on its first IO, but
	 * since kernel tracing can be enabled/disabled at any time, we currently
	 * dont know if we have done this already or not so we are lazy and do it
	 * on every IO. A minor future enhancement would be to have a per vnode
	 * flag to indicate whether we have done this for this vnode and if so,
	 * skip doing it again.
	 */
	nfs_kdebug_vnode_path(vp);

	/*
	 * Ignore any errors from nfs_kdebug_vnode_path as that is to just
	 * display the path to the file in use
	 */

	/*
	 * Expose the address of trace_id to user space which is used to match the
	 * start and end IO events. This address must be unique to this specific IO.
	 */
	vm_kernel_addrperm_external((vm_offset_t)trace_id, &trace_id_perm);

	/*
	 * Expose the address of the vnode to user space. Ariadne collects the
	 * file path just on the end IO event.
	 */
	vm_kernel_addrperm_external((vm_offset_t)vp, &vp_perm);

	/* Notify end of IO with trace id, vnode, remaining length and any error */
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_COMMON, (FSDBG_CODE(DBG_DKRW, code)) | DBG_FUNC_NONE, trace_id_perm, vp_perm, resid, error, 0);
}
