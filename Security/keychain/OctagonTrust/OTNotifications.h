/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#if __OBJC2__

#ifndef OTNOTIFICATIONS_H
#define OTNOTIFICATIONS_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * This is a Darwin notification sent when the user controllable view syncing status changes.
 */
extern NSString* OTUserControllableViewStatusChanged;

/*
 * This is a Darwin notification send whenever any peer (remote or local) updates any Octagon
 * data. Note that this will be sent even if the local device isn't trusted in Octagon, or possibly
 * even if the local device is locked and cannot respond to the change.
 */
extern NSString* OTCliqueChanged;

/*
 * This is a Darwin notification sent when an Octagon recovery happens using the private key
 * material of another device. Importantly, after this is sent, there is no expectation that
 * there are other devices online that might have any keychain or other user secret data
 * to send to this device.
 */
extern NSString* OTJoinedViaBottle;

NS_ASSUME_NONNULL_END

#endif // OTNOTIFICATIONS_H
#endif
