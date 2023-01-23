/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#ifndef EscrowTranslation_h
#define EscrowTranslation_h
#if __OBJC2__

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <OctagonTrust/OTEscrowRecord.h>
#import <OctagonTrust/OTEscrowAuthenticationInformation.h>
#import <OctagonTrust/OTICDPRecordContext.h>
#import <OctagonTrust/OTCDPRecoveryInformation.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTEscrowTranslation : NSObject

//dictionary to escrow auth
+ (OTEscrowAuthenticationInformation* _Nullable )dictionaryToEscrowAuthenticationInfo:(NSDictionary*)dictionary;

//escrow auth to dictionary
+ (NSDictionary* _Nullable)escrowAuthenticationInfoToDictionary:(OTEscrowAuthenticationInformation*)escrowAuthInfo;

//dictionary to escrow record
+ (OTEscrowRecord* _Nullable)dictionaryToEscrowRecord:(NSDictionary*)dictionary;

//escrow record to dictionary
+ (NSDictionary* _Nullable)escrowRecordToDictionary:(OTEscrowRecord*)escrowRecord;

//dictionary to icdp record context
+ (OTICDPRecordContext* _Nullable)dictionaryToCDPRecordContext:(NSDictionary*)dictionary;

//icdp record context to dictionary
+ (NSDictionary* _Nullable)CDPRecordContextToDictionary:(OTICDPRecordContext*)context;

+ (NSDictionary * _Nullable) metadataToDictionary:(OTEscrowRecordMetadata*)metadata;

+ (OTEscrowRecordMetadata * _Nullable) dictionaryToMetadata:(NSDictionary*)dictionary;

+ (BOOL)supportedRestorePath:(OTICDPRecordContext*)cdpContext;

@end

NS_ASSUME_NONNULL_END

#endif

#endif /* EscrowTranslation_h */
