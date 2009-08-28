/*
  File: AppleSCSILogicalUnitEmulator.cpp

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

#include "AppleSCSILogicalUnitEmulator.h"

#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/SCSICmds_READ_CAPACITY_Definitions.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"LUNEmulator"

#if DEBUG
#define EMULATOR_ADAPTER_DEBUGGING_LEVEL					2
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


#define super OSObject
OSDefineMetaClass ( AppleSCSILogicalUnitEmulator, OSObject );
OSDefineAbstractStructors ( AppleSCSILogicalUnitEmulator, OSObject );


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

SCSI_Sense_Data gInvalidCDBFieldSenseData =
{
	/* VALID_RESPONSE_CODE */				kSENSE_DATA_VALID | kSENSE_RESPONSE_CODE_Current_Errors,
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
	/* ADDITIONAL_SENSE_CODE */				0x24, // INVALID FIELD IN CDB
	/* ADDITIONAL_SENSE_CODE_QUALIFIER */	0x00,
	/* FIELD_REPLACEABLE_UNIT_CODE */		0x00,
	/* SKSV_SENSE_KEY_SPECIFIC_MSB */		0x00,
	/* SENSE_KEY_SPECIFIC_MID */			0x00,
	/* SENSE_KEY_SPECIFIC_LSB */			0x00
};

SCSI_Sense_Data gInvalidCommandSenseData =
{
	/* VALID_RESPONSE_CODE */				kSENSE_DATA_VALID | kSENSE_RESPONSE_CODE_Current_Errors,
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
	/* ADDITIONAL_SENSE_CODE */				0x20, // Invalid command code
	/* ADDITIONAL_SENSE_CODE_QUALIFIER */	0x00,
	/* FIELD_REPLACEABLE_UNIT_CODE */		0x00,
	/* SKSV_SENSE_KEY_SPECIFIC_MSB */		0x00,
	/* SENSE_KEY_SPECIFIC_MID */			0x00,
	/* SENSE_KEY_SPECIFIC_LSB */			0x00
};


//-----------------------------------------------------------------------------
//	SetLogicalUnitNumber
//-----------------------------------------------------------------------------

void
AppleSCSILogicalUnitEmulator::SetLogicalUnitNumber ( SCSILogicalUnitNumber logicalUnitNumber )
{

#if USE_LUN_BYTES	
	bzero ( fLogicalUnitBytes, sizeof ( SCSILogicalUnitBytes ) );
	
	if ( logicalUnitNumber < 256 )
	{
		
		fLogicalUnitBytes[0] = 0;
		fLogicalUnitBytes[1] = logicalUnitNumber & 0xFF;
		
	}
	
	else if ( logicalUnitNumber < 16384 )
	{
		
		fLogicalUnitBytes[0] = ( kREPORT_LUNS_ADDRESS_METHOD_FLAT_SPACE << 6 ) | ( ( logicalUnitNumber >> 8 ) & 0xFF );
		fLogicalUnitBytes[1] = logicalUnitNumber & 0xFF;
		
	}

#endif
	
	fLogicalUnitNumber = logicalUnitNumber;
	
}


//-----------------------------------------------------------------------------
//	GetLogicalUnitNumber
//-----------------------------------------------------------------------------

SCSILogicalUnitNumber
AppleSCSILogicalUnitEmulator::GetLogicalUnitNumber ( void )
{
	return fLogicalUnitNumber;
}


#if USE_LUN_BYTES

//-----------------------------------------------------------------------------
//	GetLogicalUnitBytes
//-----------------------------------------------------------------------------

void
AppleSCSILogicalUnitEmulator::GetLogicalUnitBytes ( SCSILogicalUnitBytes * logicalUnitBytes )
{
	bcopy ( fLogicalUnitBytes, logicalUnitBytes, sizeof ( SCSILogicalUnitBytes ) );
}

#endif	/* USE_LUN_BYTES */