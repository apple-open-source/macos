/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#ifndef __AUTOFS_MIGTYPES_H__
#define __AUTOFS_MIGTYPES_H__

/*
 * Type definitions, MIG-style.
 */
type autofs_pathname = c_string[*:PATH_MAX];
type autofs_component = array [*:NAME_MAX] of char;	/* not null-terminated! */
type autofs_fstype = c_string[*:NAME_MAX];
type autofs_opts = c_string[*:AUTOFS_MAXOPTSLEN];
type byte_buffer = array [] of uint8_t;

#endif /* __AUTOFS_MIGTYPES_H__ */
