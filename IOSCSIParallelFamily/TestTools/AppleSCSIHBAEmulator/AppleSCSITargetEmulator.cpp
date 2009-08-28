/*
  File: AppleSCSITargetEmulator.h

  Contains:

  Version: 1.0.0

  Copyright: Copyright (c) 2007 by Apple Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include "AppleSCSITargetEmulator.h"
#include "AppleSCSILogicalUnitEmulator.h"
#include "AppleSCSIPDT00Emulator.h"
#include "AppleSCSIPDT03Emulator.h"

#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/IOLocks.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"TargetEmulator"

#if DEBUG
#define TARGET_EMULATOR_DEBUGGING_LEVEL						4
#endif

#include "DebugSupport.h"

#if ( TARGET_EMULATOR_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)		
#endif

#if ( TARGET_EMULATOR_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x; IOSleep(1)
#else
#define ERROR_LOG(x)		
#endif

#if ( TARGET_EMULATOR_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep(1)
#else
#define STATUS_LOG(x)
#endif

#if ( TARGET_EMULATOR_DEBUGGING_LEVEL >= 4 )
#define COMMAND_LOG(x)		IOLog x; IOSleep(1)
#else
#define COMMAND_LOG(x)
#endif


#define super OSObject
OSDefineMetaClassAndStructors ( AppleSCSITargetEmulator, OSObject );


//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static SInt32
CompareLUNs (
	const OSMetaClassBase * obj1,
	const OSMetaClassBase * obj2,
	void * ref );


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

SCSICmd_INQUIRY_StandardData gInquiryData =
{
	kINQUIRY_PERIPHERAL_QUALIFIER_NotSupported | kINQUIRY_PERIPHERAL_TYPE_UnknownOrNoDeviceType,	// PERIPHERAL_DEVICE_TYPE
	0,	// RMB;
	5,	// VERSION
	2,	// RESPONSE_DATA_FORMAT
	sizeof ( SCSICmd_INQUIRY_StandardData ) - 5,	// ADDITIONAL_LENGTH
	0,	// SCCSReserved
	0,	// flags1
	0,	// flags2
	"APPLE  ",
	"SCSI Emulator  ",
	"1.0",
};

SCSI_Sense_Data
AppleSCSITargetEmulator::sBadLUNSenseData =
{
	/* VALID_RESPONSE_CODE */				0x80 | kSENSE_RESPONSE_CODE_Current_Errors,
	/* SEGMENT_NUMBER */					0x00, // Obsolete
	/* SENSE_KEY */							kSENSE_KEY_ILLEGAL_REQUEST,
	/* INFORMATION_1 */						0x00,
	/* INFORMATION_2 */						0x00,
	/* INFORMATION_3 */						0x00,
	/* INFORMATION_4 */						0x00,
	/* ADDITIONAL_SENSE_LENGTH */			0x00,
	/* COMMAND_SPECIFIC_INFORMATION_1 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_2 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_3 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_4 */	0x00,
	/* ADDITIONAL_SENSE_CODE */				0x25, // LOGICAL UNIT NOT SUPPORTED
	/* ADDITIONAL_SENSE_CODE_QUALIFIER */	0x00,
	/* FIELD_REPLACEABLE_UNIT_CODE */		0x00,
	/* SKSV_SENSE_KEY_SPECIFIC_MSB */		0x00,
	/* SENSE_KEY_SPECIFIC_MID */			0x00,
	/* SENSE_KEY_SPECIFIC_LSB */			0x00
};

SCSI_Sense_Data
AppleSCSITargetEmulator::sLUNInventoryChangedData =
{
	/* VALID_RESPONSE_CODE */				0x80 | kSENSE_RESPONSE_CODE_Current_Errors,
	/* SEGMENT_NUMBER */					0x00, // Obsolete
	/* SENSE_KEY */							kSENSE_KEY_UNIT_ATTENTION,
	/* INFORMATION_1 */						0x00,
	/* INFORMATION_2 */						0x00,
	/* INFORMATION_3 */						0x00,
	/* INFORMATION_4 */						0x00,
	/* ADDITIONAL_SENSE_LENGTH */			0x00,
	/* COMMAND_SPECIFIC_INFORMATION_1 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_2 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_3 */	0x00,
	/* COMMAND_SPECIFIC_INFORMATION_4 */	0x00,
	/* ADDITIONAL_SENSE_CODE */				0x3F, // LOGICAL UNIT NOT SUPPORTED
	/* ADDITIONAL_SENSE_CODE_QUALIFIER */	0x0E,
	/* FIELD_REPLACEABLE_UNIT_CODE */		0x00,
	/* SKSV_SENSE_KEY_SPECIFIC_MSB */		0x00,
	/* SENSE_KEY_SPECIFIC_MID */			0x00,
	/* SENSE_KEY_SPECIFIC_LSB */			0x00
};


