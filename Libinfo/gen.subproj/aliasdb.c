/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Unimplemented backward-compatible alias lookup routines.
 * These are only called if NetInfo is not running or if YP
 * has been turned on, in which case NetInfo will be tried first.
 * This is the standard NeXT policy for all lookup routines.
 */

#include <aliasdb.h>
#include <string.h>

void
_old_alias_setent(void)
{
}

struct aliasent *
_old_alias_getent(void)
{
	return (NULL);
}

void
_old_alias_endent(void)
{
}

struct aliasent *
_old_alias_getbyname(char *name)
{
	return (NULL);
}
