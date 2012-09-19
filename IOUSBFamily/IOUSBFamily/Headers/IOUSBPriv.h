/*
 * Copyright © 2012 Apple Inc.  All rights reserved.
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


#ifndef IOUSBFamily_USBPriv_h
#define IOUSBFamily_USBPriv_h

#include <IOKit/IOTypes.h>


#ifdef __cplusplus
extern "C" {
#endif

    /*!
	 @defined Private IOUSBFamily message codes
	 @discussion  Messages specific to the IOUSBFamily which should not be public.  Note that the iokit_usb_msg(x) translates to 0xe0004xxx, where xxx is the value in parenthesis as a hex number.
	 */
#define	kIOUSBMessageMuxFromEHCIToXHCI				iokit_usb_msg(0xe1)		// 0xe00040e1  Message from the EHCI HC for ports mux transition from EHCI to XHCI
#define	kIOUSBMessageMuxFromXHCIToEHCI				iokit_usb_msg(0xe2)		// 0xe00040e2  Message from the EHCI HC for ports mux transition from XHCI to EHCI

    const size_t kIOUSBMuxMethodNameLength = 5;	
	
#ifdef __cplusplus
}       
#endif

#endif
