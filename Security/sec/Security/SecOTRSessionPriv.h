//
//  SecOTRSessionPriv.h
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 2/23/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#ifndef _SECOTRSESSIONPRIV_H_
#define _SECOTRSESSIONPRIV_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRuntime.h>

#include <Security/SecOTR.h>
#include <corecrypto/ccn.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccsha1.h>

#include <CommonCrypto/CommonDigest.h>

#include <dispatch/dispatch.h>

#include <Security/SecOTRMath.h>
#include <Security/SecOTRDHKey.h>

__BEGIN_DECLS

typedef enum {
    kIdle,
    kAwaitingDHKey,
    kAwaitingRevealSignature,
    kAwaitingSignature,
    kDone
} SecOTRAuthState;

struct _SecOTRCacheElement {
    SecOTRFullDHKeyRef _fullKey;
    uint8_t _fullKeyHash[CCSHA1_OUTPUT_SIZE];
    SecOTRPublicDHKeyRef _publicKey;
    uint8_t _publicKeyHash[CCSHA1_OUTPUT_SIZE];

    uint8_t _sendMacKey[kOTRMessageMacKeyBytes];
    uint8_t _sendEncryptionKey[kOTRMessageKeyBytes];

    uint8_t _receiveMacKey[kOTRMessageMacKeyBytes];
    uint8_t _receiveEncryptionKey[kOTRMessageKeyBytes];

    uint64_t _counter;
    uint64_t _theirCounter;
    
};
typedef struct _SecOTRCacheElement SecOTRCacheElement;

#define kOTRKeyCacheSize 4

struct _SecOTRSession {
    CFRuntimeBase _base;
    
    SecOTRAuthState _state;
    
    SecOTRFullIdentityRef    _me;
    SecOTRPublicIdentityRef  _them;
    
    uint8_t _r[kOTRAuthKeyBytes];
    
    CFDataRef _receivedDHMessage;
    CFDataRef _receivedDHKeyMessage;

    uint32_t _keyID;
    SecOTRFullDHKeyRef _myKey;
    SecOTRFullDHKeyRef _myNextKey;

    uint32_t _theirKeyID;
    SecOTRPublicDHKeyRef _theirPreviousKey;
    SecOTRPublicDHKeyRef _theirKey;
    
    CFMutableDataRef _macKeysToExpose;

    dispatch_queue_t _queue;

    SecOTRCacheElement _keyCache[kOTRKeyCacheSize];
    
    bool _textOutput;
};

void SecOTRGetIncomingBytes(CFDataRef incomingMessage, CFMutableDataRef decodedBytes);
void SecOTRPrepareOutgoingBytes(CFMutableDataRef destinationMessage, CFMutableDataRef protectedMessage);

__END_DECLS

#endif
