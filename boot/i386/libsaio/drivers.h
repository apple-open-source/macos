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
 * Copyright 1994 NeXT Computer, Inc.
 * All rights reserved.
 */

#ifndef __LIBSAIO_DRIVERS_H
#define __LIBSAIO_DRIVERS_H

struct driver_info {
    char	*name;
    char	*bundle;
    char	*version;
    char	*configTable;
    char	*tableName;
    char	*locationTag;
    int		flags;
};

#define	DRIVER_FLAG_NONE			0x00
#define	DRIVER_FLAG_INTERESTING		0x01
#define DRIVER_FLAG_CONFIG_ADDED	0x02

#define DRIVER_NOT_FOUND			0x0
#define DRIVER_VERSION_MISMATCH		0x1

extern int driverMissing;

#endif /* !__LIBSAIO_DRIVERS_H */
