/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
/*
	File:		DVFamilyPriv.h

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Written by:	Steve Smith

	Copyright:	© 1996-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(KW)	Kevin Williams
		(jkl)	Jay Lloyd
		(CP)	Collin Pieper
		(CLP)	Collin Pieper
		(SS)	Steve Smith

	Change History (most recent first):

		<16>	 6/17/99	jkl		Added fwClientID to DVGetLocalFWReferenceID call to make the
									fwClientID part of the device info so IDH can execute
									getDeviceStandard by itself.
		<15>	  6/8/99	KW		Added a __cplusplus extern c ifdef.
		<14>	  1/4/99	GDW		Changed DVFamily names.
		<13>	 12/9/98	SS		Changed clock component instance field to clock component in
									DVDriverData struct.
		<12>	11/19/98	SS		Added fields to family struct to support queuing of DV events.
		<11>	11/16/98	SS		Added AVCisEnabled & enabled count to per-device struct. Changed
									internal notification structs for PB queuing.
		<10>	11/16/98	SS		Added enum and param struct for setting the deviceID in the
									driver (for notification purposes).
		 <9>	11/12/98	SS		Added localID & clockInstance to per-device data struct. Changed
									kDVGetID enum & param struct to kDVGetDeviceUniqueID. Added
									kDVGetLocalFWReferenceID enum and param struct.
		 <8>	10/28/98	SS		Added read/write enabled flags and reader count to the device
									structure.
		 <7>	10/22/98	jkl		Nothing. Just removed outdated System7 def.
		 <6>	 9/17/98	SS		Added generic dv event notification support. Removed AppleEvent
									support.
		 <5>	 3/12/98	SS		Added a few "driverID"-based declarations to this file that were
									originally in DVFamily.h. This is so we can move towards
									deviceIDs and refNums externally, but maintain the use of
									driverIDs internally, at least for now.
		 <4>	  2/4/98	CP		Remove unnecessary dependencies
		 <3>	 1/19/98	CP		Moved structures that were public ans shouldn't have been here
									for safe harbour...
		 <2>	 1/12/98	SS		Checked in Collin's changes: modified data structures and added
									AppleEvent support.
		 <1>	 8/18/97	CLP		first checked in
		 <4>	 2/19/97	SS		Updated to 1.0a2 FSL
		 <3>	 2/12/97	AW		Updated to 1.0d18 FSL
		 <2>	10/31/96	SS		Misc changes to the isoch and buffering data structures.
									Changes to support Blaze d16 API.

*/

//
//	DVFamilyPriv.h
//

#ifndef	__DVFAMILYPRIV__
#define __DVFAMILYPRIV__


#include <Types.h>
#include <NameRegistry.h>
#include <DriverServices.h>
#include <AppleEvents.h>
#include <Components.h>

#include "GenericDriverFamily.h"
#include "FireWire.h"
#include "DVFamily.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////
//
// types
//
///////////////////////////////////////////////////////////////////////

// holds our driver identification...
// NOTE: for this implementation, DVDriverID, DVDeviceID, 
// and DVDeviceRefNum are synonymous.
typedef UInt32 DVDriverID;

enum
{
	kInvalidDVDriverID		= 0,
	kMaxQueuedDVEvents		= 32
};

// one for each driver...

typedef struct DVDriverDataStruct DVDriverData, *DVDriverDataPtr;
								
struct DVDriverDataStruct
{
	DVDriverDataPtr				pNextDVDriverData;		// Pointer to next driver in driver list.

	DVDriverID					dvDriverID;				// ID referencing this data record.
	DriverRefNum				driverRefNum;			// Device Manager driver ref num.
	RegEntryID					deviceRegistryID;		// Name registry ID of DV device
	CSRNodeUniqueID				uniqueID;				// unique device ID
	FWReferenceID				localID;				// local FW reference ID
	FWReferenceID				fwClientID;				// camera's reference ID
	Component					clock;					// this device's clock component
	
	Str255						name;					// DV device name
	
	Boolean						writeEnabled;			// true if device is enabled for writing
	Boolean						readEnabled;			// true if device is enabled for reading
	UInt32						numReaders;				// Number of readers connected to device
	Boolean						AVCEnabled;				// true if device is enabled for AVC transactions
	UInt32						numControllers;			// Number of controllers connected to device
	
	Boolean						driverOpened;			// True if we've opened the driver.
	UInt32						numConnections;			// Number of connections to this driver.
	Boolean						deviceDisconnected;		// True if device was disconnected.
};

// notification stuff
typedef struct DVNotificationEntryStruct {
	QElemPtr							qLink;			// next queue element
//	DVEventRecord						eventRec;
	DVCDeviceID							deviceID;
	UInt32								wantedEvents;
	DVCNotifyProc						notifyProc;
	void								*userRefCon;
} DVNotificationEntry, *DVNotificationEntryPtr;

