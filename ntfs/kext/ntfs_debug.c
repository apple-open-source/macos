/*
 * ntfs_debug.c - NTFS kernel debug support.
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

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <stdarg.h>
#include <string.h>

#include <libkern/libkern.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_debug.h"
#include "ntfs_runlist.h"

#ifdef DEBUG

/* If 0, do not output debug messages.  If not zero, output debug messages. */
int ntfs_debug_messages;

/*
 * Define a sysctl "vfs.generic.ntfs.debug_messages" so debug messsages can be
 * enabled and disabled at runtime.
 */
SYSCTL_DECL(_vfs_generic);
SYSCTL_DECL(_vfs_generic_ntfs);
SYSCTL_NODE(_vfs_generic, OID_AUTO, ntfs, CTLFLAG_RW, 0, "NTFS File System");
SYSCTL_INT(_vfs_generic_ntfs, OID_AUTO, debug_messages, CTLFLAG_RW,
		&ntfs_debug_messages, 0,
		"Set to non-zero to enable debug messages.");

#endif /* DEBUG */

/*
 * A static buffer to hold the error string being displayed and a spinlock
 * to protect concurrent accesses to it as well as initialisation and
 * deinitialisation functions for them.  Those are called at module load and
 * unload time.
 */
static char ntfs_err_buf[1024];
static lck_spin_t ntfs_err_buf_lock;

/**
 * ntfs_debug_init - initialize debugging for ntfs
 *
 * Initialize the error buffer lock and if compiled with DEBUG, register our
 * sysctl.
 *
 * Note we cannot use ntfs_debug(), ntfs_warning(), and ntfs_error() before
 * this function has been called.
 */
void ntfs_debug_init(void)
{
	lck_spin_init(&ntfs_err_buf_lock, ntfs_lock_grp, ntfs_lock_attr);
#ifdef DEBUG
	/* Register our sysctl. */
	sysctl_register_oid(&sysctl__vfs_generic_ntfs);
	sysctl_register_oid(&sysctl__vfs_generic_ntfs_debug_messages);
#endif
}

/**
 * ntfs_debug_deinit - deinitialize debugging for ntfs
 *
 * Deinit the error buffer lock and if compiled with DEBUG, unregister our
 * sysctl.
 *
 * Note we cannot use ntfs_debug(), ntfs_warning(), and ntfs_error() once this
 * function has been called.
 */
void ntfs_debug_deinit(void)
{
#ifdef DEBUG
	/* Unregister our sysctl. */
	sysctl_unregister_oid(&sysctl__vfs_generic_ntfs_debug_messages);
	sysctl_unregister_oid(&sysctl__vfs_generic_ntfs);
#endif
	lck_spin_destroy(&ntfs_err_buf_lock, ntfs_lock_grp);
}

/**
 * __ntfs_warning - output a warning to the console
 * @function:	name of function outputting the warning
 * @mp:		mounted ntfs filesystem
 * @fmt:	warning string containing format specifications
 * @...:	a variable number of arguments specified in @fmt
 *
 * Outputs a warning to the console for the mounted ntfs filesystem described
 * by @mp.
 *
 * @fmt and the corresponding @... is printf style format string containing
 * the warning string and the corresponding format arguments, respectively.
 *
 * @function is the name of the function from which __ntfs_warning is being
 * called.
 *
 * Note, you should be using debug.h::ntfs_warning(@mp, @fmt, @...) instead
 * as this provides the @function parameter automatically.
 */
void __ntfs_warning(const char *function,
		struct mount *mp, const char *fmt, ...)
{
	va_list args;
	int flen = 0;

	if (function)
		flen = strlen(function);
	lck_spin_lock(&ntfs_err_buf_lock);
	va_start(args, fmt);
	vsnprintf(ntfs_err_buf, sizeof(ntfs_err_buf), fmt, args);
	va_end(args);
	if (mp)
		printf("NTFS-fs warning (device %s): %s(): %s\n",
				vfs_statfs(mp)->f_mntfromname,
				flen ? function : "", ntfs_err_buf);
	else
		printf("NTFS-fs warning: %s(): %s\n", flen ? function : "",
				ntfs_err_buf);
	lck_spin_unlock(&ntfs_err_buf_lock);
}

