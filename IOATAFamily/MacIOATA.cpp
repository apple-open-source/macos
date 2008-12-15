/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
 *	MacIOATA.cpp
 *	
 *	class defining the portions of MacIO ATA cells which are shared
 *	in common between OHare, Heathrow and Key Largo ATA Cells.
 *	These controllers share a common register file layout, interrupt 
 *	source format and all use DBDMA engines. These are different from 
 *	other ATA controllers, such as most PCI-IDE and PC-Card ATA ports.
 * 	Each cell type has some distinctive features that must be implemented
 *	by a specific driver subclass. As much common code as possible is 
 *	presented in this superclass.
 *
 *
 */
#ifdef __ppc__

#include <IOKit/IOTypes.h>
#include "IOATATypes.h"
#include "IOATAController.h"
#include "IOATADevice.h"
#include "IOATABusInfo.h"
#include "IOATADevConfig.h"

#include <IOKit/ppc/IODBDMA.h>
#include <IOKit/IOMemoryCursor.h>

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>

#include "MacIOATA.h"
#include "IOATABusCommand.h"


#ifdef DLOG
#undef DLOG
#endif

//#define ATA_DEBUG 1

#ifdef  ATA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// some day, we'll have an ATA recorder for IOKit

#define ATARecordEventMACRO(type,param,bus,data) 		(void) (type); (void) (param); (void) (bus); (void) (data)


#pragma mark -IOService Overrides -

// 33 data descriptors + NOP + STOP
#define kATAXferDMADesc 33
#define kATAMaxDMADesc kATAXferDMADesc + 2
// up to 256 ATA sectors per transfer
#define kMaxATAXfer	512 * 256


enum {
																/* Command.cmd operations*/
	OUTPUT_MORE					= 0x00000000,
	OUTPUT_LAST					= 0x10000000,
	INPUT_MORE					= 0x20000000,
	INPUT_LAST					= 0x30000000,
	STORE_QUAD					= 0x40000000,
	LOAD_QUAD					= 0x50000000,
	NOP_CMD						= 0x60000000,
	STOP_CMD					= 0x70000000,
	kdbdmaCmdMask				= (long)0xF0000000
};

//---------------------------------------------------------------------------

#define super IOATAController

OSDefineMetaClass( MacIOATA, IOATAController )
OSDefineAbstractStructors( MacIOATA, IOATAController )
    OSMetaClassDefineReservedUnused(MacIOATA, 0);
    OSMetaClassDefineReservedUnused(MacIOATA, 1);
    OSMetaClassDefineReservedUnused(MacIOATA, 2);
    OSMetaClassDefineReservedUnused(MacIOATA, 3);
    OSMetaClassDefineReservedUnused(MacIOATA, 4);
    OSMetaClassDefineReservedUnused(MacIOATA, 5);
    OSMetaClassDefineReservedUnused(MacIOATA, 6);
    OSMetaClassDefineReservedUnused(MacIOATA, 7);
    OSMetaClassDefineReservedUnused(MacIOATA, 8);
    OSMetaClassDefineReservedUnused(MacIOATA, 9);
    OSMetaClassDefineReservedUnused(MacIOATA, 10);
    OSMetaClassDefineReservedUnused(MacIOATA, 11);
    OSMetaClassDefineReservedUnused(MacIOATA, 12);
    OSMetaClassDefineReservedUnused(MacIOATA, 13);
    OSMetaClassDefineReservedUnused(MacIOATA, 14);
    OSMetaClassDefineReservedUnused(MacIOATA, 15);
    OSMetaClassDefineReservedUnused(MacIOATA, 16);
    OSMetaClassDefineReservedUnused(MacIOATA, 17);
    OSMetaClassDefineReservedUnused(MacIOATA, 18);
    OSMetaClassDefineReservedUnused(MacIOATA, 19);
    OSMetaClassDefineReservedUnused(MacIOATA, 20);


//---------------------------------------------------------------------------

