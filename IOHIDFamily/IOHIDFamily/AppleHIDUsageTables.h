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
    kHIDPage_AppleVendor                        = 0xff00,
    kHIDPage_AppleVendorKeyboard                = 0xff01,
	kHIDPage_AppleVendorTopCase                 = 0x00ff
};


/* AppleVendor Page (0xff00) */
enum
{
    kHIDUsage_AppleVendor_TopCase               = 0x0001, /* Application Collection */
    kHIDUsage_AppleVendor_Keyboard              = 0x0006  /* Application Collection */
};


/* AppleVendor Keyboard Page (0xff01) */
enum
{
    kHIDUsage_AppleVendorKeyboard_Spotlight         = 0x01,
    kHIDUsage_AppleVendorKeyboard_Dashboard         = 0x02,
    kHIDUsage_AppleVendorKeyboard_Function          = 0x03,
    kHIDUsage_AppleVendorKeyboard_Reserved          = 0x0a,
    kHIDUsage_AppleVendorKeyboard_Expose_All        = 0x10,
    kHIDUsage_AppleVendorKeyboard_Expose_Desktop    = 0x11,
    kHIDUsage_AppleVendorKeyboard_Brightness_Up     = 0x20,
    kHIDUsage_AppleVendorKeyboard_Brightness_Down   = 0x21,
};


/* AppleVendor Page Top Case (0x00ff) */
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
