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
	File:		DVFamilyLib.c

	Contains:	This is the client API for talking to DV FireWire devices.

	Version:	xxx put version here xxx

	Written by:	Steve Smith

	Copyright:	й 1996-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(KW)	Kevin Williams
		(jkl)	Jay Lloyd
		(RS)	Richard Sepulveda
		(CP)	Collin Pieper
		(CLP)	Collin Pieper
		(AW)	Adrienne Wang
		(SS)	Steve Smith

	Change History (most recent first):

		<30>	 6/17/99	jkl		Added fwClientID to DVGetLocalFWReferenceID call to make the
									fwClientID part of the device info so IDH can execute
									getDeviceStandard by itself.
		<29>	  1/4/99	GDW		Changed DVFamily names.
		<28>	 12/9/98	SS		DVGetDeviceInfo() now fills in the FireWire ID for the local
									node in the DVDeviceInfo struct. DVGetDeviceClock now returns a
									component instead of a component instance.
		<27>	11/27/98	SS		Changed DVIdle() to process all task-level events that are
									queued, rather than just one.
		<26>	11/23/98	SS		DVCancelNotification() now removes any occurrance of the given
									notification from the list of DV events that were queued for
									task time.
		<25>	11/20/98	SS		Moved DVPostEvents() call to the bottom of all functions that
									call it.
		<24>	11/19/98	SS		Added DVPostEvent() calls to Enable/Disable functions. Changed
									PostEventSIH to queue certain events that need to be reported at
									task level (see comments in PostEventSIH()). Added DVIdle()
									call, but currently only calling it from a Notification Mgr
									proc.
		<23>	11/16/98	SS		Beefed up DVGetDeviceInfo(). Notification now uses PB queues.
									NotifyMeWhen now sets deviceID based on passed connectionID.
									Changed DVPostEvent() to always run at secondary interrupt level
									to solve some reentry problems.
		<22>	11/13/98	SS		Added DVGetDeviceClock() implementation.
		<21>	 11/6/98	KW		in DVGetDeviceInfo, return if device is read enabled or write
									enabled. Allows clients to determine if devices are availiable
									for video in
		<20>	10/30/98	SS		More updates and bug fixes for hot swapping.
		<19>	10/29/98	SS		Was using local ptr before it was initialized in
									DVOpenDeviceConnection(). Doh!
		<18>	10/28/98	SS		Changed open/close connection routines to reinstate
									numConnections. Changed enable/disable read/write routines to
									better police when they can occur, and to enable multiple
									readers. These changes help address hot plugging, multiple
									client and multiple device issues.
		<17>	10/21/98	SS		Changed DVEnableRead() and DVDisableRead() to enable multiple
									readers, i.e. both vdig and sound input driver.
		<16>	 9/17/98	SS		Restructured file & added Sean-like pragmas to make it easier to
									find things. Added generic dv event notification support. Added
									dummy Enable/Disable AVC transaction routines. Removed
									AppleEvent support.
		<15>	 9/10/98	SS		Somewhat gratuitous semantic change in the DV API to further
									abstract the implementation, specifically, from DVDeviceRefNum
									to DVDeviceConnectionID, and from DVOpen/CloseDriver to
									DVOpen/CloseDeviceConnection.
		<14>	  9/3/98	SS		Checked in first pass of dv device info stuff.
		<13>	 3/19/98	RS		Remove DVNames usage due to strange crashes and its non-usage in
									the current version of Oxcart.
		<12>	 3/16/98	SS		Oops. I put the refNum stuff in the wrong place in DVOpenDriver.
									I moved it and un-did Jay's changes.
		<11>	 3/15/98	jkl		Commented out the return noErr line at the start of
									DVOpenDriver. The exporter was failing when it called this and
									expected to get a valid refNum.
		<10>	 3/12/98	SS		Changed implementation of DV driver API to use deviceIDs and
									refNums instead of driverIDs everywhere. DVDriverIDs are still
									used internally for now.
		 <9>	  3/8/98	RS		Added DVIsEnabled() call to library. It returns whether
									specified device is enabled. CP also eliminated OpenDriver and
									CloseDriver function from library.
		 <8>	  2/6/98	SS		Backing out the disconnection checks for a6 build.
		 <7>	  2/4/98	CP		Change DVGetDevicesStandard to return our own standard types and
									return an error for unknown standards...
		 <6>	 1/26/98	CP		Added more checks for disconnection errors...
		 <5>	 1/21/98	SS		Changed AVC stuff, so need to add #include "AVCSupport.h".
		 <4>	 1/19/98	CP		Added driver ID validation code and fixed a little bug in
									DVDriverClose...
		 <3>	 1/14/98	GDW		New interfaces.
		 <2>	 1/12/98	SS		Checked in Collin's changes: added new API implementation,
									removed multitude of specific AVC calls. Some functions formerly
									in this file are now in DVFamilyInternal.c.
		 <1>	 8/18/97	CLP		first checked in
		 <9>	 3/27/97	SS		Forgot to take out debug breaks.
		 <8>	 3/27/97	SS		Moved global allocation/deallocation from the expert to
									init/term routines. Added function to determine whether lib is
									fully initialized.
		 <7>	  3/5/97	AW		minor change
		 <6>	  3/4/97	AW		Changed AVCEnableDVCGrab so that it checks if device is NTSC/PAL
									and allocates mem appropriately
		 <5>	  3/3/97	AW		Fixed wrong command length in GetMediumInfo; added AVC commands
									for setting/getting signals
		 <4>	 2/19/97	SS		Updated to 1.0a2 FSL
		 <3>	 2/12/97	AW		Updated to 1.0d18 FSL
		 <2>	10/31/96	SS		Added helper routines for device control.