//-----------------------------------------------------------------------------
//	Create
//-----------------------------------------------------------------------------

AppleSCSITargetEmulator *
AppleSCSITargetEmulator::Create (
	SCSITargetIdentifier 		targetID )
{
	
	AppleSCSITargetEmulator *	emulator 	= NULL;
	bool						result		= false;

	STATUS_LOG ( ( "AppleSCSITargetEmulator::Create, targetID = %qd\n", targetID ) );
	
	emulator = OSTypeAlloc ( AppleSCSITargetEmulator );
	require_nonzero ( emulator, ErrorExit );
	
	result = emulator->Init ( targetID );
	require ( result, ReleaseEmulator );
	
	return emulator;
	
	
ReleaseEmulator:
	
	
	emulator->release ( );
	
	
ErrorExit:
	
	
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	Init
//-----------------------------------------------------------------------------

bool
AppleSCSITargetEmulator::Init (
	SCSITargetIdentifier 		targetID )
{
	
	bool						result		= false;
	AppleSCSIPDT03Emulator *	emulator	= NULL;

	STATUS_LOG ( ( "AppleSCSITargetEmulator::Init, targetID = %qd\n", targetID ) );
	
	result = super::init ( );
	require ( result, ErrorExit );
	
	fTargetID = targetID;
	
	fLock = IOLockAlloc ( );
	require_nonzero ( fLock, ErrorExit );
	
	fLUNs = OSOrderedSet::withCapacity ( 16, CompareLUNs );
	require_nonzero ( fLUNs, ReleaseLock );
	
	// Allocate LUN0 (PDT 03h device).
	emulator = OSTypeAlloc ( AppleSCSIPDT03Emulator );
	require_nonzero ( emulator, ReleaseSet );
	
	emulator->SetLogicalUnitNumber ( 0 );
	fLUNs->setObject ( emulator );
	emulator->release ( );
	
	fLUNDataAvailable		= kREPORT_LUNS_HeaderSize + sizeof ( SCSICmd_REPORT_LUNS_LUN_ENTRY );
	fLUNReportBufferSize	= PAGE_SIZE;
	
	// Start with a page of memory. Grow as required later.
	fLUNReportBuffer = ( SCSICmd_REPORT_LUNS_Header * ) IOMalloc ( fLUNReportBufferSize );
	require_nonzero ( fLUNReportBuffer, ReleaseSet );
	
	bzero ( fLUNReportBuffer, fLUNReportBufferSize );
	
	fLUNReportBuffer->LUN_LIST_LENGTH = OSSwapHostToBigInt32 ( sizeof ( SCSICmd_REPORT_LUNS_LUN_ENTRY ) );
	fLUNReportBuffer->LUN[0].FIRST_LEVEL_ADDRESSING = OSSwapHostToBigInt16 ( 0 );
	
	return true;
	
	
ReleaseSet:
	
	
	fLUNs->release ( );
	fLUNs = NULL;
	
	
ReleaseLock:
	
	
	IOLockFree ( fLock );
	fLock = NULL;
	
	
ErrorExit:
	
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	free
//-----------------------------------------------------------------------------

void
AppleSCSITargetEmulator::free ( void )
{
	
	STATUS_LOG ( ( "AppleSCSITargetEmulator::free\n" ) );
	
	if ( fLUNs != NULL )
	{
		
		fLUNs->release ( );
		fLUNs = NULL;
		
	}
	
	if ( fLUNReportBuffer != NULL )
	{
		
		IOFree ( fLUNReportBuffer, fLUNReportBufferSize );
		fLUNReportBuffer = NULL;
		fLUNReportBufferSize = 0;
		
	}
	
	IOLockFree ( fLock );
	fLock = NULL;
	
	super::free ( );
	
}


//-----------------------------------------------------------------------------
//	AddLogicalUnit
//-----------------------------------------------------------------------------

bool
AppleSCSITargetEmulator::AddLogicalUnit (
	SCSILogicalUnitNumber	logicalUnit,
	UInt64					capacity,
	IOMemoryDescriptor * 	inquiryBuffer,
	IOMemoryDescriptor * 	inquiryPage00Buffer,
	IOMemoryDescriptor * 	inquiryPage80Buffer,
	IOMemoryDescriptor * 	inquiryPage83Buffer )
{
	
	AppleSCSIPDT00Emulator *		emulator	= NULL;
	bool							result		= false;
	bool							needsWakeup = false;		
	UInt32							bufferSize	= 0;
	
	STATUS_LOG ( ( "+AppleSCSITargetEmulator::AddLogicalUnit, logicalUnit = %qd, capacity = %qd\n", logicalUnit, capacity ) );
	
	require ( ( logicalUnit < 16384 ), ErrorExit );
	require ( ( logicalUnit > 0 ), ErrorExit );
	
	emulator = AppleSCSIPDT00Emulator::WithCapacity ( capacity );
	require_nonzero ( emulator, ErrorExit );
	
	emulator->SetLogicalUnitNumber ( logicalUnit );
	
	result = emulator->SetDeviceBuffers ( inquiryBuffer, inquiryPage00Buffer, inquiryPage80Buffer, inquiryPage83Buffer );
	require ( result, ReleaseEmulator );
	
	IOLockLock ( fLock );
	
	if ( fState & kTargetStateChangeActiveMask )
	{
		
		fState |= kTargetStateChangeActiveWaitMask;
		IOLockSleep ( fLock, &fState, THREAD_UNINT );
		
	}
	
	if ( ( fLUNReportBufferSize - fLUNDataAvailable ) < sizeof ( SCSICmd_REPORT_LUNS_LUN_ENTRY ) )
	{
		
		void *	buffer = NULL;
		
		// Double the buffer size.
		bufferSize = fLUNReportBufferSize << 1;
		
		// Mark that we're changing state. Anyone who grabs the lock after we
		// drop it will be forced to sleep and wait until this state change is done.
		fState |= kTargetStateChangeActiveMask;
		
		// Drop the lock since IOMalloc might block.
		IOLockUnlock ( fLock );
		
		// Allocate the new buffer.
		buffer = IOMalloc ( bufferSize );
		
		// Reacquire the lock.
		IOLockLock ( fLock );
		
		fState &= ~kTargetStateChangeActiveMask;
		
		if ( buffer != NULL )
		{
			
			// Free the old buffer.
			IOFree ( fLUNReportBuffer, fLUNReportBufferSize );
			
			// Set the new buffer.
			fLUNReportBuffer 		= ( SCSICmd_REPORT_LUNS_Header * ) buffer;
			fLUNReportBufferSize 	= bufferSize;
			
		}
		
		else
		{
			
			if ( fState & kTargetStateChangeActiveWaitMask )
			{
				needsWakeup = true;
			}
			
			IOLockUnlock ( fLock );
			
			if ( needsWakeup == true )
			{
				IOLockWakeup ( fLock, &fState, false );
			}
			
			goto ReleaseEmulator;
			
		}
		
	}
	
	// Add this LUN emulator to the set.
	fLUNs->setObject ( emulator );
	
	// Rebuild the list.
	RebuildListOfLUNs ( );
	
	if ( fState & kTargetStateChangeActiveWaitMask )
	{
		needsWakeup = true;
	}
	
	IOLockUnlock ( fLock );
	
	if ( needsWakeup == true )
	{
		IOLockWakeup ( fLock, &fState, false );
	}
	
	emulator->release ( );
	
	STATUS_LOG ( ( "-AppleSCSITargetEmulator::AddLogicalUnit\n" ) );
	
	return result;
	
	
ReleaseEmulator:
	
	
	emulator->release ( );
	emulator = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-AppleSCSITargetEmulator::AddLogicalUnit failed!!!\n" ) );
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	RemoveLogicalUnit
//-----------------------------------------------------------------------------

void
AppleSCSITargetEmulator::RemoveLogicalUnit (
	SCSILogicalUnitNumber		logicalUnitNumber )
{
	
	AppleSCSILogicalUnitEmulator *	LUN		= NULL;
	int								index	= 0;
	int								count	= 0;

	STATUS_LOG ( ( "AppleSCSITargetEmulator::RemoveLogicalUnit, logicalUnitNumber = %qd\n", logicalUnitNumber ) );
	
	IOLockLock ( fLock );
	
	count = fLUNs->getCount ( );

	STATUS_LOG ( ( "count = %d\n", count ) );
	
	for ( index = 0; index < count; index++ )
	{
		
		LUN = ( AppleSCSILogicalUnitEmulator * ) fLUNs->getObject ( index );
		
		STATUS_LOG ( ( "LUN->GetLogicalUnitNumber ( ) = %qd\n", LUN->GetLogicalUnitNumber ( ) ) );
		
		if ( LUN->GetLogicalUnitNumber ( ) == logicalUnitNumber )
		{
			
			fLUNs->removeObject ( LUN );
			break;
			
		}
		
	}
	
	// Rebuild the list.
	RebuildListOfLUNs ( );
	
	IOLockUnlock ( fLock );
	
}


//-----------------------------------------------------------------------------
//	RebuildListOfLUNs
//-----------------------------------------------------------------------------
// MUST BE CALLED WITH fLock HELD.

void
AppleSCSITargetEmulator::RebuildListOfLUNs ( void )
{
	
	UInt32	index		= 0;
	UInt32	count 		= 0;
	UInt32	bufferSize 	= 0;

	STATUS_LOG ( ( "AppleSCSITargetEmulator::RebuildListOfLUNs\n" ) );
	
	// Get the count of emulators (including this new one).
	count = fLUNs->getCount ( );
	
	// Set the bufferSize.
	bufferSize = count * sizeof ( SCSICmd_REPORT_LUNS_LUN_ENTRY );
	
	// Zero the buffer.
	bzero ( fLUNReportBuffer, fLUNReportBufferSize );
	
	// Repopulate the buffer with correct LUN data.	
	fLUNReportBuffer->LUN_LIST_LENGTH = OSSwapHostToBigInt32 ( bufferSize );

#if USE_LUN_BYTES

	// Repopulate the buffer with correct LUN inventory.
	for ( index = 0; index < count; index++ )
	{
		
		SCSILogicalUnitBytes			logicalUnitBytes	= { 0 };
		AppleSCSILogicalUnitEmulator *	LUN					= NULL;
		
		LUN = ( AppleSCSILogicalUnitEmulator * ) fLUNs->getObject ( index );
		LUN->GetLogicalUnitBytes ( &logicalUnitBytes );
		
		bcopy ( logicalUnitBytes, &fLUNReportBuffer->LUN[index], sizeof ( SCSILogicalUnitBytes ) );
		
		STATUS_LOG ( ( "logicalUnitBytes = 0x%02x 0x%02x, Data = 0x%04x\n",
						logicalUnitBytes[0], logicalUnitBytes[1],
						fLUNReportBuffer->LUN[index].FIRST_LEVEL_ADDRESSING ) );
		
	}

#else
	
	// Repopulate the buffer with correct LUN inventory.
	for ( index = 0; index < count; index++ )
	{
		
		SCSILogicalUnitNumber			logicalUnit	= 0;
		AppleSCSILogicalUnitEmulator *	LUN			= NULL;
		
		LUN = ( AppleSCSILogicalUnitEmulator * ) fLUNs->getObject ( index );
		logicalUnit = LUN->GetLogicalUnitNumber ( );
		
		fLUNReportBuffer->LUN[index].FIRST_LEVEL_ADDRESSING = OSSwapHostToBigInt16 ( logicalUnit );
		
	}

#endif	/* USE_LUN_BYTES */
	
	fLUNDataAvailable = bufferSize + kREPORT_LUNS_HeaderSize;
	
	fLUNInventoryChanged = true;
	
}


#if USE_LUN_BYTES

//-----------------------------------------------------------------------------
//	SendCommand
//-----------------------------------------------------------------------------

int
AppleSCSITargetEmulator::SendCommand (
	UInt8 *					cdb,
	UInt8 					cdbLen,
	IOMemoryDescriptor *	dataDesc,
	UInt64 *				dataLen,
	SCSILogicalUnitBytes	logicalUnitBytes,
	SCSITaskStatus *		scsiStatus,
	SCSI_Sense_Data *		senseBuffer,
	UInt8 *					senseBufferLen )
{
	
	int		result				= 0;
	bool	processedCommand	= false;
	
	IOLockLock ( fLock );
	
	if ( fLUNInventoryChanged == true )
	{
		
		ERROR_LOG ( ( "Generating UNIT_ATTENTION for LUN inventory change\n" ) );
		fLUNInventoryChanged = false;
		
		*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
		if ( senseBuffer != NULL )
		{
			
			UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
			
			bzero ( senseBuffer, *senseBufferLen );
			bcopy ( &sLUNInventoryChangedData, senseBuffer, amount );
			
			*senseBufferLen = amount;
			
		}
		
		processedCommand = true;
		
	}
	
	IOLockUnlock ( fLock );
	
	if ( processedCommand == false )
	{
		
		if ( cdb[0] == kSCSICmd_REPORT_LUNS )
		{
			
			COMMAND_LOG ( ( "REPORT_LUNS requested = %qd\n", *dataLen ) );
			
			if ( fLUNReportBuffer != NULL )
			{
				
				*dataLen = min ( fLUNDataAvailable, *dataLen );
				dataDesc->writeBytes ( 0, fLUNReportBuffer, *dataLen );
				
				COMMAND_LOG ( ( "REPORT_LUNS realized = %qd\n", *dataLen ) );
				
				*scsiStatus = kSCSITaskStatus_GOOD;
			
			}
			
			processedCommand = true;
			
		}
		
		else
		{
			
			UInt32								index		= 0;
			UInt32								count		= 0;
			AppleSCSILogicalUnitEmulator *		LUN			= NULL;
	
			IOLockLock ( fLock );
			
			count = fLUNs->getCount ( );
			
			// Dispatch to the proper LUN.
			for ( index = 0; index < count; index++ )
			{
				
				SCSILogicalUnitBytes	tempBytes = { 0 };
				
				LUN = ( AppleSCSILogicalUnitEmulator * ) fLUNs->getObject ( index );
				LUN->GetLogicalUnitBytes ( &tempBytes );
				if ( !bcmp ( tempBytes, logicalUnitBytes, sizeof ( SCSILogicalUnitBytes ) ) )
				{
					
					IOLockUnlock ( fLock );
					
					result = LUN->SendCommand ( cdb, cdbLen, dataDesc, dataLen, scsiStatus, senseBuffer, senseBufferLen );
					processedCommand = true;
					break;
					
				}
				
			}
			
			if ( processedCommand == false )
			{
				IOLockUnlock ( fLock );
			}
			
		}
		
	}
	
	if ( processedCommand == false )
	{
		
		ERROR_LOG ( ( "No LUN found LUN = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			logicalUnitBytes[0], logicalUnitBytes[1], logicalUnitBytes[2], logicalUnitBytes[3],
			logicalUnitBytes[4], logicalUnitBytes[5], logicalUnitBytes[6], logicalUnitBytes[7] ) );
		
		if ( cdb[0] == kSCSICmd_INQUIRY )
		{

			ERROR_LOG ( ( "Special case INQUIRY to unknown LUN\n" ) );
			
			if ( *dataLen > cdb[4] )
				*dataLen = cdb[4];
			
			if ( *dataLen > 0 )
			{
				
				if ( *dataLen > sizeof ( gInquiryData ) )
				{
					*dataLen = sizeof ( gInquiryData );
				}
				
				dataDesc->writeBytes ( 0, &gInquiryData, *dataLen );
				
			}
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
			result = 0;
			
		}
		
		else
		{

			ERROR_LOG ( ( "Something other than INQUIRY to unknown LUN, report error\n" ) );
			
			*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
			if ( senseBuffer != NULL )
			{
				
				UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
				
				bzero ( senseBuffer, *senseBufferLen );
				bcopy ( &sBadLUNSenseData, senseBuffer, amount );
				
				*senseBufferLen = amount;
				
			}
			
			result = 0;
			
		}
		
	}
	
	return result;
	
}


#else


//-----------------------------------------------------------------------------
//	SendCommand
//-----------------------------------------------------------------------------

int
AppleSCSITargetEmulator::SendCommand (
	UInt8 *					cdb,
	UInt8 					cdbLen,
	IOMemoryDescriptor *	dataDesc,
	UInt64 *				dataLen,
	SCSILogicalUnitNumber	logicalUnit,
	SCSITaskStatus *		scsiStatus,
	SCSI_Sense_Data *		senseBuffer,
	UInt8 *					senseBufferLen )
{
	
	int		result				= 0;
	bool	processedCommand	= false;
	
	IOLockLock ( fLock );
	
	if ( fLUNInventoryChanged == true )
	{
		
		ERROR_LOG ( ( "Generating UNIT_ATTENTION for LUN inventory change\n" ) );
		fLUNInventoryChanged = false;
		
		*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
		if ( senseBuffer != NULL )
		{
			
			UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
			
			bzero ( senseBuffer, *senseBufferLen );
			bcopy ( &sLUNInventoryChangedData, senseBuffer, amount );
			
			*senseBufferLen = amount;
			
		}
		
		processedCommand = true;
		
	}
	
	IOLockUnlock ( fLock );
	
	if ( processedCommand == false )
	{
		
		if ( cdb[0] == kSCSICmd_REPORT_LUNS )
		{
			
			COMMAND_LOG ( ( "REPORT_LUNS requested = %qd\n", *dataLen ) );
			
			if ( fLUNReportBuffer != NULL )
			{
				
				*dataLen = min ( fLUNDataAvailable, *dataLen );
				dataDesc->writeBytes ( 0, fLUNReportBuffer, *dataLen );
				
				COMMAND_LOG ( ( "REPORT_LUNS realized = %qd\n", *dataLen ) );
				
				*scsiStatus = kSCSITaskStatus_GOOD;
			
			}
			
			processedCommand = true;
			
		}
		
		else
		{
			
			UInt32								index		= 0;
			UInt32								count		= 0;
			AppleSCSILogicalUnitEmulator *		LUN			= NULL;
	
			IOLockLock ( fLock );
			
			count = fLUNs->getCount ( );
			
			// Dispatch to the proper LUN.
			for ( index = 0; index < count; index++ )
			{
								
				LUN = ( AppleSCSILogicalUnitEmulator * ) fLUNs->getObject ( index );
				
				if ( logicalUnit == LUN->GetLogicalUnitNumber ( ) )
				{
					
					IOLockUnlock ( fLock );
					
					result = LUN->SendCommand ( cdb, cdbLen, dataDesc, dataLen, scsiStatus, senseBuffer, senseBufferLen );
					processedCommand = true;
					break;
					
				}
				
			}
			
			if ( processedCommand == false )
			{
				IOLockUnlock ( fLock );
			}
			
		}
		
	}
	
	if ( processedCommand == false )
	{
		
		ERROR_LOG ( ( "No LUN found LUN = %qd\n", logicalUnit ) );
		
		if ( cdb[0] == kSCSICmd_INQUIRY )
		{

			ERROR_LOG ( ( "Special case INQUIRY to unknown LUN\n" ) );
			
			if ( *dataLen > cdb[4] )
				*dataLen = cdb[4];
			
			if ( *dataLen > 0 )
			{
				
				if ( *dataLen > sizeof ( gInquiryData ) )
				{
					*dataLen = sizeof ( gInquiryData );
				}
				
				dataDesc->writeBytes ( 0, &gInquiryData, *dataLen );
				
			}
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
			result = 0;
			
		}
		
		else
		{

			ERROR_LOG ( ( "Something other than INQUIRY to unknown LUN, report error\n" ) );
			
			*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
			if ( senseBuffer != NULL )
			{
				
				UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
				
				bzero ( senseBuffer, *senseBufferLen );
				bcopy ( &sBadLUNSenseData, senseBuffer, amount );
				
				*senseBufferLen = amount;
				
			}
			
			result = 0;
			
		}
		
	}
	
	return result;
	
}


#endif /* USE_LUN_BYTES */


//-----------------------------------------------------------------------------
//	CompareLUNs
//-----------------------------------------------------------------------------

static SInt32
CompareLUNs (
	const OSMetaClassBase * obj1,
	const OSMetaClassBase * obj2,
	void * ref )
{
	
	AppleSCSILogicalUnitEmulator *	lun1 = ( AppleSCSILogicalUnitEmulator * ) obj1;
	AppleSCSILogicalUnitEmulator *	lun2 = ( AppleSCSILogicalUnitEmulator * ) obj2;

#if USE_LUN_BYTES
	
	SCSILogicalUnitBytes	temp1Bytes = { 0 };
	SCSILogicalUnitBytes	temp2Bytes = { 0 };
	
	lun1->GetLogicalUnitBytes ( &temp1Bytes );
	lun2->GetLogicalUnitBytes ( &temp2Bytes );
	
	// Returns a comparison result of the object, a negative value if obj1 > obj2,
	// 0 if obj1 == obj2, and a positive value if obj1 < obj2
	return bcmp ( temp2Bytes, temp1Bytes, sizeof ( SCSILogicalUnitBytes ) );

#else
	
	return ( lun1->GetLogicalUnitNumber ( ) - lun2->GetLogicalUnitNumber ( ) );

#endif	/* USE_LUN_BYTES */
	
}
