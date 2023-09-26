/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DASERVER__
#define __DISKARBITRATIOND_DASERVER__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <dispatch/private.h>
#include <dispatch/dispatch.h>

#include "DADisk.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#if TARGET_OS_OSX
extern void _DAConfigurationCallback( SCDynamicStoreRef store, CFArrayRef keys, void * info );
#endif
#if TARGET_OS_IOS
extern void DARegisterForFirstUnlockNotification( void );
#endif
extern DADiskRef DADiskListGetDisk( const char * diskID );
extern void _DAMediaAppearedCallback( void * context, io_iterator_t notification );
extern void _DAMediaDisappearedCallback( void * context, io_iterator_t notification );
extern void _DAServerCallback( CFMachPortRef port, void * message, CFIndex messageSize, void * info );
extern kern_return_t _DAServerSessionCancel( mach_port_t _session );
extern void _DAVolumeMountedCallback( void );
extern void _DAVolumeUnmountedCallback( void );
extern void _DAVolumeUpdatedCallback( void );
#if TARGET_OS_OSX
extern void _DAVolumeMountedMachHandler( void *context, dispatch_mach_reason_t reason,
                                     dispatch_mach_msg_t msg, mach_error_t err );
extern void _DAVolumeUnmountedMachHandler( void *context, dispatch_mach_reason_t reason,
                                       dispatch_mach_msg_t msg, mach_error_t err );
extern void _DAVolumeUpdatedMachHandler( void *context, dispatch_mach_reason_t reason,
                                     dispatch_mach_msg_t msg, mach_error_t err );

#endif
extern dispatch_workloop_t DAServerWorkLoop( void );
void DAServerMachHandler( void *context, dispatch_mach_reason_t reason, dispatch_mach_msg_t msg, mach_error_t error );
void DAServerInit ( void );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DASERVER__ */
