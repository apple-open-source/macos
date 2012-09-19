//
//  testgenerateoflabel.c
//
//  Copyright 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include "bless.h"


// cc -o testgenerate testgenerateoflabel.c libbless.a -framework CoreFoundation -framework ApplicationServices


int main(int argc, char *argv[])
{
    int         err;
    int         scale;
    CFStringRef path;
    CFURLRef    url;
    CFDataRef   labelData;
    
    if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "-2x"))) {
        fprintf(stderr, "Usage: %s label file [-2x]\n", getprogname());
        exit(1);
    }
    
    path = CFStringCreateWithCString(kCFAllocatorDefault, argv[2], kCFStringEncodingUTF8);
    if (!path) {
        fprintf(stderr, "Couldn't create string from %s\n", argv[2]);
        exit(1);
    }
    
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, false);
    if (!url) {
        fprintf(stderr, "Couldn't create URL for %s\n", argv[2]);
        exit(1);
    }
    
    scale = (argc == 4) ? kBitmapScale_2x : kBitmapScale_1x;
    
    err = BLGenerateLabelData(NULL, argv[1], scale, &labelData);
    if (err) {
        fprintf(stderr, "Couldn't create label data\n");
        exit(1);
    }
    
    CFURLWriteDataAndPropertiesToResource(url, labelData, NULL, NULL);
    
    CFRelease(path);
    CFRelease(url);
    CFRelease(labelData);
    
    return 0;
}