bool 
MacIOATA::init(OSDictionary * properties)
{
    // Initialize instance variables.

    DLOG("MacIOATA::init() starting\n");
   
    if (super::init(properties) == false)
    {
        DLOG("MacIOATA: super::init() failed\n");
        return false;
    }

	isMediaBay = false; // don't know yet
	isBusOnline = true; // if false, it means media bay has ejected.

	_baseAddressMap = 0;
	_dmaBaseMap = 0;
	_DMACursor = 0;
	_descriptors = 0;
	_descriptorsPhysical = 0;
	_devIntSrc = 0;
	_dmaIntSrc = 0;
	_dmaIntExpected = 0;
	_dmaState = MacIOATA::kATADMAInactive;
	_resyncInterrupts = false;

    DLOG("MacIOATA::init() done\n");


    return true;
}



/*---------------------------------------------------------------------------
 *
 *	Override IOService start.
 *
 *	Subclasses should override the start method, call the super::start
 *	first then add interrupt sources and probe their busses for devices 
 *	and create device nubs as needed.
 ---------------------------------------------------------------------------*/

bool 
MacIOATA::start(IOService *provider)
{
     DLOG("MacIOATA::start() begin\n");

 	// call start on the superclass
    if (!super::start( provider))
 	{
        DLOG("MacIOATA: super::start() failed\n");
        return false;
	}

	// Allocate the DMA descriptor area
	if( ! allocDMAChannel() )
	{
        DLOG("MacIOATA:  allocDMAChannel failed\n");
		return false;	
	
	}



	// find the DMA interrupt source and attach it to the command gate
	// if we attach it first, then DMA interrupts will get priority for scheduling.
	if( ! createDMAInterrupt() )
	{
        DLOG("MacIOATA:  createDMAInterrupt failed\n");
		return false;	
	
	}
	// Find the interrupt source and attach it to the command gate
	
	if( ! createDeviceInterrupt() )
	{
        DLOG("MacIOATA:  createDeviceInterrupts failed\n");
		return false;	
	
	}
	


	// enable interrupt sources
	if( !enableInterrupt(0) /*|| !enableInterrupt(1)*/ )
	{
        DLOG("MacIOATA: enable ints failed\n");
		return false;	
	
	}

	// check to see if this is a media-bay socket.
	OSData* nameToMatch  = OSDynamicCast( OSData, provider->getProperty( "AAPL,manually-removable" ) );
	if ( nameToMatch != 0 ) 
	{
		DLOG("MacIOATA got Name property***************************************\n");
		isMediaBay = true;
		
	} else {
	
		DLOG("MacIOATA failed to get Name property!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!.\n");
	}


    DLOG("MacIOATA::start() done\n");
    return true;
}

/*---------------------------------------------------------------------------
 *	free() - the pseudo destructor. Let go of what we don't need anymore.
 *
 *
 ---------------------------------------------------------------------------*/
void
MacIOATA::free()
{

	freeDMAChannel();

	if( _baseAddressMap )
		_baseAddressMap->release();
	
	if(_dmaBaseMap)
		_dmaBaseMap->release();
	
	if(_DMACursor)
		_DMACursor->release();

	if(_dmaIntSrc)
		_dmaIntSrc->release();
	
	if(_devIntSrc)
		_devIntSrc->release();
	
	
	
	super::free();
	
	DLOG( "MacIOATA freed.\n");

}


#pragma mark -initialization-


