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
	File:		DCFireWireDV.cp

	Contains:	Device Control component for DV on Firewire. 
												

	Copyright:	й 1997-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Kevin Williams

	Writers:

		(jkl)	Jay Lloyd
		(RS)	Richard Sepulveda
		(GDW)	George D. Wilson Jr.

	Change History (most recent first):

		<13>	  8/6/99	jkl		Got rid of DVFamily.h stuff.
		<12>	  8/3/99	RS		Returned device disconnected error when appropriate.
		<11>	 7/28/99	jkl		Used atomic operation to check commandObjectInUse flag for a
									little more safety.
		<10>	 7/27/99	jkl		Added command object in use flag to DoAVCTransaction to make
									sure it is not reentered.
		 <9>	 7/13/99	jkl		Moved setting asynch command object payload from
									DoAVCTransaction to constructor.
									
		 <8>	 7/12/99	RS		Change'd component instance storage back to application heap
									because close is handled correctly in the isoch component now.
		 <7>	  7/8/99	RS		Allocating instance storage using NewPtrSysClear() instead of
									NewPtr() because this component needs to stay open across app
									launches at times.
		 <6>	  7/5/99	RS		Allocating fCommandObjectID in the constructor and deleting in
									destructor to avoid any non-task allocation problems.
		 <5>	  7/5/99	jkl		Changed to set max payload for command object, not client. Moved
									client max payload to isoch component.
		 <4>	  7/5/99	jkl		Set max payload for FCP command object to 512. The default is 4
									bytes and was causing failures on device control commands larger
									than 4 bytes.
		 <3>	 6/21/99	RS		fDeviceEnable wasn't being initialized in the constructor.
									Caused spurious bug during enable.
		 <2>	 6/18/99	GDW		Changed things.
	   <1>	 6/15/99		KW		Created
*/

// DCFireWireDV headers.
#include "DCFireWireDV.h"
#include "DCFireWireDVVersionAndRezIDs.h"			// Version constants and Rez IDs

// MacOS headers.
#include <FireWire.h>
#include <Gestalt.h>
#include <IsochronousDataHandler.h>

// Standard C++ Library headers.
//#include <cstddef>



//
// -------- DCFireWireDV --------
//


#pragma mark ееееееееее Constructor & Destructor ееееееееее



//
// DCFireWireDV()
//
//	Constructor.
//
DCFireWireDV::DCFireWireDV(ComponentInstance self, Boolean *success)
	: fSelf(self),
	fRegistered(false),
	fTarget(0),
	fClientID(0),
	fDeviceEnable(false)	
{
	*success = true;
	
	OSErr error = FWAllocateFCPCommandObject(&fCommandObjectID);
	if (error)
		*success = false;
	else
	{
		// max packet for s100
		FWSetAsynchCommandMaxPayloadSize(fCommandObjectID, 512);
	}	

}


//
// ~DCFireWireDV()
//
//	Destructor.
//
DCFireWireDV::~DCFireWireDV()
{
	if( fCommandObjectID)
		FWDeallocateFWCommandObject(fCommandObjectID);
}



//====================================================================================
//
// DCFireWireDV operator new()
//	This allocates the memory for the DCFireWireDV object data members.
//	Though the global operator new could probably be used safely, this version will
//	be used so that it is not affected by potential changes in the runtime libraries
//	implementation of the global operator new.
//
//====================================================================================
void* DCFireWireDV::operator new(size_t size)
{
	return (void*) NewPtrClear(size);
}



//====================================================================================
//
// DCFireWireDV operator delete()
//	Dispose of the object storage.  Global operator delete is not being used since
//	global operator new isn't.
//
//====================================================================================
void DCFireWireDV::operator delete(void* ptr)
{
	if (NULL != ptr)
		DisposePtr((Ptr) ptr);
}




#pragma mark -
#pragma mark ееееееееее Default Component Calls ееееееееее

