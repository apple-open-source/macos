/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOMessage.h>

#define FIREWIREPRIVATE
#include <IOKit/firewire/IOFireWireController.h>
#undef FIREWIREPRIVATE

#include <IOKit/firewire/IOConfigDirectory.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/sbp2/IOFireWireSBP2LUN.h>
#include <IOKit/sbp2/IOFireWireSBP2Target.h>
#include "FWDebugging.h"

const OSSymbol *gCommand_Set_Spec_ID_Symbol = NULL;
const OSSymbol *gCommand_Set_Symbol = NULL;
const OSSymbol *gModule_Vendor_ID_Symbol = NULL;
const OSSymbol *gCommand_Set_Revision_Symbol = NULL;
const OSSymbol *gIOUnit_Symbol = NULL;
const OSSymbol *gFirmware_Revision_Symbol = NULL;
const OSSymbol *gDevice_Type_Symbol = NULL;
const OSSymbol *gGUID_Symbol = NULL;
const OSSymbol *gUnit_Characteristics_Symbol = NULL;
const OSSymbol *gManagement_Agent_Offset_Symbol = NULL;
const OSSymbol *gFast_Start_Symbol = NULL;
		
OSDefineMetaClassAndStructors(IOFireWireSBP2Target, IOService);

OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Target, 8);

// 
// start / stop routines
//

bool IOFireWireSBP2Target::start( IOService *provider )
{
    fProviderUnit = OSDynamicCast(IOFireWireUnit, provider);
    if (fProviderUnit == NULL)
        return false;
	
	fProviderUnit->retain();
	
	// we want the expansion data member to be zeroed if it's available 
	// so create and zero in a local then assign to the member when were done
	
	ExpansionData * exp_data = (ExpansionData*) IOMalloc( sizeof(ExpansionData) );
	if( !exp_data )
	{
		return false;
	}

	bzero( exp_data, sizeof(ExpansionData) );
	
	fExpansionData = exp_data;
	
	fControl = fProviderUnit->getController();
	
	// assume safe mode
	fFlags = kIOFWSBP2FailsOnBusResetsDuringIO;
	
    fOpenFromTarget = false;
    fOpenFromLUNCount = 0;
	fIOCriticalSectionCount = 0;
	
	//
	// create symbols
	//
	
	if( gCommand_Set_Spec_ID_Symbol == NULL )
		gCommand_Set_Spec_ID_Symbol = OSSymbol::withCString("Command_Set_Spec_ID");

	if( gCommand_Set_Symbol == NULL )		
		gCommand_Set_Symbol = OSSymbol::withCString("Command_Set");

	if( gModule_Vendor_ID_Symbol == NULL )
		gModule_Vendor_ID_Symbol = OSSymbol::withCString("Vendor_ID");

	if( gCommand_Set_Revision_Symbol == NULL )
		gCommand_Set_Revision_Symbol = OSSymbol::withCString("Command_Set_Revision");
	
	if( gIOUnit_Symbol == NULL )
		gIOUnit_Symbol = OSSymbol::withCString("IOUnit");
	
	if( gFirmware_Revision_Symbol == NULL )
		gFirmware_Revision_Symbol = OSSymbol::withCString("Firmware_Revision");
	
	if( gDevice_Type_Symbol == NULL )
		gDevice_Type_Symbol = OSSymbol::withCString("Device_Type");

	if( gGUID_Symbol == NULL )
		gGUID_Symbol = OSSymbol::withCString("GUID");

	if( gUnit_Characteristics_Symbol == NULL )
		gUnit_Characteristics_Symbol = OSSymbol::withCString("Unit_Characteristics");
	
	if( gManagement_Agent_Offset_Symbol == NULL )
		gManagement_Agent_Offset_Symbol = OSSymbol::withCString("Management_Agent_Offset");

	if( gFast_Start_Symbol == NULL )
		gFast_Start_Symbol = OSSymbol::withCString("Fast_Start");
			
    if (IOService::start(provider))
    {
		IOFireWireController * 	controller = fProviderUnit->getController();
        IOService * 			fwim = (IOService*)controller->getLink();
		OSObject *				prop = NULL;
		UInt32					byteCount1, byteCount2;
		
		//
		// read receive properties from registry		
		//
		
		byteCount1 = 0;
		prop = fwim->getProperty( "FWMaxAsyncReceiveBytes" );
		if( prop )
		{
			byteCount1 = ((OSNumber*)prop)->unsigned32BitValue();
			if( byteCount1 != 0 )
			{
				// minus 32 bytes for status block
				byteCount1 -= 32; 
			}
		}
		
		byteCount2 = 0;
		prop = fwim->getProperty( "FWMaxAsyncReceivePackets" );
		if( prop )
		{
			UInt32 packetCount = ((OSNumber*)prop)->unsigned32BitValue();
			if( packetCount != 0 )
			{
				// minus 1 for the status block
				byteCount2 = (packetCount - 1) * 512; // 512 bytes is minimum packet size
			}
		}
			
		// publish min byte size
		UInt32 size = byteCount1 < byteCount2 ? byteCount1 : byteCount2;
		if( size != 0)
			setProperty( "SBP2ReceiveBufferByteCount", size, 32 );
		
		// start scanning for LUNs
        scanForLUNs();
    }
    else
        return false;

    FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : started\n", (UInt32)this ) );
	
	fExpansionData->fStarted = true;
	
    return true;
}