/*---------------------------------------------------------------------------
 *
 *	Initialize the taskfile pointers to the addresses of the ATA registers
 *	in the MacIO chip.
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::configureTFPointers(void)
{
	
	DLOG("MacIOATA configureTFPointers begin\n");


	// get the task file pointers
	_baseAddressMap = _provider->mapDeviceMemoryWithIndex(0);
	
	if( !_baseAddressMap )
	{
		DLOG("MacIOATA no base map\n");
		return false;
	}

	volatile UInt8* baseAddress = (volatile UInt8*)_baseAddressMap->getVirtualAddress();

	if( !baseAddress )
	{
		DLOG("MacIOATA no base address\n");
		return false;
	}


	// setup the taskfile pointers inherited from the superclass
	// this allows IOATAController to scan for drives during it's start()

	_tfDataReg = (volatile UInt16*) (baseAddress + 0x00);

	_tfFeatureReg = baseAddress + 0x10;
	_tfSCountReg = baseAddress + 0x20;
	_tfSectorNReg = baseAddress + 0x30;
	_tfCylLoReg = baseAddress + 0x40;
	_tfCylHiReg = baseAddress + 0x50;
	_tfSDHReg = baseAddress + 0x60;
	_tfStatusCmdReg = baseAddress + 0x70;  
	_tfAltSDevCReg = baseAddress + 0x160;   

	_timingConfigReg = (volatile UInt32*) (baseAddress + 0x200); 
	
	DLOG("MacIOATA baseAdress = %lx\n", baseAddress);

	DLOG("MacIOATA configureTFPointers end\n");
	
	return true;
}

/*---------------------------------------------------------------------------
 *
 *	allocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::allocDMAChannel(void)
{

	// map the DMA channel control registers into our address space.

	_dmaBaseMap = _provider->mapDeviceMemoryWithIndex(1);
	
	if( !_dmaBaseMap )
	{
		DLOG("MacIOATA no DMA map\n");
		return false;
	}

	// map into the logical address space of the kernel
	_dmaControlReg = (volatile IODBDMAChannelRegisters*)_dmaBaseMap->getVirtualAddress();

	if( !_dmaControlReg )
	{
		DLOG("MacIOATA no DMA address\n");
		return false;
	}

	// Now allocate a playground for channel descriptors
	
	// under ATA-5 and earlier, ATA commands are allowed a maximum of 256 * 512 byte
	// sectors on a single command. This works out to 1 + 32 * 4096 byte chunks of physical
	// memory if the page size is 4K and the memory is completely fragmented, 
	// so we need a total of 33 descriptors, plus a stop and NO-OP command with 
	// to generate the interrupt. This allows for full transfers without any pauses
	// to regenerate a DMA chain.
	
	// allocate 35 descriptors  33 memory commands + 1 No Op, + 1 Stop.
	
	// IODBDMA-Start panics unless memory is aligned on 0x10
	_descriptors = (IODBDMADescriptor*)IOMallocContiguous( sizeof(IODBDMADescriptor) * kATAMaxDMADesc, 
						0x10, 
						&_descriptorsPhysical );
	
	if(	! _descriptors )
	{
		DLOG("MacIOATA alloc descriptors failed\n");
		return false;
	
	}



	_DMACursor = IODBDMAMemoryCursor::withSpecification(0xFFFE, /*64K - 2*/
                                       					kMaxATAXfer  /* Maybe should be unlimited? */
                                     					/*inAlignment - byte aligned by default*/);

	
	if( ! _DMACursor )
	{
		DLOG("MacIOATA alloc DMACursor failed\n");
		return false;
	}


	// fill the chain with stop commands to initialize it.	
	initATADMAChains(_descriptors);
	
	return true;
}


/*---------------------------------------------------------------------------
 *
 *	deallocate memory and resources for the DMA descriptors.
 *
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::freeDMAChannel(void)
{
	
	if( _descriptors )
	{
		// make sure the engine is stopped
		stopDMA();

		// free the descriptor table.
		IOFreeContiguous( (void*) _descriptors, 
		sizeof(IODBDMADescriptor) * kATAMaxDMADesc);
	}

	return true;
}


/*---------------------------------------------------------------------------
 *
 *	connect the device (drive) interrupt to our workloop
 *
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::createDeviceInterrupt(void)
{
	// create a device interrupt source and attach it to the work loop

	_devIntSrc = IOInterruptEventSource::
	interruptEventSource( (OSObject *)this,
	(IOInterruptEventAction) &MacIOATA::deviceInterruptOccurred, _provider, 0); 


	if( !_devIntSrc || _workLoop->addEventSource(_devIntSrc) )
	{
		DLOG("MacIOATA failed create dev intrpt source\n");
		return false;
	}

	_devIntSrc->enable();

	return true;

}

/*---------------------------------------------------------------------------
 *
 *  static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
MacIOATA::deviceInterruptOccurred(OSObject * owner, IOInterruptEventSource *evtSrc, int count)
{
	MacIOATA* self = (MacIOATA*) owner;

	self->handleDeviceInterrupt();


}

/*---------------------------------------------------------------------------
 *
 *	connect the DMA interrupt to our workloop.
 *
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::createDMAInterrupt(void)
{


	// create a dma interrupt source and attach it to the work loop

	_dmaIntSrc = IOInterruptEventSource::
	interruptEventSource( (OSObject *)this,
	(IOInterruptEventAction) &MacIOATA::dmaInterruptOccurred,
	(IOService *) _provider,
		1	 );  // index 1 is the DMA interrupt.

	if( !_dmaIntSrc || _workLoop->addEventSource(_dmaIntSrc) )
	{
		DLOG("MacIOATA failed create dma intrp source\n");
		return false;
	}

	_dmaIntSrc->enable();
	
	return true;
}

/*---------------------------------------------------------------------------
 *
 * static "action" function to connect to our object
 *
 ---------------------------------------------------------------------------*/
