/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <libkern/OSByteOrder.h>
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include "IOSCSIPeripheralDeviceType07.h"

// For debugging, set SCSI_PDT_07_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG
#define SCSI_PDT_07_DEBUGGING_LEVEL 0

#if ( SCSI_PDT_07_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PDT_07_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PDT_07_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super IOSCSIBlockCommandsDevice
OSDefineMetaClassAndStructors ( IOSCSIPeripheralDeviceType07, IOSCSIBlockCommandsDevice )

//------------------------------------------------------------------------------

bool
IOSCSIPeripheralDeviceType07::init ( OSDictionary * propTable )
{
	STATUS_LOG ( ( "IOSCSIPeripheralDeviceType07::init called\n" ) );

	if ( super::init ( propTable ) == false )
	{

		STATUS_LOG ( ( "IOSCSIPeripheralDeviceType07::init exiting false\n" ) );
		return false;

	}

	return true;
	
}

//------------------------------------------------------------------------------

bool
IOSCSIPeripheralDeviceType07::start ( IOService * provider )
{
	STATUS_LOG ( ( "IOSCSIPeripheralDeviceType07::start called\n" ) );

	// Call our super class' start routine so that all inherited
	// behavior is initialized.    
	if ( super::start ( provider ) == false )
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------

void
IOSCSIPeripheralDeviceType07::stop ( IOService * provider )
{

    STATUS_LOG ( ( "IOSCSIPeripheralDeviceType07::stop called.\n" ) );
    
    super::stop ( provider );

}

//------------------------------------------------------------------------------

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 1 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 2 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 3 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 4 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 5 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 6 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 7 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType07, 8 );
