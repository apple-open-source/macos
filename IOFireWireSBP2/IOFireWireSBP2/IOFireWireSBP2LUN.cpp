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
#include <IOKit/IOWorkLoop.h>

#include <IOKit/sbp2/IOFireWireSBP2Target.h>
#include <IOKit/sbp2/IOFireWireSBP2LUN.h>
#include <IOKit/sbp2/IOFireWireSBP2UserClient.h>
#include "FWDebugging.h"
#include "IOFireWireSBP2Diagnostics.h"

extern const OSSymbol *gCommand_Set_Spec_ID_Symbol;
extern const OSSymbol *gCommand_Set_Symbol;
extern const OSSymbol *gModule_Vendor_ID_Symbol;
extern const OSSymbol *gCommand_Set_Revision_Symbol;
extern const OSSymbol *gIOUnit_Symbol;
extern const OSSymbol *gFirmware_Revision_Symbol;
extern const OSSymbol *gDevice_Type_Symbol;

const OSSymbol *gDiagnostics_Symbol;

OSDefineMetaClassAndStructors( IOFireWireSBP2LUN, IOService );

OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2LUN, 8);

bool IOFireWireSBP2LUN::attach(IOService *provider)
{
	IOReturn status = kIOReturnSuccess;
	
	// init fields
	fProviderTarget = NULL;
	fGate 			= NULL;
	fLUNumber 		= 0;
	fORBSet 		= NULL;
	fORBSetIterator = NULL;
	
    FWKLOG( ( "IOFireWireSBP2LUN : attach\n" ) );
	
	//
	// attach to provider
	//
	
	if( status == kIOReturnSuccess )
	{
		fProviderTarget = OSDynamicCast( IOFireWireSBP2Target, provider );
		if( !fProviderTarget )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( !IOService::attach(provider) )
        	status = kIOReturnError;
	}

#if FWDIAGNOSTICS
	
	if( gDiagnostics_Symbol == NULL )
		gDiagnostics_Symbol = OSSymbol::withCString("SBP2 Diagnostics");

	fDiagnostics = IOFireWireSBP2Diagnostics::createDiagnostics();
	if( fDiagnostics )
	{
		setProperty( gDiagnostics_Symbol, fDiagnostics );
	}
	
#endif
	
	//
	// get lun number
	//
	
	if( status == kIOReturnSuccess )
	{
		OSObject *prop;

		// read lun number from registry		
		prop = getProperty( gIOUnit_Symbol );
		fLUNumber = ((OSNumber*)prop)->unsigned32BitValue();
	}
	
    //
    // create login set
    //
    
	if( status == kIOReturnSuccess )
	{
		fLoginSet = OSSet::withCapacity(1);
		if( fLoginSet == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( fLoginSet )
            fLoginSetIterator = OSCollectionIterator::withCollection( fLoginSet );
	}
    
    
	//
	// create management orb set
	//
	
	if( status == kIOReturnSuccess )
	{
		fORBSet = OSSet::withCapacity(1);
		if( fORBSet == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( fORBSet )
            fORBSetIterator = OSCollectionIterator::withCollection( fORBSet );
	}
	
	//
	// set up command gate
	//
	
	IOWorkLoop * workLoop = NULL;
	if( status == kIOReturnSuccess )
	{
		workLoop = getWorkLoop();
		if( !workLoop ) 
			status = kIOReturnNoResources;
	}
	
	if( status == kIOReturnSuccess )
	{
		fGate = IOCommandGate::commandGate( this );
		if( !fGate )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		workLoop->retain();
		workLoop->addEventSource( fGate );
	}
	
    return (status == kIOReturnSuccess);
}

//
// free
//

void IOFireWireSBP2LUN::free( void )
{
	FWKLOG( ( "IOFireWireSBP2LUN : free\n" ) );
	
	//
	// free unreleased orbs
	//
	
	flushAllManagementORBs();
		
	if( fORBSetIterator )			
		fORBSetIterator->release();

	if( fORBSet )
		fORBSet->release();

    //
    // release login set
    //
    
	if( fLoginSetIterator )			
		fLoginSetIterator->release();

	if( fLoginSet )
		fLoginSet->release();

	//
	// destroy command gate
	//
	
	if( fGate != NULL )
	{
		IOWorkLoop * workLoop = NULL;

		workLoop = fGate->getWorkLoop();
		workLoop->removeEventSource( fGate );
		workLoop->release();

		fGate->release();
		fGate = NULL;
	}
	
	//
	// free super
	//
	
	IOService::free();
}

// flushAllManagementORBs
//
//

void IOFireWireSBP2LUN::flushAllManagementORBs( void )
{
	IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteFlushAllMgmtORBs);
}
	
IOReturn IOFireWireSBP2LUN::staticExecuteFlushAllMgmtORBs( OSObject * self, void *, 
										void *, void *, void * )
{
	return ((IOFireWireSBP2LUN *)self)->executeFlushAllMgmtORBs();
}

IOReturn IOFireWireSBP2LUN::executeFlushAllMgmtORBs( void )
{
	//
	// free unreleased orbs
	//
	
	if( fORBSetIterator )
	{
		IOFireWireSBP2ManagementORB * item = NULL;		
		do
		{
            fORBSetIterator->reset();
			item = (IOFireWireSBP2ManagementORB *)fORBSetIterator->getNextObject();
			if( item )
				item->release();
		} while( item );
	}
			
	return kIOReturnSuccess;
}

////////////////////////////////////////////////////////////////////

//
// handleOpen / handleClose
//

bool IOFireWireSBP2LUN::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
    FWKLOG(( "IOFireWireSBP2LUN %d : handleOpen\n", fLUNumber ));
	
	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fProviderTarget->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

void IOFireWireSBP2LUN::handleClose( IOService * forClient, IOOptionBits options )
{
    FWKLOG(( "IOFireWireSBP2LUN %d : handleClose\n", fLUNumber ));
	
	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fProviderTarget->close(this, options);
	}
}

