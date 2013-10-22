//
//  SecOTRDHKey.h
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 3/2/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//
#ifndef _SECOTRDHKEY_H_
#define _SECOTRDHKEY_H_

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <corecrypto/ccn.h>

__BEGIN_DECLS

typedef struct _SecOTRFullDHKey* SecOTRFullDHKeyRef;
typedef struct _SecOTRPublicDHKey* SecOTRPublicDHKeyRef;

SecOTRFullDHKeyRef SecOTRFullDHKCreate(CFAllocatorRef allocator);
SecOTRFullDHKeyRef SecOTRFullDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size);

void SecFDHKNewKey(SecOTRFullDHKeyRef key);
void SecFDHKAppendSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo);
void SecFDHKAppendPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo);
uint8_t* SecFDHKGetHash(SecOTRFullDHKeyRef pubKey);


SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromFullKey(CFAllocatorRef allocator, SecOTRFullDHKeyRef full);
SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromSerialization(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size);
SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size);

void SecPDHKAppendSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo);
uint8_t* SecPDHKGetHash(SecOTRPublicDHKeyRef pubKey);

void SecPDHKeyGenerateS(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey, cc_unit* s);

bool SecDHKIsGreater(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey);

void SecOTRDHKGenerateOTRKeys(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey,
                           uint8_t* sendMessageKey, uint8_t* sendMacKey,
                           uint8_t* receiveMessageKey, uint8_t* receiveMacKey);

__END_DECLS

#endif
