/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __SCSI_TASK_CLASS_H__
#define __SCSI_TASK_CLASS_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Private includes
#include "SCSITaskIUnknown.h"
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSITaskDeviceClass.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


class SCSITaskClass : public SCSITaskIUnknown
{
	
	public:
		
		SCSITaskClass ( void );
		virtual ~SCSITaskClass ( void );
		
		static void sAbortAndReleaseTasks ( const void * value, void * context );
		static void sSetConnectionAndPort ( const void * value, void * context );
		
		virtual IOReturn Init ( SCSITaskDeviceClass * scsiTaskDevice,
								io_connect_t connection,
								mach_port_t asyncPort );
		
		virtual IOReturn SetConnectionAndPort ( io_connect_t connection, mach_port_t asyncPort );
		
		static SCSITaskInterface ** alloc ( SCSITaskDeviceClass * scsiTaskDevice,
											io_connect_t connection,
											mach_port_t asyncPort );
		
	protected:
		
		static SCSITaskInterface	sSCSITaskInterface;
		struct InterfaceMap			fSCSITaskInterfaceMap;
		
		SCSITaskDeviceClass *		fSCSITaskDevice;
		io_connect_t				fConnection;	// connection to user client in kernel
		mach_port_t					fAsyncPort;		// async port for callback from kernel
		void *						fCallbackRefCon;
		SCSITaskCallbackFunction 	fCallbackFunction;
		
		SCSITaskData				fTaskArguments;
		SCSITaskResults				fTaskResults;
		
		SCSI_Sense_Data				fSenseData;
		SCSI_Sense_Data *			fExternalSenseData;
		IOVirtualRange *			fSGList;
		SCSITaskState				fTaskState;
		
		// CFPlugIn/IOCFPlugIn stuff
		virtual HRESULT QueryInterface ( REFIID iid, void ** ppv );
		
		// SCSITaskInterface stuff
		virtual Boolean  	IsTaskActive ( void );

		virtual void 		SetTaskAttribute ( SCSITaskAttribute inAttributeValue );
		
		virtual SCSITaskAttribute 	GetTaskAttribute ( void );
		
		virtual IOReturn 	SetCommandDescriptorBlock ( UInt8 * inCDB, UInt8 inSize );
		
		virtual UInt8 		GetCommandDescriptorBlockSize ( void );
	
		virtual void	 	GetCommandDescriptorBlock ( UInt8 * outCDB );
		
		virtual IOReturn 	SetScatterGatherEntries ( IOVirtualRange * inScatterGatherList,
										   UInt8 inScatterGatherEntries,
										   UInt64 transferCount,
										   UInt8 transferDirection );
		
		virtual IOReturn	SetSenseDataBuffer ( void * buffer, UInt8 bufferSize );
		
		virtual void 		SetTimeoutDuration ( UInt32 timeoutDurationMS );
		
		virtual UInt32 		GetTimeoutDuration ( void );
		
		virtual void 		SetTaskCompletionCallback ( 
													 SCSITaskCallbackFunction callback,
													 void * refCon );
		
		virtual IOReturn 	ExecuteTaskAsync ( void );
		
		virtual IOReturn 	ExecuteTaskSync ( SCSI_Sense_Data * senseDataBuffer,
											  SCSITaskStatus * taskStatus,
											  UInt64 * realizedTransferCount );
		
		virtual IOReturn 	AbortTask ( void );
		
		virtual SCSIServiceResponse 	GetServiceResponse ( void );
		
		virtual SCSITaskState 	GetTaskState ( void );
		
		virtual SCSITaskStatus 	GetTaskStatus ( void );
		
		virtual UInt64 		GetRealizedDataTransferCount ( void );
		
		virtual IOReturn 	GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer );
		
		virtual void 		TaskCompletion ( IOReturn result, void ** args, int numArgs );
		
		// Method for getting the "this" pointer
		static inline SCSITaskClass * getThis ( void * task )
			{ return ( SCSITaskClass * ) ( ( InterfaceMap * ) task)->obj; };

		// Static methods
		static Boolean		sIsTaskActive ( void * task );		
		static IOReturn		sSetTaskAttribute ( void * task, SCSITaskAttribute inAttributeValue );
		static IOReturn 	sGetTaskAttribute ( void * task, SCSITaskAttribute * outTaskAttributeValue );
		static IOReturn 	sSetCommandDescriptorBlock ( void * task, UInt8 * inCDB, UInt8 inSize );
		static UInt8 		sGetCommandDescriptorBlockSize ( void * task );
		static IOReturn 	sGetCommandDescriptorBlock ( void * task, UInt8 * outCDB );
		static IOReturn 	sSetScatterGatherEntries ( 	void * 				task,
										   				IOVirtualRange *	inScatterGatherList,
										   				UInt8				inScatterGatherEntries,
										   				UInt64				transferCount,
										   				UInt8				transferDirection );
		static IOReturn		sSetSenseDataBuffer ( void * task, SCSI_Sense_Data * buffer, UInt8 bufferSize );
		static IOReturn 	sSetTimeoutDuration ( void * task, UInt32 timeoutDurationMS );
		static UInt32 		sGetTimeoutDuration ( void * task );
		static IOReturn		sSetTaskCompletionCallback (	void *						task,
															SCSITaskCallbackFunction	callback,
															void *						refCon );
		static IOReturn 	sExecuteTaskAsync ( void * task );
		static IOReturn 	sExecuteTaskSync (	void *				task,
												SCSI_Sense_Data *	senseDataBuffer,
												SCSITaskStatus *	taskStatus,
												UInt64 *			realizedTransferCount );
		static IOReturn 	sAbortTask ( void * task );
		static IOReturn 	sGetServiceResponse ( void * task, SCSIServiceResponse * serviceResponse );
		static IOReturn 	sGetTaskState ( void * task, SCSITaskState * outTaskState );
		static IOReturn 	sGetTaskStatus ( void * task, SCSITaskStatus * outTaskStatus );
		static UInt64 		sGetRealizedDataTransferCount ( void * task );
		static IOReturn 	sGetAutoSenseData ( void * task, SCSI_Sense_Data * senseDataBuffer );
		static void 		sTaskCompletion ( void * refcon, IOReturn result, void ** args, int numArgs );
		
	private:
		
		virtual IOReturn ExecuteTask ( void );
		
		// Disable copy constructor
		SCSITaskClass ( SCSITaskClass &src );	
		void operator = ( SCSITaskClass &src );
		
};


#endif	/* __SCSI_TASK_CLASS_H__ */