*/

#include <Notification.h>

#include "DVFamilyPriv.h"
#include "DVFamilyInternal.h"
#include "Processes.h"
#include "AVCSupport.h"


///////////////////////////////////////////////////////////////////////////
//
// globals
//
///////////////////////////////////////////////////////////////////////////

DVFamilyDataPtr		gpFamilyGlobals = nil;

///////////////////////////////////////////////////////////////////////////
//
// internal prototypes
//
///////////////////////////////////////////////////////////////////////////

OSStatus		PostEventSIH( void* p1, void* p2 );
OSErr			DVIdle( void );
void 			myNMHandler( NMRecPtr pNM );

#pragma mark ееееееееее Public Interface Calls ееееееееее

////////////////////////////////////////////////////////////////////////////////
// These are the application level interfaces routines for the DVFamily
////////////////////////////////////////////////////////////////////////////////

#pragma mark -
#pragma mark ееееееееее Device Management ееееееееее

//////////////////////////////////////////////
//
// DVCountDevices
//
//   This routine counts the number of attatched DV devices
//

UInt32 DVCCountDevices( void )
{
	return( gpFamilyGlobals->numDVDrivers );
}

//////////////////////////////////////////////
//
// DVGetIndDevice
//
//   Given an index in the range of 1 to the number of devices returned by 
// DVCountDevices, DVGetIndDevice returns a deviceID.  If you call DVGetIndDevice
// repeatedly over the entire range of the index, it returns unique device IDs for
// all currently connected and active DV devices.
//
//zzz maybe some speed optimizations for repetative calls would be nice
//    but how many DV devices are going to be connected to a machine anyway...

OSErr DVCGetIndDevice( DVCDeviceID * pDVDevice, UInt32 index )
{
	DVDriverDataPtr				pDVDriverData;
	UInt32						i;
	UInt32						count;
	OSErr						error = noErr;

	if( (index <= gpFamilyGlobals->numDVDrivers) && (index > 0 ) )
	{
		count = gpFamilyGlobals->numDVDrivers - index + 1; // cause devices are inserted at the head
		pDVDriverData = gpFamilyGlobals->pDVDriverList;
		for( i = 1; i < count; i++ )	
		{
			pDVDriverData = pDVDriverData->pNextDVDriverData;
		}
		*pDVDevice = (DVCDeviceID) pDVDriverData;
	}
	else
	{
		*pDVDevice = kInvalidDVDeviceID;
		error = paramErr;
	}
	
	return( error );
}

//////////////////////////////////////////////
//
// DVGetDeviceInfo
//
//   This routine returns DV device info
//