// stop
//
//

void IOFireWireSBP2Target::stop( IOService *provider )
{
    FWKLOG( ( "IOFireWireSBP2Target<0x%08lx>::stop\n", (UInt32)this ) );

    IOService::stop(provider);
}

// finalize
//
//

bool IOFireWireSBP2Target::finalize( IOOptionBits options )
{
    // Nuke from device tree
    detachAll( gIODTPlane );
	
	return IOService::finalize( options );
}

// free
//
//

void IOFireWireSBP2Target::free( void )
{
	FWKLOG( ( "IOFireWireSBP2Target<0x%08lx>::free\n", (UInt32)this ) );

	if( fIOCriticalSectionCount != 0 )
	{
		IOLog( "IOFireWireSBP2Target<0x%08lx>::free - fIOCriticalSectionCount == %d!\n", (UInt32)this, fIOCriticalSectionCount );
	}
	
	while( fIOCriticalSectionCount != 0 )
	{
		fIOCriticalSectionCount--;
		fControl->endIOCriticalSection();
	}

	if( fExpansionData )
	{
		if( fExpansionData->fPendingMgtAgentCommands )
			fExpansionData->fPendingMgtAgentCommands->release() ;
	
		IOFree( fExpansionData, sizeof(ExpansionData) );
		fExpansionData = NULL;
	}
	
	if( fProviderUnit )
	{
		fProviderUnit->release();
		fProviderUnit = NULL;
	}
	
	IOService::free();
}

////////////////////////////////////////////////////////////////////////

//
// message method
//

IOReturn IOFireWireSBP2Target::message( UInt32 type, IOService *nub, void *arg )
{
    IOReturn res = kIOReturnUnsupported;

    FWKLOG( ("IOFireWireSBP2Target<0x%08lx> : message 0x%x, arg 0x%08lx\n", (UInt32)this, type, arg) );

    res = IOService::message(type, nub, arg);
    if( kIOReturnUnsupported == res )
    {
        switch (type)
        {                
            case kIOMessageServiceIsTerminated:
                FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : kIOMessageServiceIsTerminated\n", (UInt32)this ) );
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsSuspended:
                FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : kIOMessageServiceIsSuspended\n", (UInt32)this ) );
                clearMgmtAgentAccess();
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsResumed:
                FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : kIOMessageServiceIsResumed\n", (UInt32)this ) );
                configurePhysicalFilter();
                res = kIOReturnSuccess;
                break;

            default: // default the action to return kIOReturnUnsupported
                break;
        }
    }

	if( type != kIOMessageServiceIsTerminated &&
		type != (UInt32)kIOMessageFWSBP2ReconnectFailed &&
		type != (UInt32)kIOMessageFWSBP2ReconnectComplete )
	{
		messageClients( type, arg );    
    } 
    
    return res;
}

// getFireWireUnit
//
// returns the FireWire unit for doing non-SBP2 work

IOFireWireUnit * IOFireWireSBP2Target::getFireWireUnit( void )
{
	return (IOFireWireUnit*)getProvider();
}

