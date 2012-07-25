/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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

// public
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWPHYPacketListener.h>

#include "FWDebugging.h"

OSDefineMetaClassAndStructors( IOFWPHYPacketListener, OSObject )

OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 0 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 1 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 2 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 3 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 4 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 5 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 6 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 7 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 8 );
OSMetaClassDefineReservedUnused( IOFWPHYPacketListener, 9 );

// createWithController
//
//

IOFWPHYPacketListener * IOFWPHYPacketListener::createWithController( IOFireWireController * controller )
{
    IOReturn				status = kIOReturnSuccess;
    IOFWPHYPacketListener * me;
        
    if( status == kIOReturnSuccess )
    {
        me = OSTypeAlloc( IOFWPHYPacketListener );
        if( me == NULL )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        bool success = me->initWithController( controller );
		if( !success )
		{
			status = kIOReturnError;
		}
    }
    
    if( status != kIOReturnSuccess )
    {
        me = NULL;
    }

	FWKLOG(( "IOFWPHYPacketListener::create() - created new IOFWPHYPacketListener %p\n", me ));
    
    return me;
}

// initWithController
//
//

bool IOFWPHYPacketListener::initWithController( IOFireWireController * control )
{	
	bool success = OSObject::init();
	FWPANICASSERT( success == true );
	
	fControl = control;

	FWKLOG(( "IOFWPHYPacketListener::initWithController() - IOFWPHYPacketListener %p initialized\n", this  ));

	return true;
}

// free
//
//

void IOFWPHYPacketListener::free()
{	
	FWKLOG(( "IOFWPHYPacketListener::free() - freeing IOFWPHYPacketListener %p\n", this ));

	OSObject::free();
}

///////////////////////////////////////////////////////////////////////////////////////
#pragma mark -

// activate
//
//

IOReturn IOFWPHYPacketListener::activate( void )
{
    return fControl->activatePHYPacketListener( this );
}

// deactivate
//
//

void IOFWPHYPacketListener::deactivate( void )
{
    fControl->deactivatePHYPacketListener( this );
}

// processPHYPacket
//
//

void IOFWPHYPacketListener::processPHYPacket( UInt32 data1, UInt32 data2 )
{
	IOLog( "IOFWPHYPacketListener<%p>::processPHYPacket - 0x%x 0x%x\n", this, (uint32_t)data1, (uint32_t)data2 );
	
	if( fCallback )
	{
		(fCallback)( fRefCon, data1, data2 );
	}
}

// setCallback
//
//

void IOFWPHYPacketListener::setCallback( FWPHYPacketCallback callback )
{
	fCallback = callback;
}

// setRefCon
//
//

void IOFWPHYPacketListener::setRefCon( void * refcon )
{
	fRefCon = refcon;
}

// getRefCon
//
//

void * IOFWPHYPacketListener::getRefCon( void )
{
	return fRefCon;
}