OSErr DVCGetDeviceInfo( DVCDeviceID deviceID, DVCDeviceInfoPtr pInfo )
{
	OSErr				error = noErr;
	DVDriverDataPtr		pDriverData;
	register char		*pSrc, *pDest;
	int					i;
	
	// make sure we've got a valid driverID
	// (actually, this could be a connID or a deviceID)
	error = DVIsValidID( (DVDriverID) deviceID );
	if( error )
		return( error );

	// for now, the deviceID _is_ the ptr to data
	pDriverData = (DVDriverDataPtr) deviceID;
	
	pInfo->dvDeviceID 				= pDriverData->dvDriverID;
	pInfo->uniqueID 				= pDriverData->uniqueID;
	pInfo->vendorID 				= nil;								// for now...
	pInfo->regEntryID 				= pDriverData->deviceRegistryID;

	pInfo->deviceIsOnline 			= !pDriverData->deviceDisconnected;

	pInfo->AVCisEnabled 			= pDriverData->AVCEnabled;

	pInfo->readChannel.isEnabled 	= pDriverData->readEnabled;
	pInfo->writeChannel.isEnabled 	= pDriverData->writeEnabled;

	// copy name str
	pDest = (char*) &(pInfo->deviceName);
	pSrc = (char*) &(pDriverData->name);
	for( i = 0; i <= 255; i++ )
		pDest[i] = pSrc[i];
	
	// remember our local node id
	pInfo->localNodeID 				= pDriverData->localID;
	pInfo->fwClientID 				= pDriverData->fwClientID;

	return( error );
}

//////////////////////////////////////////////
//
// DVGetDeviceName
//
//   This routine returns the name of a specific DV device
//

OSErr DVCGetDeviceName( DVCDeviceID deviceID, char * str )
{
	DVDriverDataPtr				pDVDriverData;
	OSErr						error = noErr;
	short						i;
	
	// make sure we've got a valid deviceID
	error = DVIsValidID( (DVDriverID) deviceID );
	if( error )
		return( error );
		
	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) deviceID;
	
	// copy name str
	for( i = 0; i <= 255; i++ )
		str[i] = pDVDriverData->name[i];
		
	return( error );
}

//////////////////////////////////////////////
//
// DVSetDeviceName
//
//   This routine sets the name of a specific DV device
//

OSErr DVCSetDeviceName( DVCDeviceID deviceID, char * str )
{
	DVDriverDataPtr				pDVDriverData;
	OSErr						error = noErr;
	short						i;
	
	// make sure we've got a valid deviceID
	error = DVIsValidID( (DVDriverID) deviceID );
	if( error )
		return( error );
		
	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) deviceID;
	
	// copy name str
	for( i = 0; i <= 255; i++ )
		pDVDriverData->name[i] = str[i];
	
//еее We aren't going to support DVNames in this version of Oxcart
//	// add name to prefs file (if add fails, don't pass error code back up to client)
//	DVAddName( &(pDVDriverData->uniqueID), pDVDriverData->name );
	
	return( error );
}

//////////////////////////////////////////////
//
// DVOpenDeviceConnection
//
//   This routine opens a connection to a particular driver
//

OSErr DVCOpenDeviceConnection( DVCDeviceID deviceID, DVCDeviceConnectionID *pConnID )
{
	DVDriverDataPtr		pDVDriverData;
	OSErr				error = noErr;

	// make sure we've got a valid deviceID
	error = DVIsValidID( (DVDriverID) deviceID );
	if( error )
		return( error );

	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) deviceID;
	
	// don't bother continuing if we're disconnected
	if( pDVDriverData->deviceDisconnected )
		return( kDVDisconnectedErr );
		
	// add a connection
	pDVDriverData->numConnections++;
	
	// this is redundant, but we'll do it so we can possibly enhance
	// multiple client support sometime in the future.
	*pConnID = (DVCDeviceConnectionID) deviceID;
	
	//zzz could actually open driver here, but advantage would probably be minimal...
	
	return( error );
}

//////////////////////////////////////////////
//
// DVCloseDeviceConnection
//
//   This routine opens a connection to a particular driver
//

OSErr DVCCloseDeviceConnection( DVCDeviceConnectionID connID )
{
	DVDriverDataPtr		pDVDriverData;
	OSErr				error = noErr;
	
	// make sure we've got a valid deviceID
	error = DVIsValidID( (DVDriverID) connID );
	if( error )
		return( error );
		
	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) connID;
	
	// remove a connection
	pDVDriverData->numConnections--;
	
	//zzz for apple events managing driver disposal here would be good
	if( ( pDVDriverData->numConnections == 0 ) && (  pDVDriverData->deviceDisconnected ) )
		DVDisposeDriver( (DVDriverID) connID );
		
	return( error );
}

