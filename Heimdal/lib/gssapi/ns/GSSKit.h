/* -*- mode: objc -*-
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef GSSKIT_GSSKIT_H_
#define GSSKIT_GSSKIT_H_

#import <GSS/gssapi.h>
#import <dispatch/dispatch.h>

enum {
	GSS_C_ENC_BINARY,
	GSS_C_ENC_BASE64
};
typedef OM_uint32 GSSEncoding;

@interface GSSError : NSObject
- (OM_uint32)majorStatus;
- (OM_uint32)minorStatus;
- (uint32_t)windowsErrorCode;
- (NSString *)displayString;
@end

@interface GSSOID : NSObject

- (NSString *)description;
- (gssOID)GSSOID;

@end

@interface GSSName : NSObject

+ (GSSName *)nameWithHostBasedService: (NSString *)service withHostName: (NSString *)hostname;
+ (GSSName *)nameWithUserName: (NSString *username);
+ (GSSName *)nameWithGSSTypes: (NSData *)data withMech: (gssOID)nameType;
    
- (NSData *)exportName;
@end

@interface GSSMechanism : NSObject
+ (GSSMechanism *)mechanismSPNEGO;
+ (GSSMechanism *)mechanismKerberos;
+ (GSSMechanism *)mechanismPKU2U;
+ (GSSMechanism *)mechanismSCRAM;
+ (GSSMechanism *)mechanismNTLM;
+ (GSSMechanism *)mechanismSASLDigestMD5;

+ (GSSMechanism *)mechanismWithOID: (gssOID)oid;
+ (GSSMechanism *)mechanismWithDERData: (NSData *)data;
+ (GSSMechanism *)mechanismWithSASLName: (NSString *)name;

- (gssOID)oid;
- (NSString *)name;
@end

@interface GSSCredential : NSObject
+ (void)credentialWithExistingCredential: (GSSName *) mech: (GSSMechanism *)mech usageflags: (OM_uint32)flags queue:(dispatch_queue_t)queue completion: (^)(GSSCredential *, GSSError *);
+ (void)credentialWithExportedData: (NSData *)exportedData queue:(dispatch_queue_t)queue completion: (^)(GSSCredential *, GSSError *);
+ (void)credentialWithName: (GSSName *) mech: (GSSMechanism *)mech usageFlags: (OM_uint32)flags authIdentity: (gss_auth_identity_t)authId queue:(dispatch_queue_t)queue completion: (^)(GSSCredential *, GSSError *);
+ (void)credentialWithNameAndPassword: (GSSName *) mech: (GSSMechanism *)mech usageFlags: (OM_uint32)flags password: (NSString *) queue:(dispatch_queue_t)queue completion: (^)(GSSCredential *, GSSError *);

+ (void)iterateWithFlags: (OM_uint32)flags ofMechanism: (GSSName *) mech
		callback: (^)(GSSMechanism mech, gss_cred_id_t cred);

- (void)mergeWithCredential: (GSSCredential *)additionalCredential;

- (void)destroy;

- (GSSName *)name;
- (OM_uint32)lifetime;
- (OM_uint32)credUsage;
- (NSArray *)mechanisms;
- (NSData *)export;


- (void)retainCredential;
- (void)releaseCredential;
@end

@interface GSSBindings : NSObject
+ bindingsFromSecCertificate: (SecCertificateRef)certificate;
- setInitiatorAddress: (NSData *)addr ofType: (OM_uint32)type;
- setAcceptorAddress: (NSData *)addr ofType: (OM_uint32)type;
- setApplicationData: (NSData *)data;
@end

@interface GSSContext : NSObject

- (void)initWithRequestFlags: (OM_uint32)flags queue: (dispatch_queue_t)queue isInitiator: (bool)initiator;

/**
 * If not set, default mechanism is SPNEGO
 */
- (void)setMechanism: (GSSMechanism *)mechanism;
- (void)setRequestFlags: (OM_uint32)flags;
- (void)setTargetName: (GSSName *)targetName;
- (void)setCredential: (GSSCredential *)credential;
- (void)setChannelBindings: (GSSChannelBindings *)bindings;

- (void)setEncoding:(GSSEncoding)encoding;

- (void)stepWithData: (NSData *)indata completionHandler: (^)(GSSStatusCode major, NSData *data, OM_uint32 flags)handler;

- (GSSMechanism *)finalMechanism;
- (OM_uint32)finalFlags;

- (GSSCredential *)delegatedCredentials;

- (GSSError *)lastError;

/*
 *
 */

- (NSData *)wrapData: (NSData *)data withFlags: (OM_uint32)flags;
- (NSData *)unwrapData: (NSData *)data withFlags: (OM_uint32 *)flags;

- (NSData *)messageIntegrityCodeFromData: (NSData *)data withFlags: (OM_uint32)flags;
- (BOOL)verifyMessageIntegrityCodeFromData: (NSData *)data withCode: (NSData *)mic returnFlags: (OM_uint32 *)flags error: (NSError *)error;

@end

@interface NetworkAuthenticationSelection : NSObject

- (bool)acquire:(^)(NSError *)completion;
- (NSDictionary *)authInfo;
- (GSSCredential *)credential;
- (GSSMechanism *)mech;
- (GSSName *)acceptorName;

@end

@interface NetworkAuthenticationHelper : NSObject


(NetworkAuthenticationHelper *)initWithHostname: (NSString *)hostname withService: (NSService *)service withParams: (NSDictionary *)info;

(NSArray *)selections;


@end