void 
MacIOATA::dmaInterruptOccurred(OSObject * owner, IOInterruptEventSource * evtSrc, int count)
{

	MacIOATA* self = (MacIOATA*) owner;
	
	self->processDMAInterrupt( );	


}



#pragma mark -DMA Interface-

/*---------------------------------------------------------------------------
 *
 * Subclasses should take necessary action to create DMA channel programs, 
 * for the current memory descriptor in _currentCommand and activate the 
 * the DMA hardware
 ---------------------------------------------------------------------------*/
IOReturn
MacIOATA::startDMA( void )
{

	IOReturn err = kATANoErr;

	// first make sure the engine is stopped.
	stopDMA();
	
	
	// reality check the memory descriptor in the current command
	
	// state flag
	_dmaState = kATADMAStarting;
	
	// create the channel commands
	err = createChannelCommands();
	
	if(	err )
	{
	
		DLOG("MacIOATA error createChannelCmds err = %ld\n", (long int)err);
		return err;
	
	}
	
	// fire the engine
	activateDMAEngine();
	
	return err;
	

}




/*---------------------------------------------------------------------------
 * Subclasses should take all actions necesary to safely shutdown DMA engines
 * in any state of activity, whether finished, pending or stopped. Calling 
 * this function must be harmless reguardless of the state of the engine.
 *
 ---------------------------------------------------------------------------*/
 IOReturn
MacIOATA::stopDMA( void )
{

	if(_dmaState != kATADMAInactive)
		shutDownATADMA();
	
	
	_dmaState = kATADMAInactive;
	return kATANoErr;

}

#pragma mark -DMA Implementation-


//----------------------------------------------------------------------------------------
//	Function:		InitATADMAChains
//	Description:	Initializes the chains with STOP commands.
//
//	Input:			Pointer to the DBDMA descriptor area: descPtr
//	
//	Output:			None
//----------------------------------------------------------------------------------------
void	
MacIOATA::initATADMAChains (IODBDMADescriptor* descPtr)
{
	int i;
	
	/* Initialize the data-transfer DBDMA channel command descriptors. */
	/* These descriptors are altered to specify the desired transfer. */
	for (i = 0; i < kATAMaxDMADesc; i++)
	{
		IOMakeDBDMADescriptor(	descPtr,
								kdbdmaStop,
								kdbdmaKeyStream0,
								kdbdmaIntNever,
								kdbdmaBranchNever,
								kdbdmaWaitNever,
								0,
								0);
		descPtr++;
	}
}

/*---------------------------------------------------------------------------
 *
 *	Process the DMA interrupt. We are either done, need more DMA, or an error
 * occurred
 *
 ---------------------------------------------------------------------------*/