/**
 * __ntfs_error - output an error to the console
 * @function:	name of function outputting the error
 * @mp:		mounted ntfs filesystem
 * @fmt:	error string containing format specifications
 * @...:	a variable number of arguments specified in @fmt
 *
 * Outputs an error to the console for the mounted ntfs filesystem described
 * by @mp.
 *
 * @fmt and the corresponding @... is printf style format string containing
 * the error string and the corresponding format arguments, respectively.
 *
 * @function is the name of the function from which __ntfs_error is being
 * called.
 *
 * Note, you should be using debug.h::ntfs_error(@mp, @fmt, @...) instead
 * as this provides the @function parameter automatically.
 */
void __ntfs_error(const char *function,
		struct mount *mp, const char *fmt, ...)
{
	va_list args;
	int flen = 0;

	if (function)
		flen = strlen(function);
	lck_spin_lock(&ntfs_err_buf_lock);
	va_start(args, fmt);
	vsnprintf(ntfs_err_buf, sizeof(ntfs_err_buf), fmt, args);
	va_end(args);
	if (mp)
		printf("NTFS-fs error (device %s): %s(): %s\n",
				vfs_statfs(mp)->f_mntfromname,
				flen ? function : "", ntfs_err_buf);
	else
		printf("NTFS-fs error: %s(): %s\n", flen ? function : "",
				ntfs_err_buf);
	lck_spin_unlock(&ntfs_err_buf_lock);
}

#ifdef DEBUG

void __ntfs_debug(const char *file, int line,
		const char *function, const char *fmt, ...)
{
	va_list args;
	const char *filename;
	int len;

	if (!ntfs_debug_messages)
		return;
	/*
	 * We really want strrchr() here but that is not exported so do it by
	 * hand.
	 */
	filename = file;
	if (filename) {
		for (len = strlen(filename); len > 0; len--) {
			if (filename[len - 1] == '/') {
				filename += len;
				break;
			}
		}
	}
	lck_spin_lock(&ntfs_err_buf_lock);
	va_start(args, fmt);
	vsnprintf(ntfs_err_buf, sizeof(ntfs_err_buf), fmt, args);
	va_end(args);
	printf("NTFS-fs DEBUG (%s, %d): %s(): %s\n", filename ? filename : "",
			line, function ? function : "", ntfs_err_buf);
	lck_spin_unlock(&ntfs_err_buf_lock);
}

/* Dump a runlist.  Caller has to provide synchronization for @rl. */
void ntfs_debug_rl_dump(const ntfs_runlist *runlist)
{
	ntfs_rl_element *rl;
	u32 elements, i;
	const char *lcn_str[5] = { "LCN_HOLE         ", "LCN_RL_NOT_MAPPED",
				   "LCN_ENOENT       ", "LCN_unknown      " };

	if (!ntfs_debug_messages)
		return;
	printf("NTFS-fs DEBUG: Dumping runlist (values in hex):\n");
	if (!runlist || !runlist->rl || !runlist->elements) {
		printf("Run list not present.\n");
		return;
	}
	rl = runlist->rl;
	elements = runlist->elements;
	printf("VCN              LCN               Run length\n");
	for (i = 0; i < elements; i++) {
		LCN lcn = rl[i].lcn;

		if (lcn < (LCN)0) {
			int index = -lcn - 1;

			if (index > -LCN_ENOENT - 1)
				index = 3;
			printf("%-16Lx %s %-16Lx%s\n",
					(unsigned long long)rl[i].vcn,
					lcn_str[index],
					(unsigned long long)rl[i].length,
					rl[i].length ? "" : " (runlist end)");
		} else
			printf("%-16Lx %-16Lx  %-16Lx%s\n",
					(unsigned long long)rl[i].vcn,
					(unsigned long long)rl[i].lcn,
					(unsigned long long)rl[i].length,
					rl[i].length ? "" : " (runlist end)");
		if (!rl[i].length)
			break;
	}
	if (i == elements - 1)
		printf("Runlist contains specified number of elements (%u).\n",
				(unsigned)elements);
	else
		printf("Error: Runlist contains %s elements than specified "
				"(%u)!\n", i < elements ? "less" : "more",
				(unsigned)elements);
}

#endif /* DEBUG */