//
// message method
//

IOReturn IOFireWireSBP2LUN::message( UInt32 type, IOService *nub, void *arg )
{
    IOReturn res = kIOReturnUnsupported;

    //IOLog("IOFireWireSBP2LUN, message 0x%x\n", type);

    res = IOService::message(type, nub, arg);

    if( kIOReturnUnsupported == res )
    {
        switch (type)
        {
            case kIOMessageServiceIsTerminated:
                FWKLOG( ( "IOFireWireSBP2LUN : kIOMessageServiceIsTerminated\n" ) );
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsSuspended:
                FWKLOG( ( "IOFireWireSBP2LUN : kIOMessageServiceIsSuspended\n" ) );
                suspendedNotify();
                res = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsResumed:
                FWKLOG( ( "IOFireWireSBP2LUN : kIOMessageServiceIsResumed\n" ) );
                resumeNotify();
                res = kIOReturnSuccess;
                break;

            default: // default the action to return kIOReturnUnsupported
                break;
        }
   }

    // we must send resumeNotify and/or suspendNotify before messaging clients
  	if( type != kIOMessageServiceIsTerminated )
		messageClients( type, arg );
	
	// send reset notification for all busy orbs
	// we must send orb reset notification after messaging clients
	if( type == kIOMessageServiceIsSuspended )
		clearAllTasksInSet();
		
    return res;
}

////////////////////////////////////////////////////////////////////

// getFireWireUnit
//
// returns the FireWire unit for doing non-SBP2 work

IOFireWireUnit * IOFireWireSBP2LUN::getFireWireUnit( void )
{
    return fProviderTarget->getFireWireUnit();
}

// getLUNumber
//
// lun number accessor

UInt32 IOFireWireSBP2LUN::getLUNumber( void )
{
    return fLUNumber;
}

// getTarget
//
// target accessor

IOFireWireSBP2Target * IOFireWireSBP2LUN::getTarget( void )
{
    return fProviderTarget;
}

////////////////////////////////////////////////////////////////////

// createLogin
//
// create a login object

IOFireWireSBP2Login * IOFireWireSBP2LUN::createLogin( void )
{
	IOReturn 				status = kIOReturnSuccess;
	IOFireWireSBP2Login *	login;
	
	status = fGate->runAction( staticCreateLogin, &login );
	
	return login;
}

IOReturn IOFireWireSBP2LUN::staticCreateLogin( OSObject *self, void * login, void *, void *, void * )
{
	return ((IOFireWireSBP2LUN *)self)->createLoginAction( (IOFireWireSBP2Login **)login );
}

IOReturn IOFireWireSBP2LUN::createLoginAction( IOFireWireSBP2Login ** login )
{
	IOFireWireSBP2Login *	newLogin;
    newLogin  = new IOFireWireSBP2Login;
	if( newLogin != NULL && !initLoginWithLUN( newLogin, this ) )
    {
    	newLogin->release();
    	newLogin = NULL;
    }

	*login = newLogin;
	
	fLoginSet->setObject( *login );
	
    return kIOReturnSuccess;
}

//
// remove login
//

// removeManagementORB
//
// remove management orb from LUN's set

void IOFireWireSBP2LUN::removeLogin( IOFireWireSBP2Login * login )
{
	IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( (IOCommandGate::Action)staticRemoveLoginAction, (void*)login );
}

IOReturn IOFireWireSBP2LUN::staticRemoveLoginAction( OSObject *self, void * login, void *, void *, void * )
{
	return ((IOFireWireSBP2LUN*)self)->removeLoginAction( (IOFireWireSBP2Login *)login );
}

IOReturn IOFireWireSBP2LUN::removeLoginAction( IOFireWireSBP2Login * login )
{
	fLoginSet->removeObject( login );
	
	return kIOReturnSuccess;
}

// clearAllTasksInSet
//
// send reset notification for all busy orbs

void IOFireWireSBP2LUN::clearAllTasksInSet( void )
{
 	if( fLoginSetIterator )
	{
		IOFireWireSBP2Login * item = NULL;
        fLoginSetIterator->reset();
        do
		{
			item = (IOFireWireSBP2Login *)fLoginSetIterator->getNextObject();
			if( item )
				item->clearAllTasksInSet();
		} while( item );
	}
}