void
MacIOATA::processDMAInterrupt (void)
{
	if( isBusOnline == false)
	{
		return;
	}



	if( _dmaIntExpected == false )
	{
		DLOG("MacIOATA DMA Int not expected\n");
		return;
	}
	// Make certain that the DMA state flags are consistent with machine state.
	_dmaIntExpected = false;

	// Bytes transferred by this DMA pass
	IOByteCount   byteCount = 0; 
	
	/* DMA is either finished or has erred. Decide which. */
	switch (_dmaState)
	{
		case kATADMAStatus:
		case kATADMAActive:
			
			//ATARecordEventMACRO(kAIMTapeName,'DMA ','stat','act1');
			
			/* DMA is running and we've gotten an interrupt. Check result. */
			if (scanATADMAChain(&byteCount))
						/* ataDmaState may be altered by ScanATADMAChain. */
				break; /* out of switch. Status is able to be interpreted. */
		default:
			/* We weren't expecting this interrupt, or the hardware shouldn't have sent it. */
			
			ATARecordEventMACRO('MacI','DMA ','Uxpt','Intr');
			shutDownATADMA();
			_dmaState = kATADMAError;
			return;
			break;
	}	
		
	// update the actual byte count for this pass.

	_currentCommand->setActualTransfer(_currentCommand->getActualTransfer() + byteCount);
	
	/* Now the deciphering of the results of the data transfer: A valid interrupt
	 * has occurred for one of 3 cases: 1) Fatal DMA error, 2) end of DMA, 3) more
	 *	DMA required.
	 */

	switch (_dmaState)
	{
		case kATADMAActive:

			/* 
			 * DMA transfer only partially complete. The current chain of descriptors
			 * (CCs) has completed correctly. Additional transfer needed.
			 * Note: Clearing the DBDMA "run" bit has the effect of clearing
			 * internal data FIFOs. To prevent data loss on  multi-pass DMA operations,
			 * DMA must not be stopped until the entire I/O is complete.
			 */
			
			if (createChannelCommands())
			{
				ATARecordEventMACRO('MacI','DMA ','chan','err1');
				/* An OSErr has been reported - likely from a memory manager call. */
				_dmaState = kATADMAError; /* Take the blame for now. */
				return;
			}
				

			ATARecordEventMACRO('MacI',' DMA','rest','art.');
			//start the engine for another DMA pass.
			
			activateDMAEngine();
			
			break;
		
		case kATADMAStatus:
			/* Flag the DMA transfer as completed. */
			ATARecordEventMACRO('MacI','DMA ','stat','good');
			_dmaState = kATADMAComplete;
			stopDMA ();

			// may need some handling here in the event that the DMA int arrives
			// after the device interrupt.
			
			break;

		case kATADMAError:
			/*
			 *	The drive is expecting additional data transfer, but the DMA has croaked. 
			 *	For now, let the device interrupt timeout happen.
			 */
			ATARecordEventMACRO('MacI','DMA ','chan','err2');
			
			stopDMA ();
			break;
		default:
			break;
	}


	// if we meet a certain edge case condition, the drive may have emptied it's data 
	// into the controller's fifo, but the DMA engine has not written data into system 
	// memory yet, the drive may assert IRQ prior to the DMA engine. In this case, we defer the
	// handling of the device interrupt until the DMA engine signals completion.
	if( _resyncInterrupts == true )
	{
		_resyncInterrupts = false;
		handleDeviceInterrupt();
	}

}


void
MacIOATA::stopDMAEngine(void)
{

	/* Not doing DMA any more. Set up to ignore any DMA interrupts. */
	/* Leave other bits intact for error detection. */


	IODBDMAStop(_dmaControlReg);

	//IOSetDBDMAChannelControl( _dmaControlReg, IOClearDBDMAChannelControlBits(kdbdmaRun));
	
	//while (IOGetDBDMAChannelStatus(_dmaControlReg) & kdbdmaActive)
	//	{;}


}



/*----------------------------------------------------------------------------------------
//	Function:		activateDMAEngine
//	Description:	Activate the DBDMA on the ATA bus associated with current device.
					engine will begin executing the command chain already programmed.
//	Input:			None
//	Output:			None
//----------------------------------------------------------------------------------------*/

void			
MacIOATA::activateDMAEngine(void)
{

	if( IOGetDBDMAChannelStatus( _dmaControlReg) & kdbdmaActive )
	{
		/* For multiple DMA chain execution, don't stop the DMA or the FIFOs lose data.*/
		/* If DMA is active already (stray from an error?), shut it down cleanly. */
		shutDownATADMA();
	}
	

    IOSetDBDMACommandPtr( _dmaControlReg, (unsigned int) _descriptorsPhysical);


	/* Blastoff! */
	//ATARecordEventMACRO(kAIMTapeName,' dma','true','StDM');
	_dmaIntExpected = true;
	
	// IODBDMAStart will flush the FIFO by clearing the run-bit, causing multiple chain execution 
	// to fail by losing whatever bytes may have accumulated in the ATA fifo.
	
	//IODBDMAStart(_dmaControlReg, (volatile IODBDMADescriptor *)_descriptorsPhysical);
	IOSetDBDMAChannelControl(_dmaControlReg, IOSetDBDMAChannelControlBits( kdbdmaRun | kdbdmaWake));
}

