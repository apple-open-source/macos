/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

/*
 *  CCLArgFunctions.h
 *  CCLLauncher
 *
 *  Created by kevine on 4/11/06.
 *  Copyright 2006-7 Apple, Inc.  All rights reserved.
 *
 */

#ifndef __CCLArgFunctions__
#define __CCLArgFunctions__

#include <CoreFoundation/CoreFoundation.h>


#define kCCL_BundleExtension    "ccl"
#define kCCL_BundleExtLen       sizeof(kCCL_BundleExtension) - sizeof('\0') 

CFDictionaryRef
GetCFDictionaryFromDict(CFDictionaryRef dict, const CFStringRef key);

bool
GetCFStringFromDict(CFDictionaryRef dict, CFStringRef *s,const CFStringRef key);
bool
CopyCStringFromDict(CFDictionaryRef dict, char** string, const CFStringRef key);

bool
GetCFNumberFromDict(CFDictionaryRef dict, CFNumberRef *n,const CFStringRef key);
bool
GetIntFromDict(CFDictionaryRef dict, int* intRef, const CFStringRef key);


#endif  // __CCLArgFunctions__
