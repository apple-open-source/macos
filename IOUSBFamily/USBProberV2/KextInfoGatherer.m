/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "KextInfoGatherer.h"


@implementation KextInfoGatherer

#define kStringInvalidLong   "????"

#define SAFE_FREE(ptr)  do {          \
if (ptr) free(ptr);               \
} while (0)
#define SAFE_FREE_NULL(ptr)  do {     \
if (ptr) free(ptr);               \
(ptr) = NULL;                     \
} while (0)
#define SAFE_RELEASE(ptr)  do {       \
if (ptr) CFRelease(ptr);          \
} while (0)
#define SAFE_RELEASE_NULL(ptr)  do {  \
if (ptr) CFRelease(ptr);          \
(ptr) = NULL;                     \
} while (0)

Boolean createCFMutableArray(CFMutableArrayRef * array,
							 const CFArrayCallBacks * callbacks)
{
    Boolean result = true;
	
    *array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
								  callbacks);
    if (!*array) {
        result = false;
    }
    return result;
}

char * createUTF8CStringForCFString(CFStringRef aString)
{
    char     * result = NULL;
    CFIndex    bufferLength = 0;
	
    if (!aString) {
        goto finish;
    }
	
    bufferLength = sizeof('\0') +
	CFStringGetMaximumSizeForEncoding(CFStringGetLength(aString),
									  kCFStringEncodingUTF8);
	
    result = (char *)malloc(bufferLength * sizeof(char));
    if (!result) {
        goto finish;
    }
    if (!CFStringGetCString(aString, result, bufferLength,
							kCFStringEncodingUTF8)) {
		
        SAFE_FREE_NULL(result);
        goto finish;
    }
	
finish:
    return result;
}

Boolean getNumValue(CFNumberRef aNumber, CFNumberType type, void * valueOut)
{
    if (aNumber) {
        return CFNumberGetValue(aNumber, type, valueOut);
    }
    return false;
}

