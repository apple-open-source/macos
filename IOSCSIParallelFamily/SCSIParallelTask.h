/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/scsi-parallel/IOSCSIParallelInterfaceController.h>
#include <IOKit/scsi-commands/SCSITask.h>

// Temporary -- INCLUDES SCSITASKDEFINITION.h
#include "SCSITaskDefinition.h"

 /*!
  @header SCSIParallelTask
    This is the SCSI Parallel Interconnect specific wrapper for the standard SCSITask
    object.  For all fields in the SCSITask that need to be read, this object will 
	make the direct call into the SCSITask object.  For all data that needs to be set
    in the SCSITask, this will have a temporary storage that will then get copied back
	during the handling of the completion notification.
*/

class SCSIParallelTask: public IOCommand
{
	
	OSDeclareDefaultStructors ( SCSIParallelTask )
	
	
public:
	
	virtual void free ( void );
	
    /*!
		@function Create
        @abstract Creates a new SCSIParallelTask.
		@discussion Creates a new SCSIParallelTask.
		@param sizeOfHBAData Size of the HBA specific data associated with this task in bytes.
        @result returns pointer to SCSIParallelTask or NULL if allocation failed.
	*/
	
	static SCSIParallelTask *	Create ( UInt32 sizeOfHBAData ); 
	
    /*!
		@function ResetForNewTask
        @abstract Resets the fields in the SCSIParallelTask for a new transaction.
		@discussion Resets the fields in the SCSIParallelTask for a new transaction.
	*/
    
	void	ResetForNewTask ( void );
	
    /*!
		@function SetSCSITaskIdentifier
        @abstract Sets the SCSITaskIdentifier associated with this request.
		@discussion Sets the SCSITaskIdentifier associated with this request.
		@param scsiRequest A valid SCSITaskIdentifier that represents the
		original request from the SCSI Application Layer client.
        @result returns true if successful.
	*/
    
	bool	SetSCSITaskIdentifier ( SCSITaskIdentifier scsiRequest );
	
    /*!
		@function GetSCSITaskIdentifier
        @abstract Method to retrieve the SCSITaskIdentifier associated with this request.
		@discussion Method to retrieve the SCSITaskIdentifier associated with this request.
        @result returns SCSITaskIdentifier that represents the original request from the SCSI 
        Application Layer client.
	*/
    
	SCSITaskIdentifier	GetSCSITaskIdentifier ( void );
	
    /*!
		@function SetTargetIdentifier
        @abstract Sets the target identifier for the target device associated
		with this request.
		@discussion Sets the target identifier for the target device associated
		with this request.
		@param theTargetID A valid SCSITargetIdentifier.
        @result returns true if successful.
	*/
    
	bool	SetTargetIdentifier ( SCSITargetIdentifier theTargetID );
	
    /*!
		@function GetTargetIdentifier
        @abstract Method to retrieve the SCSITargetIdentifier associated with this request.
		@discussion Method to retrieve the SCSITargetIdentifier associated with this request.
        @result returns SCSITargetIdentifier
	*/
    
	SCSITargetIdentifier	GetTargetIdentifier ( void );
	
	// ---- Methods for Accessing data in the client's SCSI Task Object ----	
	// Method to retrieve the LUN that identifies the Logical Unit whose Task
	// Set to which this task is to be added.

	// --> Currently this only supports Level 1 Addressing, complete
	// Hierachal LUN addressing will need to be added to the SCSI Task object
	// and the Peripheral Device Type objects which will represent Logical Units.
	// Since that will be completed before this is released, this method will be
	// changed at that time.

    /*!
		@function GetLogicalUnitNumber
        @abstract Method to retrieve the SCSILogicalUnitNumber associated with this request.
		@discussion Method to retrieve the SCSILogicalUnitNumber associated with this request.
        @result returns A valid SCSILogicalUnitNumber.
	*/
    
	SCSILogicalUnitNumber		GetLogicalUnitNumber ( void );
	
	SCSITaskAttribute			GetTaskAttribute ( void );

	SCSITaggedTaskIdentifier	GetTaggedTaskIdentifier ( void );

