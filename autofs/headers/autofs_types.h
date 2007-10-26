/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __AUTOFS_TYPES_H__
#define __AUTOFS_TYPES_H__

#include <sys/syslimits.h>
#include "autofs_defs.h"

/*
 * Type definitions, C-style.
 */
typedef char autofs_pathname[PATH_MAX+1];
typedef char autofs_component[NAME_MAX];	/* not null-terminated! */
typedef char autofs_fstype[NAME_MAX+1];
typedef char autofs_opts[AUTOFS_MAXOPTSLEN];
typedef uint8_t *byte_buffer;

#endif /* __AUTOFS_TYPES_H__ */