+ (NSMutableArray *)loadedExtensions {
	CFMutableArrayRef	bundleIDs = NULL;          // must release
    CFArrayRef			loadedKextInfo = NULL;     // must release
    const NXArchInfo *	runningKernelArch;  // do not free
	NSMutableArray *	returnArray = [[NSMutableArray alloc] init];
    CFIndex				count, i;
	
	if (!createCFMutableArray(&bundleIDs, &kCFTypeArrayCallBacks)) {
        goto finish;
	}
	
	runningKernelArch = OSKextGetRunningKernelArchitecture();
    if (!runningKernelArch) {
		NSLog(@"USB Prober: couldn't get running kernel architecture.\n");
        goto finish;
    }
	
    loadedKextInfo = OSKextCreateLoadedKextInfo(bundleIDs);
	
    if (!loadedKextInfo) {
  		NSLog(@"USB Prober: couldn't get list of loaded kexts from kernel.\n");
        goto finish;
    }
	
    count = CFArrayGetCount(loadedKextInfo);
    for (i = 0; i < count; i++) {
        CFDictionaryRef kextInfo = (CFDictionaryRef)CFArrayGetValueAtIndex(loadedKextInfo, i);
		
      //  printKextInfo(kextInfo, &toolArgs);

		CFNumberRef       loadAddress            = NULL;  // do not release
		CFNumberRef       loadSize               = NULL;  // do not release
		CFNumberRef       wiredSize              = NULL;  // do not release
		CFStringRef       bundleID               = NULL;  // do not release
		CFStringRef       bundleVersion          = NULL;  // do not release
		
		uint64_t          loadAddressValue       = (uint64_t)-1;
		uint32_t          loadSizeValue          = (uint32_t)-1;
		uint32_t          wiredSizeValue         = (uint32_t)-1;
		char            * bundleIDCString        = NULL;  // must free
		char            * bundleVersionCString   = NULL;  // must free
		
		
		loadAddress = (CFNumberRef)CFDictionaryGetValue(kextInfo,
														CFSTR(kOSBundleLoadAddressKey));
		loadSize = (CFNumberRef)CFDictionaryGetValue(kextInfo,
													 CFSTR(kOSBundleLoadSizeKey));
		wiredSize = (CFNumberRef)CFDictionaryGetValue(kextInfo,
													  CFSTR(kOSBundleWiredSizeKey));
		bundleID = (CFStringRef)CFDictionaryGetValue(kextInfo,
													 kCFBundleIdentifierKey);
		bundleVersion = (CFStringRef)CFDictionaryGetValue(kextInfo,
														  kCFBundleVersionKey);
		if (!getNumValue(loadAddress, kCFNumberSInt64Type, &loadAddressValue)) {
			loadAddressValue = (uint64_t)-1;
		}
		if (!getNumValue(loadSize, kCFNumberSInt32Type, &loadSizeValue)) {
			loadSizeValue = (uint32_t)-1;
		}
		if (!getNumValue(wiredSize, kCFNumberSInt32Type, &wiredSizeValue)) {
			wiredSizeValue = (uint32_t)-1;
		}
		
		bundleIDCString = createUTF8CStringForCFString(bundleID);
		bundleVersionCString = createUTF8CStringForCFString(bundleVersion);
		
		
		NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
        [dict setObject:[NSString stringWithFormat:@"%s", bundleIDCString] forKey:@"Name"];
        [dict setObject:[NSString stringWithFormat:@"%s", bundleVersionCString] forKey:@"Version"];
        if (loadSizeValue != 0) {
            if (loadSizeValue > 1024) {
                [dict setObject:[NSString stringWithFormat:@"%U KB",loadSizeValue/1024] forKey:@"Size"];
            } else {
                [dict setObject:[NSString stringWithFormat:@"%U Bytes",loadSizeValue] forKey:@"Size"];
            }
        } else {
            [dict setObject:@"n/a" forKey:@"Size"];
        }
        if (wiredSizeValue != 0) {
            if ((wiredSizeValue) > 1024) {
                [dict setObject:[NSString stringWithFormat:@"%U KB", (wiredSizeValue)/1024] forKey:@"Wired"];
            } else {
                [dict setObject:[NSString stringWithFormat:@"%U bytes", wiredSizeValue] forKey:@"Wired"];
            }
        } else {
            [dict setObject:@"n/a" forKey:@"Wired"];
        }
        [dict setObject:[NSString stringWithFormat:@"%-18p",loadAddressValue] forKey:@"Address"];
        
		if (runningKernelArch->cputype & CPU_ARCH_ABI64) {
			if (loadAddressValue == (uint64_t)-1) {
				[dict setObject:[NSString stringWithFormat:@"%%-18s", kStringInvalidLong] forKey:@"Address"];
			} else {
				[dict setObject:[NSString stringWithFormat:@"%#-18llx",(uint64_t)loadAddressValue] forKey:@"Address"];
			}
		} else {
			if (loadAddressValue == (uint64_t)-1) {
				[dict setObject:[NSString stringWithFormat:@"%%-10s",kStringInvalidLong] forKey:@"Address"];
				fprintf(stdout, " %-10s", kStringInvalidLong);
			} else {
				[dict setObject:[NSString stringWithFormat:@"%#-10x",(uint32_t)loadAddressValue] forKey:@"Address"];
			}
		}
		
		[returnArray addObject:dict];
        [dict release];
		SAFE_FREE(bundleIDCString);
		SAFE_FREE(bundleVersionCString);
    }
	
finish:
	SAFE_RELEASE(bundleIDs);
	SAFE_RELEASE(loadedKextInfo);
	
    return [returnArray autorelease];
}

+ (NSMutableArray *)loadedExtensionsContainingString:(NSString *)string {
    NSMutableArray *returnArray = [NSMutableArray arrayWithArray:[KextInfoGatherer loadedExtensions]];
    if (returnArray != nil) {
		NSArray * arrayCopy = [ returnArray copy ];
        NSEnumerator *enumerator = [arrayCopy objectEnumerator];
        NSDictionary *thisKext = NULL;
        
        while (thisKext = [enumerator nextObject]) {
            if ([[thisKext objectForKey:@"Name"] rangeOfString:string options:NSCaseInsensitiveSearch].location == NSNotFound) {
                [returnArray removeObject:thisKext];
            }
        }
		[arrayCopy release];
    }
     
        
    return returnArray;
}

@end
