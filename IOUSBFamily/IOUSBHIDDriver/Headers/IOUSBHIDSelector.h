/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef IOUSBHIDSELECTOR_H
#define IOUSBHIDSELECTOR_H


enum {
    kUSBHIDGetReport = 0,
    kUSBHIDSetReport,
    kUSBHIDControlDevice,
    kUSBHIDGetHIDDescriptor,
    kUSBHIDGetVendorID,
    kUSBHIDGetProductID,
    kUSBHIDGetVersionNumber,
    //  kUSBHIDGetInterfaceRef,			// Provided by IOUserClient::getService()
    kUSBHIDGetMaxReportSize,
    kUSBHIDGetManufacturerString,
    kUSBHIDGetProductString,
    kUSBHIDGetSerialNumberString,
    //kUSBHIDGetVendorSpecificStart,	// Should just be part of start().
    kUSBHIDGetIndexedString,
    // kUSBHIDOpenDevice,				// Provided by start().
    // kUSBHIDCloseDevice,				// Provided by stop().
    // kUSBHIDInstallReportHandler,		// Provided by DataQueue management.
    // kUSBHIDRemoveReportHandler,			// " 
    // kUSBHIDCallPreviousReportHandler,	// " 
    kUSBHIDNumMethods
};

#endif	// IOUSBHIDSELECTOR_H
