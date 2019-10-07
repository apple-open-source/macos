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
#if OCTAGON

#import <Foundation/Foundation.h>

@interface OTBottledPeerRecord : NSObject

@property (nonatomic, strong) NSString* peerID;
@property (nonatomic, strong) NSString* spID;
@property (nonatomic, strong) NSData*   bottle;
@property (nonatomic, strong) NSString* escrowRecordID;
@property (nonatomic, strong) NSData*   escrowedSigningSPKI;
@property (nonatomic, strong) NSData*   peerSigningSPKI;
@property (nonatomic, strong) NSData*   signatureUsingEscrowKey;
@property (nonatomic, strong) NSData*   signatureUsingPeerKey;
@property (nonatomic, strong) NSData*   encodedRecord;
@property (nonatomic, readonly) NSString* recordName;
@property (nonatomic, strong) NSString* launched;

+ (NSString*) constructRecordID:(NSString*)escrowRecordID escrowSigningSPKI:(NSData*)escrowSigningSPKI;

@end
#endif