//////////////////////////////////////////////
//
// DVGetDeviceClock
//
//   This routine opens a connection to a particular driver
//

OSErr DVCGetDeviceClock( DVCDeviceID deviceID, Component *clock )
{
	*clock = ((DVDriverDataPtr)deviceID)->clock;
	
	return( noErr );
}


#pragma mark -
#pragma mark ееееееееее DV Event Notification Calls ееееееееее

///////////////////////////////////////////////////////////////////////
//
// DVNewNotification
//
//	create new notification record
//
OSErr DVCNewNotification( DVCDeviceConnectionID connID, DVCNotifyProc notifyProc,
						void *userData, DVCNotificationID *pNotifyID )
{
	DVNotificationEntryPtr	pEntry;
	DVCDeviceID				deviceID;
	OSErr					error = noErr;

	// check the parameters
	if ( notifyProc == nil )
		error = paramErr;
	
	// create new entry
	if ( error == noErr )
	{
		pEntry = (DVNotificationEntryPtr) 
				PoolAllocateResident( sizeof( DVNotificationEntry ), true );
		if ( pEntry == nil )
			error = memFullErr;
	}
	
	// get the deviceID from the connectionID
	deviceID = (DVCDeviceID) connID;
	
	// fill it out
	if ( error == noErr )
	{
		pEntry->deviceID						= deviceID;
		pEntry->wantedEvents					= nil;
		pEntry->notifyProc						= notifyProc;
		pEntry->userRefCon						= userData;
	
		*pNotifyID = (DVCNotificationID) pEntry;	// notification id

		// put new entry at the back of the line
		error = PBEnqueueLast( (QElemPtr) pEntry, gpFamilyGlobals->notificationQueue );
	}
	
	return error;	
}
	
//////////////////////////////////////////////
//
// DVNotifyMeWhen
//
//	activate notification 
//
OSErr DVCNotifyMeWhen( DVCDeviceConnectionID connID, DVCNotificationID notifyID, UInt32 events)
{
	DVNotificationEntryPtr	pEntry;
	DVCDeviceID				deviceID;
	OSErr					error = noErr;
	
	// check the parameters
	if ( events & kDVEveryEvent == nil )
			error = paramErr;
	
	// get the deviceID from the connectionID
	deviceID = (DVCDeviceID) connID;
	
	if ( error == noErr )
	{
		pEntry = (DVNotificationEntryPtr) notifyID;
		
		if ( pEntry != nil )
		{
			pEntry->wantedEvents = events;
			// this is sort of a back door - you can specify any device here
			pEntry->deviceID = deviceID;
		}
		else
			error = paramErr;
	}
		
	return error;	
}

//////////////////////////////////////////////
//
// DVCancelNotification
//
//	deactivate notification
//
OSErr DVCCancelNotification( DVCDeviceConnectionID connID, DVCNotificationID notifyID )
{
	DVNotificationEntryPtr	pEntry;
	OSErr					error = noErr;
	DVEventEntryPtr			pEventEntry;
	
	pEntry = (DVNotificationEntryPtr) notifyID;
	
	if ( pEntry != nil )
	{
		// don't notify this guy
		pEntry->wantedEvents = 0L;
		
		// check the queue to make sure he's not about to be notified.
		pEventEntry = (DVEventEntryPtr) gpFamilyGlobals->receivedDVEvents->qHead;
		while ( pEventEntry )
		{
			if ( pEventEntry->eventRec.eventHeader.notifID == notifyID )
				PBDequeue( (QElemPtr) pEventEntry, gpFamilyGlobals->receivedDVEvents );
				// could be in the queue more than once, so keep going...
				
			pEventEntry = (DVEventEntryPtr) pEventEntry->qLink;
		}
	}
	else
		error = paramErr;
		
	return error;	
}

