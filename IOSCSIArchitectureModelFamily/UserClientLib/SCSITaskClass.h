/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#ifndef __SCSI_TASK_CLASS_H__
#define __SCSI_TASK_CLASS_H__

#include <IOKit/IOCFPlugIn.h>

__BEGIN_DECLS
#include <sys/semaphore.h>
__END_DECLS

#include "SCSITaskLib.h"
#include "SCSITaskDeviceClass.h"

__BEGIN_DECLS
extern void * SCSITaskUserClientLibFactory ( CFAllocatorRef allocator, CFUUIDRef typeID );
__END_DECLS

class SCSITaskClass
{

public:

	struct InterfaceMap 
	{
        IUnknownVTbl *	pseudoVTable;
        SCSITaskClass *	obj;
    };

	SCSITaskClass ( void );
	virtual ~SCSITaskClass ( void );
	
	static void sAbortAndReleaseTasks ( const void * value, void * context );
	static void sSetConnectionAndPort ( const void * value, void * context );
	
	virtual IOReturn Init ( SCSITaskDeviceClass * scsiTaskDevice,
							io_connect_t connection,
							mach_port_t asyncPort );

	virtual IOReturn SetConnectionAndPort ( io_connect_t connection, mach_port_t asyncPort );
	
protected:
	
	//////////////////////////////////////
	// CFPlugIn interfaces
	
	static SCSITaskInterface	sSCSITaskInterface;
	InterfaceMap				fSCSITaskInterfaceMap;

	//////////////////////////////////////
	// CFPlugIn refcounting
	
	UInt32 						fRefCount;
	
	//////////////////////////////////////
	// user client connection
	
	SCSITaskDeviceClass *		fSCSITaskDevice;
	io_connect_t				fConnection;	// connection to user client in kernel
	mach_port_t					fAsyncPort;		// async port for callback from kernel
	void *						fCallbackRefCon;
	SCSITaskCallbackFunction 	fCallbackFunction;
	
	UInt32						fTaskReference;	// reference to kernel task object
	
	// Cached variables for Getter functions
	UInt8						fCDBSize;
	SCSICommandDescriptorBlock	fCDB;
	UInt32						fTimeoutDuration;
	UInt64						fRealizedTransferCount;
	bool						fIsTaskSynch;
	SCSIServiceResponse			fServiceResponse;
	SCSITaskStatus				fTaskStatus;
	
	//////////////////////////////////////	
	// IUnknown Interface methods
	
	static HRESULT 	staticQueryInterface ( void * self, REFIID iid, void ** ppv );
	virtual HRESULT QueryInterface ( REFIID iid, void ** ppv );

	static UInt32 	staticAddRef ( void * self );
	virtual UInt32 	AddRef ( void );

	static UInt32 	staticRelease ( void * self );
	virtual UInt32 	Release ( void );

	//////////////////////////////////////	
	// SCSITask Interface methods
	static Boolean		staticIsTaskActive ( void * task );
	virtual Boolean  	IsTaskActive ( void );

	static IOReturn 	staticSetTaskAttribute ( void * task, SCSITaskAttribute inAttributeValue );
	virtual IOReturn 	SetTaskAttribute ( SCSITaskAttribute inAttributeValue );

	static IOReturn 	staticGetTaskAttribute ( void * task, SCSITaskAttribute * outAttribute );
	virtual IOReturn 	GetTaskAttribute ( SCSITaskAttribute * outAttribute );

	static IOReturn 	staticSetCommandDescriptorBlock ( void * task, UInt8 * inCDB, UInt8 inSize );
	virtual IOReturn 	SetCommandDescriptorBlock ( UInt8 * inCDB, UInt8 inSize );

	static UInt8 		staticGetCommandDescriptorBlockSize ( void * task );
	virtual UInt8 		GetCommandDescriptorBlockSize ( void );

	static IOReturn 	staticGetCommandDescriptorBlock ( void * task, UInt8 * outCDB );
	virtual IOReturn 	GetCommandDescriptorBlock ( UInt8 * outCDB );
	

	static IOReturn 	staticSetScatterGatherEntries ( void * task,
									   IOVirtualRange * inScatterGatherList,
									   UInt8 inScatterGatherEntries,
									   UInt64 transferCount,
									   UInt8 transferDirection );
	virtual IOReturn 	SetScatterGatherEntries ( IOVirtualRange * inScatterGatherList,
									   UInt8 inScatterGatherEntries,
									   UInt64 transferCount,
									   UInt8 transferDirection );
	
	static IOReturn 	staticSetTimeoutDuration ( void * task, UInt32 timeoutDurationMS );
	virtual IOReturn 	SetTimeoutDuration ( UInt32 timeoutDurationMS );

	static UInt32 		staticGetTimeoutDuration ( void * task );
	virtual UInt32 		GetTimeoutDuration ( void );

	static IOReturn 	staticSetTaskCompletionCallback ( void * task,
												 SCSITaskCallbackFunction callback,
												 void * refCon );
	virtual IOReturn 	SetTaskCompletionCallback ( 
												 SCSITaskCallbackFunction callback,
												 void * refCon );

	static IOReturn 	staticExecuteTaskAsync ( void * task );
	virtual IOReturn 	ExecuteTaskAsync ( void );
	
	static IOReturn 	staticExecuteTaskSync ( void * task, SCSI_Sense_Data * senseDataBuffer,
												SCSITaskStatus * taskStatus, UInt64 * realizedTransferCount );
	virtual IOReturn 	ExecuteTaskSync ( SCSI_Sense_Data * senseDataBuffer,
										  SCSITaskStatus * taskStatus,
										  UInt64 * realizedTransferCount );
	
	static IOReturn 	staticAbortTask ( void * task );
	virtual IOReturn 	AbortTask ( void );

	static IOReturn 	staticGetServiceResponse ( void * task, SCSIServiceResponse * outResponse );
	virtual IOReturn 	GetServiceResponse ( SCSIServiceResponse * outResponse );

	static IOReturn 	staticGetTaskState ( void * task, SCSITaskState * outState );
	virtual IOReturn 	GetTaskState ( SCSITaskState * outState );

	static IOReturn 	staticGetTaskStatus ( void * task, SCSITaskStatus * outStatus );
	virtual IOReturn 	GetTaskStatus ( SCSITaskStatus * outStatus );

	static UInt64 		staticGetRealizedDataTransferCount ( void * task );
	virtual UInt64 		GetRealizedDataTransferCount ( void );

	static IOReturn 	staticGetAutoSenseData ( void * task, SCSI_Sense_Data * senseDataBuffer );
	virtual IOReturn 	GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer );
	
	static void 		staticTaskCompletion ( void * refcon, IOReturn result, void ** args, int numArgs );
	virtual void 		TaskCompletion ( IOReturn result, void ** args, int numArgs );

	// Method for getting the "this" pointer
	static inline SCSITaskClass * getThis ( void * task )
		{ return ( SCSITaskClass * ) ( ( InterfaceMap * ) task)->obj; };
	
private:
	
	// Disable copy constructor
	SCSITaskClass ( SCSITaskClass &src );	
	void operator = ( SCSITaskClass &src );

public:
	
	static SCSITaskInterface ** alloc ( SCSITaskDeviceClass * scsiTaskDevice,
										io_connect_t connection,
										mach_port_t asyncPort );
	
};


#endif /* !__SCSI_TASK_CLASS_H__ */