void	
MacIOATA::shutDownATADMA (void)
{

	//ATARecordEventMACRO(kAIMTapeName,'Shut','Down',' DMA');

	// Disable interrupts while we flush the chain
	_dmaIntSrc->disable();
	stopDMAEngine();

	// Make certain that the DMA state flags are consistent with machine state.
	_dmaIntExpected = false;

	// set the state semaphore 
	_dmaState = kATADMAInactive;

	// Reenable interrupts

	_dmaIntSrc->enable();

}


/*---------------------------------------------------------------------------
 *
 *	create the DMA channel commands.
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
MacIOATA::createChannelCommands(void)
{
	IOMemoryDescriptor* descriptor = _currentCommand->getBuffer();

	if( ! descriptor )
	{
	
		DLOG("drvMacIO nil buffer!\n");
		return -1;
	}

	// calculate remaining bytes in this transfer

	IOByteCount bytesRemaining = _currentCommand->getByteCount() 
								- _currentCommand->getActualTransfer();



	// calculate position pointer
	IOByteCount xfrPosition = _currentCommand->getPosition() + 
							_currentCommand->getActualTransfer();

	IOByteCount  transferSize = 0; 

	// have the DMA cursor fill out the addresses in the CC table
	// it will return the number of descriptors consumed.

	UInt32 segmentCount = _DMACursor->getPhysicalSegments(
										descriptor,
				       					xfrPosition,
				       					_descriptors,
				     					kATAXferDMADesc,
				     					bytesRemaining,  // limit to the requested number of bytes in the event the descriptors is larger
				       					&transferSize);
				       					
	if( transferSize > bytesRemaining)
	{
		DLOG("drvMacIO DMA too long!!!\n");
		return -1;	
	
	}

	if( segmentCount == 0)
	{
		DLOG("drvMacIO seg count 0!!!\n");
		return -1;	
	
	}


	// check if the xfer satisfies the needed size
	if( transferSize < bytesRemaining )
	{
		// indicate we need to do more DMA when the interrupt happens
	
		_dmaState = kATADMAActive;
		DLOG("MacIOATA will make two passes\n");
	
	} else {
		
		// transfer is satisfied and only need to check status on interrupt.
		_dmaState = kATADMAStatus;
		DLOG("MacIOATA will make one pass\n");
	
	}

	UInt32 command = kdbdmaOutputMore;

	if( _currentCommand->getFlags() & mATAFlagIORead)
	{
		command = kdbdmaInputMore;
	
	}

	DLOG("MacIOATA making CC chain for %ld segs for xfersize %ld\n", segmentCount, transferSize);

	// now walk the descriptor chain and insert the commands
	for( UInt32 i = 0; i < segmentCount; i++)
	{
	
		IODBDMADescriptor* currDesc = & (_descriptors[i]);
		
		UInt32 addr = IOGetCCAddress(currDesc);
		UInt32 count =  IOGetCCOperation(currDesc) & kdbdmaReqCountMask;
		OSSynchronizeIO();
		
		IOMakeDBDMADescriptor(currDesc,
								command,
								kdbdmaKeyStream0,
								kdbdmaIntNever,
								kdbdmaBranchNever,
								kdbdmaWaitNever,
								count,
								addr);
	
		DLOG("macIOATA desc# %ld at %x \n", i, currDesc );
		DLOG("addr = %lx  count = %lx  \n", addr, count );
		
		DLOG("%lx  %lx  %lx  %lx\n", currDesc->operation, currDesc->address ,currDesc->cmdDep ,currDesc->result );
		
	}

	// insert a NOP + interrupt. after the last data command
	IOMakeDBDMADescriptor(&(_descriptors[segmentCount]),
						kdbdmaNop,
						kdbdmaKeyStream0,
						kdbdmaIntAlways,
						kdbdmaBranchNever,
						kdbdmaWaitNever,
						0,
						0);


	// insert a stop after the NOP command
	IOMakeDBDMADescriptor(&(_descriptors[segmentCount + 1]),
						kdbdmaStop,
						kdbdmaKeyStream0,
						kdbdmaIntNever,
						kdbdmaBranchNever,
						kdbdmaWaitNever,
						0,
						0);


	// chain is now ready for execution.

	return kATANoErr;

}

bool	
MacIOATA::scanATADMAChain (IOByteCount* byteCount)
{
	volatile IODBDMADescriptor*	descPtr 		= _descriptors; 
	bool				validState		= true;
	
	*byteCount = 0;	/* No bytes confirmed this DMA pass. */

	UInt32 descIndex = 0;
	/*
	 *	Parse the chain for completion status. Normal completion is
	 *	indicated by all expected CC status words contain just the
	 *	"run" and "active" bits, indicating that the DMA has executed the
	 * 	chain element and updated the status. 
	 */

	 while ((IOGetCCOperation(descPtr) & kdbdmaCmdMask) != STOP_CMD)
	 {
		/* For speed, check the normal case - "run" and "active" bits  and residual count == 0 */
		if ( (IOGetCCResult(descPtr) & 0x8C000000) == 0x84000000 )
		{
			/* The DMA has completed this channel command.*/
			
			*byteCount += (IOGetCCOperation(descPtr) & 0x0000FFFF); // low 16 bits are request count
			descPtr++;  /* Advance pointer to the next DMA command */
			descIndex++;
			continue;
		}

		/* Get here if something's wrong with DMA execution. See whether hardware 
		 * is telling us something directly, or whether something amiss is noted
		 * in the status written to memory.
		 */
		
		if ((IOGetDBDMAChannelStatus(_dmaControlReg) & kdbdmaStatusDead) || ((IOGetCCResult(descPtr) & kdbdmaStatusRun)))
		{
			/*
			 *	Dead means fatal DMA error - hardware claims it failed. Report error 
			 *	if dead or if the DMA was running when the interrupt occurred.
			 */
			
			DLOG("MacIOATA DMA error state\n");
			DLOG("MacIOATA desc# %ld byte count %ld\n",descIndex, *byteCount); 
			DLOG("ChanStat= %lx   CCResult= %lx\n", IOGetDBDMAChannelStatus(_dmaControlReg), IOGetCCResult(descPtr) );
			
			_dmaState = kATADMAError;
			break;	/* out of do/while loop */
		
		}  else {
		
 			/*
			 *	The DMA status indicates that command descriptor execution is not
			 *	responsible for the interrupt. Another case to ignore.
			 */
		
			DLOG("MacIOATA DMA Invalid state\n");
			validState = false;
			break; /* Ignore the interrupt */
		}
				 
		
				 
	}	/* end while !STOP_CMD */
		
	/* Return to caller, indicating whether dmaState is valid. */
	DLOG("MacIOATA desc# %ld byte count %ld\n",descIndex, *byteCount); 
	
	return validState;
} 



