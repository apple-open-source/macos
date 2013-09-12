/*
 * Copyright (c) 2009-2012 Apple Inc. All rights reserved.
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

#ifndef __IOKIT_IO_IOUSBMASSTORAGECLASS_TIMESTAMPS__
#define __IOKIT_IO_IOUSBMASSTORAGECLASS_TIMESTAMPS__

#include <stdint.h>

#include <sys/kdebug.h>

#if KERNEL
#include <IOKit/IOTimeStamp.h>
#endif

#include <IOKit/scsi/IOSCSIArchitectureModelFamilyTimestamps.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UInt32		gUSBDebugFlags;

#if KERNEL
    
//--------------------------------------------------------------------------------------------------
//	RecordUSBTimeStamp											       						[STATIC]
//--------------------------------------------------------------------------------------------------

static inline void
RecordUSBTimeStamp ( 
	unsigned int code,
	unsigned int a, unsigned int b,
	unsigned int c, unsigned int d )
{

	if ( gUSBDebugFlags != 0 )
	{
		IOTimeStampConstant ( code, a, b, c, d );
	}
	
}

#endif 
    
#define USBMASS_SYSCTL	"debug.USBMassStorageClass"

typedef struct USBSysctlArgs
{
	uint32_t		type;
	uint32_t		operation;
	uint32_t		debugFlags;
} USBSysctlArgs;


#define kUSBTypeDebug	'USBM'

enum
{
	kUSBOperationGetFlags 	= 0,
	kUSBOperationSetFlags	= 1
};


/* The trace codes consist of the following:
 *
 * ----------------------------------------------------------------------
 *|              |               |              |               |Func   |
 *| Class (8)    | SubClass (8)  | SAM Class(6) |  Code (8)     |Qual(2)|
 * ----------------------------------------------------------------------
 *
 * DBG_IOKIT(05h)  DBG_IOSAM(27h)
 *
 * See <sys/kdebug.h> and IOTimeStamp.h for more details.
 *
 */


// USB Mass Storage Class Tracepoints	0x05278800 - 0x05278BFC
enum
{

	// Generic UMC Tracepoints			0x05278800 - 0x052788FC
	kAbortedTask                        = 0x00,
	kCompleteSCSICommand				= 0x01,
	kNewCommandWhileTerminating         = 0x02,
	kLUNConfigurationComplete			= 0x03,
	kIOUMCStorageCharacDictFound		= 0x04,
	kNoProtocolForDevice				= 0x05,
	kIOUSBMassStorageClassStart         = 0x06,
	kIOUSBMassStorageClassStop          = 0x07,
	kAtUSBAddress						= 0x08,
	kMessagedCalled						= 0x09,
	kWillTerminateCalled				= 0x0A,
	kDidTerminateCalled					= 0x0B,
	kCDBLog1							= 0x0C,
	kCDBLog2							= 0x0D,
	kClearEndPointStall					= 0x0E,
	kGetEndPointStatus					= 0x0F,
	kHandlePowerOnUSBReset				= 0x10,
	kUSBDeviceResetWhileTerminating		= 0x11,
	kUSBDeviceResetAfterDisconnect		= 0x12,
	kUSBDeviceResetReturned				= 0x13,
	kAbortCurrentSCSITask				= 0x14,
	kCompletingCommandWithError			= 0x15,
    kDeviceInformation                  = 0x16,
    kSuspendPort                        = 0x17,
    kSubclassUse                        = 0x18,

	// CBI Tracepoints					0x05278900 - 0x0527897C
	kCBIProtocolDeviceDetected			= 0x40,
	kCBICommandAlreadyInProgress		= 0x41,
	kCBISendSCSICommandReturned			= 0x42,
	kCBICompletion						= 0x43,
	
	// UFI Tracepoints					0x05278980 - 0x052789FC

	// Bulk-Only Tracepoints			0x05278A00 - 0x05278BFC
	kBODeviceDetected					= 0x80,
	kBOPreferredMaxLUN					= 0x81,
	kBOGetMaxLUNReturned				= 0x82,
	kBOCommandAlreadyInProgress			= 0x83,
	kBOSendSCSICommandReturned			= 0x84,
	kBOCBWDescription					= 0x85,
	kBOCBWBulkOutWriteResult			= 0x86,
	kBODoubleCompleteion				= 0x87,
	kBOCompletionDuringTermination		= 0x88,
	kBOCompletion						= 0x89
	
};
    
// SubclassCodes                    
enum
{
    
    kSubclassCode_AppleUSBODD           = 0x00,
    kSubclassCode_AppleUSBCardReaderUMC = 0x01
    
};
    
// SubclassCodes for AppleUSBODD
enum
{
    
    kAppleUSBODD_probe                              = 0x00,
    kAppleUSBODD_start                              = 0x01,
    kAppleUSBODD_requestedExtraPower                = 0x02,
    kAppleUSBODD_isMacModelSupported                = 0x03,
    kAppleUSBODD_FindACPIPlatformDevice             = 0x04,
    kAppleUSBODD_CheckForACPIFlags                  = 0x05
    
};
    
// SubclassCodes for AppleUSBCardReaderUMC
enum
{
    
    kAppleUSBCardReaderUMC_start                    = 0x00,
    kAppleUSBCardReaderUMC_stop                     = 0x01,
    kAppleUSBCardReaderUMC_HandlePowerChange        = 0x02,
    kAppleUSBCardReaderUMC_message                  = 0x03,
    kAppleUSBCardReaderUMC_setProperty              = 0x04,
    kAppleUSBCardReaderUMC_gpioMediaDetectFired     = 0x05,
    kAppleUSBCardReaderUMC_gpioMediaDetectEnable    = 0x06,
    kAppleUSBCardReaderUMC_controllerReset          = 0x07,
    kAppleUSBCardReaderUMC_powerControl             = 0x08,
    kAppleUSBCardReaderUMC_waitForReconnect         = 0x09,
    kAppleUSBCardReaderUMC_systemWillShutdown       = 0x0A,
    kAppleUSBCardReaderUMC_generalPurpose           = 0x0B,
    kAppleUSBCardReaderUMC_stop_2                   = 0x0C
    
};
    
// Tracepoint macros.                                          
#define UMC_TRACE( code )           ( ( ( DBG_IOKIT & 0xFF ) << 24 ) | ( ( DBG_IOSAM & 0xFF ) << 16 ) | ( ( kSAMClassUSB & 0x3F ) << 10 ) | ( ( code & 0xFF ) << 2 ) )
#define USBODD_TRACE( code )        ( ( ( kSubclassCode_AppleUSBODD & 0xFF ) << 24 ) | ( code & 0xFFFFFF ) )
#define USBCARDREADER_TRACE( code )	( ( ( kSubclassCode_AppleUSBCardReaderUMC & 0xFF ) << 24 ) | ( code & 0xFFFFFF ) )
    
#ifdef __cplusplus
}
#endif


#endif	/* __IOKIT_IO_IOUSBMASSTORAGECLASS_TIMESTAMPS__ */
