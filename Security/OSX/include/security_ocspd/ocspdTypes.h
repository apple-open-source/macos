/*
 * Copyright (c) 2000,2002,2011,2013-2014 Apple Inc. All Rights Reserved.
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

#ifndef	_OCSPD_TYPES_H_
#define _OCSPD_TYPES_H_

#include <mach/mach_types.h>
#include <MacTypes.h>

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