//////////////////////////////////////////////
//
// DVDisposeNotification
//
//	remove notification from list
//
OSErr DVCDisposeNotification( DVCDeviceConnectionID connID, DVCNotificationID notifyID )
{
	DVNotificationEntryPtr	pEntry;
	OSErr					error = noErr;
	
	// we're not going to do a check, but it's REAL important not to call
	// this function from anywhere but task level.

	// go find the entry and remove it from the list
	pEntry = (DVNotificationEntryPtr) notifyID;
	
	if ( pEntry != nil )
	{
		PBDequeue( (QElemPtr) pEntry, gpFamilyGlobals->notificationQueue );
		PoolDeallocate( (void*) pEntry );
	}
	else
		error = paramErr;
	
	return error;	
}


#pragma mark -
#pragma mark ееееееееее DV Isoch Read Calls ееееееееее

//////////////////////////////////////////////
//
// DVEnableRead
//
//   This routine initializes the driver for input
//

OSErr DVCEnableRead( DVCDeviceConnectionID connID )
{
	DVBasicCmdParams			intParams;
	OSErr						error = noErr;
	DVDriverDataPtr				pDVDriverData;
	DVCEventRecord				theEvent;
		
	// make sure we've got a valid driverID
	error = DVIsValidID( (DVDriverID) connID );
	if( error )
		return( error );

	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) connID;
	
	// make sure we're not already write enabled
	if ( pDVDriverData->writeEnabled )
		return ( kAlreadyEnabledErr );
		
	// add a connection
	pDVDriverData->numReaders++;
	
	// if this is the first connection, call the driver
	// to set the read up.
	if ( pDVDriverData->numReaders == 1 )
	{
		// set up driver's enable isoch read parameters
		intParams.interfaceSelector = kDVCEnableIsochRead;
	
		error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );
		
		if ( error == noErr )
			pDVDriverData->readEnabled = true;
	}

	if ( error == noErr )
	{
		// post a DV event to let the curious know...
		theEvent.eventHeader.deviceID	= (DVCDeviceID) connID;
		theEvent.eventHeader.theEvent 	= kDVIsochReadEnabled;
		DVCPostEvent( &theEvent );
	}

	return( error );
}

//////////////////////////////////////////////
//
// DVDisableRead
//
//   This routine deinitializes the driver for input
//

OSErr DVCDisableRead( DVCDeviceConnectionID connID )
{
	DVBasicCmdParams			intParams;
	OSErr						error = noErr;
	DVDriverDataPtr				pDVDriverData;
	DVCEventRecord				theEvent;

	// make sure we've got a valid driverID
	error = DVIsValidID( (DVDriverID) connID );
	if( error )
		return( error );

	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) connID;
	
	// delete a connection
	pDVDriverData->numReaders--;
	
	// if this is the last connection, call the driver
	// to tear down the read.
	if ( pDVDriverData->numReaders == 0 )
	{
		// set up driver's disable isoch read parameters
		intParams.interfaceSelector = kDVCDisableIsochRead;
	
		error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );
		
		// even if there's an error, we're done
		pDVDriverData->readEnabled = false;
	}
	
	if ( error == noErr )
	{
		// post a DV event to let the curious know...
		theEvent.eventHeader.deviceID	= (DVCDeviceID) connID;
		theEvent.eventHeader.theEvent 	= kDVIsochReadDisabled;
		DVCPostEvent( &theEvent );	
	}

	return( error );
}

//////////////////////////////////////////////
//
// DVReadFrame
//
//   This routine reads a frame of data from the DV device
//

OSErr DVCReadFrame( DVCDeviceConnectionID connID, Ptr *ppReadBuffer, UInt32 * pSize )
{
	DVGetBufferParams			intParams;
	OSErr						error = noErr;
	
	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCReadIsochData;
	intParams.ppBuffer = ppReadBuffer;
	intParams.pBufferSize = pSize;
	
	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );	

	return( error );
}

//////////////////////////////////////////////
//
// DVReleaseFrame
//
//   This routine returns frame of data to the driver for use
//

OSErr DVCReleaseFrame( DVCDeviceConnectionID connID, Ptr pReadBuffer )
{
	DVPassBufferParams			intParams;
	OSErr						error = noErr;

	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCReleaseReadBuffer;
	intParams.pBuffer = pReadBuffer;
	
	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );	

	return( error );
}

#pragma mark -
#pragma mark ееееееееее DV Isoch Write Calls ееееееееее

//////////////////////////////////////////////
//
// DVEnableWrite
//
//   This routine initializes the driver for output
//

