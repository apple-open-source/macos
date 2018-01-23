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

#if OCTAGON

#include <securityd/SecDbItem.h>

@class CKKSItem;
@class CKKSMirrorEntry;
@class CKKSKey;
@class CKKSOutgoingQueueEntry;
@class CKKSAESSIVKey;
@class CKRecordZoneID;

NS_ASSUME_NONNULL_BEGIN

#define CKKS_PADDING_MARK_BYTE 0x80

@interface CKKSItemEncrypter : NSObject

+ (CKKSItem* _Nullable)encryptCKKSItem:(CKKSItem*)baseitem
                        dataDictionary:(NSDictionary*)dict
                      updatingCKKSItem:(CKKSItem* _Nullable)olditem
                             parentkey:(CKKSKey*)parentkey
                                 error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (NSDictionary* _Nullable)decryptItemToDictionary:(CKKSItem*)item error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (NSData* _Nullable)encryptDictionary:(NSDictionary*)dict
                                   key:(CKKSAESSIVKey*)key
                     authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                                 error:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (NSDictionary* _Nullable)decryptDictionary:(NSData*)encitem
                                         key:(CKKSAESSIVKey*)key
                           authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                                       error:(NSError* _Nullable __autoreleasing* _Nullable)error;

+ (NSData*)padData:(NSData*)input blockSize:(NSUInteger)blockSize additionalBlock:(BOOL)extra;
+ (NSData* _Nullable)removePaddingFromData:(NSData*)input;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
