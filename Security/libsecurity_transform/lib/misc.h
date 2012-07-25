/*
 *  misc.h
 *  libsecurity_transform
 *
 *  Created by JOsborne on 3/19/10.
 *  Copyright 2010 Apple. All rights reserved.
 *
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