OSErr DVCEnableWrite( DVCDeviceConnectionID connID )
{
	DVBasicCmdParams			intParams;
	DVDriverDataPtr				pDVDriverData;
	OSErr						error = noErr;
	DVCEventRecord				theEvent;

	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) connID;
	
	// make sure we're not already enabled for reading or writing
	if ( pDVDriverData->readEnabled || pDVDriverData->writeEnabled )
		return ( kAlreadyEnabledErr );
		
	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCEnableIsochWrite;

	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );
	
	if ( error == noErr )
	{
		pDVDriverData->writeEnabled = true;
		
		// post a DV event to let the curious know...
		theEvent.eventHeader.deviceID	= (DVCDeviceID) connID;
		theEvent.eventHeader.theEvent 	= kDVIsochWriteEnabled;
		DVCPostEvent( &theEvent );
	}
		
	return( error );

}

//////////////////////////////////////////////
//
// DVDisableWrite
//
//   This routine deinitializes the driver for output
//

OSErr DVCDisableWrite( DVCDeviceConnectionID connID )
{
	DVBasicCmdParams			intParams;
	DVDriverDataPtr				pDVDriverData;
	OSErr						error = noErr;
	DVCEventRecord				theEvent;

	// extract our driver data pointer from our supposedly opaque reference
	pDVDriverData = (DVDriverDataPtr) connID;
	
	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCDisableIsochWrite;

	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );	
	
	// even if there's an error, we're done
	pDVDriverData->writeEnabled = false;
	
	// post a DV event to let the curious know...
	theEvent.eventHeader.deviceID	= (DVCDeviceID) connID;
	theEvent.eventHeader.theEvent 	= kDVIsochWriteDisabled;
	DVCPostEvent( &theEvent );

	return( error );

}

//////////////////////////////////////////////
//
// DVGetEmptyFrame
//
//   This routine retrieves an empty frame from the driver for output
//

OSErr DVCGetEmptyFrame( DVCDeviceConnectionID connID, Ptr *ppEmptyFrameBuffer, UInt32 * pSize )
{
	DVGetBufferParams			intParams;
	OSErr						error = noErr;

	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCGetEmptyFrame;
	intParams.ppBuffer = ppEmptyFrameBuffer;
	intParams.pBufferSize = pSize;
	
	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );	
	return( error );
}

//////////////////////////////////////////////
//
// DVWriteFrame
//
//   This routine sends a frame of data to the camera
//

OSErr DVCWriteFrame( DVCDeviceConnectionID connID, Ptr pWriteBuffer )
{
	DVPassBufferParams			intParams;
	OSErr						error = noErr;

	// set up driver's enable isoch read parameters
	intParams.interfaceSelector = kDVCWriteFrame;
	intParams.pBuffer = pWriteBuffer;
	
	error = DVCallDriver( (DVDriverID) connID, (Ptr) &intParams );	
	return( error );
}

#pragma mark -
#pragma mark ееееееееее AVC Transaction Calls ееееееееее

//////////////////////////////////////////////
//
// DVEnableAVCTransactions
//
//   This routine initializes the driver for 
//	 performing avc transactions
//
OSErr DVCEnableAVCTransactions( DVCDeviceConnectionID connID )
{
	OSErr						error = noErr;

	return( error );
}

//////////////////////////////////////////////
//
// DVDoAVCTransaction
//
//   This routine sends a transaction block to the driver
//

OSErr DVCDoAVCTransaction( DVCDeviceConnectionID connID, AVCTransactionParamsPtr pParams )
{
	DVAVCTransactionParams		transactionParams;
	OSErr						error = noErr;
	
	// fill out the internal tansaction block
	transactionParams.interfaceSelector 	= kAVCDoTransaction;
	transactionParams.commandBufferPtr		= pParams->commandBufferPtr;
	transactionParams.commandLength			= pParams->commandLength;
	transactionParams.responseBufferPtr		= pParams->responseBufferPtr;
	transactionParams.responseBufferSize	= pParams->responseBufferSize;
	transactionParams.responseHandler		= pParams->responseHandler;

	error = DVCallDriver( (DVDriverID) connID, (Ptr) &transactionParams );

	return( error );
}

//////////////////////////////////////////////
//
// DVDisableAVCTransactions
//
//   This routine deinitializes the driver for 
//	 performing avc transactions
//
OSErr DVCDisableAVCTransactions( DVCDeviceConnectionID connID )
{
	OSErr						error = noErr;

	return( error );
}