/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here so we can make sure that the DMA has
 * processed in the event the interrupts arrive out of order for some reason.
 *
 ---------------------------------------------------------------------------*/
IOReturn
MacIOATA::handleDeviceInterrupt(void)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}
	// if we meet a certain edge case condition, the drive may have emptied it's data 
	// into the controller's fifo, but the DMA engine has not written data into system 
	// memory yet, the drive may assert IRQ prior to the DMA engine. In this case, we defer the
	// handling of the device interrupt until the DMA engine signals completion.

	if( _dmaIntExpected == true 
		&& _currentCommand->state == IOATAController::kATAStatus)	
	{	
		DLOG("MacIOATA, DMA int out of order\n");

		// read the device's status register in order to clear the IRQ assertion
		volatile UInt8 status = *_tfStatusCmdReg;
		OSSynchronizeIO();
		//if an error or check condition bit is set then DMA isn't going to happen at this 
		// point. go ahead and allow the superclass to process it.
		if( status & 0x01 )
		{
			return super::handleDeviceInterrupt();
		}
		_resyncInterrupts = true;
		return kATANoErr;
	}

	return super::handleDeviceInterrupt();
	
}



/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here to allow for reporting of DMA errs
 *
 ---------------------------------------------------------------------------*/
IOReturn	
MacIOATA::asyncStatus(void)
{

	IOReturn err = super::asyncStatus();
	
	if( _dmaState == kATADMAError)
	{
	
		err = kATADMAErr;
	
	}
	return err;

}


