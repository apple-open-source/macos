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
#include <IOKit/firewire/IOFireWireLink.h>
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

    fOpenFromTarget = false;
    fOpenFromLUNCount = 0;
	
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

    if (IOService::start(provider))
    {
        scanForLUNs();
    }
    else
        return false;

    FWKLOG( ( "IOFireWireSBP2Target : started\n" ) );
    return true;
}

void IOFireWireSBP2Target::stop( IOService *provider )
{
    FWKLOG( ( "IOFireWireSBP2Target : stopped\n" ) );

    IOService::stop(provider);
}

////////////////////////////////////////////////////////////////////////

//
// message method
//

IOReturn IOFireWireSBP2Target::message( UInt32 type, IOService *nub, void *arg )
{
    IOReturn res = kIOReturnUnsupported;

    FWKLOG( ("IOFireWireSBP2Target : message 0x%x, arg 0x%08lx\n", type, arg) );

    res = IOService::message(type, nub, arg);
    if( kIOReturnUnsupported == res )
    {
        switch (type)
        {                
            case kIOMessageServiceIsTerminated:
                FWKLOG( ( "IOFireWireSBP2Target : kIOMessageServiceIsTerminated\n" ) );
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsSuspended:
                FWKLOG( ( "IOFireWireSBP2Target : kIOMessageServiceIsSuspended\n" ) );
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsResumed:
                FWKLOG( ( "IOFireWireSBP2Target : kIOMessageServiceIsResumed\n" ) );
                configurePhysicalFilter();
                res = kIOReturnSuccess;
                break;

            default: // default the action to return kIOReturnUnsupported
                break;
        }
    }

	if( type != kIOMessageServiceIsTerminated )
		messageClients( type, arg );    
    
    return res;
}

// getFireWireUnit
//
// returns the FireWire unit for doing non-SBP2 work

IOFireWireUnit * IOFireWireSBP2Target::getFireWireUnit( void )
{
    return fProviderUnit;
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

    FWKLOG(( "enter handleOpen fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", fOpenFromLUNCount, fOpenFromTarget ));
    
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
                FWKLOG(( "called open\n" ));
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
                FWKLOG(( "called open\n" ));
            }
        }
        else
            ok = false;   // already open
    }

    FWKLOG(( "exit handleOpen fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", fOpenFromLUNCount, fOpenFromTarget ));
    
    return ok;
}