//====================================================================================
//
// Open()
//	The Component Manager issues an 'open' request whenever a client tries to open a
//	connection to the component by calling the Open[Default]Component() function.
//
//	Since each ComponentInstance has a separate DCFireWireDV object instance
//	associated with it, the state of each ComponentInstance can be independently
//	maintained.  Any information that needs to be shared between ComponentInstances
//	will be accessible through the ComponentRefcon.
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Open(
		DCFireWireDV* unused,
		ComponentInstance self)
{
	Boolean success;
	
	// Attempt to construct a DCFireWireDV object.
	DCFireWireDV* dc = new DCFireWireDV(self, &success);
	if (NULL == dc || success == false)
		return memFullErr;						// Unable to make new DCFireWireDV
		
		
	// DCFireWireDV was successfully instantiated.  Set this  ComponentInstance's storage
	// to be a pointer to the DCFireWireDV object.  This will allow subsequent calls
	// easy access to the DCFireWireDV that was created for this ComponentInstance
	// since the pointer will be passed in automatically by the dispatch routine.
	
	SetComponentInstanceStorage(dc->fSelf, reinterpret_cast<char**>(dc));



	// Check the ComponentRefcon to see if this 'Open' request is part of the
	// registering process (Open, Register, Close) or a true 'Open'.
	// By convention in DeviceControl, the ComponentRefcon is used to convey
	// Registration  information and to point to a structure of items that are shared
	// by component instances as shown below:
	//
	//			NULL
	//			The component has NOT been successfully registered.
	//		
	//			0xFFFFFFFF
	//			The component has been successfully registered, but NO shared data
	//			has been allocated or initialized.
	//
	//			Other Value
	//			Any other value means that the component has been successfully
	//			registered.  Additionally, it points to the data that is shared
	//			between ComponentInstances.


	SInt32 refcon = GetComponentRefcon(reinterpret_cast<Component>(dc->fSelf));
	
	if (NULL == refcon)
		return (noErr);				// Haven't been registered, so simply return
	

	// Component had been successfully registered, so this is a true 'Open' request.
	dc->fRegistered = true;
	

	// Make sure that the appropriate version of QuickTime is around.  Only versions
	// 4.0 or greater are supported.
	
	enum { kMinimumQuickTimeVersion	= 0x0400 };

	SInt32 gestaltResult;	

	OSErr osErr = Gestalt(gestaltQuickTimeVersion, &gestaltResult);
	if ((noErr != osErr) || (kMinimumQuickTimeVersion > (gestaltResult >> 16))) 
		return qtParamErr;


	return noErr;	
}



//====================================================================================
//
// Close()
//	The Component Manager issues a 'close' request whenever a client closes its 
//	connection to the component by calling the CloseComponent() function.
//
//	As noted on page 6-21 of "IM More Macintosh Toolbox", the Component Manager will
//	issue a close request even if the open request failed.
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Close(
		DCFireWireDV* dc,
		ComponentInstance self)
{
	
	// Make sure that the DCFireWireDV* is non-NULL.  If will be NULL only if the
	// constructor failed in Open().
	
	if (NULL == dc)
		return noErr;
	
	// See if the last component instance is being closed.  If so, deallocate any
	// shared data (if appropriate) and set the ComponentRefcon to NULL to signify
	// that no shared data is allocated.
	
	SInt32 instanceCount = CountComponentInstances((Component) dc->fSelf);
		
	// Last instance is being closed?
	if (1 == instanceCount)	
	{

		if (!dc->fRegistered)
		{
			// The component has been unregistered, so set the ComponentRefcon to
			// NULL to signify that.
			SetComponentRefcon((Component) dc->fSelf, NULL);
		}
	}
	
	// Since this instance is being closed, it's storage will never be referenced
	// again, so set it to NULL (just being paranoid), and delete the DCFireWireDV
	// object.
	
	SetComponentInstanceStorage(dc->fSelf, reinterpret_cast<char**>(0));
	delete dc;

	return noErr;
}



//====================================================================================
//
// Version()
//	The Component Manager issues a 'version' request when a client calls the 
//	GetComponentVersion() function to retrieve the component's version number.
//
//		<- ComponentResult
//		The version number.  The high-order 16 bits represents the major version
//		(the component specification level), and the low-order 16 bits represent the
//		minor version (implementation level).
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Version(DCFireWireDV* dc)
{
		
	return ((kDCFireWireDVInterfaceVersion << 16) | (kDCFireWireDVCodeVersion));
}



//====================================================================================
//
// Register()
//	The Component Manager issues a 'register' request when the component is registered.
//	This gives the component an opportunity to determine whether it can operate in the
//	current environment.  System resources should not normally be allocated in
//	response to a register request.
//
//	When the Component Manager is attempting to register a component, it will send it
//	the following request sequence:  open, register, close.
//
//		<- ComponentResult
//		If the component should be registered, return 'noErr', otherwise return a
//		qtParamErr.
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Register(DCFireWireDV* dc)
{			
	// Everything necessary for a successful registration has occurred.
	// Therefore, following IDH conventions, set the component refCon to 0xFFFFFFFF
	// to signify that and set fRegistered to 'true'
	
	dc->fRegistered = true;
	SetComponentRefcon((Component) dc->fSelf, (long) 0xFFFFFFFF);

		
	return noErr;
}



