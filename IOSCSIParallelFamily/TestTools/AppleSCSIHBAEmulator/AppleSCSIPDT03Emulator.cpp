/*
  File: AppleSCSIPDT03Emulator.cpp

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

#include "AppleSCSIPDT03Emulator.h"

#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/SCSICmds_READ_CAPACITY_Definitions.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"PDT03LUNEmulator"

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
OSDefineMetaClassAndStructors ( AppleSCSIPDT03Emulator, AppleSCSILogicalUnitEmulator );


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

SCSICmd_INQUIRY_StandardData sInquiryData =
{
	kINQUIRY_PERIPHERAL_TYPE_ProcessorSPCDevice,	// PERIPHERAL_DEVICE_TYPE
	0,	// RMB;
	5,	// VERSION
	2,	// RESPONSE_DATA_FORMAT
	sizeof ( SCSICmd_INQUIRY_StandardData ) - 5,	// ADDITIONAL_LENGTH
	0,	// SCCSReserved
	0,	// flags1
	0,	// flags2
	"APPLE",
	"SCSI Emulator",
	"1.0",
};



//-----------------------------------------------------------------------------
//	Create
//-----------------------------------------------------------------------------

AppleSCSIPDT03Emulator *
AppleSCSIPDT03Emulator::Create ( void )
{
	
	AppleSCSIPDT03Emulator *	logicalUnit = NULL;
	bool						result		= false;
	
	STATUS_LOG ( ( "AppleSCSIPDT03Emulator::Create\n" ) );
	
	logicalUnit = OSTypeAlloc ( AppleSCSIPDT03Emulator );
	require_nonzero ( logicalUnit, ErrorExit );
	
	result = logicalUnit->init ( );
	require ( result, ReleaseLogicalUnit );
	
	return logicalUnit;
	
	
ReleaseLogicalUnit:
	
	
	logicalUnit->release ( );
	
	
ErrorExit:
	
	
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	SendCommand
//-----------------------------------------------------------------------------

int
AppleSCSIPDT03Emulator::SendCommand (
	UInt8 *					cdb,
	UInt8 					cbdLen,
	IOMemoryDescriptor *	dataDesc,
	UInt64 *				dataLen,
	SCSITaskStatus *		scsiStatus,
	SCSI_Sense_Data *		senseBuffer,
	UInt8 *					senseBufferLen )
{
		
	STATUS_LOG ( ( "AppleSCSIPDT03Emulator::SendCommand, LUN = %qd\n", GetLogicalUnitNumber ( ) ) );
	
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
								
				COMMAND_LOG ( ( "INQUIRY VPD requested, PDT03 doesn't support it\n" ) );
				
				*scsiStatus = kSCSITaskStatus_CHECK_CONDITION;
				
				if ( senseBuffer != NULL )
				{
					
					UInt8	amount = min ( *senseBufferLen, sizeof ( SCSI_Sense_Data ) );
					
					bzero ( senseBuffer, *senseBufferLen );
					bcopy ( &gInvalidCDBFieldSenseData, senseBuffer, amount );
					
					*senseBufferLen = amount;
					
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
				
				*dataLen = min ( sizeof ( sInquiryData ), *dataLen );
				dataDesc->writeBytes ( 0, &sInquiryData, *dataLen );
				
				*scsiStatus = kSCSITaskStatus_GOOD;
				
			}
			
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