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

#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
#include "SCSIReducedBlockCommands.h"

#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#define DEBUG_ASSERT(x)		assert x
#else
#define PANIC_NOW(x)
#define DEBUG_ASSERT(x)
#endif

#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_RBC_COMMANDS_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define		READ_CAPACITY_DATA_SIZE		8

#define super SCSIPrimaryCommands
OSDefineMetaClassAndStructors ( SCSIReducedBlockCommands, SCSIPrimaryCommands );

//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject
//
//----------------------------------------------------------------------
//		
//		get and instance of the command builder
//
//----------------------------------------------------------------------

SCSIReducedBlockCommands *
SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject ( void )
{

	return new SCSIReducedBlockCommands;

}

#pragma mark -
#pragam mark RBC Command Methods
// SCSI Block Commands as defined in T10:990-D SBC
// Revision 8c, November 13, 1997


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::FORMAT_UNIT
//
//----------------------------------------------------------------------
//		
//		The FORMAT_UNIT command as defined in section 5.1.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::FORMAT_UNIT (
						SCSITask *				request,
   					    SCSICmdField1Bit		IMMED,
   						SCSICmdField1Bit		PROGRESS,
   						SCSICmdField1Bit		PERCENT_TIME,
   						SCSICmdField1Bit		INCREMENT )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::FORMAT_UNIT called\n" ) );
	
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PROGRESS, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PROGRESS = %x not valid\n", PROGRESS ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PERCENT_TIME, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "PERCENT_TIME = %x not valid\n", PERCENT_TIME ) );
		return false;
		
	}
	
	if ( IsParameterValid ( INCREMENT, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "INCREMENT = %x not valid\n", INCREMENT ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								0x00,
								( IMMED << 3 ) | ( PROGRESS << 2 ) |
									( PERCENT_TIME << 1 ) | INCREMENT,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
									
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::READ_10
//
//----------------------------------------------------------------------
//		
//		The READ_10 command as defined in section 5.2.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::READ_10 (
					SCSITask *				request,
   					IOMemoryDescriptor *	dataBuffer,
	    			UInt32					blockSize,
					SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 		TRANSFER_LENGTH )
{
	
	UInt32 		requestedByteCount;
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::READ_10 called\n" ) );
	
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		STATUS_LOG ( ( "blockSize = %x not valid\n" ) );
		return false;
		
	}
		
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n", LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "TRANSFER_LENGTH = %x not valid\n", TRANSFER_LENGTH ) );
		return false;
		
	}
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	if ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %x\n", dataBuffer, requestedByteCount ) );
		return false;
		
	}

	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_10,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );
									
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::READ_CAPACITY
//
//----------------------------------------------------------------------
//		
//		The READ_CAPACITY command as defined in section 5.3.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::READ_CAPACITY (
						SCSITask *				request,
						IOMemoryDescriptor *	dataBuffer )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::READ_CAPACITY called\n" ) );
	
	if ( IsBufferAndCapacityValid ( dataBuffer, READ_CAPACITY_DATA_SIZE ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, READ_CAPACITY_DATA_SIZE = %x\n", dataBuffer, READ_CAPACITY_DATA_SIZE ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_CAPACITY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								READ_CAPACITY_DATA_SIZE );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::START_STOP_UNIT
//
//----------------------------------------------------------------------
//		
//		The START_STOP_UNIT command as defined in section 5.4.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::START_STOP_UNIT (
				SCSITask *					request,
				SCSICmdField1Bit 			IMMED,
				SCSICmdField4Bit 			POWER_CONDITIONS,
				SCSICmdField1Bit 			LOEJ,
				SCSICmdField1Bit 			START )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::START_STOP_UNIT called\n" ) );
	
	if ( IsParameterValid ( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
		
	}
	
	if ( IsParameterValid ( POWER_CONDITIONS, kSCSICmdFieldMask4Bit ) == false )
	{
		
		STATUS_LOG ( ( "POWER_CONDITIONS = %x not valid\n", POWER_CONDITIONS ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LOEJ, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "LOEJ = %x not valid\n", LOEJ ) );
		return false;
		
	}

	if ( IsParameterValid ( START, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "START = %x not valid\n", START ) );
		return false;
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( POWER_CONDITIONS << 4 ) | ( LOEJ << 1 ) | START,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;	
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::SYNCHRONIZE_CACHE
//
//----------------------------------------------------------------------
//		
//		The SYNCHRONIZE_CACHE command as defined in section 5.5.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::SYNCHRONIZE_CACHE (
							SCSITask *		request )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::SYNCRONIZE_CACHE called\n" ) );
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SYNCHRONIZE_CACHE,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::WRITE_10
//
//----------------------------------------------------------------------
//		
//		The WRITE_10 command as defined in section 5.6.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::WRITE_10 (
						SCSITask *				request,
						IOMemoryDescriptor *	dataBuffer,
			    		UInt32					blockSize,
						SCSICmdField1Bit        FUA,
						SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 		TRANSFER_LENGTH )
{
	
	UInt32		requestedByteCount;

	STATUS_LOG ( ( "SCSIReducedBlockCommands::WRITE_10 called\n" ) );
	
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		STATUS_LOG ( ( "blockSize = %x not valid\n", blockSize ) );
		return false;
		
	}
		
    if ( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		
		STATUS_LOG ( ( "FUA = %x not valid\n", FUA ) );
		return false;
		
	}
	
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n", LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "TRANSFER_LENGTH = %x not valid\n", TRANSFER_LENGTH ) );
		return false;
		
	}
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	if ( IsBufferAndCapacityValid ( dataBuffer, requestedByteCount ) == false )
	{
		
		STATUS_LOG ( ( "dataBuffer = %x not valid, requestedByteCount = %x\n", dataBuffer, requestedByteCount ) );
		return false;
	
	}

	// This is a 10-Byte command, fill out the cdb appropriately
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( FUA << 3 ),
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								requestedByteCount );	
	
	return true;
	
}


//----------------------------------------------------------------------
//
//		SCSIReducedBlockCommands::VERIFY
//
//----------------------------------------------------------------------
//		
//		The VERIFY command as defined in section 5.7.
//
//----------------------------------------------------------------------

bool
SCSIReducedBlockCommands::VERIFY (
					SCSITask *				request,
					SCSICmdField4Byte 		LOGICAL_BLOCK_ADDRESS,
					SCSICmdField2Byte 		VERIFICATION_LENGTH )
{
	
	STATUS_LOG ( ( "SCSIReducedBlockCommands::VERIFY called\n" ) );
	
	if ( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		
		STATUS_LOG ( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n", LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if ( IsParameterValid ( VERIFICATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		
		STATUS_LOG ( ( "VERIFICATION_LENGTH = %x not valid\n", VERIFICATION_LENGTH ) );
		return false;
		
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_VERIFY_10,
								0x00,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( VERIFICATION_LENGTH >> 8 ) & 0xFF,
								  VERIFICATION_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferControl ( 	request,
			      				0,
								kSCSIDataTransfer_NoDataTransfer );	
	
	return true;
	
