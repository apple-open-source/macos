/*
  File: AppleSCSIPDT00Emulator.cpp

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

#include "AppleSCSIPDT00Emulator.h"

#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/SCSICmds_READ_CAPACITY_Definitions.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"PDT00Emulator"

#if DEBUG
#define EMULATOR_ADAPTER_DEBUGGING_LEVEL					4
#endif

#include "DebugSupport.h"

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)		
#endif

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x; IOSleep(1)
#else
#define ERROR_LOG(x)		
#endif

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep(1)
#else
#define STATUS_LOG(x)
#endif

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 4 )
#define COMMAND_LOG(x)		IOLog x; IOSleep(1)
#else
#define COMMAND_LOG(x)
#endif


#define super AppleSCSILogicalUnitEmulator
OSDefineMetaClassAndStructors ( AppleSCSIPDT00Emulator, AppleSCSILogicalUnitEmulator );


//-----------------------------------------------------------------------------
//	WithCapacity
//-----------------------------------------------------------------------------

AppleSCSIPDT00Emulator *
AppleSCSIPDT00Emulator::WithCapacity ( UInt64 capacity )
{
	
	AppleSCSIPDT00Emulator *	logicalUnit = NULL;
	bool						result		= false;
	
	STATUS_LOG ( ( "AppleSCSIPDT00Emulator::WithCapacity, capacity = %qd\n", capacity ) );
	
	logicalUnit = OSTypeAlloc ( AppleSCSIPDT00Emulator );
	require_nonzero ( logicalUnit, ErrorExit );
	
	result = logicalUnit->InitWithCapacity ( capacity );
	require ( result, ReleaseLogicalUnit );
	
	return logicalUnit;
	
	
ReleaseLogicalUnit:
	
	
	logicalUnit->release ( );
	
	
ErrorExit:
	
	
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	InitWithCapacity
//-----------------------------------------------------------------------------

bool
AppleSCSIPDT00Emulator::InitWithCapacity ( UInt64 capacity )
{
	
	fMemoryBuffer = IOBufferMemoryDescriptor::inTaskWithOptions (
		kernel_task,
		0,
		capacity,
		PAGE_SIZE );
	
	require_nonzero ( fMemoryBuffer, ErrorExit );
	
	fMemory 	= ( UInt8 * ) fMemoryBuffer->getBytesNoCopy ( );
	fBufferSize = capacity;
	
	return true;
	
	
ErrorExit:
	
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	SetDeviceBuffers
//-----------------------------------------------------------------------------

bool
AppleSCSIPDT00Emulator::SetDeviceBuffers ( 
		IOMemoryDescriptor * 	inquiryBuffer,
		IOMemoryDescriptor * 	inquiryPage00Buffer,
		IOMemoryDescriptor * 	inquiryPage80Buffer,
		IOMemoryDescriptor * 	inquiryPage83Buffer )
{
	
	require_nonzero ( inquiryBuffer, ErrorExit );
	require_nonzero ( inquiryPage00Buffer, ErrorExit );
	require_nonzero ( inquiryPage80Buffer, ErrorExit );
	require_nonzero ( inquiryPage83Buffer, ErrorExit );
	
	fInquiryDataSize 		= inquiryBuffer->getLength ( );
	fInquiryPage00DataSize	= inquiryPage00Buffer->getLength ( );
	fInquiryPage80DataSize	= inquiryPage80Buffer->getLength ( );
	fInquiryPage83DataSize	= inquiryPage83Buffer->getLength ( );

	ERROR_LOG ( ( "fInquiryDataSize = %d\n", fInquiryDataSize ) );
	ERROR_LOG ( ( "fInquiryPage00DataSize = %d\n", fInquiryPage00DataSize ) );
	ERROR_LOG ( ( "fInquiryPage80DataSize = %d\n", fInquiryPage80DataSize ) );
	ERROR_LOG ( ( "fInquiryPage83DataSize = %d\n", fInquiryPage83DataSize ) );
	
	fInquiryData = ( UInt8 * ) IOMalloc ( fInquiryDataSize );
	require_nonzero ( fInquiryData, ErrorExit );

	fInquiryPage00Data = ( UInt8 * ) IOMalloc ( fInquiryPage00DataSize );
	require_nonzero ( fInquiryPage00Data, ErrorExit );

	fInquiryPage80Data = ( UInt8 * ) IOMalloc ( fInquiryPage80DataSize );
	require_nonzero ( fInquiryPage80Data, ErrorExit );

	fInquiryPage83Data = ( UInt8 * ) IOMalloc ( fInquiryPage83DataSize );
	require_nonzero ( fInquiryPage83Data, ErrorExit );
	
	inquiryBuffer->readBytes ( 0, fInquiryData, fInquiryDataSize );
	inquiryPage00Buffer->readBytes ( 0, fInquiryPage00Data, fInquiryPage00DataSize );
	inquiryPage80Buffer->readBytes ( 0, fInquiryPage80Data, fInquiryPage80DataSize );
	inquiryPage83Buffer->readBytes ( 0, fInquiryPage83Data, fInquiryPage83DataSize );
	
	return true;
	
	
ErrorExit:
	
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	free
//-----------------------------------------------------------------------------

void
AppleSCSIPDT00Emulator::free ( void )
{
	
	STATUS_LOG ( ( "AppleSCSIPDT00Emulator::free\n" ) );
	
	if ( fMemoryBuffer != NULL )
	{
		
		fMemoryBuffer->release ( );
		fMemoryBuffer = NULL;
		
	}
	
	if ( fInquiryData != NULL )
	{
		
		IOFree ( fInquiryData, fInquiryDataSize );
		fInquiryData = NULL;
		
	}
	
	if ( fInquiryPage00Data != NULL )
	{
		
		IOFree ( fInquiryPage00Data, fInquiryPage00DataSize );
		fInquiryPage00Data = NULL;
		
	}
	
	if ( fInquiryPage80Data != NULL )
	{
		
		IOFree ( fInquiryPage80Data, fInquiryPage80DataSize );
		fInquiryPage80Data = NULL;
		
	}
	
	if ( fInquiryPage83Data != NULL )
	{
		
		IOFree ( fInquiryPage83Data, fInquiryPage83DataSize );
		fInquiryPage83Data = NULL;
		
	}
	
	super::free ( );
	
}


//-----------------------------------------------------------------------------
//	SendCommand
//-----------------------------------------------------------------------------

int
AppleSCSIPDT00Emulator::SendCommand (
	UInt8 *					cdb,
	UInt8 					cbdLen,
	IOMemoryDescriptor *	dataDesc,
	UInt64 *				dataLen,
	SCSITaskStatus *		scsiStatus,
	SCSI_Sense_Data *		senseBuffer,
	UInt8 *					senseBufferLen )
{
	
	UInt32		lba;
	UInt16 		transferLength;
	UInt32		byteOffset;
	UInt32		numBytes;
	
	STATUS_LOG ( ( "AppleSCSIPDT00Emulator::sendCommand, LUN = %qd\n", GetLogicalUnitNumber ( ) ) );
	
	switch ( cdb[0] )
	{
		
		case kSCSICmd_TEST_UNIT_READY:
		{	
			
			COMMAND_LOG ( ( "SCSI Command: TEST_UNIT_READY\n" ) );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			*dataLen = 0;
			break;
			
		}
		
		case kSCSICmd_INQUIRY:
		{
			
			COMMAND_LOG ( ( "SCSI Command: INQUIRY\n" ) );
			
			if ( cdb[1] == 1 )
			{
				
				UInt8 *		buffer;
				UInt64		amount;
				
				COMMAND_LOG ( ( "INQUIRY VPD requested\n" ) );
				
				// Asking for EVPD. Return EVPD data based on PAGE_CODE parameter.
				if ( cdb[2] == kINQUIRY_Page00_PageCode )
				{
					
					COMMAND_LOG ( ( "Page 00h requested\n" ) );
					
					buffer = ( UInt8 * ) fInquiryPage00Data;
					amount = fInquiryPage00DataSize;
					
				}

				else if ( cdb[2] == kINQUIRY_Page80_PageCode )
				{
					
					COMMAND_LOG ( ( "Page 80h requested\n" ) );
					
					buffer = ( UInt8 * ) fInquiryPage80Data;
					amount = fInquiryPage80DataSize;
					
				}
				
				else if ( cdb[2] == kINQUIRY_Page83_PageCode )
				{
					
					COMMAND_LOG ( ( "Page 83h requested\n" ) );
					
					buffer = ( UInt8 * ) fInquiryPage83Data;
					amount = fInquiryPage83DataSize;
					
				}
				
				if ( buffer != NULL )
				{

					COMMAND_LOG ( ( "Requested = %ld\n", *dataLen ) );
					COMMAND_LOG ( ( "Amount = %ld\n", amount ) );
					
					*dataLen = min ( amount, *dataLen );
					dataDesc->writeBytes ( 0, buffer, *dataLen );

					COMMAND_LOG ( ( "Realized = %ld\n", *dataLen ) );

					*scsiStatus = kSCSITaskStatus_GOOD;
					
				}
				
				else
				{
					
					*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
					
					if ( senseBuffer != NULL )
					{
						
						UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
						
						bzero ( senseBuffer, *senseBufferLen );
						bcopy ( &gInvalidCDBFieldSenseData, senseBuffer, amount );
						
						*senseBufferLen = amount;
						
					}
					
				}
				
			}
			
			else if ( ( cdb[1] == 2 ) || ( cdb[2] != 0 ) || ( cdb[3] != 0 ) )
			{
				
				COMMAND_LOG ( ( "Illegal request\n" ) );
				
				// Don't support CMDDT bit, or PAGE_CODE without EVPD set.
				*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;

				if ( senseBuffer != NULL )
				{
					
					UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
					
					bzero ( senseBuffer, *senseBufferLen );
					bcopy ( &gInvalidCDBFieldSenseData, senseBuffer, amount );
					
					*senseBufferLen = amount;
					
				}
			
			}
			
			else
			{
				
				COMMAND_LOG ( ( "Standard INQUIRY\n" ) );
				
				*dataLen = min ( fInquiryDataSize, *dataLen );
				dataDesc->writeBytes ( 0, fInquiryData, *dataLen );
				
				*scsiStatus = kSCSITaskStatus_GOOD;
				
			}
			
		}
		break;

		case kSCSICmd_READ_CAPACITY:
		{
			
			SCSI_Capacity_Data	data;
			UInt32				lastBlock;
			
			COMMAND_LOG ( ( "SCSI Command: READ_CAPACITY\n" ) );
			
			lastBlock = ( fBufferSize / kBlockSize ) - 1;
			data.RETURNED_LOGICAL_BLOCK_ADDRESS = OSSwapHostToBigInt32 ( lastBlock );
			data.BLOCK_LENGTH_IN_BYTES = OSSwapHostToBigInt32 ( kBlockSize );

			*dataLen = min ( sizeof ( data ), *dataLen );
			
			dataDesc->writeBytes ( 0, &data, *dataLen );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}
		break;

		case kSCSICmd_WRITE_10:
		{	
			
			lba				= OSReadBigInt32 ( cdb, 2 );
			transferLength 	= OSReadBigInt16 ( cdb, 7 );
			
			byteOffset 		= lba * kBlockSize;
			numBytes 		= transferLength * kBlockSize;
			
			COMMAND_LOG ( ( "SCSI Command: WRITE_10 - %qd (0x%qX) bytes at 0x%X (ptr = %p)\n", numBytes, numBytes, byteOffset, &fMemory[byteOffset] ) );
			
			dataDesc->readBytes ( 0, &fMemory[byteOffset], numBytes );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}	
		break;
			
		case kSCSICmd_READ_10:
		{
			
			lba				= OSReadBigInt32 ( cdb, 2 );
			transferLength 	= OSReadBigInt16 ( cdb, 7 );
			
			byteOffset		= lba * kBlockSize;
			numBytes		= transferLength * kBlockSize;
			
			COMMAND_LOG ( ( "SCSI Command: READ_10 - %qd (0x%qX) bytes at 0x%X (ptr = %p)\n", *dataLen, *dataLen, byteOffset, &fMemory[byteOffset] ) );
			
			dataDesc->writeBytes ( 0, &fMemory[byteOffset], *dataLen );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}
		break;

		case kSCSICmd_START_STOP_UNIT:
		{
			
			COMMAND_LOG ( ( "SCSI Command: START_STOP_UNIT\n" ) );
			
			// For now just set the status to success.
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}
		break;
			
		case kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL:
		{
			
			COMMAND_LOG ( ( "SCSI Command: PREVENT_ALLOW_MEDIUM_REMOVAL - prevent = 0x%02X\n", cdb[4] & 0x03 ) );
							
			// We're not a changeable medium... safe to ignore for now
			*scsiStatus = kSCSITaskStatus_GOOD;
			*senseBufferLen = 0;
			
		}
		break;
			
		case kSCSICmd_REQUEST_SENSE:
		{
			
			COMMAND_LOG ( ( "SCSI Command: REQUEST_SENSE (desc = %s, allocation length = %d bytes) - returning CHECK CONDITION with INVALID COMMAND\n", (cdb[1] & 0x01) ? "TRUE" : "FALSE", cdb[4] ) );
			
			*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
			*dataLen = 0;
			
			if ( senseBuffer != NULL )
			{
				
				UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
				
				bzero ( senseBuffer, *senseBufferLen );
				bcopy ( &gInvalidCommandSenseData, senseBuffer, amount );
				
				*senseBufferLen = amount;
				
			}
			
		}
		break;


		case kSCSICmd_MODE_SENSE_6:
		{
			
			SPCModeParameterHeader6		header;
			
			COMMAND_LOG ( ( "SCSI Command: MODE_SENSE_6\n" ) );
			
			// We don't support any mode pages, but we support the mode parameter header.
			// Just return the header.			
			*senseBufferLen = 0;
			
			COMMAND_LOG ( ( "*dataLen = %ld, sizeof(header) = %d\n", *dataLen, sizeof ( header ) ) );
			COMMAND_LOG ( ( "pageCode = 0x%02x\n", cdb[2] & 0x3FFFF ) );
			
			*dataLen = min ( sizeof ( header ), *dataLen );
			
			header.MODE_DATA_LENGTH				= sizeof ( header ) - sizeof ( header.MODE_DATA_LENGTH );
			header.MEDIUM_TYPE					= 0;	// Must be 0h by SBC spec.
			header.DEVICE_SPECIFIC_PARAMETER	= 0;	// Not write protected. Doesn't support DPOFUA.
			header.BLOCK_DESCRIPTOR_LENGTH		= 0;	// No block descriptors.
			
			COMMAND_LOG ( ( "header.MODE_DATA_LENGTH = %d, *dataLen = %ld\n", header.MODE_DATA_LENGTH, *dataLen ) );
			
			dataDesc->writeBytes ( 0, &header, *dataLen );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}
		break;
		
		case kSCSICmd_MODE_SENSE_10:
		{
			
			SPCModeParameterHeader10		header;
			
			COMMAND_LOG ( ( "SCSI Command: MODE_SENSE_10\n" ) );
			
			// We don't support any mode pages, but we support the mode parameter header.
			// Just return the header.			
			*senseBufferLen = 0;
			
			COMMAND_LOG ( ( "*dataLen = %ld, sizeof(header) = %d\n", *dataLen, sizeof ( header ) ) );
			COMMAND_LOG ( ( "pageCode = 0x%02x\n", cdb[2] & 0x3FFFF ) );
			
			*dataLen = min ( sizeof ( header ), *dataLen );
			
			header.MODE_DATA_LENGTH				= sizeof ( header ) - sizeof ( header.MODE_DATA_LENGTH );
			header.MEDIUM_TYPE					= 0;	// Must be 0h by SBC spec.
			header.DEVICE_SPECIFIC_PARAMETER	= 0;	// Not write protected. Doesn't support DPOFUA.
			header.BLOCK_DESCRIPTOR_LENGTH		= 0;	// No block descriptors.
			
			COMMAND_LOG ( ( "header.MODE_DATA_LENGTH = %d, *dataLen = %ld\n", header.MODE_DATA_LENGTH, *dataLen ) );
			
			dataDesc->writeBytes ( 0, &header, *dataLen );
			
			*scsiStatus = kSCSITaskStatus_GOOD;
			
		}
		break;
			
		default:
		{
			
			COMMAND_LOG ( ( "SCSI Command: Unknown: 0x%X\n", cdb[0] ) );
			
			*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
			
			if ( senseBuffer != NULL )
			{
				
				UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
				
				bzero ( senseBuffer, *senseBufferLen );
				bcopy ( &gInvalidCommandSenseData, senseBuffer, amount );
				
				*senseBufferLen = amount;
				
			}
			
		}
		break;
		
	}
	
	return 1;
	
}