    /*!
		@function GetCommandDescriptorBlockSize
        @abstract Method to retrieve the size of the SCSI Command Descriptor Block (CDB).
		@discussion Method to retrieve the size of the SCSI Command Descriptor Block (CDB).
        @result returns The size of the Command Descriptor Block in bytes.
	*/
    
	UInt8	GetCommandDescriptorBlockSize ( void );
	
    /*!
		@function GetCommandDescriptorBlock
        @abstract Method to retrieve the SCSICommandDescriptorBlock from this request.
		@discussion This will always return a 16 Byte CDB.  If the Protocol Layer driver
        does not support 16 Byte CDBs, it will have to create a local SCSICommandDescriptorBlock
        variable to get the CDB data and then transfer the needed bytes from there.
        @param cdbData is a SCSICommandDescriptorBlock pointer to 16 byte CDB.
        @result returns true if data was copied to cdbData pointer.
	*/
    
	bool	GetCommandDescriptorBlock ( 
        					SCSICommandDescriptorBlock * cdbData );

    /*!
		@function GetDataTransferDirection
        @abstract Method to retrieve the data transfer direction for this request.
		@discussion Method to retrieve the data transfer direction for this request.
        @result The data transfer direction as defined in <IOKit/scsi-commands/SCSITask.h>.
	*/
    
	UInt8	GetDataTransferDirection ( void );
	
    /*!
		@function GetRequestedDataTransferCount
        @abstract Get Requested Data Transfer Count
        @result xxx
	*/
    
	UInt64	GetRequestedDataTransferCount ( void );

    /*!
		@function GetRealizedDataTransferCount
        @abstract Get Realized Data Transfer Count
        @result xxx
	*/
    
	UInt64	GetRealizedDataTransferCount ( void );
    

    /*!
		@function SetRealizedDataTransferCount
        @abstract Set Realized Data Transfer Count
        @param realizedTransferCountInBytes
        @result xxx
	*/
    
