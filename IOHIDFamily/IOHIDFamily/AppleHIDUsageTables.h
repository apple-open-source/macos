/*
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
#ifndef _APPLEHIDUSAGETABLES_H
#define _APPLEHIDUSAGETABLES_H

/* ******************************************************************************************
 * Apple HID Usage Tables
 *
 * The following constants are Apple Vendor specific usages
 * ****************************************************************************************** */


/* Usage Pages */
enum
{
	kHIDPage_AppleVendor	= 0xff
};


/* AppleVendor Page (0xff) */
enum
{
    kHIDUsage_AppleVendor_KeyboardFn            = 0x03,
    kHIDUsage_AppleVendor_BrightnessUp          = 0x04,
    kHIDUsage_AppleVendor_BrightnessDown        = 0x05,
    kHIDUsage_AppleVendor_VideoMirror           = 0x06,
    kHIDUsage_AppleVendor_IlluminationToggle    = 0x07,
    kHIDUsage_AppleVendor_IlluminationUp        = 0x08,
    kHIDUsage_AppleVendor_IlluminationDown      = 0x09,
    kHIDUsage_AppleVendor_Reserved_MouseData    = 0xc0
};


#endif /* _APPLEHIDUSAGETABLES_H */