////////////////////////////////////////////////////////////////////////
// open / close internals

//
// handleOpen / handleClose
//
// we override these two methods to allow a reference counted open from
// LUNs only.  Exculsive access is enforced for non-LUN clients
//

bool IOFireWireSBP2Target::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
    bool ok = true;

    FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleOpen entered fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", (UInt32)this, fOpenFromLUNCount, fOpenFromTarget ));
    
    IOFireWireSBP2LUN * lunClient = OSDynamicCast( IOFireWireSBP2LUN, forClient );
    if( lunClient != NULL )
    {
        // bail if we're open from the target
        if( fOpenFromTarget )
            return false;

        // if this is the first open call, actually do an open
        if( fOpenFromLUNCount == 0 )
        {
            ok = fProviderUnit->open(this, options, arg);
            if( ok )
            {
                fOpenFromLUNCount++;
                ok = IOService::handleOpen( this, options, arg );
                FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleOpen called open\n", (UInt32)this ));
            }
        }
        else
        {
            // otherwise just increase the reference count
            fOpenFromLUNCount++;
        }
    }
    else
    {
        // bail if we're open as a LUN
        if( fOpenFromLUNCount != 0 )
            return false;

        // try to open
        if( !fOpenFromTarget )  // extra safe
        {
            ok = fProviderUnit->open(this, options, arg);
            if( ok )
            {
                fOpenFromTarget = true;
                ok = IOService::handleOpen( forClient, options, arg );
                FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleOpen called open\n", (UInt32)this ));
            }
        }
        else
            ok = false;   // already open
    }

    FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleOpen - exit handleOpen fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", (UInt32)this, fOpenFromLUNCount, fOpenFromTarget ));
    
    return ok;
}

void IOFireWireSBP2Target::handleClose( IOService * forClient, IOOptionBits options )
{
    FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleClose enter handleClose fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", (UInt32)this, fOpenFromLUNCount, fOpenFromTarget ));
    
    IOFireWireSBP2LUN * lunClient = OSDynamicCast( IOFireWireSBP2LUN, forClient );
    if( lunClient != NULL )
    {
        if( fOpenFromLUNCount != 0 ) // extra safe
        {
            fOpenFromLUNCount--;

            if( fOpenFromLUNCount == 0 ) // close if we're down to zero
            {
                IOService::handleClose( this, options);
                fProviderUnit->close(this, options);
                FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleClose - called close\n", (UInt32)this ));
            }
        }
    }
    else
    {
        if( fOpenFromTarget ) // if we were open from the target
        {
            fOpenFromTarget = false;
            IOService::handleClose(forClient, options);
            fProviderUnit->close(this, options);
            FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleClose - called close\n", (UInt32)this ));
        }
    }

    FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::handleClose - exit handleClose fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", (UInt32)this, fOpenFromLUNCount, fOpenFromTarget ));
}

//
// handleIsOpen
//
// Default implementation is for single object access.  We use this when someone
// has opened the target, but since multiple LUNs can open this object we override
// the behavior in this case.

bool IOFireWireSBP2Target::handleIsOpen( const IOService * forClient ) const
{
    // are we open from one or more LUNs?
    if( fOpenFromLUNCount != 0 )
    {
        // is the client really a LUN?
        IOFireWireSBP2LUN * lunClient = OSDynamicCast( IOFireWireSBP2LUN, forClient );
        return (lunClient != NULL );
    }

    // are we open from the target
    if( fOpenFromTarget )
    {
        // is the client the one who opened us?
        return IOService::handleIsOpen( forClient );
    }

    // we're not open
    return false;
}

////////////////////////////////////////////////////////////////////////
// LUN discovery and creation

// scanForLUNS
//
// look for LUNs, publish the information in the registry and initiate matching.

