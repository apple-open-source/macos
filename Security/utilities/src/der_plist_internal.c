//
//  SecCFToDER.c
//  utilities
//
//  Created by Mitch Adler on 7/2/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include "utilities/der_plist_internal.h"
#include "utilities/SecCFError.h"
#include "utilities/SecCFRelease.h"
#include <CoreFoundation/CoreFoundation.h>

CFStringRef sSecDERErrorDomain = CFSTR("com.apple.security.cfder.error");

void SecCFDERCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError) {
    SecCFCreateError(errorCode, sSecDERErrorDomain, descriptionString, previousError, newError);
}
