/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDTRANSACTIONELEMENT_H
#define _IOKIT_HID_IOHIDTRANSACTIONELEMENT_H

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDValue.h>

__BEGIN_DECLS

enum {
    kIOHIDTransactionDefault	= 0x01,
    kIOHIDTransactionCurrent	= 0x02
};

/*!
	@typedef IOHIDTransactionElementRef
	This is the type of a reference to the IOHIDTransactionElement.
*/
typedef struct __IOHIDTransactionElement * IOHIDTransactionElementRef;

/*!
	@function IOHIDTransactionElementGetTypeID
	Returns the type identifier of all IOHIDTransactionElement instances.
*/
CF_EXPORT
CFTypeID IOHIDTransactionElementGetTypeID(void);

CF_EXPORT
IOHIDTransactionElementRef IOHIDTransactionElementCreate(CFAllocatorRef allocator, IOHIDElementRef element, IOOptionBits options);

CF_EXPORT
IOHIDElementRef IOHIDTransactionElementGetElement(IOHIDTransactionElementRef element);

CF_EXPORT
void IOHIDTransactionElementSetDefaultValue(IOHIDTransactionElementRef element, IOHIDValueRef value);

CF_EXPORT
IOHIDValueRef IOHIDTransactionElementGetDefaultValue(IOHIDTransactionElementRef element);

CF_EXPORT
void IOHIDTransactionElementSetValue(IOHIDTransactionElementRef element, IOHIDValueRef value);

CF_EXPORT
IOHIDValueRef IOHIDTransactionElementGetValue(IOHIDTransactionElementRef element);

__END_DECLS

#endif /* _IOKIT_HID_IOHIDTRANSACTIONELEMENT_H */
