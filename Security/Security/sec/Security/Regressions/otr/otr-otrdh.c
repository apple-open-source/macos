/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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


#include "Security_regressions.h"

#include <Security/SecOTRMath.h>
#include <Security/SecOTRDHKey.h>

int otr_otrdh(int argc, char *const * argv)
{
    plan_tests(4);
    
    SecOTRFullDHKeyRef aliceFull = SecOTRFullDHKCreate(kCFAllocatorDefault);
    SecOTRPublicDHKeyRef alicePublic = SecOTRPublicDHKCreateFromFullKey(kCFAllocatorDefault, aliceFull);
    
    SecOTRFullDHKeyRef bobFull = SecOTRFullDHKCreate(kCFAllocatorDefault);
    SecOTRPublicDHKeyRef bobPublic = SecOTRPublicDHKCreateFromFullKey(kCFAllocatorDefault, bobFull);
    
    uint8_t aliceMessageKeys[2][kOTRMessageKeyBytes];
    uint8_t aliceMacKeys[2][kOTRMessageMacKeyBytes];
    
    SecOTRDHKGenerateOTRKeys(aliceFull, bobPublic,
                          aliceMessageKeys[0], aliceMacKeys[0],
                          aliceMessageKeys[1], aliceMacKeys[1]);
    
    uint8_t bobMessageKeys[2][kOTRMessageKeyBytes];
    uint8_t bobMacKeys[2][kOTRMessageMacKeyBytes];
    
    SecOTRDHKGenerateOTRKeys(bobFull, alicePublic,
                          bobMessageKeys[0], bobMacKeys[0],
                          bobMessageKeys[1], bobMacKeys[1]);
    
    
    ok(0 == memcmp(aliceMessageKeys[0], bobMessageKeys[1], sizeof(aliceMessageKeys[0])), "Mac Keys don't match!!");
    ok(0 == memcmp(aliceMessageKeys[1], bobMessageKeys[0], sizeof(aliceMessageKeys[1])), "Mac Keys don't match!!");
    ok(0 == memcmp(aliceMacKeys[0], bobMacKeys[1], sizeof(aliceMacKeys[0])), "Mac Keys don't match!!");
    ok(0 == memcmp(aliceMacKeys[1], bobMacKeys[0], sizeof(aliceMacKeys[1])), "Mac Keys don't match!!");
    
    return 0;
}
