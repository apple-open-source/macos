/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
cc alloccount.c -o alloccount -Wall -framework IOKit
 */

#include <sysexits.h>
#include <malloc/malloc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

/*********************************************************************
*********************************************************************/
static Boolean printNumber(
    CFDictionaryRef dict,
    CFStringRef     name,
    char         ** nameCString)
{
    int           result     = FALSE;
    CFIndex       nameLength = 0;
    static char * nameBuffer = NULL;  // free if nameCString is NULL
    CFNumberRef   num        = NULL;  // do not release
    SInt32	      num32      = 0;
    Boolean       gotName    = FALSE;
    Boolean       gotNum     = FALSE;

   /* Note that errors displaying the name and value are not considered
    * fatal and do not affect the exit status of the program.
    */
    num = (CFNumberRef)CFDictionaryGetValue(dict, name);
    if (num) {
        nameLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name),
            kCFStringEncodingUTF8);
        if (!nameCString || !*nameCString) {
            nameBuffer = (char *)malloc((1 + nameLength) * sizeof(char));
        } else if ((1 + nameLength) > malloc_size(nameBuffer)) {
            nameBuffer = (char *)realloc(*nameCString,
                (1 + nameLength) * sizeof(char));
        }
        if (nameBuffer) {
            gotName = CFStringGetCString(name, nameBuffer, 1 + nameLength,
                kCFStringEncodingUTF8);
        } else {
            fprintf(stderr, "Memory allocation failure.\n");
            goto finish;
        }
        printf("%22s = ", gotName ? nameBuffer : "??");

        if (CFNumberGetTypeID() == CFGetTypeID(num)) {
            gotNum = CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
        }
        if (gotNum) {
            printf("0x%08lx = %4lu K\n",
                   (unsigned long)num32,
                   (unsigned long)(num32 / 1024));
        } else {
            printf("?? (error reading/converting value)\n");
        }
    }

    result = TRUE;

finish:
    if (nameCString) {
        *nameCString = nameBuffer;
    } else {
        if (nameBuffer) free(nameBuffer);
    }
    return result;
}

/*********************************************************************
*********************************************************************/
int main(int argc, char ** argv)
{
    int                  result      = EX_OSERR;
    io_registry_entry_t  root        = IO_OBJECT_NULL;  // must IOObjectRelease()
    CFDictionaryRef 	 rootProps   = NULL;            // must release
    CFDictionaryRef 	 allocInfo   = NULL;            // do not release
    kern_return_t        status      = KERN_FAILURE;
    char               * nameCString = NULL;            // must free

    // Obtain the registry root entry.

    root = IORegistryGetRootEntry(kIOMasterPortDefault);
    if (!root) {
        fprintf(stderr, "Error: Can't get registry root.\n");
        goto finish;
    }

    status = IORegistryEntryCreateCFProperties(root,
        (CFMutableDictionaryRef *)&rootProps,
        kCFAllocatorDefault, kNilOptions );
    if (KERN_SUCCESS != status) {
        fprintf(stderr, "Error: Can't read registry root properties.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(rootProps)) {
        fprintf(stderr, "Error: Registry root properties not a dictionary.\n");
        goto finish;
    }

    allocInfo = (CFDictionaryRef)CFDictionaryGetValue(rootProps,
        CFSTR(kIOKitDiagnosticsKey));
    if (!allocInfo) {
        fprintf(stderr, "Error: Allocation information missing.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(allocInfo)) {
        fprintf(stderr, "Error: Allocation information not a dictionary.\n");
        goto finish;
    }

    if (!(printNumber(allocInfo, CFSTR("Instance allocation"),  &nameCString)  &&
          printNumber(allocInfo, CFSTR("Container allocation"), &nameCString) &&
          printNumber(allocInfo, CFSTR("IOMalloc allocation"),  &nameCString)  &&
          printNumber(allocInfo, CFSTR("Pageable allocation"),  &nameCString) )) {
          
          goto finish;
    }

    result = EX_OK;

finish:
    if (nameCString)            free(nameCString);
    if (rootProps)              CFRelease(rootProps);
    if (root != IO_OBJECT_NULL) IOObjectRelease(root);

    return result;
}

