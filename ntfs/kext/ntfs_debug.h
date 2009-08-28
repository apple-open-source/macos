/*
 * ntfs_debug.h - Defines for NTFS kernel debug support.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
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

#ifndef _OSX_NTFS_DEBUG_H
#define _OSX_NTFS_DEBUG_H

#include <sys/cdefs.h>

#include "ntfs_runlist.h"

/* Forward declaration so we do not have to include <sys/mount.h> here. */
struct mount;

__private_extern__ void ntfs_debug_init(void);
__private_extern__ void ntfs_debug_deinit(void);

__private_extern__ void __ntfs_warning(const char *function,
		struct mount *mp, const char *fmt, ...) __printflike(3, 4);
#define ntfs_warning(mp, fmt, a...)	\
		__ntfs_warning(__FUNCTION__, mp, fmt, ##a)

__private_extern__ void __ntfs_error(const char *function,
		struct mount *mp, const char *fmt, ...) __printflike(3, 4);
#define ntfs_error(mp, fmt, a...)	\
		__ntfs_error(__FUNCTION__, mp, fmt, ##a)

#ifdef DEBUG

/**
 * ntfs_debug - write a debug message to the console
 * @fmt:	a printf format string containing the message
 * @...:	the variables to substitute into @fmt
 *
 * ntfs_debug() writes a message to the console but only if the driver was
 * compiled with -DDEBUG.  Otherwise, the call turns into a NOP.
 */
__private_extern__ void __ntfs_debug(const char *file, int line,
		const char *function, const char *fmt, ...)
		__printflike(4, 5);
#define ntfs_debug(fmt, a...)		\
		__ntfs_debug(__FILE__, __LINE__, __FUNCTION__, fmt, ##a)

__private_extern__ void ntfs_debug_runlist_dump(const ntfs_runlist *rl);
__private_extern__ void ntfs_debug_attr_list_dump(const u8 *al,
		const unsigned size);

#else /* !DEBUG */

#define ntfs_debug(fmt, a...)			do {} while (0)
#define ntfs_debug_runlist_dump(rl)		do {} while (0)
#define ntfs_debug_attr_list_dump(al, size)	do {} while (0)

#endif /* !DEBUG */

#endif /* !_OSX_NTFS_DEBUG_H */
