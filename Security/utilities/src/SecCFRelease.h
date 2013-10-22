//
//  SecCFRelease.h
//  utilities
//
//  Created by Mitch Adler on 2/9/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _SECCFRELEASE_H_
#define _SECCFRELEASE_H_

#include <CoreFoundation/CFBase.h>

#define CFRetainSafe(CF) {  \
    CFTypeRef _cf = (CF);   \
    if (_cf)                \
        CFRetain(_cf);      \
    }

#define CFReleaseSafe(CF) { \
    CFTypeRef _cf = (CF);   \
    if (_cf)                \
        CFRelease(_cf);     \
    }

#define CFReleaseNull(CF) { \
    CFTypeRef _cf = (CF);   \
    if (_cf) {              \
        (CF) = NULL;        \
        CFRelease(_cf);     \
        }                   \
    }

#endif
