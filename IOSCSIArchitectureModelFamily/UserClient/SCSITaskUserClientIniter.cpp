/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/scsi/IOSCSIProtocolInterface.h>
#include <IOKit/scsi/IOSCSIPeripheralDeviceNub.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSITaskUserClientIniter.h"

// For debugging, set SCSI_TASK_USER_CLIENT_INITER_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG
#define SCSI_TASK_USER_CLIENT_INITER_DEBUGGING_LEVEL 0

#if ( SCSI_TASK_USER_CLIENT_INITER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TASK_USER_CLIENT_INITER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TASK_USER_CLIENT_INITER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOService
OSDefineMetaClassAndStructors ( SCSITaskUserClientIniter, IOService );

bool
SCSITaskUserClientIniter::start ( IOService * provider )
{
	
	OSDictionary *					mergeProperties	= NULL;
	IOSCSIPeripheralDeviceNub *		nub				= NULL;
	UInt32							deviceType		= 0;
	bool							status 			= false;
	bool							doMerge 		= false;
	OSString *						matchKey		= NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClientIniter::start called\n" ) );
	
	if ( !super::start ( provider ) )
		return status;
	
	matchKey = OSString::withCString ( kSCSITaskUserClientIniterKey );
	if ( matchKey != NULL )
	{
		
		OSString *	key = ( OSString * ) provider->getProperty ( kIOMatchCategoryKey );
		
		if ( key != NULL )
		{
			
			if ( matchKey->isEqualTo ( key ) )
			{
				
				doMerge = true;
				
			}
			
		}
		
		matchKey->release ( );
		matchKey = NULL;
		
	}
	
	// Special case the IOSCSIPeripheralDeviceNub. Since there are
	// in-kernel drivers for some devices, we only merge the properties
	// for devices with no in-kernel driver (e.g. a tape drive).
	nub = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	if ( nub != NULL )
	{
		
		STATUS_LOG ( ( "provider's classname = %s\n", provider->getName ( ) ) );
		
		// Get the peripheral device type as defined in Inquiry data returned
		// by the drive.
		deviceType = ( ( OSNumber * ) nub->getProperty ( kIOPropertySCSIPeripheralDeviceType ) )->unsigned32BitValue ( );
		switch ( deviceType )
		{
			
			case 0x00000000:
			case 0x00000005:
			case 0x00000007:
			case 0x0000000E:
				doMerge = false;
				break;
			default:				
				break;
				
		}
		
		STATUS_LOG ( ( "deviceType = %ld\n", deviceType ) );
		
	}
		
	// We're going to merge the properties from the dictionary into the provider object.
	if ( doMerge )
	{
		
		mergeProperties = OSDynamicCast ( OSDictionary, getProperty ( "IOProviderMergeProperties" ) );
		if ( mergeProperties != NULL )
		{
			
			UInt32		uid[3];
			
			// Create a GUID for the provider object. The GUID consists of 3 longs,
			// the first is the provider object's address, the second two are the
			// time in the form of a mach_timespec.
			
			uid[0] = ( UInt32 ) provider;
			IOGetTime ( ( mach_timespec * ) &uid[1] );
			
			STATUS_LOG ( ( "merging properties\n" ) );
			
			// Make sure we use setProperty to set these properties. If we try to just call merge() on the
			// dictionary, we could have an issue with something else in the registry trying to modify
			// it while we are merging, so we explicitly use setProperty() which will grab a lock and
			// then atomically update the registry entry.
			provider->setProperty ( kIOPropertySCSITaskUserClientInstanceGUID, &uid[0], sizeof ( uid ) );			
			provider->setProperty ( kIOUserClientClassKey, mergeProperties->getObject ( kIOUserClientClassKey ) );
			provider->setProperty ( kIOCFPlugInTypesKey, mergeProperties->getObject ( kIOCFPlugInTypesKey ) );
			provider->setProperty ( kIOPropertySCSITaskDeviceCategory, mergeProperties->getObject ( kIOPropertySCSITaskDeviceCategory ) );
			
			// Set status to be true to keep us loaded in memory so the user client code stays resident
			// in the kernel.
			status = true;
			
		}
		
	}
	
	STATUS_LOG ( ( "status = %d\n", status ) );
	
	return status;
	
}