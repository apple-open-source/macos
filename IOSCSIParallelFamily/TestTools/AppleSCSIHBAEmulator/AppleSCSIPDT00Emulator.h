/*
  File: AppleSCSIPDT00Emulator.h

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

#ifndef __APPLE_SCSI_PDT00_EMULATOR_H__
#define __APPLE_SCSI_PDT00_EMULATOR_H__


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/scsi/SCSITask.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi/SCSICmds_MODE_Definitions.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>

#include "AppleSCSILogicalUnitEmulator.h"


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

#define kTwentyMegabytes	(20 * 1024 * 1024)
#define kBlockSize			512

extern SCSICmd_INQUIRY_StandardData		gInquiryData;


//-----------------------------------------------------------------------------
//	Class declaration
//-----------------------------------------------------------------------------

class AppleSCSIPDT00Emulator : public AppleSCSILogicalUnitEmulator
{
	
	OSDeclareDefaultStructors ( AppleSCSIPDT00Emulator )
	
public:
	
	static AppleSCSIPDT00Emulator *
	WithCapacity ( UInt64 capacity = kTwentyMegabytes );

	bool	InitWithCapacity ( UInt64 capacity );
	
	bool SetDeviceBuffers ( 
		IOMemoryDescriptor * 	inquiryBuffer,
		IOMemoryDescriptor * 	inquiryPage00Buffer,
		IOMemoryDescriptor * 	inquiryPage80Buffer,
		IOMemoryDescriptor * 	inquiryPage83Buffer );
	
	void free ( void );
	
	int SendCommand ( UInt8 *				cdb,
					  UInt8					cbdLen,
					  IOMemoryDescriptor * 	dataDesc,
					  UInt64 *				dataLen,
					  SCSITaskStatus * 		scsiStatus,
					  SCSI_Sense_Data * 	senseBuffer,
					  UInt8 *				senseBufferLen );
	
private:
	
	UInt64						fBufferSize;
	IOBufferMemoryDescriptor *	fMemoryBuffer;
	UInt8 *						fMemory;
	
	UInt8 *						fInquiryData;
	UInt32						fInquiryDataSize;
	
	UInt8 *						fInquiryPage00Data;
	UInt32						fInquiryPage00DataSize;
	
	UInt8 *						fInquiryPage80Data;
	UInt32						fInquiryPage80DataSize;
	
	UInt8 *						fInquiryPage83Data;
	UInt32						fInquiryPage83DataSize;
	
};


#endif	/* __APPLE_SCSI_PDT00_EMULATOR_H__ */