void IOFireWireSBP2Target::handleClose( IOService * forClient, IOOptionBits options )
{
    FWKLOG(( "enter handleClose fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", fOpenFromLUNCount, fOpenFromTarget ));
    
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
                FWKLOG(( "called close\n" ));
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
            FWKLOG(( "called close\n" ));
        }
    }

    FWKLOG(( "exit handleClose fOpenFromLUNCount = %d, fOpenFromTarget = %d\n", fOpenFromLUNCount, fOpenFromTarget ));
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
    
	UInt32			cmdSpecID 			= 0;
	UInt32			cmdSet 				= 0;
	UInt32			vendorID			= 0;	
	UInt32			softwareRev 		= 0;
	UInt32			firmwareRev 		= 0;
	UInt32			lun					= 0;
	UInt32			devType				= 0;
	
    //
    // get root directory
    //

    IOConfigDirectory *		directory;
    
    IOFireWireDevice *  device;
    IOService *		providerService = fProviderUnit->getProvider();
    if( providerService == NULL )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        device = OSDynamicCast( IOFireWireDevice, providerService );
        if( device == NULL )
            status = kIOReturnError;
        FWKLOG( ("IOFireWireSBP2Target : unit = 0x%08lx, provider = 0x%08lx, device = 0x%08lx\n", fProviderUnit, providerService, device) );
    }
    
    if( status == kIOReturnSuccess )
    {
        status = device->getConfigDirectory( directory );
        FWKLOG( ( "IOFireWireSBP2Target : status = %d\n", status ) );
    }
    
    //
    // find vendor id
    //

    if( status == kIOReturnSuccess )
        tempStatus = directory->getKeyValue( kConfigModuleVendorIdKey, vendorID );

	//
	// get unit directory
    //
    
    status = fProviderUnit->getConfigDirectory( directory );
    FWKLOG( ( "IOFireWireSBP2Target : status = %d\n", status ) );
  	
	//
	// find command spec id
	//

    if( status == kIOReturnSuccess )
        status = directory->getKeyValue( kCmdSpecIDKey, cmdSpecID );
    
	//
	// find command set
	//

    if( status == kIOReturnSuccess )
        status = directory->getKeyValue( kCmdSetKey, cmdSet );

    // failure to find one of the following is not fatal, hence the use of tempStatus
    
	//
	// find software rev
	//
    
    if( status == kIOReturnSuccess )
         tempStatus = directory->getKeyValue( kSoftwareRevKey, softwareRev );

    //
	// find firmware rev
	//

    if( status == kIOReturnSuccess )
        tempStatus = directory->getKeyValue( kFirmwareRevKey, firmwareRev );

     FWKLOG( ( "IOFireWireSBP2Target : status = %d, cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d, firmwareRev = %d\n",
                             status, cmdSpecID, cmdSet, vendorID, softwareRev, firmwareRev ) );

    //
	// find management agent offset
    // and add ourselves to the DeviceTree with that location
    // for OpenFirmware boot path generation.
	//
    
	UInt32 offset;
    status = directory->getKeyValue( kManagementAgentOffsetKey, offset );
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
            sprintf(location, "%lx", offset);
            attachToParent(parent, gIODTPlane);
            setLocation(location, gIODTPlane);
            setName("sbp-2", gIODTPlane);
        }
    }
 
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
            FWKLOG( ( "IOFireWireSBP2Target : tempStatus = %d, pos = %d, key = %d\n",
                             tempStatus, pos, key ) );
            
            // if it was the LUN key
            if( tempStatus == kIOReturnSuccess && key >> kConfigEntryKeyValuePhase == kLUNKey )
            {
                UInt32 data;
                tempStatus = directory->getIndexValue( pos, data );
                if( tempStatus == kIOReturnSuccess )
                {
                    lun = data  & 0x0000ffff;
                    devType = (data & 0x001f0000) >> 16;

                    FWKLOG( ( "IOFireWireSBP2Target : cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d\n",
                             cmdSpecID, cmdSet, vendorID, softwareRev ) );
                    FWKLOG( ( "IOFireWireSBP2Target : firmwareRev = %d, lun = %d, devType = %d\n",
                             firmwareRev, lun, devType ) );

                    // force vendors to use real values, (0, 0) is not legal
                    if( (cmdSpecID & 0x00ffffff) || (cmdSet & 0x00ffffff) )
                    {
                        createLUN( cmdSpecID, cmdSet, vendorID, softwareRev, firmwareRev, lun, devType );
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
            // get lun value
            
            tempStatus = lunDirectory->getKeyValue( kLUNKey, lunValue );
            if( tempStatus == kIOReturnSuccess )
            {
                lun = lunValue  & 0x0000ffff;
                devType = (lunValue & 0x001f0000) >> 16;

                FWKLOG( ( "IOFireWireSBP2Target : cmdSpecID = %d, cmdSet = %d, vendorID = %d, softwareRev = %d\n",
                         cmdSpecID, cmdSet, vendorID, softwareRev ) );
                FWKLOG( ( "IOFireWireSBP2Target : firmwareRev = %d, lun = %d, devType = %d\n",
                         firmwareRev, lun, devType ) );

                // force vendors to use real values, (0, 0) is not legal
                if( (cmdSpecID & 0x00ffffff) || (cmdSet & 0x00ffffff) )
                {
                    createLUN( cmdSpecID, cmdSet, vendorID, softwareRev, firmwareRev, lun, devType );
                }
            }
        }
        
        directoryIterator->release();
    }

    //
    // we found all the luns so lets call registerService on ourselves
    // for drivers that want to drive all luns
    //

    OSObject *prop;

    if( status == kIOReturnSuccess )
    {
        // all of these values are 24 bits or less, even though we have specified 32 bits

		prop = OSNumber::withNumber( cmdSpecID, 32 );
        setProperty( gCommand_Set_Spec_ID_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( cmdSet, 32 );
        setProperty( gCommand_Set_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( vendorID, 32 );
        setProperty( gModule_Vendor_ID_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( softwareRev, 32 );
        setProperty( gCommand_Set_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( firmwareRev, 32 );
        setProperty( gFirmware_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( devType, 32 );
        setProperty( gDevice_Type_Symbol, prop );
        prop->release();

        registerService();
    }
}

IOReturn IOFireWireSBP2Target::createLUN( UInt32 cmdSpecID, UInt32 cmdSet, UInt32 vendorID, UInt32 softwareRev,
                                          UInt32 firmwareRev, UInt32 lun, UInt32 devType )

{
    IOReturn	status = kIOReturnSuccess;
    
    OSDictionary * propTable = OSDictionary::withCapacity(7);

     OSObject *prop;

    if( propTable )
    {

#if 0
		const OSSymbol * propSymbol;

		////////////////////////////////////////////
        // make user client sub dictionary
        OSDictionary * userClient = OSDictionary::withCapacity(2);

        propSymbol = OSSymbol::withCString("A45B8156-B51B-11D4-AB4B-000A277E7234");
        prop = OSString::withCString("IOFireWireSBP2Lib.plugin");
        userClient->setObject( propSymbol, prop );
        prop->release();
        propSymbol->release();

        propSymbol = OSSymbol::withCString("631F68D2-B9E6-11D4-8147-000A277E7234");
        prop = OSString::withCString("SBP2SampleDriver.plugin");
        userClient->setObject( propSymbol, prop );
        prop->release();
        propSymbol->release();


        ////////////////////////////////////////////
                
        propSymbol = OSSymbol::withCString("IOCFPlugInTypes");
        propTable->setObject( propSymbol, userClient );
        propSymbol->release();
#endif        
        // all of these values are 24 bits or less, even though we have specified 32 bits

        prop = OSNumber::withNumber( cmdSpecID, 32 );
        propTable->setObject( gCommand_Set_Spec_ID_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( cmdSet, 32 );
        propTable->setObject( gCommand_Set_Symbol, prop );
        prop->release();

		prop = OSNumber::withNumber( vendorID, 32 );
        propTable->setObject( gModule_Vendor_ID_Symbol, prop );
        prop->release();
  
        prop = OSNumber::withNumber( softwareRev, 32 );
        propTable->setObject( gCommand_Set_Revision_Symbol, prop );
        prop->release();
 
        prop = OSNumber::withNumber( firmwareRev, 32 );
        propTable->setObject( gFirmware_Revision_Symbol, prop );
        prop->release();

        prop = OSNumber::withNumber( lun, 32 );
        propTable->setObject( gIOUnit_Symbol, prop );
        prop->release();

        prop = OSNumber::withNumber( devType, 32 );
        propTable->setObject( gDevice_Type_Symbol, prop );
        prop->release();
 
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

            FWKLOG( ( "IOFireWireSBP2Target : created LUN object - success = %d\n", success ) );

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
				compareProperty(table, gDevice_Type_Symbol);
				
    return res;
}

void IOFireWireSBP2Target::setTargetFlags( UInt32 flags )
{
    fFlags = flags;
    
    // IOLog( "IOFireWireSBP2Target::setTargetFlags 0x%08lx\n", fFlags );
    
    configurePhysicalFilter();
}

UInt32 IOFireWireSBP2Target::getTargetFlags( void )
{
    return fFlags;
}

void IOFireWireSBP2Target::configurePhysicalFilter( void )
{
    bool disablePhysicalAccess = false;
    
    //
    // determine if we should turn off physical access
    //
    
    if( fFlags & kIOFWSBP2FailsOnAckBusy )
    {
        IOFireWireController * controller = fProviderUnit->getController();
        IOFireWireLink * fwim = controller->getLink();
 
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
		FWKLOG(( "IOFireWireSBP2Target::configurePhysicalFilter disabling physical access for unit 0x%08lx\n", fProviderUnit ));
        UInt32 flags = fProviderUnit->getNodeFlags();
        fProviderUnit->setNodeFlags( flags | kIOFWDisableAllPhysicalAccess );
    }
    else
    {
		FWKLOG(( "IOFireWireSBP2Target::configurePhysicalFilter enabling physical access for unit 0x%08lx\n", fProviderUnit ));
        UInt32 flags = fProviderUnit->getNodeFlags();
        fProviderUnit->setNodeFlags( flags & (~kIOFWDisableAllPhysicalAccess) );
    }
    
}