typedef struct DVEventEntryStruct {
	QElemPtr							qLink;			// next queue element
	DVCNotifyProc						notifyProc;
	void								*userRefCon;
	DVCEventRecord						eventRec;
} DVEventEntry, *DVEventEntryPtr;	

// the main family structure...

typedef struct DVFamilyDataStruct
{
	UInt32					useCount;
	DVDriverDataPtr			pDVDriverList;
	UInt32					numDVDrivers;
	QHdrPtr					notificationQueue;
	
	DVEventEntry			events[ kMaxQueuedDVEvents ];
	QHdrPtr					availableDVEvents;
	QHdrPtr					receivedDVEvents;
	NMRec					dvNMRec;
	UInt32					nmIsInstalled;			// for compare and swap
} DVFamilyData, * DVFamilyDataPtr;


///////////////////////////////////////////////////////////////////////
//
// external prototypes
//
///////////////////////////////////////////////////////////////////////


// for the expert...
OSStatus 	DVHandleDeviceAdded( GDFDeviceEventDataPtr pGDFDeviceEventData );
OSStatus 	DVHandleDeviceRemoved( GDFDeviceEventDataPtr pGDFDeviceEventData );

OSErr 		DVCPostEvent( DVCEventRecordPtr	pEvent );



//////////////////////////////////////////////////////////////////////
//
// structures for communicating with the driver
//
//////////////////////////////////////////////////////////////////////

// for sending commands with no data to driver
typedef struct DVBasicCmdParamsStruct 
{
	UInt32					interfaceSelector;
} DVBasicCmdParams, *DVBasicCmdParamsPtr;



// for getting buffers and sizes
// used by DVGetEmptyFrame & DVReadFrame
 
typedef struct DVGetBufferParamsStruct
{
	UInt32					interfaceSelector;
	Ptr						*ppBuffer;
	UInt32					*pBufferSize;
} DVGetBufferParams, *DVGetBufferParamsPtr;



// for handing buffers back to the driver
// used by DVWriteFrame & DVReleaseFrame

typedef struct DVPassBufferParamsStruct 
{
	UInt32					interfaceSelector;
	Ptr						pBuffer;
} DVPassBufferParams, *DVPassBufferParamsPtr;



// for getting a unique camera id from the driver

typedef struct DVGetDeviceUniqueIDParamsStruct 
{
	UInt32					interfaceSelector;
	CSRNodeUniqueID			id;
} DVGetDeviceUniqueIDParams, *DVGetDeviceUniqueIDParamsPtr;


// for getting the local FW Ref id from the driver

typedef struct DVGetLocalFWReferenceIDParamsStruct 
{
	UInt32					interfaceSelector;
	FWReferenceID			id;
	FWReferenceID			fwClientID;
} DVGetLocalFWReferenceIDParams, *DVGetLocalFWReferenceIDParamsPtr;


// for setting the family's deviceID for the driver

typedef struct DVSetFamilyDeviceIDParamsStruct 
{
	UInt32					interfaceSelector;
	DVCDeviceID				id;
	DVCDeviceID				fwClientID;
} DVSetFamilyDeviceIDParams, *DVSetFamilyDeviceIDParamsPtr;


// for sending AVC commands
typedef struct DVAVCTransactionParamsStruct {
	UInt32					interfaceSelector;
	Ptr						commandBufferPtr;
	UInt32					commandLength;
	Ptr						responseBufferPtr;
	UInt32					responseBufferSize;
	FCPResponseHandlerPtr	responseHandler;
} DVAVCTransactionParams, *DVAVCTransactionParamsPtr;

/////////////////////////////////////////////////////////
//
// driver interface selectors
//

enum {
	kAVCInitialize				= 1,
	kAVCTerminate				= 2,
	kAVCDoTransaction			= 3,
	
	kDVCEnableIsochRead			= 0x10,
	kDVCDisableIsochRead		= 0x11,
	kDVCReadIsochData			= 0x12,
	kDVCReleaseReadBuffer		= 0x13,

	kDVCEnableIsochWrite		= 0x20,
	kDVCDisableIsochWrite		= 0x21,
	kDVCGetEmptyFrame			= 0x22,
	kDVCWriteFrame				= 0x23,
	
	kDVCEnableDVCGrab			= 0x100,
	kDVCDisableDVCGrab			= 0x101,
	kDVCGrabOneDVCFrame			= 0x102,
	kDVCReleaseDVCFrame			= 0x103,
	
	kDVGetDeviceUniqueID		= 0x30,
	kDVGetLocalFWReferenceID	= 0x31,
	kDVSetDeviceFamilyID		= 0x32
};

enum {
	kServiceTypeAVCServices	= 'avc '
};


#ifdef __cplusplus
}
#endif


#endif __DVFAMILYPRIV__
