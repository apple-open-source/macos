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
	printf("%22s = %08lx = %ld K\n",
                CFStringGetCStringPtr(name, kCFStringEncodingMacRoman),
                num32, num32 / 1024);
    }
}

int main(int argc, char **argv)
{
    mach_port_t		   masterPort;
    io_registry_entry_t	   root;
    CFDictionaryRef        props;
    kern_return_t          status;

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

    props = (CFDictionaryRef)
		CFDictionaryGetValue( props, CFSTR(kIOKitDiagnosticsKey));
    assert( props );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(props));

    printNumber(props, CFSTR("Instance allocation"));
    printNumber(props, CFSTR("Container allocation"));
    printNumber(props, CFSTR("IOMalloc allocation"));

    CFRelease(props);
    IOObjectRelease(root);

    exit(0);	
}

