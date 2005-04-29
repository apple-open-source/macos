/*
cc ioclasscount.c -o /tmp/ioclasscount -Wall -framework IOKit -framework CoreFoundation
 */

#include <assert.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

static int CompareKey(const void * left, const void * right)
{
    return (CFStringCompare(*((CFStringRef *)left), *((CFStringRef *)right), 
	    kCFCompareCaseInsensitive));
}

int main(int argc, char **argv)
{
    kern_return_t	   status;
    io_registry_entry_t	   root;
    CFDictionaryRef	   dictionary;
    CFMutableDictionaryRef props;
    CFStringRef		   key;
    CFNumberRef		   num;
    int			   arg;

    // Obtain the registry root entry.

    root = IORegistryGetRootEntry(kIOMasterPortDefault);
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

    if (argc < 2) {
	CFIndex count;
	CFStringRef * keys;
	SInt32	num32;
        char cstr[256];

	count = CFDictionaryGetCount(dictionary);
	keys = (CFStringRef *) calloc(count, sizeof(CFStringRef));
	CFDictionaryGetKeysAndValues(dictionary, (const void **)keys, NULL);
	qsort(keys, count, sizeof(CFStringRef), &CompareKey);

	for (arg = 0; arg < count; arg++) {
	    if (!CFStringGetCString(keys[arg],
		cstr, sizeof(cstr) - 1, kCFStringEncodingMacRoman))
		continue;
	    CFNumberGetValue(CFDictionaryGetValue(dictionary, keys[arg]),
				kCFNumberSInt32Type, &num32);
            printf("%s = %d\n", cstr, (int)num32);
	}
	num = 0;
	free(keys);

    } else {
	for( arg = 1; arg < argc; arg++ ) {
	    key = CFStringCreateWithCString(kCFAllocatorDefault,
			    argv[arg], kCFStringEncodingMacRoman);
	    assert(key);
	    num = (CFNumberRef) CFDictionaryGetValue(dictionary, key);
	    CFRelease(key);
	    printf("%s = ", argv[arg]);
	    if( num) {
		SInt32	num32;
		assert( CFNumberGetTypeID() == CFGetTypeID(num) );
		CFNumberGetValue(num, kCFNumberSInt32Type, &num32);
		printf("%d, ", (int)num32);
	    } else
		printf("<no such class>, ");
	}
	printf("\n");
    }

    CFRelease(props);
    IOObjectRelease(root);

    exit(0);	
}

