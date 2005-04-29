/*
cc -o /tmp/iogt graphics.subproj/IOGraphicsTest.c -framework CoreGraphics -Wall -undefined warning
*/

#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>

int main( int argc, char * argv[] )
{
    io_service_t 	service;
    CFDictionaryRef	dict;
    CFMutableArrayRef	array;
    CGSError		err;
    int			i, j;

    CGSInitialize();

    array = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                    &kCFTypeArrayCallBacks);

    for( i = 0; i < CGSGetNumberOfDisplays(); i++ ) {
        err = CGSServiceForDisplayNumber(i, &service);
        if( err != kCGSErrorSuccess)
            continue;
    
        dict = IOCreateDisplayInfoDictionary( service, kNilOptions );
        if( dict) {
            printf("Display %d\n", i);
            CFShow(dict);
            CFRelease(dict);
        }

        dict = IOCreateDisplayInfoDictionary( service,
                                            kIODisplayMatchingInfo );
        if( dict) {
            printf("Display %d, kIODisplayMatchingInfo\n", i);
            CFShow(dict);

            CFArrayAppendValue(array, dict);
            CFRelease(dict);
			CFIndex count = CFArrayGetCount(array);
            for( j = 0; j < count; j++) {
                printf("%d matches(%ld)\n", j,
                    IODisplayMatchDictionaries( dict,
                        CFArrayGetValueAtIndex(array, j), kNilOptions));
            }
        }
    }

    exit(0);
    return(0);
}

