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

#ifndef OTCloudStoreState_h
#define OTCloudStoreState_h

#if OCTAGON
#import "keychain/ckks/CKKSSQLDatabaseObject.h"

@interface OTCloudStoreState : CKKSSQLDatabaseObject

@property NSString* ckzone;
@property bool ckzonecreated;
@property bool ckzonesubscribed;
@property (getter=getChangeToken, setter=setChangeToken:) CKServerChangeToken* changeToken;
@property NSData* encodedChangeToken;
@property NSDate* lastFetchTime;

+ (instancetype)state:(NSString*)ckzone;

+ (instancetype)fromDatabase:(NSString*)ckzone error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)ckzone error:(NSError* __autoreleasing*)error;

- (instancetype)initWithCKZone:(NSString*)ckzone
                   zoneCreated:(bool)ckzonecreated
                zoneSubscribed:(bool)ckzonesubscribed
                   changeToken:(NSData*)changetoken
                     lastFetch:(NSDate*)lastFetch;

- (CKServerChangeToken*)getChangeToken;
- (void)setChangeToken:(CKServerChangeToken*)token;

- (BOOL)isEqual:(id)object;
@end

#endif
#endif /* OTCloudStoreState_h */
