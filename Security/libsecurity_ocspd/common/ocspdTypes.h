/*
 * Copyright (c) 2002,2000 Apple Computer, Inc. All rights reserved.
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

#ifndef	_OCSPD_TYPES_H_
#define _OCSPD_TYPES_H_

#include <mach/mach_types.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

/* Explicitly enable MIG type checking per Radar 4735696 */
#undef __MigTypeCheck
#define __MigTypeCheck 1

typedef void *Data;

/*
 * Standard bootstrap name and an env var name to override it with (!NDEBUG only)
 */
#define OCSPD_BOOTSTRAP_NAME		"com.apple.ocspd"
#define OCSPD_BOOTSTRAP_ENV			"OCSPD_SERVER"

#endif	/* _OCSPD_TYPES_H_ */
