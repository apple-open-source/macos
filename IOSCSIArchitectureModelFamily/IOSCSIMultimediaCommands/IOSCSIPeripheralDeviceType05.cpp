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

#include "IOSCSIPeripheralDeviceType05.h"


#if ( SCSI_PDT_05_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PDT_05_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PDT_05_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIMultimediaCommandsDevice
OSDefineMetaClassAndStructors ( IOSCSIPeripheralDeviceType05, IOSCSIMultimediaCommandsDevice );


bool
IOSCSIPeripheralDeviceType05::init ( OSDictionary * propTable )
{
	
	STATUS_LOG ( ( "IOSCSIPeripheralDeviceType05::init called\n" ) );
	
	if ( super::init ( propTable ) == false )
	{
		
		STATUS_LOG ( ( "IOSCSIPeripheralDeviceType05::init exiting false\n" ) );
		return false;
		
	}
	
	STATUS_LOG ( ( "IOSCSIPeripheralDeviceType05::init exiting true\n" ) );
	
	return true;
	
}


bool
IOSCSIPeripheralDeviceType05::start ( IOService * provider )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
		
	// Call our super class' start routine so that all inherited
	// behavior is initialized.    
	if ( !super::start ( provider ) )
    {
		
		return false;
	
	}
	
	return true;
	
}


void
IOSCSIPeripheralDeviceType05::stop ( IOService * provider )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	super::stop ( provider );
	
	STATUS_LOG ( ( "%s::%s exiting\n", getName ( ), __FUNCTION__ ) );
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 1 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 2 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 3 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 4 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 5 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 6 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 7 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceType05, 8 );
