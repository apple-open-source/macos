/*
 * Copyright (c) 2010-2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef __INCLUDED_TRANSFORMS_MISC_H__
#define __INCLUDED_TRANSFORMS_MISC_H__

#include <stdio.h>
#include "SecTransform.h"

#ifdef __cplusplus
extern "C" {
#endif
	
	
	CFErrorRef fancy_error(CFStringRef domain, CFIndex code, CFStringRef description);
	extern void graphviz(FILE *f, SecTransformRef tr);
	extern void CFfprintf(FILE *f, const char *format, ...);
	CFErrorRef GetNoMemoryError();
	CFErrorRef GetNoMemoryErrorAndRetain();
	void CFSafeRelease(CFTypeRef object);
    
    // NOTE: the return may or allocate a fair bit more space then it needs.
    // Use it for short lived conversions (or strdup the result).
    extern char *utf8(CFStringRef s);

#ifdef __cplusplus
}
#endif
		
#endif