#pragma mark -
#pragma mark ееееееееее To be discontinued... ееееееееее

//////////////////////////////////////////////
//
// DVIsEnabled
//
//   This routine tells if this device is enabled
//

OSErr DVCIsEnabled( DVCDeviceConnectionID connID, Boolean *isEnabled)
{
	DVDriverDataPtr		pDVDriverData;
	OSErr				error = noErr;
	
	// make sure we've got a valid driverID
	error = DVIsValidID( (DVDriverID) connID );
	if( error )
		return( error );

	pDVDriverData = (DVDriverDataPtr) connID;
	
	// is it enabled
	if( pDVDriverData->numConnections > 0)
		*isEnabled = true;
	else
		*isEnabled = false;
		
	return error;
}

//////////////////////////////////////////////
//
// DVGetDeviceStandard
//
//   This routine returns the video standard
//

OSErr DVCGetDeviceStandard( DVCDeviceConnectionID connID, UInt32 * pStandard )
{
	AVCCTSFrameStruct		avcFrame;
	AVCTransactionParams	transactionParams;
	UInt8					responseBuffer[ 16 ];
	OSErr					theErr = noErr;
	UInt32					currentSignal, AVCStatus;
	
	// fill up the avc frame
	avcFrame.cmdType_respCode	= kAVCStatusInquiryCommand;
	avcFrame.headerAddress 		= 0x20;						// for now
	avcFrame.opcode 			= kAVCOutputSignalModeOpcode;
	avcFrame.operand[ 0 ] 		= kAVCSignalModeDummyOperand;
	
	// fill up the transaction parameter block
	transactionParams.commandBufferPtr		= (Ptr) &avcFrame;
	transactionParams.commandLength			= 4;
	transactionParams.responseBufferPtr		= (Ptr) responseBuffer;
	transactionParams.responseBufferSize	= 16;
	transactionParams.responseHandler		= nil;
	
	theErr = DVCDoAVCTransaction( (DVDriverID) connID, &transactionParams );

	currentSignal = ((responseBuffer[ 2 ] << 8) | responseBuffer[ 3 ]);
	AVCStatus = responseBuffer[ 0 ];
	
	*pStandard = kUnknownStandard;
	switch (currentSignal & 0x000000ff) 
	{
		case kAVCSignalModeSD525_60: 
		case kAVCSignalModeSDL525_60:
		case kAVCSignalModeHD1125_60: 
			*pStandard = kNTSCStandard;
			return( theErr );
	
		case kAVCSignalModeSD625_50: 
		case kAVCSignalModeSDL625_50: 
		case kAVCSignalModeHD1250_50: 
			*pStandard = kPALStandard;
			return( theErr );
	
		default:
			return( kUnknownStandardErr ); // how should I handle this?
	}
}

#pragma mark -
#pragma mark ееееееееее Private Interface Calls ееееееееее
#pragma mark -

