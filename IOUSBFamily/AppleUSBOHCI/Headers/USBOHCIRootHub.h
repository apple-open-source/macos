/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
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

#ifndef __OHCIROOTHUB__
#define __OHCIROOTHUB__

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT_SUPPORTED
#pragma import on
#endif

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=power
#endif



// Root hub status reg
enum
{
	kOHCIHcRhStatus_LPS			= kOHCIBit0,
	kOHCIHcRhStatus_OCI			= kOHCIBit1,
	kOHCIHcRhStatus_DRWE		= kOHCIBit15,
	kOHCIHcRhStatus_LPSC		= kOHCIBit16,
	kOHCIHcRhStatus_OCIC		= kOHCIBit17,
	kOHCIHcRhStatus_CRWE		= kOHCIBit31,
	kOHCIHcRhStatus_Change		= kOHCIHcRhStatus_LPSC|kOHCIHcRhStatus_OCIC
};

// Port status reg 
enum
{
	kOHCIHcRhPortStatus_CCS			= kOHCIBit0,
	kOHCIHcRhPortStatus_PES			= kOHCIBit1,
	kOHCIHcRhPortStatus_PSS			= kOHCIBit2,
	kOHCIHcRhPortStatus_POCI		= kOHCIBit3,
   	kOHCIHcRhPortStatus_PRS			= kOHCIBit4,
    	kOHCIHcRhPortStatus_PPS			= kOHCIBit8,
	kOHCIHcRhPortStatus_LSDA		= kOHCIBit9,
	kOHCIHcRhPortStatus_CSC			= kOHCIBit16,
	kOHCIHcRhPortStatus_PESC		= kOHCIBit17,
	kOHCIHcRhPortStatus_PSSC		= kOHCIBit18,
	kOHCIHcRhPortStatus_OCIC		= kOHCIBit19,
	kOHCIHcRhPortStatus_PRSC		= kOHCIBit20,
	kOHCIHcRhPortStatus_Change		= kOHCIHcRhPortStatus_CSC|kOHCIHcRhPortStatus_PESC|
				kOHCIHcRhPortStatus_PSSC|kOHCIHcRhPortStatus_OCIC|kOHCIHcRhPortStatus_PRSC
};


enum
{
	kOHCINumPortsMask				= OHCIBitRange (0, 7),
	kOHCIPowerSwitchingMask			= OHCIBitRange (9, 9),
	kOHCIGangedSwitchingMask		= OHCIBitRange (8, 8),
	kOHCICompoundDeviceMask			= OHCIBitRange (10, 10),
	kOHCIOverCurrentMask			= OHCIBitRange (12, 12),
	kOHCIGlobalOverCurrentMask		= OHCIBitRange (11, 11),
	kOHCIDeviceRemovableMask		= OHCIBitRange (0, 15),
	kOHCIGangedPowerMask			= OHCIBitRange (16, 31),

	kOHCIPowerSwitchingOffset		= 9,
	kOHCIGangedSwitchingOffset		= 8,
	kOHCICompoundDeviceOffset		= 10,
	kOHCIOverCurrentOffset			= 12,
	kOHCIGlobalOverCurrentOffset	= 11,
	kOHCIGangedPowerOffset			= 0,
	kOHCIDeviceRemovableOffset		= 16,

	kOHCIPortFlagsMask				= OHCIBitRange (0, 16),

	kOHCIportChangeFlagsOffset		= 16
};


#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

#if PRAGMA_IMPORT_SUPPORTED
#pragma import off
#endif

#ifdef __cplusplus
}
#endif

#endif /* __OHCIROOTHUB__ */
