/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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

/*
 *
 *	IOATABusCommand.cpp
 *
 */
 


 
#include<IOKit/IOTypes.h>
#include"IOATATypes.h"
#include"IOATABusCommand.h"


#ifdef DLOG
#undef DLOG
#endif

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


//---------------------------------------------------------------------------

#define super IOATACommand

OSDefineMetaClassAndStructors( IOATABusCommand, IOATACommand )
    OSMetaClassDefineReservedUnused(IOATABusCommand, 0);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 1);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 2);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 3);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 4);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 5);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 6);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 7);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 8);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 9);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 10);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 11);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 12);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 13);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 14);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 15);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 16);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 17);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 18);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 19);
    OSMetaClassDefineReservedUnused(IOATABusCommand, 20);


/*-----------------------------------------------------------------------------
 *  Static allocator.
 *
 *-----------------------------------------------------------------------------*/
IOATABusCommand* 
IOATABusCommand::allocateCmd(void)
{
	IOATABusCommand* cmd = new IOATABusCommand;
	
	if( cmd == 0L)
		return 0L;
		
		
	if( ! cmd->init() )
	{
		cmd->free();
		return 0L;
	}
		
	return cmd;
	
}


/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
bool
IOATABusCommand::init()
{
	if( ! super::init() )
		return false;

	zeroCommand();
	
	return true;

}

/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
void 
IOATABusCommand::zeroCommand(void)
{
	queue_init( &queueChain );
	state = 0;
	syncer = 0L;

	super::zeroCommand();

}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
  	// return the command opcode
ataOpcode 
IOATABusCommand::getOpcode( void )
{

	return _opCode;

}
  
	
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	// get the command flags
ataFlags 
IOATABusCommand::getFlags ( void )
{

	return (ataFlags) _flags;

}
 
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
ataRegMask
IOATABusCommand::getRegMask( void )
{
	return _regMask;

}



/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	// return the unit id (0 master, 1 slave)
ataUnitID 
IOATABusCommand::getUnit( void )
{

	return _unit;

}
 
	
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	// return the timeout value for this command 
UInt32 
IOATABusCommand::getTimeoutMS (void )
{

	return _timeoutMS;

}
 
	
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	// return the callback pointer
IOATACompletionFunction* 
IOATABusCommand::getCallbackPtr (void )
{

	return _callback;

}
 
	
/*-----------------------------------------------------------------------------
 *
 *
 *-----------------------------------------------------------------------------*/
	// call the completion callback function
void 
IOATABusCommand::executeCallback(void)
{
	_inUse = false;

	if(_callback != 0L)
	{
		 (*_callback)(this);

	} else if( syncer != 0L ) {
	
		syncer->signal();
		syncer = 0L;
	
	}


}

/*-----------------------------------------------------------------------------
 * get the number of bytes between intervening interrupts for this transfer. 
 *
 *-----------------------------------------------------------------------------*/
IOByteCount
IOATABusCommand::getTransferChunkSize(void)
{

	return _logicalChunkSize;	

}	

ataTaskFile* 
IOATABusCommand::getTaskFilePtr(void)  
{
	return &(_taskFile.taskFile) ;
}

UInt16 
IOATABusCommand::getPacketSize(void) 
{
	return _packet.atapiPacketSize;
}
	
	
UInt16*	
IOATABusCommand::getPacketData(void) 
{ 
	return _packet.atapiCommandByte; 
}

IOByteCount 
IOATABusCommand::getByteCount (void)
{

	return _byteCount;
}


IOByteCount
IOATABusCommand::getPosition (void)
{

	return _position;

}



IOMemoryDescriptor* 
IOATABusCommand::getBuffer ( void)
{
	return _desc;
}


void 
IOATABusCommand::setActualTransfer ( IOByteCount bytesTransferred )
{
	_actualByteCount = bytesTransferred;
}

void 
IOATABusCommand::setResult( IOReturn inResult)
{

	_result = inResult;
}


void
IOATABusCommand::setCommandInUse( bool inUse /* = true */)
{

	_inUse = inUse;

}


