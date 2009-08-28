/*
 * Copyright (c) 2007 Apple Inc. All Rights Reserved.
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


//
// dtrace - dtrace support in securityd
//
#ifndef _H_DTRACE
#define _H_DTRACE


//
// Typedefs used in the DTrace static probe interface
//
typedef const void *DTHandle;
typedef uint32_t DTPort;
typedef uint32_t DTGuest;


#define DTHANDLE(it)	(dynamic_cast<const void *>((it)))
#define DTSELF			DTHANDLE(this)


#include "securityd_dtrace.h"



#endif //_H_DTRACE