////////////////////////////////////////////////////////////////////

// createManagementORB
//
// create a management object

IOFireWireSBP2ManagementORB * IOFireWireSBP2LUN::createManagementORB( void * refCon, FWSBP2ManagementCallback completion )
{
	IOReturn status = kIOReturnSuccess;
	IOFireWireSBP2ManagementORB * orb;
	
	status = fGate->runAction( staticCreateManagementORBAction, refCon, (void*)completion, &orb );
	
	return orb;
}

IOReturn IOFireWireSBP2LUN::staticCreateManagementORBAction( OSObject *self, 
								void * refCon, void * completion, 
								void * orb, void * )
{
	return ((IOFireWireSBP2LUN *)self)->createManagementORBAction(  refCon, (FWSBP2ManagementCallback)completion, (IOFireWireSBP2ManagementORB **)orb );
}

IOReturn IOFireWireSBP2LUN::createManagementORBAction( 
								void * refCon, FWSBP2ManagementCallback completion,  
								IOFireWireSBP2ManagementORB ** orb )
{
    IOFireWireSBP2ManagementORB * newORB = new IOFireWireSBP2ManagementORB;
    if( newORB != NULL && !initMgmtORBWithLUN( newORB, this, refCon, completion ) )
    {
    	newORB->release();
    	newORB = NULL;
    }

	*orb = newORB;
	
	fORBSet->setObject( *orb );
	
    return kIOReturnSuccess;
}

// removeManagementORB
//
// remove management orb from LUN's set

void IOFireWireSBP2LUN::removeManagementORB( IOFireWireSBP2ManagementORB * orb )
{
	IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( (IOCommandGate::Action)staticRemoveManagementORBAction, (void*)orb );
}

IOReturn IOFireWireSBP2LUN::staticRemoveManagementORBAction( OSObject *self, void * orb, 
																void *, void *, void * )
{
	return ((IOFireWireSBP2LUN*)self)->removeManagementORBAction( (IOFireWireSBP2ManagementORB *)orb );
}

IOReturn IOFireWireSBP2LUN::removeManagementORBAction( 
								IOFireWireSBP2ManagementORB * orb )
{
	fORBSet->removeObject( orb );
	
	return kIOReturnSuccess;
}

////////////////////////////////////////////////////////////////////

// matchPropertyTable
//
//

bool IOFireWireSBP2LUN::matchPropertyTable(OSDictionary * table)
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
				compareProperty(table, gIOUnit_Symbol) &&
				compareProperty(table, gFirmware_Revision_Symbol) &&
				compareProperty(table, gDevice_Type_Symbol);
				
    return res;
}

// newUserClient
//
//

IOReturn IOFireWireSBP2LUN::newUserClient(task_t  owningTask, void * /* security_id */,
                                          UInt32 type,  IOUserClient **	handler )
{
    IOReturn			err = kIOReturnSuccess;
    IOFireWireSBP2UserClient *	client;

    if( type != kIOFireWireSBP2LibConnection )
        return( kIOReturnBadArgument);

    client = IOFireWireSBP2UserClient::withTask(owningTask);

    if( !client || (false == client->attach( this )) ||
        (false == client->start( this )) ) {
        if(client) {
            client->detach( this );
            client->release();
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;
    return( err );
}

////////////////////////////////////////////////////////////////////

//
// IOFireWireSBP2ManagementORB friend class wrappers
//

bool IOFireWireSBP2LUN::initMgmtORBWithLUN( IOFireWireSBP2ManagementORB * orb, IOFireWireSBP2LUN * lun, 
									 void * refCon, 
									 FWSBP2ManagementCallback completion )
{ 
	return orb->initWithLUN( lun, refCon, completion ); 
}

//		
// IOFireWireSBP2Login friend class wrappers
//

bool IOFireWireSBP2LUN::initLoginWithLUN( IOFireWireSBP2Login * login, IOFireWireSBP2LUN * lun )
{ 
	return login->initWithLUN( lun ); 
}

void IOFireWireSBP2LUN::suspendedNotify( void )
{ 
    if( fLoginSetIterator )
	{
		IOFireWireSBP2Login * item = NULL;
		fLoginSetIterator->reset();        
		do
		{
			item = (IOFireWireSBP2Login *)fLoginSetIterator->getNextObject();
			if( item )
				item->suspendedNotify();
		} while( item );
	}
}

void IOFireWireSBP2LUN::resumeNotify( void )
{ 
    if( fLoginSetIterator )
	{
		IOFireWireSBP2Login * item = NULL;
		fLoginSetIterator->reset();
        do
		{
			item = (IOFireWireSBP2Login *)fLoginSetIterator->getNextObject();
			if( item )
				item->resumeNotify();
		} while( item );
	}
}

// getDiagnostics
//
// return a pointer to the diagnostics object

OSObject * IOFireWireSBP2LUN::getDiagnostics( void )
{
	return fDiagnostics;
}
