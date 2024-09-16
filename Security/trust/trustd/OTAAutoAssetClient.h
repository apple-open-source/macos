/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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
 *
 * OTAAutoAssetClient.h
 */

#ifndef _OTAAUTOASSETCLIENT_H_
#define _OTAAUTOASSETCLIENT_H_  1

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTAAutoAssetClient : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (nullable instancetype)initWithError:(NSError **)error;
- (nullable NSString *)startUsingLocalAsset;
- (void)stopUsingLocalAsset;
- (void)registerForAssetChangedNotificationsWithBlock:(void (^)(void))assetDidChangeHandler;
+ (BOOL)saveTrustStoreAssetPath:(NSString *)assetPath;
+ (nullable NSString *)validTrustStoreAssetPath:(NSString *)assetPath mustExist:(BOOL)mustExist;
+ (nullable NSString *)savedTrustStoreAssetPath;
@end

NS_ASSUME_NONNULL_END

__END_DECLS

#endif /* _OTAAUTOASSETCLIENT_H_ */
