/**
 * StartupItems.h - Startup Item management routines
 * Wilfredo Sanchez | wsanchez@opensource.apple.com
 * $Apple$
 **
 * Copyright (c) 1999-2001 Apple Computer, Inc. All rights reserved.
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
 **/

#ifndef _StartupItems_H_
#define _StartupItems_H_

#include <NSSystemDirectories.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>

/*
 * Find all available startup items in NSDomains specified by aMask.
 */
CFMutableArrayRef StartupItemListCreateMutable (NSSearchPathDomainMask aMask);

/*
 * Given aWaitingList of startup items, and aStatusDict describing the
 * current startup state, returns the next startup item to run, if any.
 * Returns nil if none is available.
 * The startup order depends on the dependancies between items and the
 * priorities of the items.
 * Note that this is not necessarily deterministic; if more than one
 * startup item with the same priority is ready to run, which item gets
 * returned is not specified.
 */
CFDictionaryRef StartupItemListGetNext (CFArrayRef      aWaitingList,
                                        CFDictionaryRef aStatusDict);

/*
 * Run the startup item.
 */
#define kRunSuccess CFSTR("success")
#define kRunFailure CFSTR("failure")

void StartupItemRun (CFDictionaryRef anItem, CFMutableDictionaryRef aStatusDict);
CFStringRef StartupItemCreateLocalizedString (CFDictionaryRef anItem, CFStringRef aString);

#endif /* _StartupItems_H_ */
