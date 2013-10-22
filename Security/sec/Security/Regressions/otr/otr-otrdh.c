//
//  otr-otrdh.c
//  OTR
//
//  Created by Mitch Adler on 7/22/11.
//  Copyright (c) 2011 Apple Inc. All rights reserved.
//

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
