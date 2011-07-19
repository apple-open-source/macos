/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _PPPCONTROLLER_TYPES_H
#define _PPPCONTROLLER_TYPES_H

/*
 * Keep IPC functions private
 */
#ifdef mig_external
#undef mig_external
#endif
#define mig_external __private_extern__

/* Turn MIG type checking on by default */
#define __MigTypeCheck       1

/*
 * Mach server port name
 */
#define PPPCONTROLLER_SERVER		"com.apple.SystemConfiguration.PPPController"

/*
 * installed events values
 */
#define APPLICATION_INSTALLED	1
#define APPLICATION_REMOVED		2

/*
 * Input arguments: serialized key's, list delimiters, ...
 *	(sent as out-of-line data in a message)
 */
typedef const char * xmlData_t;


/* Output arguments: serialized data, lists, ...
 *	(sent as out-of-line data in a message)
 */
typedef char * xmlDataOut_t;


#endif _PPPOLCONTROLLER_TYPES_H
