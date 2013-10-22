//
//  SecOTRPackets.h
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 2/23/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#include <Security/SecOTRSession.h>

#include <CoreFoundation/CFData.h>

#ifndef _SECOTRPACKETS_H_
#define _SECOTRPACKETS_H_

void SecOTRAppendDHMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendDHKeyMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendRevealSignatureMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);
void SecOTRAppendSignatureMessage(SecOTRSessionRef session, CFMutableDataRef appendTo);

typedef enum {
    kDHMessage = 0x02,
    kDataMessage = 0x03,
    kDHKeyMessage = 0x0A,
    kRevealSignatureMessage = 0x11,
    kSignatureMessage = 0x12,
    
    kInvalidMessage = 0xFF
} OTRMessageType;

#endif