/*---------------------------------------------------------------------------
 *
 *	handleDeviceInterrupt - overriden here to allow clean up of DMA resyncInterrupt flag
 *
 ---------------------------------------------------------------------------*/
void
MacIOATA::handleTimeout( void )
{
	if( isBusOnline == false)
	{
		return;
	}

	_resyncInterrupts = false;
	super::handleTimeout();

}

// media bay support

IOReturn 
MacIOATA::message (UInt32 type, IOService* provider, void* argument)
{

	switch( type )
	{
		case kATARemovedEvent:
		DLOG( "MacIOATA got removed event.\n");
		// mark the bus as dead.
		if(isBusOnline == true)
		{
			isBusOnline = false;
			// lock the queue, don't dispatch immediates or anything else.
			_queueState = IOATAController::kQueueLocked;
			// disable the interrupt source(s) and timers
			_dmaIntSrc->disable();
			_devIntSrc->disable();
			stopTimer();
			
			_workLoop->removeEventSource(_dmaIntSrc);
			_workLoop->removeEventSource(_devIntSrc);
			_workLoop->removeEventSource(_timer);
			
			//_provider->unregisterInterrupt(0);
			//_provider->unregisterInterrupt(1);
			// flush any commands in the queue
			handleQueueFlush();
			
			// if there's a command active then call through the command gate
			// and clean it up from inside the workloop.
			// 

			if( _currentCommand != 0)
			{
			
				DLOG( "MacIOATA Calling cleanup bus.\n");
				
					_cmdGate->runAction( (IOCommandGate::Action) 
						&MacIOATA::cleanUpAction,
            			0, 			// arg 0
            			0, 		// arg 1
            			0, 0);						// arg2 arg 3

			
			
			}
			_workLoop->removeEventSource(_cmdGate);
			DLOG( "MacIOATA notify the clients.\n");			
			terminate();
			
		}
		break;
	
	
		default:
		
		DLOG( "MacIOATA got some other event = %d\n", (int) type);
		return super::message(type, provider, argument);
		break;
	}


	return kATANoErr;

}


/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
IOReturn
MacIOATA::handleQueueFlush( void )
{

	UInt32 savedQstate = _queueState;

	_queueState = IOATAController::kQueueLocked;

	IOReturn errPerCommand = kIOReturnError;

	if( isBusOnline == false )
	{
	
		errPerCommand = kIOReturnOffline;
	
	}

	IOATABusCommand* cmdPtr = 0;

	while( cmdPtr = dequeueFirstCommand() )
	{
	
		cmdPtr->setResult(errPerCommand);
		cmdPtr->executeCallback();
	
	}

	_queueState = savedQstate;

	return kATANoErr;
}

/*---------------------------------------------------------------------------
 *
 *
 *
 *
 ---------------------------------------------------------------------------*/
bool
MacIOATA::checkTimeout( void )
{
	if( isBusOnline == false )
	{
		// signal a timeout if we are within the workloop
		return true;
	
	} 

	return super::checkTimeout();
}

/*---------------------------------------------------------------------------
 *
 *	The main call which puts something on the work loop
 *
 ---------------------------------------------------------------------------*/

IOReturn 
MacIOATA::executeCommand(IOATADevice* nub, IOATABusCommand* command)
{
	if( isBusOnline == false)
	{
		return kIOReturnOffline;
	}
		
	return super::executeCommand(nub, command);

}


// called through the commandGate when I get a notification that a media bay has gone away

void
MacIOATA::cleanUpAction(OSObject * owner,
                                               void *     arg0,
                                               void *     arg1,
                                               void *  /* arg2 */,
                                               void *  /* arg3 */)
{


	MacIOATA* self = (MacIOATA*) owner;
	self->cleanUpBus();
}


void
MacIOATA::cleanUpBus(void)
{
	if( _currentCommand != 0)
	{
	
		_currentCommand->setResult(kIOReturnOffline);
		_currentCommand->executeCallback();
		_currentCommand = 0;
	}

}
IOReturn
MacIOATA::handleBusReset(void)
{
//	if( _devIntSrc )
//		_devIntSrc->disable();
		
	IOReturn result = super::handleBusReset();
	
//	if( _devIntSrc )
//		_devIntSrc->enable();

	return result;

}

#endif // __ppc__
