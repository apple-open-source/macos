/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
 * SecFramework.c - generic non API class specific functions
 */

#define SEC_BUILDER 1

//#include "SecFramework.h"
#include <strings.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURLAccess.h>
#include "SecRandomP.h"
#include <CommonCrypto/CommonRandomSPI.h>
#include <stdlib.h>

/* Default random ref for /dev/random. */
const SecRandomRef kSecRandomDefault = NULL;


int SecRandomCopyBytes(SecRandomRef rnd, size_t count, uint8_t *bytes) {
    if (rnd != kSecRandomDefault)
        return errSecParam;
    return CCRandomCopyBytes(kCCRandomDefault, bytes, count);
}


CFDataRef
SecRandomCopyData(SecRandomRef rnd, size_t count)
{
    uint8_t *bytes;
    CFDataRef retval = NULL;
    
    if (rnd != kSecRandomDefault) return NULL;
    if((bytes = malloc(count)) == NULL) return NULL;
    if(CCRandomCopyBytes(kCCRandomDefault, bytes, count) == kCCSuccess)
        retval = CFDataCreate(kCFAllocatorDefault, bytes, count);
    bzero(bytes, count);
    free(bytes);
    return retval;
}

