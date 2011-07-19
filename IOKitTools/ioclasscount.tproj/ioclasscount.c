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
cc ioclasscount.c -o /tmp/ioclasscount -Wall -framework IOKit -framework CoreFoundation
 */

#include <sysexits.h>
 #include <malloc/malloc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

/*********************************************************************
*********************************************************************/
static int compareClassNames(const void * left, const void * right)
{
    switch (CFStringCompare(*((CFStringRef *)left), *((CFStringRef *)right), 
        (CFStringCompareFlags)kCFCompareCaseInsensitive)) {
    case kCFCompareLessThan:
        return -1;
        break;
    case kCFCompareEqualTo:
        return 0;
        break;
    case kCFCompareGreaterThan:
        return 1;
        break;
    default:
        fprintf(stderr, "fatal error\n");
        exit(EX_OSERR);
        return 0;
        break;
    }
}

/*********************************************************************
*********************************************************************/
static Boolean printInstanceCount(
    CFDictionaryRef dict,
    CFStringRef     name,
    char         ** nameCString,
    Boolean         addNewlineFlag)
{
    int           result     = FALSE;
    CFIndex       nameLength = 0;
    static char * nameBuffer = NULL;  // free if nameCString is NULL
    CFNumberRef   num        = NULL;  // do not release
    SInt32	      num32      = 0;
    Boolean       gotName    = FALSE;
    Boolean       gotNum     = FALSE;

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
    
   /* Note that errors displaying the name and value are not considered
    * fatal and do not affect the exit status of the program.
    */
    printf("%s = ", gotName ? nameBuffer : "??");

    num = (CFNumberRef)CFDictionaryGetValue(dict, name);
    if (num) {

        if (CFNumberGetTypeID() == CFGetTypeID(num)) {
            gotNum = CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
        }
        if (gotNum) {
            printf("%lu", (unsigned long)num32);
        } else {
            printf("?? (error reading/converting value)");
        }
    } else {
        printf("<no such class>");
    }
    
    if (addNewlineFlag) {
        printf("\n");
    } else {
        printf(", ");
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
    int                    result      = EX_OSERR;
    kern_return_t          status      = KERN_FAILURE;
    io_registry_entry_t    root        = IO_OBJECT_NULL;  // must IOObjectRelease()
    CFMutableDictionaryRef rootProps   = NULL;            // must release
    CFDictionaryRef        diagnostics = NULL;            // do not release
    CFDictionaryRef        classes     = NULL;            // do not release
    CFStringRef          * classNames  = NULL;            // must free
    CFStringRef            className   = NULL;            // must release
    char                 * nameCString = NULL;            // must free
    
    // Obtain the registry root entry.
    
    root = IORegistryGetRootEntry(kIOMasterPortDefault);
    if (!root) {
        fprintf(stderr, "Error: Can't get registry root.\n");
        goto finish;
    }
    
    status = IORegistryEntryCreateCFProperties(root,
        &rootProps, kCFAllocatorDefault, kNilOptions);
    if (KERN_SUCCESS != status) {
        fprintf(stderr, "Error: Can't read registry root properties.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(rootProps)) {
        fprintf(stderr, "Error: Registry root properties not a dictionary.\n");
        goto finish;
    }
    
    diagnostics = (CFDictionaryRef)CFDictionaryGetValue(rootProps,
        CFSTR(kIOKitDiagnosticsKey));
    if (!diagnostics) {
        fprintf(stderr, "Error: Allocation information missing.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(diagnostics)) {
        fprintf(stderr, "Error: Allocation information not a dictionary.\n");
        goto finish;
    }
    
    classes = (CFDictionaryRef)CFDictionaryGetValue(diagnostics, CFSTR("Classes"));
    if (!classes) {
        fprintf(stderr, "Error: Class information missing.\n");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(classes)) {
        fprintf(stderr, "Error: Class information not a dictionary.\n");
        goto finish;
    }
    
    if (argc < 2) {
        CFIndex       index, count;
        
        count = CFDictionaryGetCount(classes);
        classNames = (CFStringRef *)calloc(count, sizeof(CFStringRef));
        if (!classNames) {
            fprintf(stderr, "Memory allocation failure.\n");
            goto finish;
        }
        CFDictionaryGetKeysAndValues(classes, (const void **)classNames, NULL);
        qsort(classNames, count, sizeof(CFStringRef), &compareClassNames);
        
        for (index = 0; index < count; index++) {
            printInstanceCount(classes, classNames[index], &nameCString,
                /* addNewline? */ TRUE);
        }
        
    } else {
        uint32_t index = 0;
        for (index = 1; index < argc; index++ ) {

            if (className) CFRelease(className);
            className = NULL;

            className = CFStringCreateWithCString(kCFAllocatorDefault,
                argv[index], kCFStringEncodingUTF8);
            if (!className) {
                fprintf(stderr, "Error: Can't create CFString for '%s'.\n",
                    argv[index]);
                goto finish;
            }
            printInstanceCount(classes, className, &nameCString,
                /* addNewline? */ (index + 1 == argc));
        }
    }
    
    result = EX_OK;
finish:
    if (rootProps)              CFRelease(rootProps);
    if (root != IO_OBJECT_NULL) IOObjectRelease(root);
    if (classNames)             free(classNames);
    if (className)              CFRelease(className);
    if (nameCString)            free(nameCString);

    return result;
}

