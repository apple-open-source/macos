/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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


#ifndef OTContextRecord_h
#define OTContextRecord_h

#if OCTAGON

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


@interface OTContextRecord : NSObject

@property (nonatomic, strong)   NSString*       contextID;
@property (nonatomic, strong)   NSString*       dsid;
@property (nonatomic, strong)   NSString*       contextName;
@property (nonatomic)           BOOL            zoneCreated;
@property (nonatomic)           BOOL            subscribedToChanges;
@property (nonatomic, strong)   NSData*         changeToken;
@property (nonatomic, strong)   NSString*       egoPeerID;
@property (nonatomic, strong)   NSDate*         egoPeerCreationDate;
@property (nonatomic, strong)   NSData*         recoverySigningSPKI;
@property (nonatomic, strong)   NSData*         recoveryEncryptionSPKI;


-(BOOL)isEqual:(OTContextRecord*)record;

@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
#endif /* OTContextRecord_h */