void IOFireWireSBP2Target::scanForLUNs( void )
{
    IOReturn		status = kIOReturnSuccess;
    IOReturn		tempStatus = kIOReturnSuccess;
    
	LUNInfo info;

	info.cmdSpecID 				= 0;
	info.cmdSet 				= 0;
	info.vendorID				= 0;
	info.softwareRev 			= 0;
	info.firmwareRev 			= 0;
	info.lun					= 0;
	info.devType				= 0;
	info.unitCharacteristics 	= 0;
	info.managementOffset 		= 0;
	info.revision				= 0;
	info.fastStartSupported 	= false;
	info.fastStart				= 0;
	
	//
    // get root directory
    //

    IOConfigDirectory *		directory;
    
    IOFireWireDevice *  device = NULL;
    IOService *		providerService = fProviderUnit->getProvider();
    if( providerService == NULL )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        device = OSDynamicCast( IOFireWireDevice, providerService );
        if( device == NULL )
            status = kIOReturnError;
        FWKLOG( ("IOFireWireSBP2Target<0x%08lx> : unit = 0x%08lx, provider = 0x%08lx, device = 0x%08lx\n", (UInt32)this, fProviderUnit, providerService, device) );
    }
    
    if( status == kIOReturnSuccess )
    {
        status = device->getConfigDirectory( directory );
        FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : status = %d\n", (UInt32)this, status ) );
    }
    
    //
    // find vendor id
    //

    if( status == kIOReturnSuccess )
        tempStatus = directory->getKeyValue( kConfigModuleVendorIdKey, info.vendorID );

	//
	// get unit directory
    //
    
    status = fProviderUnit->getConfigDirectory( directory );
    FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : status = %d\n", (UInt32)this, status ) );
  	
	//
	// find command spec id
	//

    if( status == kIOReturnSuccess )
        status = directory->getKeyValue( kCmdSpecIDKey, info.cmdSpecID );
    
	//
	// find command set
	//

    if( status == kIOReturnSuccess )
        status = directory->getKeyValue( kCmdSetKey, info.cmdSet );

	//
	// find unit characteristics
	//
	
	if( status == kIOReturnSuccess )
        status = directory->getKeyValue( kUnitCharacteristicsKey, info.unitCharacteristics );

	//
	// find management agent offset
    // and add ourselves to the DeviceTree with that location
    // for OpenFirmware boot path generation.
	//
    
    status = directory->getKeyValue( kManagementAgentOffsetKey, info.managementOffset );
    if( status == kIOReturnSuccess )
    {
        IOService *parent = this;
        while(parent) {
            if(parent->inPlane(gIODTPlane))
                break;
            parent = parent->getProvider();
        }
        if(parent) {
            char location[9];
            snprintf( location, sizeof(location), "%lx", info.managementOffset );
            attachToParent(parent, gIODTPlane);
            setLocation(location, gIODTPlane);
            setName("sbp-2", gIODTPlane);
        }
    }
 
    // failure to find one of the following is not fatal, hence the use of tempStatus
	
	//
	// find revision
	//
	
	if( status == kIOReturnSuccess )
        tempStatus = directory->getKeyValue( kRevisionKey, info.revision );

	//
	// find fast start info
	//
	
	if( status == kIOReturnSuccess && info.revision != 0 )
	{
		tempStatus = directory->getKeyValue( kFastStartKey, info.fastStart );
		if( tempStatus == kIOReturnSuccess )
		{
			info.fastStartSupported = true;
		}
	}

	//
	// find software rev
	//
    
    if( status == kIOReturnSuccess )
         tempStatus = directory->getKeyValue( kSoftwareRevKey, info.softwareRev );

    //
	// find firmware rev
	//

    if( status == kIOReturnSuccess )
        tempStatus = directory->getKeyValue( kFirmwareRevKey, info.firmwareRev );

     FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : status = %d, cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d, firmwareRev = %d\n",
                             (UInt32)this, status, info.cmdSpecID, info.cmdSet, info.vendorID, info.softwareRev, info.firmwareRev ) );

    //
    // look for luns implemented as immediate values
    //
 
    if( status == kIOReturnSuccess )
    {
        // look at each entry
        for( int pos = 0; pos < directory->getNumEntries(); pos++ )
        {
            // get index key
            UInt32 key;
            tempStatus = directory->getIndexEntry( pos, key );
            FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : tempStatus = %d, pos = %d, key = %d\n",
                             (UInt32)this, tempStatus, pos, key ) );
            
            // if it was the LUN key
            if( tempStatus == kIOReturnSuccess && key >> kConfigEntryKeyValuePhase == kLUNKey )
            {
                UInt32 data;
                tempStatus = directory->getIndexValue( pos, data );
                if( tempStatus == kIOReturnSuccess )
                {
                    info.lun = data  & 0x0000ffff;
                    info.devType = (data & 0x001f0000) >> 16;

                    FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d\n",
                             (UInt32)this, info.cmdSpecID, info.cmdSet, info.vendorID, info.softwareRev ) );
                    FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : firmwareRev = %d, lun = %d, devType = %d\n",
                             (UInt32)this, info.firmwareRev, info.lun, info.devType ) );
					FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : unitCharacteristics = %d, managementOffset = %d,\n",
                             (UInt32)this, info.unitCharacteristics, info.managementOffset ) );
					FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : revision = %d, fastStartSupported = %d, fastStart = 0x%08lx\n",
                         (UInt32)this, info.revision, info.fastStartSupported, info.fastStart ) );
						 
                    // force vendors to use real values, (0, 0) is not legal
                    if( (info.cmdSpecID & 0x00ffffff) || (info.cmdSet & 0x00ffffff) )
                    {
                    	fExpansionData->fNumLUNs++;
                        createLUN( &info );
                    }
                }
            }
        }
    }

    //
    // look for luns implemented as directories
    //

    if( status == kIOReturnSuccess )
    {
        OSIterator *			directoryIterator;
        IOConfigDirectory *		lunDirectory;
        UInt32 					lunValue;

        status = directory->getKeySubdirectories( kLUNDirectoryKey, directoryIterator );

        // iterate through directories
        while( (lunDirectory = OSDynamicCast(IOConfigDirectory,directoryIterator->getNextObject())) != NULL )
        {
			//
			// find fast start info
			//
			
			if( info.revision != 0 )
			{
				tempStatus = directory->getKeyValue( kFastStartKey, info.fastStart );
				if( tempStatus == kIOReturnSuccess )
				{
					info.fastStartSupported = true;
				}
			}
			
            // get lun value
            
            tempStatus = lunDirectory->getKeyValue( kLUNKey, lunValue );
            if( tempStatus == kIOReturnSuccess )
            {
                info.lun = lunValue  & 0x0000ffff;
                info.devType = (lunValue & 0x001f0000) >> 16;

                FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d\n",
                         (UInt32)this, info.cmdSpecID, info.cmdSet, info.vendorID, info.softwareRev ) );
                FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : firmwareRev = %d, lun = %d, devType = %d\n",
                         (UInt32)this, info.firmwareRev, info.lun, info.devType ) );
				FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : revision = %d, fastStartSupported = %d, fastStart = 0x%08lx\n",
                         (UInt32)this, info.revision, info.fastStartSupported, info.fastStart ) );
						 
                // force vendors to use real values, (0, 0) is not legal
                if( (info.cmdSpecID & 0x00ffffff) || (info.cmdSet & 0x00ffffff) )
                {
                	fExpansionData->fNumLUNs++;
					createLUN( &info );
                }
            }
        }
        
        directoryIterator->release();
    }

	fExpansionData->fPendingMgtAgentCommands = OSArray::withCapacity( fExpansionData->fNumLUNs) ;

    //
    // we found all the luns so lets call registerService on ourselves
    // for drivers that want to drive all luns
    //

    OSObject *prop;

    if( status == kIOReturnSuccess )
    {
        // all of these values are 24 bits or less, even though we have specified 32 bits

		prop = OSNumber::withNumber( info.cmdSpecID, 32 );
        setProperty( gCommand_Set_Spec_ID_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( info.cmdSet, 32 );
        setProperty( gCommand_Set_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( info.vendorID, 32 );
        setProperty( gModule_Vendor_ID_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( info.softwareRev, 32 );
        setProperty( gCommand_Set_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( info.firmwareRev, 32 );
        setProperty( gFirmware_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( info.devType, 32 );
        setProperty( gDevice_Type_Symbol, prop );
        prop->release();

        prop = fProviderUnit->getProperty(gGUID_Symbol);
        if( prop )
            setProperty( gGUID_Symbol, prop );
 
		prop = fProviderUnit->getProperty(gFireWireModel_ID);
        if( prop )
			setProperty( gFireWireModel_ID, prop );

		prop = fProviderUnit->getProperty(gFireWireProduct_Name);
        if( prop )
            setProperty( gFireWireProduct_Name, prop );

		prop = fProviderUnit->getProperty(gFireWireVendor_Name);
        if( prop )
            setProperty( gFireWireVendor_Name, prop );
			       
        registerService();
    }
}

IOReturn IOFireWireSBP2Target::createLUN( LUNInfo * info )

{
    IOReturn	status = kIOReturnSuccess;
    
    OSDictionary * propTable = OSDictionary::withCapacity(7);

     OSObject *prop;

    if( propTable )
    {
        // all of these values are 24 bits or less, even though we have specified 32 bits

        prop = OSNumber::withNumber( info->cmdSpecID, 32 );
        propTable->setObject( gCommand_Set_Spec_ID_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( info->cmdSet, 32 );
        propTable->setObject( gCommand_Set_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( info->vendorID, 32 );
        propTable->setObject( gModule_Vendor_ID_Symbol, prop );
        prop->release();
  
        prop = OSNumber::withNumber( info->softwareRev, 32 );
        propTable->setObject( gCommand_Set_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( info->firmwareRev, 32 );
        propTable->setObject( gFirmware_Revision_Symbol, prop );
        prop->release();

        prop = OSNumber::withNumber( info->lun, 32 );
        propTable->setObject( gIOUnit_Symbol, prop );
        prop->release();

        prop = OSNumber::withNumber( info->devType, 32 );
        propTable->setObject( gDevice_Type_Symbol, prop );
        prop->release();
 
		prop = OSNumber::withNumber( info->unitCharacteristics, 32 );
        propTable->setObject( gUnit_Characteristics_Symbol, prop );
        prop->release();
		
		prop = OSNumber::withNumber(info->managementOffset, 32 );
        propTable->setObject( gManagement_Agent_Offset_Symbol, prop );
        prop->release();
		
		if( info->fastStartSupported )
		{
			prop = OSNumber::withNumber( info->fastStart, 32 );
			propTable->setObject( gFast_Start_Symbol, prop );
			prop->release();
		}
		
        prop = fProviderUnit->getProperty(gGUID_Symbol);
        if( prop )
            propTable->setObject( gGUID_Symbol, prop );
 
		prop = fProviderUnit->getProperty(gFireWireModel_ID);
        if( prop )
            propTable->setObject( gFireWireModel_ID, prop );

		prop = fProviderUnit->getProperty(gFireWireProduct_Name);
        if( prop )
            propTable->setObject( gFireWireProduct_Name, prop );

		prop = fProviderUnit->getProperty(gFireWireVendor_Name);
        if( prop )
            propTable->setObject( gFireWireVendor_Name, prop );
        
		//
        // create lun
        //

        IOFireWireSBP2LUN * newLUN = new IOFireWireSBP2LUN;
        if( newLUN != NULL )
        {
            bool success = true;

            if( success )
                success = newLUN->init(propTable);

            if( success )
                success = newLUN->attach(this);

            if( success )
                newLUN->registerService();

            FWKLOG( ( "IOFireWireSBP2Target<0x%08lx> : created LUN object - success = %d\n", (UInt32)this, success ) );

            if( !success )
                status = kIOReturnError;
            
            newLUN->release();
        }

        propTable->release();
    }
    
    return status;
}

// matchPropertyTable
//
//

bool IOFireWireSBP2Target::matchPropertyTable(OSDictionary * table)
{
    
	//
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
	
    if( !IOService::matchPropertyTable(table) )  
		return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    bool res = 	compareProperty(table, gCommand_Set_Spec_ID_Symbol) &&
				compareProperty(table, gCommand_Set_Symbol) &&
				compareProperty(table, gModule_Vendor_ID_Symbol) &&
				compareProperty(table, gCommand_Set_Revision_Symbol) &&
				compareProperty(table, gFirmware_Revision_Symbol) &&
				compareProperty(table, gDevice_Type_Symbol) &&
                compareProperty(table, gGUID_Symbol) &&
                compareProperty(table, gFireWireModel_ID);
				
    return res;
}

void IOFireWireSBP2Target::setTargetFlags( UInt32 flags )
{
	fFlags |= flags;
    
	FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::setTargetFlags 0x%08lx\n", (UInt32)this, fFlags ));
    
    configurePhysicalFilter();
}

void IOFireWireSBP2Target::clearTargetFlags( UInt32 flags )
{
	fFlags &= ~flags;
    
	FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::clearTargetFlags 0x%08lx\n", (UInt32)this, fFlags ));
    
    configurePhysicalFilter();
}

UInt32 IOFireWireSBP2Target::getTargetFlags( void )
{
    return fFlags;
}

void IOFireWireSBP2Target::configurePhysicalFilter( void )
{
    bool disablePhysicalAccess = false;
    
	// sometimes message() gets called before start completes
	// we shouldn't try to configure anything until start is done
	
	if( fExpansionData == NULL )
		return;
		
	if( !fExpansionData->fStarted )
		return;
		
    //
    // determine if we should turn off physical access
    //
    
    if( fFlags & kIOFWSBP2FailsOnAckBusy )
    {
        IOFireWireController * controller = fProviderUnit->getController();
        IOService * fwim = (IOService*)controller->getLink();
 
        UInt32 deviceCount = 0;
		
        // get self id property
        OSData * data = (OSData*)controller->getProperty( "FireWire Self IDs" );
        
        // get self id data
        UInt32 	numIDs = data->getLength() / sizeof(UInt32);
        UInt32 	*IDs = (UInt32*)data->getBytesNoCopy();
        
        // count nodes on bus
        UInt32 	i;
        for( i = 0; i < numIDs; i++ )
        {
            UInt32 current_id = IDs[i];
            // count all type zero selfid with the linkon bit set
            if( (current_id & kFWSelfIDPacketType) == 0 &&
                (current_id & kFWSelfID0L) )
            {
                deviceCount++;
            }
        }
        
		// if PhysicalUnitBlocksOnReads and more than one device on the bus, 
        // then turn off the physical unit for this node
        if( (deviceCount > 2) && (fwim->getProperty( "PhysicalUnitBlocksOnReads" ) != NULL) )
        {
            disablePhysicalAccess = true;
        }
	}
    
    //
    // turn on or off physical access
    //
    
    if( disablePhysicalAccess )
    {
		FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::configurePhysicalFilter disabling physical access for unit 0x%08lx\n", (UInt32)this, fProviderUnit ));
        fProviderUnit->setNodeFlags( kIOFWDisableAllPhysicalAccess );
    }
    else
    {
		FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::configurePhysicalFilter enabling physical access for unit 0x%08lx\n", (UInt32)this, fProviderUnit ));
        fProviderUnit->clearNodeFlags( kIOFWDisableAllPhysicalAccess );
    }
    
}

// beginIOCriticalSection
//
//

IOReturn IOFireWireSBP2Target::beginIOCriticalSection( void )
{
	IOReturn status = kIOReturnSuccess;

	FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::beginIOCriticalSection\n", (UInt32)this ));
	
	if( fFlags & kIOFWSBP2FailsOnBusResetsDuringIO )
	{
		FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::beginIOCriticalSection fControl->disableSoftwareBusResets()\n", (UInt32)this ));
		status = fControl->beginIOCriticalSection();
		if( status == kIOReturnSuccess )
		{
			fIOCriticalSectionCount++;
		}
	}
	
	FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::beginIOCriticalSection status = 0x%08lx\n", (UInt32)this, (UInt32)status ));
	
	return status;
}

// endIOCriticalSection
//
//

void IOFireWireSBP2Target::endIOCriticalSection( void )
{
	FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::endIOCriticalSection\n", (UInt32)this ));

	if( fFlags & kIOFWSBP2FailsOnBusResetsDuringIO )
	{
		FWKLOG(( "IOFireWireSBP2Target<0x%08lx>::endIOCriticalSection fControl->enableSoftwareBusResets()\n", (UInt32)this ));
		
		if( fIOCriticalSectionCount != 0 )
		{
			fIOCriticalSectionCount--;
			fControl->endIOCriticalSection();
		}
		else
		{
			IOLog( "IOFireWireSBP2Target<0x%08lx>::endIOCriticalSection - fIOCriticalSectionCount == 0!\n", (UInt32)this );
		}
	}
}

// synchMgmtAgentAccess
//
//

IOReturn IOFireWireSBP2Target::synchMgmtAgentAccess(
	IOFWCommand	*				mgmtOrbCommand
)
{
	if( fExpansionData->fNumLUNs > 1 )
	{	
		FWKLOG(("IOFireWireSBP2Target::synchMgmtAgentAccess count %x\n",fExpansionData->fNumberPendingMgtAgentOrbs + 1));
	
		if( fExpansionData->fNumberPendingMgtAgentOrbs++ )
		{
			FWKLOG(("IOFireWireSBP2Target::synchMgmtAgentAccess Mgmt Agent Busy, saving submit command %08x\n",mgmtOrbCommand));
			fExpansionData->fPendingMgtAgentCommands->setObject( mgmtOrbCommand );
		}
		else
			return( mgmtOrbCommand->submit() );
	}
	else
		return( mgmtOrbCommand->submit() );

	return kIOReturnSuccess;
	
}

// completeMgmtAgentAccess
//
//

void IOFireWireSBP2Target::completeMgmtAgentAccess( )
{
	IOFWAsyncCommand	*		mgmtOrbCommand;
	IOReturn 					status = kIOReturnSuccess;

	if( fExpansionData->fNumLUNs > 1 )
	{

		FWKLOG(("IOFireWireSBP2Target::completeMgmtAgentAccess >>  count %x\n",fExpansionData->fNumberPendingMgtAgentOrbs - 1));
		if( fExpansionData->fNumberPendingMgtAgentOrbs-- )
		{
			mgmtOrbCommand = (IOFWAsyncCommand *)fExpansionData->fPendingMgtAgentCommands->getObject( 0 ) ;
		
			if( mgmtOrbCommand )
			{
				FWKLOG(("IOFireWireSBP2Target::completeMgmtAgentAccess, calling submit command %08x\n",mgmtOrbCommand));
				fExpansionData->fPendingMgtAgentCommands->removeObject( 0 ) ;
				
				status = mgmtOrbCommand->submit() ;
				
				if( status )
				{
					FWKLOG(("IOFireWireSBP2Target::completeMgmtAgentAccess, submit for command %08x failed with %08x\n",mgmtOrbCommand,status));
					mgmtOrbCommand->gotPacket( kFWResponseBusResetError, NULL, 0 );
				}
			}
		}
		FWKLOG(("IOFireWireSBP2Target::completeMgmtAgentAccess << count %x\n",fExpansionData->fNumberPendingMgtAgentOrbs));
	}
}

// clearMgmtAgentAccess
//
//

void IOFireWireSBP2Target::cancelMgmtAgentAccess( IOFWCommand * mgmtOrbCommand )
{
	// should only have one instance of a given command in this list, but I'll use a loop for good measure.
	
	int index;
	while( (index = fExpansionData->fPendingMgtAgentCommands->getNextIndexOfObject( mgmtOrbCommand, 0 )) != -1 )
	{
		fExpansionData->fPendingMgtAgentCommands->removeObject( index );		
	}
}

// clearMgmtAgentAccess
//
//

void IOFireWireSBP2Target::clearMgmtAgentAccess( )
{
	if( fExpansionData && fExpansionData->fPendingMgtAgentCommands )
	{
		IOFWAsyncCommand *				mgmtOrbCommand;
		
		FWKLOG(("IOFireWireSBP2Target::clearMgmtAgentAccess\n",mgmtOrbCommand));
			
		while( (mgmtOrbCommand = (IOFWAsyncCommand *)fExpansionData->fPendingMgtAgentCommands->getObject( 0 ))  )
		{
			mgmtOrbCommand->retain();
			
			fExpansionData->fPendingMgtAgentCommands->removeObject( 0 ) ;
			
			FWKLOG(("IOFireWireSBP2Target::clearMgmtAgentAccess, failing command %08x\n",mgmtOrbCommand));
			mgmtOrbCommand->gotPacket( kFWResponseBusResetError, NULL, 0 );
		
			mgmtOrbCommand->release();
		}
		
		fExpansionData->fNumberPendingMgtAgentOrbs = 0;
	}
}
