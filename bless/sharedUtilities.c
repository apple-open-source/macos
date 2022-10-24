/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
 */

#include <stdio.h>
#include "sharedUtilities.h"

CFDataRef CreateDataFromFileURL(CFAllocatorRef alloc, CFURLRef fileURL, CFErrorRef *error)
{
    CFDataRef result = NULL;
    CFNumberRef fileSizeValue;
    // get the file size from the file URL
    if (CFURLCopyResourcePropertyForKey(fileURL, kCFURLFileSizeKey, &fileSizeValue, error)) {
        if (fileSizeValue) {
            CFIndex fileSize;
            // get the fileSize as a CFIndex
            if (CFNumberGetValue(fileSizeValue, kCFNumberCFIndexType, &fileSize)) {
                if (fileSize == 0) {
                    // zero-length file, return a zero-length CFData
                    result = CFDataCreate(alloc, NULL, 0);
                } else {
                    CFAllocatorRef bytesAllocator;
                    // we need a non-NULL allocator to use with CFDataCreateWithBytesNoCopy
                    if (alloc != NULL) {
                        bytesAllocator = alloc;
                    } else {
                        bytesAllocator = kCFAllocatorSystemDefault;
                    }
                    // create the read stream
                    CFReadStreamRef readStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, fileURL);
                    if (readStream) {
                        // open the read stream
                        if (CFReadStreamOpen(readStream)) {
                            // allocate the mutableBytes buffer to read into
                            UInt8 *mutableBytes = CFAllocatorAllocate(bytesAllocator, fileSize, 0);
                            if (mutableBytes) {
                                // read the file into the mutableBytes
                                CFIndex lengthRead = CFReadStreamRead(readStream, mutableBytes, fileSize);
                                if (lengthRead >= 0) {
                                    // create a CFData with mutableBytes
                                    result = CFDataCreateWithBytesNoCopy(bytesAllocator, mutableBytes, lengthRead, bytesAllocator);
                                    if (!result && error) {
                                        *error = CFErrorCreate(alloc, kCFErrorDomainPOSIX, ENOMEM, NULL);
                                    }
                                    // else success!
                                } else if (error) {
                                    *error = CFReadStreamCopyError(readStream);
                                }
                                if (!result) {
                                    CFAllocatorDeallocate(bytesAllocator, mutableBytes);
                                }
                                // else mutableBytes will be released when result is released
                            } else if (error) {
                                *error = CFErrorCreate(alloc, kCFErrorDomainPOSIX, ENOMEM, NULL);
                            }
                            CFReadStreamClose(readStream);
                        } else if (error) {
                            *error = CFReadStreamCopyError(readStream);
                        }
                        CFRelease(readStream);
                    } else if (error) {
                        // this should never happen unless a non-file URL is passed
                        *error = CFErrorCreate(alloc, kCFErrorDomainOSStatus, kIOReturnBadArgument, NULL);
                    }
                }
            } else if (error) {
                // the file size was larger than a CFIndex
                *error = CFErrorCreate(alloc, kCFErrorDomainPOSIX, EFBIG, NULL);
            }
        } else if (error) {
            // this should never happen unless a non-file URL is passed
            *error = CFErrorCreate(alloc, kCFErrorDomainOSStatus, kIOReturnBadArgument, NULL);
        }
    }
    return result;
}
