/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2023 Apple Computer, Inc.  All Rights Reserved.
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

#pragma once

#include <IOKit/IOKitLib.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

typedef void(^ConsoleModeBlock)(bool status);

/*!
 * IOHIDAnalyticsGetConsoleModeStatus
 *
 * @abstract
 * Get the current app console mode state according to GamePolicy
 *
 * @discussion
 * Connects to an XPC service running in gamepolicyd to fetch the state.
 *
 * @param status
 * where the current state value is returned if kIOReturnSuccess is returned. Otherwise, the value will not change.
 *
 * @param timeout
 * The timeout in nanoseconds to wait for the response.
 * 
 * @return
 * kIOReturnSuccess if the state was fetched before the timeout, kIOReturnTimeout otherwise.
 * kIOReturnError if the framework fails to load.
 * 
 */
CF_EXPORT
IOReturn IOHIDAnalyticsGetConsoleModeStatus(ConsoleModeBlock block);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS
