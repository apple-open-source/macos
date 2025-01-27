#ifndef _OPEN_SOURCE_

/*
 * Copyright (c) 1999-2008 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_HIDTYPES_H
#define _IOKIT_HID_HIDTYPES_H

#include <TargetConditionals.h>

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

/*!
	@typedef IOHIDSessionRef
	This is the type of a reference to a IOHIDSession.  IOHIDSession is mutable by defualt.
*/
typedef struct CF_BRIDGED_TYPE(id) __IOHIDSession * IOHIDSessionRef;

/*!
	@typedef IOHIDServiceRef
	This is the type of a reference to a IOHIDService.  IOHIDService is mutable by defualt.
*/
typedef struct CF_BRIDGED_TYPE(id) __IOHIDService * IOHIDServiceRef;

/*!
	@typedef IOHIDNotificationRef
	This is the type of a reference to a IOHIDNotification.  IOHIDNotification is immutable by defualt.
*/
typedef const struct CF_BRIDGED_TYPE(id) __IOHIDNotification * IOHIDNotificationRef;

typedef void (*IOHIDMatchingServicesCallback)(void * _Nullable target, void * _Nullable refcon, void * _Nullable sender, CFArrayRef services);

typedef void (*IOHIDServiceCallback)(void * _Nullable target, void * _Nullable refcon, IOHIDServiceRef service);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* _IOKIT_HID_HIDTYPES_H */

#endif /* _OPEN_SOURCE_ */