	bool	SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );

    /*!
		@function IncrementRealizedDataTransferCount
        @abstract Increment Realized Data Transfer Count
        @param realizedTransferCountInBytes
	*/
    
	void	IncrementRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );

    /*!
		@function GetDataBuffer
        @abstract Accessor to get client buffer
        @result returns pointer IOMemoryDescriptor of data buffer 
    */
    
	IOMemoryDescriptor * GetDataBuffer ( void );

    /*!
		@function GetDataBufferOffset
        @abstract Accessor to get offset into client buffer
        @result returns offset in bytes
	*/
    
	UInt64	GetDataBufferOffset ( void );

    /*!
		@function GetTimeoutDuration
        @abstract Accessor to get client timeout duration
        @result returns timeout duration in milliseconds
	*/
    
	UInt32	GetTimeoutDuration ( void );
	
    /*!
		@function SetAutoSenseData
        @abstract Accessor to client auto sense data buffer
        @param senseData pointer to auto sense data buffer
        @param senseDataSize Size of the amount of sense data supplied.
        @result returns true if data in senseData was succesfully copied into the task object
	*/
    
    bool	SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize );
	
    /*!
		@function GetAutoSenseData
        @abstract Accessor to client auto sense data buffer
        @param newSensedata pointer to auto sense data buffer
        @param senseDataSize amount of sense data to retrieve.
        @result returns true if successfully copied data into receivingBuffer
	*/
    
 	bool    GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer, UInt8 senseDataSize );

    /*!
		@function GetAutoSenseDataSize
        @abstract Accessor to client auto sense data buffer size
        @result returns size of sense data buffer
	*/
    
 	UInt8	GetAutoSenseDataSize ( void );
	
    /*!
		@function GetAutosenseRealizedDataCount
        @abstract Accessor to amount of valid client auto sense data buffer in bytes
        @result returns valid amount of sense data.
	*/
	
	UInt64	GetAutosenseRealizedDataCount ( void );
	
    /*!
		@function SetSCSIParallelFeatureNegotiation
        @abstract Set Wide Data Transfer Negotiation
        @discussion Set SCSI Parallel Device object if Wide Data Transfers need to be negotiated.
        @param newControl SCSIParallelFeatureRequest
	*/
    
	void	SetSCSIParallelFeatureNegotiation ( SCSIParallelFeature			requestedFeature,
												SCSIParallelFeatureRequest	newRequest );

    /*!
		@function GetSCSIParallelFeatureNegotiation
        @abstract Get Wide Data Transfer Negotiation
        @discussion Query as to whether the SCSI Parallel Device object has negotiated
        Wide Data Transfers.
        @param newControl SCSIParallelFeatureRequest
	*/
    
	SCSIParallelFeatureRequest	GetSCSIParallelFeatureNegotiation ( SCSIParallelFeature requestedFeature );
	
	/*!
		@function GetSCSIParallelFeatureNegotiationCount
		@abstract Method to retrieve the number of requested negotiations.
		@discussion Query as to the number of SCSI Parallel Features that are
		requested to either be negotitated or cleared.  These are all features
		that are set to either kSCSIParallelFeature_AttemptNegotiation or 
		kSCSIParallelFeature_ClearNegotiation.  If the return value is zero,
		then all features requests are set to kSCSIParallelFeature_NoNegotiation.
		@param none.
		@result .
	*/
	
	UInt64	GetSCSIParallelFeatureNegotiationCount ( void );
	
    /*!
		@function SetSCSIParallelFeatureNegotiationResult
        @abstract Set Wide Data Transfer Negotiation Result
        @discussion Set SCSI Parallel Device object if Wide Data Transfers need to be negotiated.
        @param newControl SCSIParallelFeatureResult
	*/
    
	void	SetSCSIParallelFeatureNegotiationResult ( SCSIParallelFeature		requestedFeature,
													  SCSIParallelFeatureResult newResult );

    /*!
		@function GetSCSIParallelFeatureNegotiationResult
        @abstract Get Wide Data Transfer Negotiation Result
        @discussion Query as to whether the SCSI Parallel Controller object has negotiated
        Wide Data Transfers.
        @param newControl SCSIParallelFeatureResult
	*/
    
	SCSIParallelFeatureResult	GetSCSIParallelFeatureNegotiationResult ( SCSIParallelFeature requestedFeature );

	/*!
		@function GetSCSIParallelFeatureNegotiationResultCount
		@abstract Method to retrieve the number of changed negotiations.
		@discussion Query as to the number of SCSI Parallel Features that have
		been changed to either negotitated or cleared.  These are all features
		that are set to either kSCSIParallelFeature_NegotitiationCleared or 
		kSCSIParallelFeature_NegotitiationSuccess.  If the return value is zero,
		then all features are set to kSCSIParallelFeature_NegotitiationUnchanged.
		@param none.
		@result .
	*/
	
	UInt64	GetSCSIParallelFeatureNegotiationResultCount ( void );

	void	SetControllerTaskIdentifier ( UInt64 newIdentifier );
	
	UInt64	GetControllerTaskIdentifier ( void );
	
    /*!
		@function GetHBADataSize
        @abstract Accessor to Get HBA Data Size
        @discussion xxx
        @result returns HBA Data size in bytes
	*/
    
	UInt32	GetHBADataSize ( void );

    /*!
		@function GetHBADataPointer
        @abstract Accessor to Get HBA Data Pointer
        @discussion xxx
        @result returns pointer to buffer for HBA specific data, NULL if 
        none found or GetHBADataSize is zero.
	*/
    
	void *	GetHBADataPointer ( void );

    /*!
		@function GetHBADataDescriptor
        @abstract Accessor to Get HBA Data Descriptor
        @discussion xxx
        @result returns pointer to IOMemoryDescriptor that wraps the HBA specific
        data buffer, NULL if none found or GetHBADataSize is zero.
	*/
    
	IOMemoryDescriptor *	GetHBADataDescriptor ( void );
    
    /*!
		@function GetPreviousTaskInList
        @abstract Accessor to Get Previous Task In List
        @discussion Get previous object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @result returns pointer to next SCSIParallelTask, NULL if none found
	*/
    
	SCSIParallelTask *	GetPreviousTaskInList ( void );

    /*!
		@function SetPreviousTaskInList
        @abstract Set Previous Task In List
        @discussion Set previous object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @param newPrev pointer to SCSIParallelTask
	*/
    
	void	SetPreviousTaskInList ( SCSIParallelTask * newPrev );
    
    /*!
		@function GetNextTaskInList
        @abstract Accessor to Get Next Task In List
        @discussion Get next object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @result returns pointer to next SCSIParallelTask, NULL if none found
	*/
    
	SCSIParallelTask *	GetNextTaskInList ( void );

    /*!
		@function SetNextTaskInList
        @abstract Set Next Task In List
        @discussion Set next object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @param newNext pointer to SCSIParallelTask
	*/
    
	void	SetNextTaskInList( SCSIParallelTask * newNext );

    /*!
		@function GetPreviousResendTaskInList
        @abstract Accessor to Get Previous Task In List
        @discussion Get previous object pointer for the resend task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @result returns pointer to next SCSIParallelTask, NULL if none found
	*/
    
	SCSIParallelTask *	GetPreviousResendTaskInList ( void );

    /*!
		@function SetPreviousResendTaskInList
        @abstract Set Previous Resend Task In List
        @discussion Set previous object pointer for the resend task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @param newPrev pointer to SCSIParallelTask
	*/
    
	void	SetPreviousResendTaskInList ( SCSIParallelTask * newPrev );
    
    /*!
		@function GetNextResendTaskInList
        @abstract Accessor to Get Next Task In List
        @discussion Get next object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @result returns pointer to next SCSIParallelTask, NULL if none found
	*/
    
	SCSIParallelTask *	GetNextResendTaskInList ( void );

    /*!
		@function SetNextResendTaskInList
        @abstract Set Next Task In List
        @discussion Set next object pointer for the outstanding task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @param newNext pointer to SCSIParallelTask
	*/
    
	void	SetNextResendTaskInList ( SCSIParallelTask * newNext );

    /*!
		@function GetNextTimeoutTaskInList
        @abstract Accessor to Get Next Task In List
        @discussion Get next object pointer for the timeout task list. Should be
        used only by the SCSI Parallel Device object for maintaining its list of
        outstanding tasks.
        @result returns pointer to next SCSIParallelTask, NULL if none found
	*/
    
	SCSIParallelTask *	GetNextTimeoutTaskInList ( void );

    /*!
		@function SetNextTimeoutTaskInList
        @abstract Set Next Task In List
        @discussion Set next object pointer for the timeout task list. Should be
        used only by the SCSI Parallel Device  object for maintaining its list of
        outstanding tasks.
        @param newNext pointer to SCSIParallelTask
	*/
    
	void	SetNextTimeoutTaskInList ( SCSIParallelTask * newNext );

	AbsoluteTime	GetTimeoutDeadline ( void );
	void			SetTimeoutDeadline ( AbsoluteTime time );
	
	
#if 0
#pragma mark -
#pragma mark Subclass API
#pragma mark -
#endif

	// Since this class functions as a transport for data between an SCSI Parallel
	// Interface controller object and an SCSI Parallel Interface device, this class is
	// not intended to be subclassed.  Therefore, there are no methods or member variables
	// defined in this section.

#if 0
#pragma mark -
#pragma mark Internal Use Only
#pragma mark -
#endif

private:
	
	SCSITargetIdentifier		fTargetID;
	
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
	
	// Member variables to maintain the previous and next element in the 
	// outstanding task list.
	SCSIParallelTask *			fPreviousParallelTask;
	SCSIParallelTask *			fNextParallelTask;
	
	// Member variables to maintain the previous and next element in the 
	// resend task list.
	SCSIParallelTask *			fPreviousResendTask;
	SCSIParallelTask *			fNextResendTask;

	// Member variables to maintain the next element in the 
	// timeout list and the timeout deadline.
	SCSIParallelTask *			fNextTimeoutTask;
	AbsoluteTime				fTimeoutDeadline;
	
};


#endif	/* __SCSI_PARALLEL_TASK_H__ */