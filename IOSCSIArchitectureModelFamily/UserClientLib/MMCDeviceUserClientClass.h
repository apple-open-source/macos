/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef __MMC_DEVICE_USER_CLIENT_CLASS_H__
#define __MMC_DEVICE_USER_CLIENT_CLASS_H__

#include <IOKit/IOCFPlugIn.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"

class MMCDeviceUserClientClass
{
	
	public:
	
		struct InterfaceMap 
		{
			IUnknownVTbl *	pseudoVTable;
			MMCDeviceUserClientClass *	obj;
		};

		MMCDeviceUserClientClass ( void );
		virtual ~MMCDeviceUserClientClass ( void );
					
	protected:
		//////////////////////////////////////
		// cf plugin interfaces
		
		static IOCFPlugInInterface 				sIOCFPlugInInterface;
		InterfaceMap 			   				fIOCFPlugInInterface;
		static MMCDeviceInterface				sMMCDeviceInterface;
		InterfaceMap							fMMCDeviceInterfaceMap;
	
		//////////////////////////////////////
		// CFPlugIn refcounting
		
		CFUUIDRef 	fFactoryId;	
		UInt32 		fRefCount;
	
		//////////////////////////////////////	
		// user client connection
		
		io_service_t 	fService;
		io_connect_t 	fConnection;
				
		// utility function to get "this" pointer from interface
		static inline MMCDeviceUserClientClass * getThis ( void * self )
			{ return ( MMCDeviceUserClientClass * ) ( ( InterfaceMap * ) self)->obj; };
	
		//////////////////////////////////////	
		// IUnknown static methods
		
		static HRESULT staticQueryInterface ( void * self, REFIID iid, void **ppv );
		virtual HRESULT QueryInterface ( REFIID iid, void **ppv );
	
		static UInt32 staticAddRef ( void * self );
		virtual UInt32 AddRef ( void );
	
		static UInt32 staticRelease ( void * self );
		virtual UInt32 Release ( void );
		
		//////////////////////////////////////
		// CFPlugin methods
		
