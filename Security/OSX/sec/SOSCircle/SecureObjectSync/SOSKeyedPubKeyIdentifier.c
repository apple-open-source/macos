/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

//
//  SOSKeyedPubKeyIdentifier.c
//  Security
//

#include "SOSKeyedPubKeyIdentifier.h"
#include "AssertMacros.h"
#include "SOSInternal.h"
#include <utilities/debugging.h>

#define SEPARATOR CFSTR("-")
#define SEPLOC 2

bool SOSKeyedPubKeyIdentifierIsPrefixed(CFStringRef kpkid) {
    CFRange seploc = CFStringFind(kpkid, SEPARATOR, 0);
    return seploc.location == SEPLOC;
}

static CFStringRef SOSKeyedPubKeyIdentifierCreateWithPrefixAndID(CFStringRef prefix, CFStringRef id) {
    CFMutableStringRef retval = NULL;
    require_quiet(prefix, errOut);
    require_quiet(id, errOut);
    require_quiet(CFStringGetLength(prefix) == SEPLOC, errOut);
    retval = CFStringCreateMutableCopy(kCFAllocatorDefault, 50, prefix);
    CFStringAppend(retval, SEPARATOR);
    CFStringAppend(retval, id);
errOut:
    return retval;
}

CFStringRef SOSKeyedPubKeyIdentifierCreateWithData(CFStringRef prefix, CFDataRef pubKeyData) {
    CFErrorRef localError = NULL;
    CFStringRef id = SOSCopyIDOfDataBuffer(pubKeyData, &localError);
    CFStringRef retval = SOSKeyedPubKeyIdentifierCreateWithPrefixAndID(prefix, id);
    if(!id) secnotice("kpid", "Couldn't create kpid: %@", localError);
    CFReleaseNull(id);
    CFReleaseNull(localError);
    return retval;
}

CFStringRef SOSKeyedPubKeyIdentifierCreateWithSecKey(CFStringRef prefix, SecKeyRef pubKey) {
    CFErrorRef localError = NULL;
    CFStringRef id = SOSCopyIDOfKey(pubKey, &localError);
    CFStringRef retval = SOSKeyedPubKeyIdentifierCreateWithPrefixAndID(prefix, id);
    if(!id) secnotice("kpid", "Couldn't create kpid: %@", localError);
    CFReleaseNull(id);
    CFReleaseNull(localError);
    return retval;
}


CFStringRef SOSKeyedPubKeyIdentifierCopyPrefix(CFStringRef kpkid) {
    CFRange seploc = CFStringFind(kpkid, SEPARATOR, 0);
    if(seploc.location != SEPLOC) return NULL;
    CFRange prefloc = CFRangeMake(0, SEPLOC);
    return CFStringCreateWithSubstring(kCFAllocatorDefault, kpkid, prefloc);;
}

CFStringRef SOSKeyedPubKeyIdentifierCopyHpub(CFStringRef kpkid) {
    CFRange seploc = CFStringFind(kpkid, SEPARATOR, 0);
    if(seploc.location != SEPLOC) return NULL;
    CFRange idloc = CFRangeMake(seploc.location+1, CFStringGetLength(kpkid) - (SEPLOC+1));
    return CFStringCreateWithSubstring(kCFAllocatorDefault, kpkid, idloc);;
}

