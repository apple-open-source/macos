/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
// Sample SCSI Parallel Interface Controller Driver

#include <IOKit/scsi/IOSCSIParallelInterfaceController.h>

class SampleSCSIController : public IOSCSIParallelInterfaceController
{
public:
	// The ExecuteParallelTask call is made by the client to submit a SCSIParallelTask
	// for execution.
	// -->the actual return value should not be void but maybe a SCSI Service response
	virtual SCSIServiceResponse ExecuteParallelTask( SCSIParallelTask * parallelRequest );

protected:
	// ---- Methods to retrieve the HBA specifics. ----
	// These methods will not be called before the InitializeController call,
	// and will not be called after the TerminateController call.  But in the
	// interval between those calls, they shall report the correct requested
	// information.
	
	// This method will be called to determine the SCSI Device Identifer that
	// the Initiator is assigned for this HBA.
	virtual SCSIDeviceIdentifier	ReportInitiatorIdentifier( void );

	// This method will be called to determine the value of the highest SCSI
	// Device Identifer supported by the HBA.  This value will be used to
	// determine the last ID to process
	virtual SCSIDeviceIdentifier	ReportHighestSupportedDeviceID( void );
	
	// This method will be called to retrieve the maximum number of tasks that
	// the HBA can process during the same 
	virtual UInt32					ReportMaximumTaskCount( void );
	
	// This method is used to retireve the amount of memory that will be allocated
	// in the SCSI Parallel Task for HBA specific use.
	virtual UInt32					ReportHBASpecificDataSize( void );
	
	virtual bool	InitializeController( void );
	virtual bool	TerminateController( void );

	virtual bool	StartController( void );
	virtual void	StopController( void );
	
	// ---- Suspend and Resume Methods for the subclass ----
	// The SuspendServices method
	virtual bool	SuspendServices( void );
	
	// The ResumeServices method
	virtual void	ResumeServices( void );
	
	// ---- Support for Interrupt notification
	// The HandleInterruptRequest is used to notifiy an HBA specific subclass that an 
	// interrupt request needs to be handled.
	virtual void	HandleInterruptRequest( void );
	
	// All necessary supporting methods for handling interrupts 
}
