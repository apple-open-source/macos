/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
cc classcount.c -o classcount -Wall -framework IOKit
 */

#include <assert.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

int main(int argc, char **argv)
{
    kern_return_t	status;
    mach_port_t		masterPort;
    io_registry_entry_t	root;
    CFDictionaryRef	dictionary;
    CFDictionaryRef	props;
    CFStringRef		key;
    CFNumberRef		num;
    int			arg;

    // Parse args

    if( argc < 2 ) {
	printf("%s ClassName...\n", argv[0]);
	exit(0);
    }

    // Obtain the I/O Kit communication handle.

    status = IOMasterPort(bootstrap_port, &masterPort);
    assert(status == KERN_SUCCESS);

    // Obtain the registry root entry.

    root = IORegistryGetRootEntry(masterPort);
    assert(root);

    status = IORegistryEntryCreateCFProperties(root,
			(CFTypeRef *)&props,
			kCFAllocatorDefault, kNilOptions );
    assert( KERN_SUCCESS == status );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(props));

    dictionary = (CFDictionaryRef)
		CFDictionaryGetValue( props, CFSTR(kIOKitDiagnosticsKey));
    assert( dictionary );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(dictionary));

    dictionary = (CFDictionaryRef)
		CFDictionaryGetValue( dictionary, CFSTR("Classes"));
    assert( dictionary );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(dictionary));

    for( arg = 1; arg < argc; arg++ ) {
	key = CFStringCreateWithCString(kCFAllocatorDefault,
			argv[arg], CFStringGetSystemEncoding());
	assert(key);
        num = (CFNumberRef) CFDictionaryGetValue(dictionary, key);
	CFRelease(key);
        if( num) {
	    SInt32	num32;
            assert( CFNumberGetTypeID() == CFGetTypeID(num) );
	    CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
            printf("%s = %d, ", argv[arg], (int)num32);
	}
    }
    if( num)
        printf("\n");

    CFRelease(props);
    IOObjectRelease(root);

    exit(0);	
}

