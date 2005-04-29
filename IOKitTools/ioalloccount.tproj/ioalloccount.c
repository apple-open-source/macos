/*
cc alloccount.c -o alloccount -Wall -framework IOKit
 */

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

static void printNumber( CFDictionaryRef dict, CFStringRef name )
{
    CFNumberRef	num;
    SInt32	num32;

    num = (CFNumberRef) CFDictionaryGetValue(dict, name);
    if( num) {
        assert( CFNumberGetTypeID() == CFGetTypeID(num) );
        CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
	printf("%22s = 0x%08lx = %4ld K\n",
                CFStringGetCStringPtr(name, kCFStringEncodingMacRoman),
                num32, num32 / 1024);
    }
}

int main(int argc, char **argv)
{
    io_registry_entry_t	   root;
    CFDictionaryRef 	   props;
    kern_return_t          status;

    // Obtain the registry root entry.

    root = IORegistryGetRootEntry(kIOMasterPortDefault);
    assert(root);

    status = IORegistryEntryCreateCFProperties(root,
			(CFMutableDictionaryRef *) &props,
			kCFAllocatorDefault, kNilOptions );
    assert( KERN_SUCCESS == status );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(props));

    props = (CFDictionaryRef)
		CFDictionaryGetValue( props, CFSTR(kIOKitDiagnosticsKey));
    assert( props );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(props));

    printNumber(props, CFSTR("Instance allocation"));
    printNumber(props, CFSTR("Container allocation"));
    printNumber(props, CFSTR("IOMalloc allocation"));
    printNumber(props, CFSTR("Pageable allocation"));

    CFRelease(props);
    IOObjectRelease(root);

    exit(0);	
}

