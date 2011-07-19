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
	File:		DCFireWireDV.h

	Contains:	The concrete derived class of an IDHDigitizer which digitizes 
				video from a Gossamer source.
				
			
	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Sean Williams

	Writers:

		(jkl)	Jay Lloyd
		(RS)	Richard Sepulveda
		(GDW)	George D. Wilson Jr.

	Change History (most recent first):

		 <5>	 7/28/99	jkl		Made sure commandObjectInUse element is long word aligned.
		 <4>	 7/27/99	jkl		Added object in use flag.
		 <3>	  7/5/99	RS		Added fCommandObjectID cause we want it to stay allocated over
									the lifetime of the instance.
		 <2>	 6/18/99	GDW		Added some new data elements.
	   <1>	 6/15/99		KW		Created
*/


#ifndef __DCFIREWIREDV__
#define __DCFIREWIREDV__

// MacOS headers.
#include <Components.h>
#include <DeviceControl.h>
#include <DeviceControlPriv.h>
#include <FireWire.h>

// Standard C++ Library headers.
//#include <cstddef>
typedef unsigned long size_t;


//
// -------- DCFireWireDV --------
//

class DCFireWireDV
{

public:
 
 	DCFireWireDV(ComponentInstance self, Boolean *success);
	virtual ~DCFireWireDV();

	void* operator new(size_t);
	void operator delete(void*);



	// These routines are the analogs to the default component calls.
	//  
	// *** Important ***
	// People familiar with other component implementations (both in C and C++) will
	// notice the lack of the 'storage' and 'componentInstance' parameters.  This is
	// due to a combination of implementation choices as well as taking advantage of
	// C++ facilities.
	//
	// The 'storage' parameter for each ComponentInstance is set to the corresponding
	// 'this' pointer of the object that is instansiated for each ComponentInstance.
	//
	// Similarly, the 'componentInstance' parameter is passed to the
	// constructor, which stores it in a private data member for future use.

	static pascal ComponentResult Open(
			DCFireWireDV* unused,
			ComponentInstance self);

	static pascal ComponentResult Close(DCFireWireDV* dc, ComponentInstance self);
	static pascal ComponentResult Version(DCFireWireDV* dc);
	static pascal ComponentResult Register(DCFireWireDV* dc);

	static pascal ComponentResult Target(
		DCFireWireDV* dc,
		ComponentInstance parentComponent);

	static pascal ComponentResult Unregister(DCFireWireDV* dc);


	//
	// Public API Calls
	//

	static pascal ComponentResult DoAVCTransaction(
		DCFireWireDV* dc,
		DVCTransactionParams* inTransaction);

	//
	// Private API Calls
	//

	static pascal ComponentResult EnableAVCTransactions(DCFireWireDV* dc);
	static pascal ComponentResult DisableAVCTransactions(DCFireWireDV* dc);

	static pascal ComponentResult SetDeviceConnectionID(
			DCFireWireDV* dc,
			DeviceConnectionID connectionID);
			
	static pascal ComponentResult GetDeviceConnectionID(
			DCFireWireDV* dc,
			DeviceConnectionID* connectionID);


	




protected :

private :
	
	//
	// Static Routines for Callback Functions
	//
	// When a member function is declared as 'static' in the class definition, that
	// means there is no implicit 'this' pointer when the routine is called.  This is
	// necessary for callbacks, since they use calling conventions, which are 
	// 'this'less.
	//


	//
	// Routines to implement utility functions
	//
	

	

	//
	// Data Members
	//
	
	ComponentInstance fSelf;		// ComponentInstance for this object
	ComponentInstance fTarget;		// if Targeted....
	FWClientID fClientID;			// Firewire client ID used in AVC transcatin
	FWCommandObjectID fCommandObjectID;	// command object used to send FCP command
	UInt32 fCommandObjectInUse;		// true during an AVC transaction, must be long word aligned
	Boolean fRegistered;			// true when component is registered
	Boolean	fDeviceEnable;			// Allows transactions to be enabled
};



#endif // __DCFIREWIREDV__