///////////////////////////////////////////////////////////////////////
//
// DVPostEvent
//
//	used for sending the real notification
//
///////////////////////////////////////////////////////////////////////
OSErr DVCPostEvent( DVCEventRecordPtr	pEvent )
{
	OSErr					error = noErr;

	// make sure it's a legit event
	if ( (pEvent->eventHeader.theEvent & kDVEveryEvent) == nil )
			error = paramErr;
	
	// get to secondary interrupt level as quickly as possible, where
	// things are nice and synchronized...
	error = CallSecondaryInterruptHandler2( (SecondaryInterruptHandler2) PostEventSIH,
											nil,
											pEvent,
											nil );
	
	// now that we've pre-processed the event, give task level
	// notifications a chance to run, if appropriate.
	if ( CurrentExecutionLevel() == kTaskLevel )
		DVIdle();
											
	return( error );
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
OSStatus
PostEventSIH( void* p1, void* p2 )
{
	DVCEventRecordPtr			pEvent = (DVCEventRecordPtr) p1;
	DVEventEntryPtr				pEventEntry;
	DVNotificationEntryPtr		pEntry;
	OSErr						error = noErr;

	// We now have two broad classifications of events - ones that need to be
	// reported ASAP, which are stream related:
	//
	// 		kDVIsochReadComplete
	//		kDVIsochWriteComplete
	//
	// and ones that are device management related, whose notifications will
	// probably generate massive amounts of task-level only Toolbox calls:
	//
	//		kDVDeviceAdded
	//		kDVDeviceRemoved
	//		kDVIsochReadEnabled
	//		kDVIsochReadDisabled
	//		kDVIsochWriteEnabled
	//		kDVIsochWriteDisabled
	//
	// We ship the low-latency notifications to secondary interrupt, while
	// the task level calls we queue and get back to them when someone
	// calls DVIdle().
	//
	
	// ok, so let's go find out who's waiting for this event

	// go through list looking for the curious
	pEntry = (DVNotificationEntryPtr) gpFamilyGlobals->notificationQueue->qHead;
	while ( pEntry != nil )
	{
		if ( (pEvent->eventHeader.theEvent & pEntry->wantedEvents) != nil )
		{
			// only send notification if it's a global connection id or if
			// the event came from the same deviceID as this notif entry
			if ( (pEntry->deviceID == kDVGlobalEventConnectionID) || 
				(pEvent->eventHeader.deviceID == pEntry->deviceID) )
			{				
				// we currently only support a one-shot notification, like clock callbacks
				pEntry->wantedEvents = nil;
				
				// make sure the event contains this notification id
				pEvent->eventHeader.notifID = (DVCNotificationID) pEntry;
				

				// check before calling..
				switch( pEvent->eventHeader.theEvent )
				{
					case kDVIsochReadComplete:
					case kDVIsochWriteComplete:
						// process event immediately...
						error = (*pEntry->notifyProc)( pEvent, pEntry->userRefCon );
						break;
			
					case kDVDeviceAdded:
					case kDVDeviceRemoved:
					case kDVIsochReadEnabled:
					case kDVIsochReadDisabled:
					case kDVIsochWriteEnabled:
					case kDVIsochWriteDisabled:
						// queue the event and proc for later processing...
						
						// get an entry
						error = PBDequeueFirst( gpFamilyGlobals->availableDVEvents,
												(QElemPtr*) &pEventEntry );
						
						// if we don't have any more available event elements, 
						// we just drop the events on the floor
						
						// copy the notify proc & refcon
						if ( error == noErr )
						{
							pEventEntry->notifyProc	= pEntry->notifyProc;
							pEventEntry->userRefCon = pEntry->userRefCon;
						}
						
						// copy the event
						if ( error == noErr )
							BlockCopy( pEvent, &(pEventEntry->eventRec), sizeof( DVCEventRecord ) );
						
						// queue it
						if ( error == noErr )
							PBEnqueue( (QElemPtr) pEventEntry, gpFamilyGlobals->receivedDVEvents );
							
						// If we haven't already sent notification 
						// to Notification Mgr to run tasks, do it now...
						if ( CompareAndSwap( false, true, &(gpFamilyGlobals->nmIsInstalled) ) )
							NMInstall( &(gpFamilyGlobals->dvNMRec) );
						
						break;
				
					default:
						break;
				}
	
			}
		}

		// next entry
		pEntry = (DVNotificationEntryPtr) pEntry->qLink;
	}

	return( error );
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
OSErr
DVIdle( void )
{
	DVEventEntryPtr		pEventEntry;
	OSErr				error = noErr;
	
	// this routine should only get called at task level
	
	// go see if there are any task level notifications that
	// need to be done. we take from the back end since that
	// was the oldest one.
	while( true )
	{
		error = PBDequeueLast( gpFamilyGlobals->receivedDVEvents,
								(QElemPtr*) &pEventEntry );
		
		if ( error == noErr )
		{
			if ( pEventEntry->notifyProc )
			{
				// process it...
				error = (*pEventEntry->notifyProc)( &(pEventEntry->eventRec), pEventEntry->userRefCon );
			}
			
			// and put it back into available queue
			PBEnqueue( (QElemPtr) pEventEntry, gpFamilyGlobals->availableDVEvents );
		}
		else
			// we're out of events (probably)
			break;
	}
	
	return( error );
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void
myNMHandler( NMRecPtr pNM )
{
	do {
		DVIdle();
	} while( CompareAndSwap( true, false, &(gpFamilyGlobals->nmIsInstalled) ) );
	
	// until next time...
	NMRemove( pNM );
}