		static IOReturn staticProbe ( void * self, CFDictionaryRef propertyTable, 
									io_service_t service, SInt32 * order );
		virtual IOReturn Probe ( CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
	
		static IOReturn staticStart ( void * self, CFDictionaryRef propertyTable, io_service_t service );
		virtual IOReturn Start ( CFDictionaryRef propertyTable, io_service_t service );
	
		static IOReturn staticStop ( void * self );
		virtual IOReturn Stop ( void );
		
		//////////////////////////////////////
		// MMCDeviceInterface methods
	
		static IOReturn staticInquiry ( void * self, SCSICmd_INQUIRY_StandardData * inquiryBuffer,
										UInt32 inqBufferSize, SCSITaskStatus * outTaskStatus,
										SCSI_Sense_Data * senseDataBuffer );
		IOReturn Inquiry ( SCSICmd_INQUIRY_StandardData * inquiryBuffer, UInt32 bufferSize,
						   SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer );

		static IOReturn staticTestUnitReady ( void * self,
											  SCSITaskStatus * outTaskStatus,
											  SCSI_Sense_Data * senseDataBuffer );
		IOReturn TestUnitReady ( SCSITaskStatus * outTaskStatus,
								 SCSI_Sense_Data * senseDataBuffer );

		static IOReturn staticGetPerformance ( void * self, UInt8 TOLERANCE, UInt8 WRITE, UInt8 EXCEPT,
											   UInt32 STARTING_LBA, UInt16 MAXIMUM_NUMBER_OF_DESCRIPTORS,
											   void * buffer, UInt16 bufferSize, SCSITaskStatus * taskStatus,
											   SCSI_Sense_Data * senseDataBuffer );
		
		virtual IOReturn GetPerformance ( UInt8 TOLERANCE, UInt8 WRITE, UInt8 EXCEPT, UInt32 STARTING_LBA,
										  UInt16 MAXIMUM_NUMBER_OF_DESCRIPTORS, void * buffer, UInt16 bufferSize,
										  SCSITaskStatus * taskStatus, SCSI_Sense_Data * senseDataBuffer );
		
		static IOReturn staticGetConfiguration ( void * self, UInt8 RT, UInt16 STARTING_FEATURE_NUMBER,
												 void * buffer, UInt16 bufferSize, SCSITaskStatus * taskStatus,
												 SCSI_Sense_Data * senseDataBuffer );
		
		virtual IOReturn GetConfiguration ( UInt8 RT, UInt16 STARTING_FEATURE_NUMBER,
											void * buffer, UInt16 bufferSize, SCSITaskStatus * taskStatus,
											SCSI_Sense_Data * senseDataBuffer );
				
		static IOReturn staticModeSense10 ( void * self, UInt8 LLBAA, UInt8 DBD, UInt8 PC,
											UInt8 PAGE_CODE, void * buffer, UInt16 bufferSize,
											SCSITaskStatus * outTaskStatus,
											SCSI_Sense_Data * senseDataBuffer );
		
		virtual IOReturn ModeSense10 ( UInt8 LLBAA, UInt8 DBD, UInt8 PC, UInt8 PAGE_CODE,
									   void * buffer, UInt16 bufferSize,
									   SCSITaskStatus * outTaskStatus,
									   SCSI_Sense_Data * senseDataBuffer );
		
		static IOReturn staticSetWriteParametersModePage ( void * self, void * buffer, UInt8 bufferSize,
														   SCSITaskStatus * taskStatus,
														   SCSI_Sense_Data * senseDataBuffer );
		
		virtual IOReturn SetWriteParametersModePage ( void * buffer, UInt8 bufferSize,
													  SCSITaskStatus * taskStatus,
													  SCSI_Sense_Data * senseDataBuffer );
		
		static IOReturn staticGetTrayState ( void * self, UInt8 * trayState );
		IOReturn GetTrayState ( UInt8 * trayState );

		static IOReturn staticSetTrayState ( void * self, UInt8 trayState );
		IOReturn SetTrayState ( UInt8 trayState );

		static IOReturn staticReadTableOfContents ( void * self, UInt8 MSF, UInt8 format,
													UInt8 trackSessionNumber,
													void * buffer, UInt16 bufferSize,
													SCSITaskStatus * outTaskStatus,
													SCSI_Sense_Data * senseDataBuffer );
		IOReturn ReadTableOfContents ( UInt8 MSF, UInt8 format, UInt8 trackSessionNumber,
									   void * buffer, UInt16 bufferSize,
									   SCSITaskStatus * outTaskStatus,
									   SCSI_Sense_Data * senseDataBuffer );

		static IOReturn staticReadDiscInformation ( void * self, void * buffer, UInt16 bufferSize,
													SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer);
		IOReturn ReadDiscInformation ( void * buffer, UInt16 bufferSize,
									   SCSITaskStatus * outTaskStatus,
									   SCSI_Sense_Data * senseDataBuffer );

		static IOReturn staticReadTrackInformation ( void * self, UInt8 addressNumberType,
													 UInt32 lbaTrackSessionNumber,
													 void * buffer, UInt16 bufferSize,
													 SCSITaskStatus * outTaskStatus,
													 SCSI_Sense_Data * senseDataBuffer );
		IOReturn ReadTrackInformation ( UInt8 addressNumberType, UInt32 lbaTrackSessionNumber,
										void * buffer, UInt16 bufferSize, SCSITaskStatus * outTaskStatus,
									    SCSI_Sense_Data * senseDataBuffer );

		static IOReturn staticReadDVDStructure ( void * self, UInt32 logicalBlockAddress, UInt8 layerNumber,
										 		 UInt8 format, void * buffer, UInt16 bufferSize,
												 SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer );
										 		   
		IOReturn ReadDVDStructure ( UInt32 logicalBlockAddress, UInt8 layerNumber,
									UInt8 format, void * buffer, UInt16 bufferSize,
									SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer );

		static SCSITaskDeviceInterface ** 	staticGetSCSITaskDeviceInterface ( void * self );
		SCSITaskDeviceInterface ** 			GetSCSITaskDeviceInterface ( void );
		
	private:
		
		// Disable Copying
		MMCDeviceUserClientClass ( MMCDeviceUserClientClass &src );
		void operator = ( MMCDeviceUserClientClass &src );
	
	public:
	
		static IOCFPlugInInterface ** alloc ( void );
	
};


#endif /* __MMC_DEVICE_USER_CLIENT_CLASS_H__ */