/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
 */

#ifndef sharedUtilities_h
#define sharedUtilities_h

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

CFDataRef CreateDataFromFileURL(CFAllocatorRef alloc, CFURLRef fileURL, CFErrorRef *error);

#endif /* sharedUtilities_h */
