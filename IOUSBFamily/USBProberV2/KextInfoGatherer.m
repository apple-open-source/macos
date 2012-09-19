/*
 * Copyright © 1998-2010 Apple Inc.  All rights reserved.
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
	return NULL;
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
