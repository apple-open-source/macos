/*
  File: AppleSCSIEmulatorAdapter.h

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

// Code to implement a basic Parallel Tasking SCSI HBA.  In this case, a SCSI-based RAM disk.

#ifndef __APPLE_SCSI_EMULATOR_ADAPTER_H__
#define __APPLE_SCSI_EMULATOR_ADAPTER_H__


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#include <IOKit/scsi/SCSITask.h>

#include "AppleSCSIEmulatorAdapterUC.h"

// Forward declarations
class AppleSCSIEmulatorEventSource;


//-----------------------------------------------------------------------------
//	Class declaration
//-----------------------------------------------------------------------------

class AppleSCSIEmulatorAdapter : public IOSCSIParallelInterfaceController
{

	OSDeclareDefaultStructors ( AppleSCSIEmulatorAdapter )
	
public:
    
	SCSILogicalUnitNumber	ReportHBAHighestLogicalUnitNumber ( void );
	
	bool	DoesHBASupportSCSIParallelFeature ( 
							SCSIParallelFeature 		theFeature );
	
	bool	InitializeTargetForID (  
							SCSITargetIdentifier 		targetID );
	
	SCSIServiceResponse	AbortTaskRequest ( 	
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL,
							SCSITaggedTaskIdentifier	theQ );
	
	SCSIServiceResponse AbortTaskSetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL );
	
	SCSIServiceResponse ClearACARequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL );
	
	SCSIServiceResponse ClearTaskSetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL );
	
	SCSIServiceResponse LogicalUnitResetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL );
	
	SCSIServiceResponse TargetResetRequest (
							SCSITargetIdentifier 		theT );
	
	// Methods the user client calls
	IOReturn	CreateLUN ( EmulatorTargetParamsStruct * targetParameters, task_t task );
	IOReturn	DestroyLUN ( SCSITargetIdentifier targetID, SCSILogicalUnitNumber logicalUnit );
	IOReturn	DestroyTarget ( SCSITargetIdentifier targetID );
	
	
protected:
	
	void SetControllerProperties ( void );
	
	void TaskComplete ( SCSIParallelTaskIdentifier parallelRequest );

	void CompleteTaskOnWorkloopThread (
							SCSIParallelTaskIdentifier		parallelRequest,
							bool							transportSuccessful,
							SCSITaskStatus					scsiStatus,
							UInt64							actuallyTransferred,
							SCSI_Sense_Data *				senseBuffer,
							UInt8							senseLength );
	
	SCSIInitiatorIdentifier	ReportInitiatorIdentifier ( void );
	
	SCSIDeviceIdentifier	ReportHighestSupportedDeviceID ( void );
		
	UInt32		ReportMaximumTaskCount ( void );
		
	UInt32		ReportHBASpecificTaskDataSize ( void );
	
	UInt32		ReportHBASpecificDeviceDataSize ( void );
	
	void		ReportHBAConstraints ( OSDictionary * constraints );
	
	bool		DoesHBAPerformDeviceManagement ( void );

	bool	InitializeController ( void );
	
	void	TerminateController ( void );
	
	bool	StartController ( void );
		
	void	StopController ( void );
	
	void	HandleInterruptRequest ( void );

	SCSIServiceResponse ProcessParallelTask (
							SCSIParallelTaskIdentifier parallelRequest );
	
	IOInterruptEventSource * CreateDeviceInterrupt ( 
											IOInterruptEventSource::Action			action,
											IOFilterInterruptEventSource::Filter	filter,
											IOService *								provider );
	
private:
	
	AppleSCSIEmulatorEventSource *	fEventSource;
	OSArray *						fTargetEmulators;
	
};


#endif	/* __APPLE_SCSI_EMULATOR_ADAPTER_H__ */