/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifdef __APPLE_PR3375657_HACK__

#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <alloca.h>
#include <string.h>

__private_extern__ int _sem_match(const char *name);

int
sem_unlink (const char *name)
{
        char            *buffer;

        /*
        * To work-around applications that don't play
        * well in multiple GUI sessions, we append
        * shared memory names with the effective user ID.
        * It would be better to append the region name
        * with a session ID, but nothing like that
        * exists at this level of the system yet.
        */

        if (_sem_match(name) && (buffer = alloca(strlen(name) + 32)) != NULL) {
                sprintf(buffer, "%s\t%d", name, geteuid());
		name = buffer;
        }

	return syscall (SYS_sem_unlink, name);
}

#endif /* __APPLE_PR3375657_HACK__ */
