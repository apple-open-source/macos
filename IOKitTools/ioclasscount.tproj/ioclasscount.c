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
			&props,
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
			argv[arg], kCFStringEncodingMacRoman);
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

