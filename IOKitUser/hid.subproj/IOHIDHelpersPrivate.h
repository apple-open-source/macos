/*
 * Copyright (c) 2025 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/IOKitLib.h>

typedef void (^IOHIDTraverseDevicesBlock) (io_service_t deviceService);

/*! @function   \_IOHIDTraverseDevicesFromParentService
    @abstract   Iterates over all IOHIDDevice instances which are children of the provided parent service
    @discussion For each IOHIDDevice which is present in the children of the parent service, the provided block is applied
    @param      parentService io_service_t from which we will begin iterating over all children
    @param      handler Block that is run for each IOHIDDevice
 */
bool _IOHIDTraverseDevicesFromParentService(io_service_t parentService, _Nonnull IOHIDTraverseDevicesBlock handler);
