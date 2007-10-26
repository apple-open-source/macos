/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
/*
 * We need O_NOCTTY from fcntl.h, but that would also drag in the variadic
 * prototype for open(), and so we'd have to use stdarg.h to get the mode.
 * So open.h just contains O_NOCTTY, which it gets from fcntl.h.
 *
 * This is for legacy only.
 */
#include "open.h"

int __open_nocancel(const char *path, int flags, mode_t mode);
int open(const char *path, int flags, mode_t mode) LIBC_ALIAS_C(open);

/*
 * open stub: The legacy interface never automatically associated a controlling
 * tty, so we always pass O_NOCTTY.
 */
int
open(const char *path, int flags, mode_t mode)
{
	return(__open_nocancel(path, flags | O_NOCTTY, mode));
}
