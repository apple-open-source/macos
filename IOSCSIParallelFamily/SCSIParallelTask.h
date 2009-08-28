/*
 * Copyright (c) 2002-2008 Apple Inc. All rights reserved.
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


/* The SCSI Parallel Task object wraps a SCSI Task object and adds the 
 * necessary information for the SCSI Parallel physical interconnect.
 * All set accessors do not directly affect the original SCSI Task and any
 * data that is added to the Parallel Task object via such accessors must
 * be set in the original SCSI Task by the SCSI Parallel Device obejct
 * before completing the original SCSI Task.
 */

#ifndef __SCSI_PARALLEL_TASK_H__
#define __SCSI_PARALLEL_TASK_H__

#include <IOKit/IODMACommand.h>
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#include <IOKit/scsi/SCSITask.h>

#include <IOKit/scsi/SCSITaskDefinition.h>


//-----------------------------------------------------------------------------
//	Class Declarations
//-----------------------------------------------------------------------------

class SCSIParallelTask: public IODMACommand
{
	
	OSDeclareDefaultStructors ( SCSIParallelTask )
	
public:
	
	queue_chain_t		fResendTaskChain;
	queue_chain_t		fTimeoutChain;
	
	static SCSIParallelTask *	Create ( UInt32 sizeOfHBAData, UInt64 alignmentMask ); 
	
	void 	free ( void );
	bool	InitWithSize ( UInt32 sizeOfHBAData, UInt64 alignmentMask );
	
	void	ResetForNewTask ( void );

	bool				SetSCSITaskIdentifier ( SCSITaskIdentifier scsiRequest );
	SCSITaskIdentifier	GetSCSITaskIdentifier ( void );
	
	bool					SetTargetIdentifier ( SCSITargetIdentifier theTargetID );
	SCSITargetIdentifier	GetTargetIdentifier ( void );

	bool							SetDevice ( IOSCSIParallelInterfaceDevice * device );
	IOSCSIParallelInterfaceDevice *	GetDevice ( void );
	
	// ---- Methods for Accessing data in the client's SCSI Task Object ----	
	// Method to retrieve the LUN that identifies the Logical Unit whose Task
	// Set to which this task is to be added.

	// --> Currently this only supports Level 1 Addressing, complete
	// Hierachal LUN addressing will need to be added to the SCSI Task object
	// and the Peripheral Device Type objects which will represent Logical Units.
	// Since that will be completed before this is released, this method will be
	// changed at that time.

	SCSILogicalUnitNumber		GetLogicalUnitNumber ( void );
	SCSITaskAttribute			GetTaskAttribute ( void );
	SCSITaggedTaskIdentifier	GetTaggedTaskIdentifier ( void );
	UInt8						GetCommandDescriptorBlockSize ( void );
	bool						GetCommandDescriptorBlock ( 
        							SCSICommandDescriptorBlock * cdbData );
	
	UInt8	GetDataTransferDirection ( void );
	UInt64	GetRequestedDataTransferCount ( void );
	UInt64	GetRealizedDataTransferCount ( void );
	bool	SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );
	void	IncrementRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );
	
	IOMemoryDescriptor *	GetDataBuffer ( void );
	UInt64					GetDataBufferOffset ( void );
	UInt32					GetTimeoutDuration ( void );
	bool					SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize );
	bool					GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer, UInt8 senseDataSize );
	UInt8					GetAutoSenseDataSize ( void );	
	UInt64					GetAutosenseRealizedDataCount ( void );
	
	void	SetSCSIParallelFeatureNegotiation ( SCSIParallelFeature			requestedFeature,
												SCSIParallelFeatureRequest	newRequest );

	SCSIParallelFeatureRequest	GetSCSIParallelFeatureNegotiation ( SCSIParallelFeature requestedFeature );
	UInt64	GetSCSIParallelFeatureNegotiationCount ( void );
	void	SetSCSIParallelFeatureNegotiationResult ( SCSIParallelFeature		requestedFeature,
													  SCSIParallelFeatureResult newResult );

	SCSIParallelFeatureResult	GetSCSIParallelFeatureNegotiationResult ( SCSIParallelFeature requestedFeature );
	UInt64	GetSCSIParallelFeatureNegotiationResultCount ( void );
	
	void	SetControllerTaskIdentifier ( UInt64 newIdentifier );
	UInt64	GetControllerTaskIdentifier ( void );
	
	UInt32	GetHBADataSize ( void );
	void *	GetHBADataPointer ( void );
	IOMemoryDescriptor *	GetHBADataDescriptor ( void );
    	
	AbsoluteTime	GetTimeoutDeadline ( void );
	void			SetTimeoutDeadline ( AbsoluteTime time );
	
	inline IOReturn SetBuffer ( IOMemoryDescriptor * buffer )
	{
		return setMemoryDescriptor ( buffer, false );
	}	
	
private:
	
	SCSITargetIdentifier				fTargetID;
	IOSCSIParallelInterfaceDevice *		fDevice;
	
	// --> Wide, Sync and other parallel specific fields.
	// The member variables to indicate if wide transfers should be
	// negotiated for the target and whether the negotiation was successful.
	// If the request is made to negotiate for wide transfers, if the HBA supports
	// such transfers, it should honor it regardless of whether a successful negotiation
	// was made as it is the responsiblity of the target driver to manage whether or not
	// such a negotiation exists.
	// The target should only ask for such a negotiation if both the target and the bus
	// support such transfers and if there is no outstanding negotiation for such.
	// On notification of a bus or target reset, the target device should request a new
	// negotiation. 
	// Wide support in this object only implies Wide16, as Wide32 was obsoleted by SPI-3.
	SCSIParallelFeatureRequest	fSCSIParallelFeatureRequest[kSCSIParallelFeature_TotalFeatureCount];
	SCSIParallelFeatureResult	fSCSIParallelFeatureResult[kSCSIParallelFeature_TotalFeatureCount];

	UInt64						fSCSIParallelFeatureRequestCount;
	UInt64						fSCSIParallelFeatureRequestResultCount;
		
	// This is the SCSI Task that is to be executed on behalf of the Application
	// Layer client that controls the Target.
	SCSITaskIdentifier			fSCSITask;
	
	// This is a value that can be used by a controller to uniquely identify a given
	// task.
	UInt64						fControllerTaskIdentifier;
	
	// This is the size and space of the HBA data as requested on
	// when the task object was created.
	UInt32						fHBADataSize;
	void *						fHBAData;
	IOMemoryDescriptor *		fHBADataDescriptor;
	
	// Local storage for data that needs to be copied back to the client's SCSI Task
	UInt64						fRealizedTransferCount;
	
	// Member variables to maintain the next element in the 
	// timeout list and the timeout deadline.
	AbsoluteTime				fTimeoutDeadline;
	
};


#endif	/* __SCSI_PARALLEL_TASK_H__ */