//====================================================================================
//
// Target()
//	The Component Manager issues a 'target' request to inform the component that it 
//	has been targeted by another component.  After being targeted, the targeted
//	component should call the component that targeted it whenever it would normally
//	have called itself.
//
//		-> parentComponent	ComponentInstance to pass calls on to.
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Target(
		DCFireWireDV* dc,
		ComponentInstance parentComponent)
{
	dc->fTarget = parentComponent;
	// CallComponentTarget needs to be called does it not?
	return noErr;
}



//====================================================================================
//
// Unregister()
//	The Component Manager issues an 'unregister' request when the component is
//	unregistered.  This gives the component an opportunity to perform any clean up
//	operations, such as resetting the hardware.  An 'unregister' request will be sent
//	to the component even if no clients opened the client.
//
//====================================================================================
pascal ComponentResult DCFireWireDV::Unregister(DCFireWireDV* dc)
{
	
	// Set the fRegistered data member to 'false' so that the subsequent Close() will
	// know to set the ComponentRefcon to NULL to signify that the component is no
	// longer registered.
	
	dc->fRegistered = false;
		
	return noErr;
}



#pragma mark -
#pragma mark ееееееееее Public Device Control Calls  ееееееееее



//====================================================================================
//
// DoAVCTransaction()
//
//	ToDo:
//====================================================================================
pascal ComponentResult DCFireWireDV::DoAVCTransaction(
		DCFireWireDV* dc,
		DVCTransactionParams* inTransaction)
{
	ComponentResult result = noErr;

	if ( dc->fClientID == (FWClientID) kIDHInvalidDeviceID )
		return(kIDHErrInvalidDeviceID);
	
	if ( !dc->fDeviceEnable )
		return(kIDHErrDeviceDisconnected);

	if( dc->fCommandObjectID == nil)
		return kIDHErrInvalidDeviceID;
		
	if (CompareAndSwap(0, 1, &dc->fCommandObjectInUse))
	{
	
		// Set up FCP command params to tell camera to do something.
		result = FWSetFWCommandParams(	dc->fCommandObjectID,						// objectID
										dc->fClientID,								// ref ID
										kFWCommandSyncFlag,							// cmd flags
										nil,										// completion proc
										0);											// completion data							
	
		result = FWSetFCPCommandParams(	dc->fCommandObjectID,						// objectID
										inTransaction->commandBufferPtr,			// cmd buffer
										inTransaction->commandLength,				// cmd length
										inTransaction->responseBufferPtr,			// response buffer
										inTransaction->responseBufferSize,			// response size
										100 * durationMillisecond,					// timeout
										8,											// max retries
										0,											// transfer flags
										nil );										// response handler
										
		// send the FCP command
		result = FWSendFCPCommand(dc->fCommandObjectID);

		dc->fCommandObjectInUse = 0;
	}
	else
		result = kIDHErrDeviceBusy;
	
	return result;
}



#pragma mark -
#pragma mark ееееееееее FWDVCodec Private Dispatch Calls  ееееееееее


//====================================================================================
//
// EnableAVCTransactions()
//
//
//====================================================================================
pascal ComponentResult DCFireWireDV::EnableAVCTransactions(
		DCFireWireDV* dc)
{
	ComponentResult				result = noErr;
	
	if ( dc->fClientID != (FWClientID) kIDHInvalidDeviceID )
		dc->fDeviceEnable = true;
	else
		result = kIDHErrDeviceNotOpened;
	
	return result ;
}

//====================================================================================
//
// DisableAVCTransactions()
//
//
//====================================================================================
pascal ComponentResult DCFireWireDV::DisableAVCTransactions(
		DCFireWireDV* dc)
{
	ComponentResult				result = noErr;
	
	dc->fDeviceEnable = false;
	
	return result ;
}

//====================================================================================
//
// SetDeviceConnectionID()
//
//
//====================================================================================
pascal ComponentResult DCFireWireDV::SetDeviceConnectionID(
		DCFireWireDV* dc, DeviceConnectionID connectionID)
{
	ComponentResult		result = noErr;

	if ( dc->fDeviceEnable )
		result = kIDHErrDeviceInUse;
	else
		dc->fClientID = reinterpret_cast<FWClientID>(connectionID);
	
	return result;
}

//====================================================================================
//
// GetDeviceConnectionID()
//
//
//====================================================================================
pascal ComponentResult DCFireWireDV::GetDeviceConnectionID(
		DCFireWireDV* dc, DeviceConnectionID* connectionID)
{
	ComponentResult				result = noErr;
	
	*connectionID = reinterpret_cast<DeviceConnectionID>(dc->fClientID);
	return result ;
}









#pragma mark -
#pragma mark ееееееееее Static Callback Routines еееееее



#pragma mark -
#pragma mark ееееееееее Utility Routines for DCFireWireDV еееееее




#pragma mark -
#pragma mark ееееееееее Static Utility Routines for DCFireWireDV еееееее


