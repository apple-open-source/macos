/*
* Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
* Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
*
* HISTORY
* 8 June 1999 wgulland created.
*
*/
/*
	$Log: IOFireWireUserClient.cpp,v $
	Revision 1.161  2009/05/08 01:10:34  calderon
	<rdar://6863576> FireWire tracepoints should be inlined for performance
	
	Revision 1.160  2009/03/07 00:37:23  calderon
	Small change for 6641573 to give more useful debugging info
	Add Firelog info.plist file
	
	Revision 1.159  2009/03/06 18:45:15  calderon
	<rdar://problem/6641573> Panic in IOFireWireFamily/SL 10A286 when hot-plugging Mackie Onyx 1200F
	Add FireLog specific plist to work around XCode plist editor bug
	Bump version
	
	Revision 1.158  2008/12/12 04:43:57  collin
	user space compare swap command fixes
	
	Revision 1.157  2008/11/26 23:55:21  collin
	fix user physical address spaces on K64
	
	Revision 1.156  2008/11/20 01:59:12  calderon
	More tracepoint logging
	
	Revision 1.155  2008/11/11 01:12:03  calderon
	First part of tracepoints logging
	
	Revision 1.154  2008/05/07 03:27:59  collin
	64 bit session ref support
	
	Revision 1.153  2008/04/30 03:02:13  collin
	publicize the exporter
	
	Revision 1.152  2008/04/24 00:01:39  collin
	more K640
	
	Revision 1.151  2008/04/11 00:52:37  collin
	some K64 changes
	
	Revision 1.150  2008/04/02 01:42:50  collin
	fix build failure
	
	Revision 1.149  2007/10/16 16:50:21  ayanowit
	Removed existing "work-in-progress" support for buffer-fill isoch.
	
	Revision 1.148  2007/06/21 04:08:44  collin
	*** empty log message ***
	
	Revision 1.147  2007/05/04 06:05:26  collin
	*** empty log message ***
	
	Revision 1.146  2007/04/28 02:54:22  collin
	*** empty log message ***
	
	Revision 1.145  2007/04/28 01:42:35  collin
	*** empty log message ***
	
	Revision 1.144  2007/04/24 02:50:08  collin
	*** empty log message ***
	
	Revision 1.143  2007/03/14 01:01:13  collin
	*** empty log message ***
	
	Revision 1.142  2007/03/12 22:15:28  arulchan
	mach_vm_address_t & io_user_reference_t changes
	
	Revision 1.141  2007/03/10 07:48:25  collin
	*** empty log message ***
	
	Revision 1.140  2007/03/10 05:11:36  collin
	*** empty log message ***
	
	Revision 1.139  2007/03/10 04:15:25  collin
	*** empty log message ***
	
	Revision 1.138  2007/03/10 02:58:03  collin
	*** empty log message ***
	
	Revision 1.137  2007/03/09 23:57:53  collin
	*** empty log message ***
	
	Revision 1.136  2007/03/08 18:13:56  ayanowit
	Fix for 5047793. A problem where user-space CompareSwap() was not checking lock results.
	
	Revision 1.135  2007/03/08 02:37:09  collin
	*** empty log message ***
	
	Revision 1.134  2007/03/03 01:26:46  calderon
	pico
	
	Revision 1.133  2007/02/16 00:54:39  ayanowit
	Working IRMAllocation callbacks from user-space :)
	
	Revision 1.132  2007/02/16 00:28:25  ayanowit
	More work on IRMAllocation APIs
	
	Revision 1.131  2007/02/09 20:36:46  ayanowit
	More Leopard IRMAllocation changes.
	
	Revision 1.130  2007/02/07 06:35:20  collin
	*** empty log message ***
	
	Revision 1.129  2007/02/06 01:08:41  ayanowit
	More work on Leopard features such as new User-space IRM allocation APIs.
	
	Revision 1.128  2007/01/26 20:52:31  ayanowit
	changes to user-space isoch stuff to support 64-bit apps.
	
	Revision 1.127  2007/01/24 04:10:13  collin
	*** empty log message ***
	
	Revision 1.126  2007/01/05 00:11:00  ayanowit
	yet more 64-bit changes.
	
	Revision 1.124  2006/12/21 21:17:44  ayanowit
	More changes necessary to eventually get support for 64-bit apps working (4222965).
	
	Revision 1.123  2006/12/06 01:12:54  arulchan
	AsyncStream Listener Merges to Dispatcher
	
	Revision 1.122  2006/12/06 00:01:08  arulchan
	Isoch Channel 31 Generic Receiver
	
	Revision 1.121  2006/11/29 18:42:52  ayanowit
	Modified the IOFireWireUserClient to use the Leopard externalMethod method of dispatch.
	
	Revision 1.120  2006/09/09 01:59:56  collin
	*** empty log message ***
	
	Revision 1.119  2006/08/16 01:41:41  collin
	*** empty log message ***
	
	Revision 1.118  2006/07/07 20:18:25  calderon
	4227201: SpeedMap and HopCount table reductions.
	
	Revision 1.117  2006/02/27 19:03:18  niels
	*** empty log message ***
	
	Revision 1.116  2006/02/09 00:21:51  niels
	merge chardonnay branch to tot
	
	Revision 1.110.4.4  2006/01/31 04:49:51  collin
	*** empty log message ***
	
	Revision 1.110.4.2  2005/08/17 03:33:57  collin
	*** empty log message ***
	
	Revision 1.110.4.1  2005/07/23 00:30:44  collin
	*** empty log message ***
	
	Revision 1.110  2005/01/18 23:40:16  collin
	Revision 1.109  2004/09/16 04:28:21  collin
	Revision 1.108  2004/05/12 00:00:08  niels
	3626775 - Brego 7L8: Digidesign Pro Tools LE FireWire 002 system doesn't work
	3641955 - Digi 002 Pro Tools LE fails to launch on 10.3.4 7H46
	
	Revision 1.107  2004/03/25 00:08:59  niels
	fix panic allocating large physical address spaces
	
	Revision 1.106  2004/03/25 00:00:59  niels
	fix panic allocating large physical address spaces
	
	Revision 1.105  2004/03/25 00:00:23  niels
	fix panic allocating large physical address spaces
	
	Revision 1.104  2004/02/17 23:13:23  niels
	Revision 1.103  2004/02/17 23:12:26  niels
	Revision 1.102  2004/02/11 22:30:02  niels
	Revision 1.101  2004/02/11 22:13:08  niels
	fix final cut pro panic/object leak when calling TurnOffNotification on isoch channels in user space
	
	Revision 1.100  2004/01/28 22:13:32  niels
	Revision 1.99  2004/01/22 01:49:59  niels
	fix user space physical address space getPhysicalSegments
	
	Revision 1.98  2003/12/19 22:07:46  niels
	send force stop when channel dies/system sleeps
	
	Revision 1.97  2003/12/18 00:42:37  niels
	Revision 1.96  2003/11/14 01:00:53  collin
	Revision 1.95  2003/11/07 21:24:28  niels
	Revision 1.94  2003/11/07 21:01:18  niels
	Revision 1.93  2003/11/05 00:29:42  niels
	Revision 1.92  2003/11/03 19:11:35  niels
	fix local config rom reading; fix 3401223
	
	Revision 1.91  2003/10/31 02:40:58  niels
	Revision 1.90  2003/09/20 00:54:17  collin
	Revision 1.89  2003/09/16 21:40:51  collin
	Revision 1.88  2003/09/11 20:59:46  collin
	Revision 1.87  2003/09/04 19:43:34  collin
	Revision 1.86  2003/09/02 23:48:12  collin
	Revision 1.85  2003/09/02 22:58:55  collin
	Revision 1.84  2003/08/30 00:16:45  collin
	Revision 1.83  2003/08/26 05:23:34  niels
	Revision 1.82  2003/08/26 05:11:21  niels
	Revision 1.81  2003/08/25 08:39:16  niels
	Revision 1.80  2003/08/20 18:48:43  niels
	Revision 1.79  2003/08/19 01:48:54  niels
	Revision 1.78  2003/08/14 19:46:06  niels
	Revision 1.77  2003/08/14 17:47:33  niels
	Revision 1.76  2003/08/08 22:30:32  niels
	Revision 1.75  2003/08/08 21:03:27  gecko1
	Merge max-rec clipping code into TOT
	
	Revision 1.74  2003/07/26 04:47:24  collin
	Revision 1.73  2003/07/24 20:49:48  collin
	Revision 1.72  2003/07/24 06:30:58  collin
	Revision 1.71  2003/07/24 03:06:27  collin
	Revision 1.70  2003/07/22 10:49:47  niels
	Revision 1.69  2003/07/21 07:29:48  niels
	Revision 1.68  2003/07/21 06:52:59  niels
	merge isoch to TOT
	
	Revision 1.66.2.5  2003/07/21 06:44:45  niels
	Revision 1.66.2.4  2003/07/18 00:17:42  niels
	Revision 1.66.2.3  2003/07/11 18:15:34  niels
	Revision 1.66.2.2  2003/07/09 21:24:01  niels
	Revision 1.66.2.1  2003/07/01 20:54:07  niels
	isoch merge
	
	Revision 1.66  2003/06/12 21:27:14  collin
	Revision 1.65  2003/06/07 01:30:40  collin
	Revision 1.64  2003/06/05 01:19:31  niels
	fix crash on close
	
	Revision 1.63  2003/04/22 02:45:28  collin
	Revision 1.62  2003/03/17 01:05:22  collin
	Revision 1.61  2003/03/01 00:10:07  collin
	Revision 1.60  2002/11/20 00:34:27  niels
	fix minor bug in getAsyncTargetAndMethodForIndex
	
	Revision 1.59  2002/10/18 23:29:45  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.58  2002/10/17 00:29:44  collin
	reenable FireLog
	
	Revision 1.57  2002/10/16 21:42:00  niels
	no longer panic trying to get config directory on local node.. still can't get the directory, however
	
	Revision 1.56  2002/09/25 00:27:25  niels
	flip your world upside-down
	
	Revision 1.55  2002/09/12 22:41:54  niels
	add GetIRMNodeID() to user client
	
*/

// public
#import "IOFireWireFamilyCommon.h"
#import "IOFireWireNub.h"
#import "IOLocalConfigDirectory.h"
#import "IOFireWireController.h"
#import "IOFireWireDevice.h"
#import "IOFWDCLProgram.h"

#import <sys/proc.h>
#import <IOKit/IOMessage.h>
#include <IOKit/IOKitKeysPrivate.h>

#if FIRELOG
#import <IOKit/firewire/FireLog.h>
#endif

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFWUserPseudoAddressSpace.h"
#import "IOFWUserPhysicalAddressSpace.h"
#import "IOFWUserIsochChannel.h"
#import "IOFWUserIsochPort.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLocalNode.h"
#import "IOFWUserCommand.h"
#import "IOFWUserObjectExporter.h"
#import "IOLocalConfigDirectory.h"
#import "IOFWUserAsyncStreamListener.h"
#import "IOFWUserVectorCommand.h"
#import "IOFWUserPHYPacketListener.h"

#if IOFIREWIREUSERCLIENTDEBUG > 0

#undef super
#define super OSObject

OSDefineMetaClassAndStructors( IOFWUserDebugInfo, OSObject )

bool
IOFWUserDebugInfo::init( IOFireWireUserClient & userClient )
{
	if ( ! super::init() )
		return false ;

	fUserClient = & userClient ;
//	fIsochCallbacks = OSNumber::withNumber( (long long unsigned)0, 64 ) ;
//	if ( !fIsochCallbacks )
//		return false ;
	
	return true ;
}

bool
IOFWUserDebugInfo::serialize ( 
	OSSerialize * s ) const
{
	s->clearText() ;
	
	const unsigned objectCount = 1 ;
	const OSObject * objects[ objectCount ] = 
	{ 
		fUserClient->fExporter
//		, fIsochCallbacks
	} ;
	
	const OSSymbol * keys[ objectCount ] = 
	{ 
		OSSymbol::withCStringNoCopy("user objects")
//		, OSSymbol::withCStringNoCopy( "total isoch callbacks" )
	} ;
	
	OSDictionary * dict = OSDictionary::withObjects( objects, keys, objectCount ) ;
	if ( !dict )
		return false ;
		
	bool result = dict->serialize( s ) ;	
	dict->release() ;
	
	return result ;
}

void
IOFWUserDebugInfo::free ()
{
//	if ( fIsochCallbacks )
//		fIsochCallbacks->release() ;
	OSObject::free() ;
}

#endif

#pragma mark -

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOFireWireUserClient, super ) ;


bool IOFireWireUserClient::initWithTask(
                    task_t owningTask, void * securityToken, UInt32 type,
                    OSDictionary * properties)
{
    if( properties )
		properties->setObject( "IOUserClientCrossEndianCompatible" , kOSBooleanTrue);

    bool res = IOUserClient::initWithTask( owningTask, securityToken, type, properties );
	
	fTask = owningTask;
	
    return res;
}

bool
IOFireWireUserClient::start( IOService * provider )
{
	if (!OSDynamicCast(IOFireWireNub, provider))
		return false ;

	if ( ! super::start ( provider ) )
		return false;

	fOwner = (IOFireWireNub *)provider;
	fOwner->retain();
	
	FWTrace( kFWTUserClient, kTPUserClientStart, (uintptr_t)(fOwner->getController()->getLink()), 0, 0, 0 );

	//
	// init object table
	//
	fObjectTable[0] = NULL ;
	fObjectTable[1] = this ;
	fObjectTable[2] = getOwner()->getBus() ;
	
	// initialize notification structures
	fBusResetAsyncNotificationRef[0] = 0 ;
	fBusResetDoneAsyncNotificationRef[0] = 0 ;

	bool result = true ;

	fExporter = IOFWUserObjectExporter::createWithOwner( this );
	
	if ( ! fExporter )
		result = false ;
		
#if IOFIREWIREUSERCLIENTDEBUG > 0
	if (result)
	{
		fDebugInfo = OSTypeAlloc( IOFWUserDebugInfo );
		if ( fDebugInfo && ! fDebugInfo->init( *this ) )
		{
			fDebugInfo->release() ;
			fDebugInfo = NULL ;
			
			ErrorLog( "Couldn't create statistics object\n" ) ;
		}

		if ( fDebugInfo )
			setProperty( "Debug Info", fDebugInfo ) ;
	}
#endif

#if 0
	// turn off because sadly the proc structure is now opaque
	
	// borrowed from bsd/vm/vm_unix.c, function pid_for_task() which has some other weird crap
	// going on too... I just took this part:
	if ( result )
	{
		proc *				p 			= (proc *)get_bsdtask_info( fTask );
		OSNumber*			pidProp 	= OSNumber::withNumber( p->p_pid, sizeof(p->p_pid) * 8 ) ;
		if ( pidProp )
		{
			setProperty( "Owning PID", pidProp ) ;
			pidProp->release() ;			// property table takes a reference
		}
		else
			result = false ;
	}
#endif

	return result ;
}

void
IOFireWireUserClient::free()
{
	if ( fOwner ) {
		FWTrace( kFWTUserClient, kTPUserClientFree, (uintptr_t)(fOwner->getController()->getLink()), (uintptr_t)this, 0, 0 );
	} else {
		FWTrace( kFWTUserClient, kTPUserClientFree, 0xdeadbeef, (uintptr_t)this, 0, 0 );
	}
	
	DebugLog( "free user client %p\n", this ) ;

#if IOFIREWIREUSERCLIENTDEBUG > 0
	if ( fDebugInfo )
		fDebugInfo->release() ;
#endif

	if ( fExporter )
	{
		fExporter->release() ;
		fExporter = NULL ;
	}
	
	if ( fOwner )
	{
		fOwner->release() ;
	}
	
	super::free () ;
}

IOReturn
IOFireWireUserClient::clientClose ()
{
	FWTrace( kFWTUserClient, kTPUserClientClientClose, (uintptr_t)(fOwner->getController()->getLink()), 0, 0, 0 );
	
	clipMaxRec2K( false );	// Make sure maxRec isn't clipped

	IOReturn	result = userClose() ;

	if ( getProvider() && fOwner->isOpen() )
	{
		DebugLog("IOFireWireUserClient::clientClose(): client left user client open, should call close. Closing...\n") ;
	}
	else if ( result == kIOReturnNotOpen )
	{
		result = kIOReturnSuccess ;
	}
	
	if ( !terminate() )
	{
		IOLog("IOFireWireUserClient::clientClose: terminate failed!, getOwner()->isOpen( this ) returned %u\n", getOwner()->isOpen(this)) ;
	}
	
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::clientDied ()
{
	if ( fOwner ) {
		FWTrace( kFWTUserClient, kTPUserClientClientDied, (uintptr_t)(fOwner->getController()->getLink()), 1, 0, 0 );
	} else {
		FWTrace( kFWTUserClient, kTPUserClientClientDied, 0xdeadbeef, 1, 0, 0 );
	}
	
	if ( fOwner )
	{
		FWTrace( kFWTUserClient, kTPUserClientClientDied, (uintptr_t)(fOwner->getController()->getLink()), 2, 0, 0 );
		fOwner->getBus()->resetBus() ;
	}

	IOReturn error = clientClose () ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::setProperties (
	OSObject * properties )
{
	IOReturn result = kIOReturnSuccess ;

	OSDictionary*	dict = OSDynamicCast( OSDictionary, properties ) ;

	if ( dict )
	{
		OSObject*	value = dict->getObject( "unsafe bus resets" ) ;

		if ( value and OSDynamicCast(OSNumber, value ) )
		{
			fUnsafeResets = ( ((OSNumber*)value)->unsigned8BitValue() != 0 ) ;
		}
		else
		{
			result = super::setProperties ( properties ) ;
		}
	}
	else
		result = super::setProperties ( properties ) ;
	
	return result ;
}

#pragma mark -


static UInt32 getSelectorObjectLookupIndex(UInt32 selector) 
{
	UInt32 selectorObjectLookupIndex = 1;  // Note: A 1 here specifies use of the IOFireWireUserClient object
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	// NOTE: If the selector is not in this lookup table, it is handled directly by the IOFireWireUserClient
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	switch (selector)
	{
		/////////////////////////////////////////////////////////////////////////
		// The follwing selectors all are handled by the IOFireWireBus
		/////////////////////////////////////////////////////////////////////////
		case kCycleTime:
		case kGetBusCycleTime:
			selectorObjectLookupIndex = 2;  // Note: A 2 here specifies use of the IOFireWireBus object
			break;
		
		/////////////////////////////////////////////////////////////////////////////////
		// The follwing selectors all are handled by object exporter managed objects
		/////////////////////////////////////////////////////////////////////////////////
		case kPhysicalAddrSpace_GetSegmentCount_d:			// Handled by a IOFWUserPhysicalAddressSpace object
		case kIsochPort_AllocatePort_d:						// Handled by a IOFWUserLocalIsochPort object
		case kIsochPort_ReleasePort_d:						// Handled by a IOFWUserLocalIsochPort object
		case kIsochPort_Start_d:							// Handled by a IOFWUserLocalIsochPort object
		case kIsochPort_Stop_d:								// Handled by a IOFWUserLocalIsochPort object
		case kLocalIsochPort_ModifyJumpDCL_d:				// Handled by a IOFWUserLocalIsochPort object
		case kLocalIsochPort_Notify_d:						// Handled by a IOFWUserLocalIsochPort object
		case kIsochChannel_UserReleaseChannelComplete_d:	// Handled by a IOFWUserIsochChannel object
		case kCommand_Cancel_d:								// Handled by a IOFWCommand object
		case kIsochPort_SetIsochResourceFlags_d:			// Handled by a IOFWLocalIsochPort object
		case kVectorCommandSubmit:							// Handled by a IOFWUserVectorCommand object
		case kVectorCommandSetBuffers:						// Handled by a IOFWUserVectorCommand object
		case kPHYPacketListenerSetPacketCallback:			// Handled by a IOFWUserPHYPacketListener object
		case kPHYPacketListenerSetSkippedCallback:			// Handled by a IOFWUserPHYPacketListener object
		case kPHYPacketListenerActivate:					// Handled by a IOFWUserPHYPacketListener object
		case kPHYPacketListenerDeactivate:					// Handled by a IOFWUserPHYPacketListener object
		case kPHYPacketListenerClientCommandIsComplete:		// Handled by a IOFWUserPHYPacketListener object
			selectorObjectLookupIndex = 0;  // Note: A 0 here specifies a lookup into the object exporter!
			break;

		default:
			// Any other selector is handled by the IOFireWireUserClient!
			break;
	};
	return selectorObjectLookupIndex;
}

IOReturn
IOFireWireUserClient::externalMethod( uint32_t selector, 
										IOExternalMethodArguments * arguments, 
										IOExternalMethodDispatch * dispatch, 
										OSObject * target, 
										void * reference)
{
	IOReturn result = kIOReturnBadArgument;
	IOService *targetObject;

	UInt32 actualSelector = selector & 0xFFFF ;
	if ( actualSelector >= kNumMethods )
		return result;

	targetObject = fObjectTable[ getSelectorObjectLookupIndex(actualSelector)] ;

	if ( !targetObject )
	{
		const OSObject * userObject = fExporter->lookupObject( (UserObjectHandle)( selector >> 16 ) ) ;
		
		targetObject = (IOService*)userObject ;		// "interesting code" note:
													// when we don't have an object set in our method table,
													// the object handle is encoded in the upper 16 bits of the 
													// method index...
													// We extract the object handle here to get the object to call...
		
		if (targetObject)
		{
			(targetObject)->release() ;	// exporter retains returned objects, we have to release it here.. 
										// hopefully no one will release the object until we're done..
		}
		else
			return result;
	}
	
	// Dispatch the method call
	switch (actualSelector)
	{
		case kOpen:
			result = ((IOFireWireUserClient*) targetObject)->userOpen();
			break;
		
		case kOpenWithSessionRef:
			result = ((IOFireWireUserClient*) targetObject)->userOpenWithSessionRef((IOFireWireLib::UserObjectHandle) arguments->scalarInput[0]);
			break;
		
		case kClose:
			result = ((IOFireWireUserClient*) targetObject)->userClose();
			break;
		
		case kReadQuad:
			result = ((IOFireWireUserClient*) targetObject)->
									readQuad((const ReadQuadParams*) arguments->structureInput,
											(UInt32*) arguments->structureOutput);
			break;
		
		case kRead:
			result = ((IOFireWireUserClient*) targetObject)->
											read((const ReadParams*) arguments->structureInput,
												(IOByteCount*) arguments->structureOutput);
			break;
		
		case kWriteQuad:
			result = ((IOFireWireUserClient*) targetObject)->writeQuad((const WriteQuadParams*) arguments->structureInput);
			break;
		
		case kWrite:
			result = ((IOFireWireUserClient*) targetObject)->
											write((const WriteParams*) arguments->structureInput,
													(IOByteCount*) arguments->structureOutput);
			break;
		
		case kCompareSwap:
			result = ((IOFireWireUserClient*) targetObject)->
											compareSwap((const CompareSwapParams*) arguments->structureInput,
													(UInt32*) arguments->structureOutput);
			break;
		
		case kBusReset:
			result = ((IOFireWireUserClient*) targetObject)->busReset();
			break;
		
		case kCycleTime:
		{
			UInt32 cycleTime;
			UInt64 upTime;
			result = ((IOFireWireController*) targetObject)->getCycleTimeAndUpTime(cycleTime, upTime);
			arguments->scalarOutput[0] = cycleTime;
			arguments->scalarOutput[1] = upTime;
		}
			break;
		
		case kGetGenerationAndNodeID:
		{
			UInt32 outGeneration;
			UInt32 outNodeID;
			result = ((IOFireWireUserClient*) targetObject)->getGenerationAndNodeID(&outGeneration,&outNodeID);
			arguments->scalarOutput[0] = outGeneration;
			arguments->scalarOutput[1] = outNodeID;
		}
			break;
		
		case kGetLocalNodeID:
		{
			UInt32 outLocalNodeID;
			result = ((IOFireWireUserClient*) targetObject)->getLocalNodeID(&outLocalNodeID);
			arguments->scalarOutput[0] = outLocalNodeID;
		}
			break;
		
		case kGetResetTime:
			result = ((IOFireWireUserClient*) targetObject)->getResetTime((AbsoluteTime*) arguments->structureOutput);
			break;
		
		case kReleaseUserObject:
			result = ((IOFireWireUserClient*) targetObject)->releaseUserObject((UserObjectHandle)arguments->scalarInput[0]);
			break;
		
		case kGetOSStringData:
		{
			UInt32 outTextLength;
			result = ((IOFireWireUserClient*) targetObject)->getOSStringData((UserObjectHandle)arguments->scalarInput[0],
																			(UInt32)arguments->scalarInput[1],
																			(mach_vm_address_t)arguments->scalarInput[2],
																			&outTextLength);
			arguments->scalarOutput[0] = outTextLength;
		}
			break;
		
		case kGetOSDataData:
		{
			IOByteCount outDataLen;
			result = ((IOFireWireUserClient*) targetObject)->getOSDataData((UserObjectHandle)arguments->scalarInput[0],
																			(IOByteCount)arguments->scalarInput[1],
																			(mach_vm_address_t)arguments->scalarInput[2],
																			&outDataLen);
			arguments->scalarOutput[0] = outDataLen;
		}
			break;
		
		case kLocalConfigDirectory_Create:
		{
			UserObjectHandle outDir;
			result = ((IOFireWireUserClient*) targetObject)->localConfigDirectory_Create(&outDir);
			arguments->scalarOutput[0] = (uint64_t) outDir;
		}
			break;
		
		case kLocalConfigDirectory_AddEntry_Buffer:
			result = ((IOFireWireUserClient*) targetObject)->
						localConfigDirectory_addEntry_Buffer((UserObjectHandle)arguments->scalarInput[0],
															(int)arguments->scalarInput[1],
															(char *)arguments->scalarInput[2],
															(UInt32)arguments->scalarInput[3],
															(const char *)arguments->scalarInput[4],
															(UInt32)arguments->scalarInput[5]);
			break;
		
		case kLocalConfigDirectory_AddEntry_UInt32:
			result = ((IOFireWireUserClient*) targetObject)->
						localConfigDirectory_addEntry_UInt32((UserObjectHandle)arguments->scalarInput[0],
															(int)arguments->scalarInput[1],
															(UInt32)arguments->scalarInput[2],
															(const char *)arguments->scalarInput[3],
															(UInt32)arguments->scalarInput[4]);
			break;
		
		case kLocalConfigDirectory_AddEntry_FWAddr:
			result = ((IOFireWireUserClient*) targetObject)->
				localConfigDirectory_addEntry_FWAddr((UserObjectHandle)arguments->scalarInput[0],
													(int)arguments->scalarInput[1],
													(const char *)arguments->scalarInput[2],
													(UInt32)arguments->scalarInput[3],
													(FWAddress *) arguments->structureInput);
			break;
		
		case kLocalConfigDirectory_AddEntry_UnitDir:
			// TODO -  Note: This wasn't hooked-up in Tiger either!
			break;
		
		case kLocalConfigDirectory_Publish:
			result = ((IOFireWireUserClient*) targetObject)->localConfigDirectory_Publish((UserObjectHandle)arguments->scalarInput[0]);
			break;
		
		case kLocalConfigDirectory_Unpublish:
			result = ((IOFireWireUserClient*) targetObject)->localConfigDirectory_Unpublish((UserObjectHandle)arguments->scalarInput[0]);
			break;
		
		case kPseudoAddrSpace_Allocate:
			result = ((IOFireWireUserClient*) targetObject)->
									addressSpace_Create((AddressSpaceCreateParams *) arguments->structureInput,
														(UserObjectHandle *) arguments->structureOutput);
			break;
		
		case kPseudoAddrSpace_GetFWAddrInfo:
			result = ((IOFireWireUserClient*) targetObject)->
								addressSpace_GetInfo((UserObjectHandle)arguments->scalarInput[0],
											(AddressSpaceInfo *) arguments->structureOutput);
			break;
		
		case kPseudoAddrSpace_ClientCommandIsComplete:
			result = ((IOFireWireUserClient*) targetObject)->
						addressSpace_ClientCommandIsComplete((UserObjectHandle)arguments->scalarInput[0],
															(FWClientCommandID)arguments->scalarInput[1],
															(IOReturn)arguments->scalarInput[2]);
			break;
		
		case kPhysicalAddrSpace_Allocate:
		{
			UserObjectHandle outAddressSpaceHandle;
			result = ((IOFireWireUserClient*) targetObject)->physicalAddressSpace_Create((mach_vm_size_t)arguments->scalarInput[0],
																						 (mach_vm_address_t)arguments->scalarInput[1],
																						 (UInt32)arguments->scalarInput[2],
																						 &outAddressSpaceHandle);
			arguments->scalarOutput[0] = (uint64_t) outAddressSpaceHandle;
		}
			break;
		
		case kPhysicalAddrSpace_GetSegmentCount_d:
		{
			UInt32 outSegmentCount;
			result = ((IOFWUserPhysicalAddressSpace*) targetObject)->getSegmentCount(&outSegmentCount);
			arguments->scalarOutput[0] = outSegmentCount;
		}
			break;
		
		case kPhysicalAddrSpace_GetSegments:
		{
			UInt32 outSegmentCount;
			result = ((IOFireWireUserClient*) targetObject)->physicalAddressSpace_GetSegments((UserObjectHandle)arguments->scalarInput[0],
																			 (UInt32)arguments->scalarInput[1],
																			 (mach_vm_address_t)arguments->scalarInput[2],
																			 &outSegmentCount);
			arguments->scalarOutput[0] = outSegmentCount;
		}
			break;
		
		case kConfigDirectory_Create:
		{
			UserObjectHandle outDirRef;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_Create(&outDirRef);
			arguments->scalarOutput[0] = (uint64_t) outDirRef;
		}
			break;
		
		case kConfigDirectory_GetKeyType:
		{
			IOConfigKeyType outType;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_GetKeyType((UserObjectHandle)arguments->scalarInput[0],
																						(int)arguments->scalarInput[1],
																						&outType);
			arguments->scalarOutput[0] = outType;
		}
			break;
		
		case kConfigDirectory_GetKeyValue_UInt32:
		{
			UInt32 outValue; 
			UserObjectHandle outTextHandle; 
			UInt32 outTextLength;
			result = ((IOFireWireUserClient*) targetObject)->
												configDirectory_GetKeyValue_UInt32((UserObjectHandle)arguments->scalarInput[0],
																(int)arguments->scalarInput[1],
																(UInt32)arguments->scalarInput[2],
																&outValue,
																&outTextHandle,
																&outTextLength);
			arguments->scalarOutput[0] = outValue;
			arguments->scalarOutput[1] = (uint64_t) outTextHandle;
			arguments->scalarOutput[2] = outTextLength;
		}
			break;
		
		case kConfigDirectory_GetKeyValue_Data:
			result = ((IOFireWireUserClient*) targetObject)->
								configDirectory_GetKeyValue_Data((UserObjectHandle)arguments->scalarInput[0],
																(int)arguments->scalarInput[1],
																(UInt32)arguments->scalarInput[2],
																(GetKeyValueDataResults *) arguments->structureOutput);
			break;
		
		case kConfigDirectory_GetKeyValue_ConfigDirectory:
		{
			UserObjectHandle outDirHandle;
			UserObjectHandle outTextHandle; 
			UInt32 outTextLength;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_GetKeyValue_ConfigDirectory((UserObjectHandle)arguments->scalarInput[0],
																										 (int)arguments->scalarInput[1],
																										 (UInt32)arguments->scalarInput[2],
																										 &outDirHandle,
																										 &outTextHandle,
																										 &outTextLength);
			arguments->scalarOutput[0] = (uint64_t) outDirHandle;
			arguments->scalarOutput[1] = (uint64_t) outTextHandle;
			arguments->scalarOutput[2] = outTextLength;
		}
			break;
		
		case kConfigDirectory_GetKeyOffset_FWAddress:
			result = ((IOFireWireUserClient*) targetObject)->
								configDirectory_GetKeyOffset_FWAddress((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		(UInt32)arguments->scalarInput[2],
																		(GetKeyOffsetResults *) arguments->structureOutput);
			break;
		
		case kConfigDirectory_GetIndexType:
		{
			IOConfigKeyType outType;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_GetIndexType((UserObjectHandle)arguments->scalarInput[0],
																						  (int)arguments->scalarInput[1],
																						  &outType);
			arguments->scalarOutput[0] = (IOConfigKeyType) outType;
		}
			break;
		
		case kConfigDirectory_GetIndexKey:
		{
			int outKey;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_GetIndexKey((UserObjectHandle)arguments->scalarInput[0],
																						  (int)arguments->scalarInput[1],
																						  &outKey);
			arguments->scalarOutput[0] = outKey;
		}
			break;
		
		case kConfigDirectory_GetIndexValue_UInt32:
			// configDirectory_GetIndexValue_UInt32
		{
			UInt32 outKey;
			result = ((IOFireWireUserClient*) targetObject)->configDirectory_GetIndexValue_UInt32((UserObjectHandle)arguments->scalarInput[0],
																						 (int)arguments->scalarInput[1],
																						 &outKey);
			arguments->scalarOutput[0] = outKey;
		}
			break;
		
		case kConfigDirectory_GetIndexValue_Data:
		{
			UserObjectHandle outDataHandle;
			IOByteCount outDataLen;
			result = ((IOFireWireUserClient*) targetObject)->
										configDirectory_GetIndexValue_Data((UserObjectHandle)arguments->scalarInput[0],
																			(int)arguments->scalarInput[1],
																			&outDataHandle,
																			&outDataLen);
			arguments->scalarOutput[0] = (uint64_t) outDataHandle;
			arguments->scalarOutput[1] = outDataLen;
		}
			break;
		
		case kConfigDirectory_GetIndexValue_String:
		{
			UserObjectHandle outTextHandle;
			UInt32 outTextLength;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetIndexValue_String((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		&outTextHandle,
																		&outTextLength);
			arguments->scalarOutput[0] = (uint64_t) outTextHandle;
			arguments->scalarOutput[1] = outTextLength;
		}
			break;
		
		case kConfigDirectory_GetIndexValue_ConfigDirectory:
		{
			UserObjectHandle outDirHandle;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetIndexValue_ConfigDirectory((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		&outDirHandle);
			arguments->scalarOutput[0] = (uint64_t) outDirHandle;
		}
		break;
		
		case kConfigDirectory_GetIndexOffset_FWAddress:
			result = ((IOFireWireUserClient*) targetObject)->
								configDirectory_GetIndexOffset_FWAddress((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		(FWAddress *) arguments->structureOutput);
			break;
		
		case kConfigDirectory_GetIndexOffset_UInt32:
		{
			UInt32 outValue;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetIndexOffset_UInt32((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		&outValue);
			arguments->scalarOutput[0] = outValue;
		}
			break;
		
		case kConfigDirectory_GetIndexEntry:
		{
			UInt32 outValue;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetIndexEntry((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		&outValue);
			arguments->scalarOutput[0] = outValue;
		}
			break;
		
		case kConfigDirectory_GetSubdirectories:
		{
			UserObjectHandle outIteratorHandle; 
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetSubdirectories((UserObjectHandle)arguments->scalarInput[0],
																		&outIteratorHandle);
			arguments->scalarOutput[0] = (uint64_t) outIteratorHandle;
		}
			break;
		
		case kConfigDirectory_GetKeySubdirectories:
		{
			UserObjectHandle outIteratorHandle; 
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetKeySubdirectories((UserObjectHandle)arguments->scalarInput[0],
																		(int)arguments->scalarInput[1],
																		&outIteratorHandle);
			arguments->scalarOutput[0] = (uint64_t) outIteratorHandle;
		}
			break;

		case kConfigDirectory_GetType:
		{
			int outType;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetType((UserObjectHandle)arguments->scalarInput[0],&outType);
			arguments->scalarOutput[0] = outType;
		}
			break;
		
		case kConfigDirectory_GetNumEntries:
		{
			int outNumEntries;
			result = ((IOFireWireUserClient*) targetObject)->
									configDirectory_GetNumEntries((UserObjectHandle)arguments->scalarInput[0],&outNumEntries);
			arguments->scalarOutput[0] = outNumEntries;
		}
			break;
		
		case kIsochPort_GetSupported:
		{
			IOFWSpeed outMaxSpeed;
			UInt32 outChanSupportedHi;
			UInt32 outChanSupportedLo;
			result = ((IOFireWireUserClient*) targetObject)->
									localIsochPort_GetSupported((UserObjectHandle)arguments->scalarInput[0],
																		&outMaxSpeed,
																		&outChanSupportedHi,
																		&outChanSupportedLo);
			arguments->scalarOutput[0] = outMaxSpeed;
			arguments->scalarOutput[1] = outChanSupportedHi;
			arguments->scalarOutput[2] = outChanSupportedLo;
		}
			break;
		
		case kIsochPort_AllocatePort_d:
			result = ((IOFWUserLocalIsochPort*) targetObject)->allocatePort((IOFWSpeed)arguments->scalarInput[0],
																			(UInt32)arguments->scalarInput[1]);
			break;
		
		case kIsochPort_ReleasePort_d:
			result = ((IOFWUserLocalIsochPort*) targetObject)->releasePort();
			break;
		
		case kIsochPort_Start_d:
			result = ((IOFWUserLocalIsochPort*) targetObject)->start();
			break;
		
		case kIsochPort_Stop_d:
			result = ((IOFWUserLocalIsochPort*) targetObject)->stop();
			break;
		
		case kLocalIsochPort_Allocate:
			result = ((IOFireWireUserClient*) targetObject)->
								localIsochPort_Create((LocalIsochPortAllocateParams*) arguments->structureInput,
														(UserObjectHandle*) arguments->structureOutput);
			break;

		case kLocalIsochPort_ModifyJumpDCL_d:
			result = ((IOFWUserLocalIsochPort*) targetObject)->modifyJumpDCL((UInt32)arguments->scalarInput[0],
																			(UInt32)arguments->scalarInput[1]);
			break;
		
		case kLocalIsochPort_Notify_d:
			if (arguments->scalarInput[0] == kFWNuDCLModifyNotification)
			{
				IOMemoryDescriptor * userDCLExportDesc = NULL ;
				IOReturn error ;
				UInt8 *pUserImportDCLBuffer;
				userDCLExportDesc = IOMemoryDescriptor::withAddressRange( arguments->scalarInput[2], 
																		 arguments->scalarInput[3], 
																		 kIODirectionOut, 
																		 getOwningTask() ) ;
				// get map of program export data
				if ( userDCLExportDesc )
				{
					error = userDCLExportDesc->prepare() ;
				}
				else
					error = kIOReturnVMError;
				
				if ( !error )
				{
					pUserImportDCLBuffer = new UInt8[ arguments->scalarInput[3] ] ;
					if ( !pUserImportDCLBuffer )
					{
						error = kIOReturnVMError ;
					}
				}				
				
				if ( !error )
				{
					unsigned byteCount = userDCLExportDesc->readBytes( 0, (void*)pUserImportDCLBuffer, arguments->scalarInput[3] ) ;
					if ( byteCount < arguments->scalarInput[3] )
					{
						error = kIOReturnVMError ;
					}
				}
				
				if ( !error )
				{
					result = ((IOFWUserLocalIsochPort*) targetObject)->userNotify((UInt32)arguments->scalarInput[0],
																				  (UInt32)arguments->scalarInput[1],
																				  (void *) pUserImportDCLBuffer,
																				  arguments->scalarInput[3]);
					userDCLExportDesc->complete() ;
					userDCLExportDesc->release();
					delete [] pUserImportDCLBuffer;
				}
				else
				{
					result = kIOReturnVMError;
				}
			}
			else
				result = ((IOFWUserLocalIsochPort*) targetObject)->
								userNotify((UInt32)arguments->scalarInput[0],
											(UInt32)arguments->scalarInput[1],
											(void *) arguments->structureInput,
											arguments->structureInputSize);
			break;
		
		case kLocalIsochPort_SetChannel:
			result = ((IOFireWireUserClient*) targetObject)->
											localIsochPort_SetChannel((UserObjectHandle)arguments->scalarInput[0],
																	(UserObjectHandle)arguments->scalarInput[1]);
			break;

		case kIsochChannel_Allocate:
		{
			UserObjectHandle outChannelHandle;
			result = ((IOFireWireUserClient*) targetObject)->
											isochChannel_Create((bool)arguments->scalarInput[0],
																	(UInt32)arguments->scalarInput[1],
																	(IOFWSpeed)arguments->scalarInput[2],
																	&outChannelHandle);
			arguments->scalarOutput[0] = (uint64_t) outChannelHandle;
		}
			break;
		
		case kIsochChannel_UserAllocateChannelBegin:
		{
			UInt32 outSpeed;
			UInt32 outChannel;
			result = ((IOFireWireUserClient*) targetObject)->
											isochChannel_AllocateChannelBegin((UserObjectHandle)arguments->scalarInput[0],
																	(UInt32)arguments->scalarInput[1],
																	(UInt32)arguments->scalarInput[2],
																	(UInt32)arguments->scalarInput[3],
																	&outSpeed,
																	&outChannel);
			arguments->scalarOutput[0] = outSpeed;
			arguments->scalarOutput[1] = outChannel;
		}
			break;
		
		case kIsochChannel_UserReleaseChannelComplete_d:
			result = ((IOFWUserIsochChannel*) targetObject)->releaseChannelComplete();
			break;
		
		case kCommand_Cancel_d:
			result = ((IOFWCommand*) targetObject)->cancel((IOReturn)arguments->scalarInput[0]);
			break;
		
		case kSeize:
			result = ((IOFireWireUserClient*) targetObject)->seize((IOOptionBits) arguments->scalarInput[0]);
			break;
		
		case kFireLog:
			result = ((IOFireWireUserClient*) targetObject)->
							firelog((const char*) arguments->structureInput,arguments->structureInputSize);
			break;
		
		case kGetBusCycleTime:
		{
			UInt32 busTime;
			UInt32 cycleTime;
			result = ((IOFireWireBus*) targetObject)->getBusCycleTime(busTime,cycleTime);
			arguments->scalarOutput[0] = busTime;
			arguments->scalarOutput[1] = cycleTime;
		}
			break;
		
		case kGetBusGeneration:
		{
			UInt32 outGeneration;
			result = ((IOFireWireUserClient*) targetObject)->getBusGeneration(&outGeneration);
			arguments->scalarOutput[0] = outGeneration;
		}
			break;
		
		case kGetLocalNodeIDWithGeneration:
		{
			UInt32 outLocalNodeID;
			result = ((IOFireWireUserClient*) targetObject)->
										getLocalNodeIDWithGeneration((UInt32)arguments->scalarInput[0],&outLocalNodeID);
			arguments->scalarOutput[0] = outLocalNodeID;
		}
			break;
		
		case kGetRemoteNodeID:
		{
			UInt32 outRemoteNodeID;
			result = ((IOFireWireUserClient*) targetObject)->
										getRemoteNodeID((UInt32)arguments->scalarInput[0],&outRemoteNodeID);
			arguments->scalarOutput[0] = outRemoteNodeID;
		}
			break;
		
		case kGetSpeedToNode:
		{
			UInt32 outSpeed;
			result = ((IOFireWireUserClient*) targetObject)->
										getSpeedToNode((UInt32)arguments->scalarInput[0],&outSpeed);
			arguments->scalarOutput[0] = outSpeed;
		}
			break;
		
		case kGetSpeedBetweenNodes:
		{
			UInt32 outSpeed;
			result = ((IOFireWireUserClient*) targetObject)->
										getSpeedBetweenNodes((UInt32)arguments->scalarInput[0],
															(UInt32)arguments->scalarInput[1],
															(UInt32)arguments->scalarInput[2],
															&outSpeed);
			arguments->scalarOutput[0] = outSpeed;
		}
			break;
		
		case kGetIRMNodeID:
		{
			UInt32 irmNodeID;
			result = ((IOFireWireUserClient*) targetObject)->
										getIRMNodeID((UInt32)arguments->scalarInput[0],&irmNodeID);
			arguments->scalarOutput[0] = irmNodeID;
		}
			break;
		
		case kClipMaxRec2K:
			result = ((IOFireWireUserClient*) targetObject)->clipMaxRec2K((Boolean) arguments->scalarInput[0]);
			break;
		
		case kIsochPort_SetIsochResourceFlags_d:
			result = ((IOFWLocalIsochPort*) targetObject)->setIsochResourceFlags((IOFWIsochResourceFlags) arguments->scalarInput[0]);
			break;
		
		case kGetSessionRef:
		{
			IOFireWireSessionRef sessionRef=NULL;
			result = ((IOFireWireUserClient*) targetObject)->getSessionRef(&sessionRef);
			arguments->scalarOutput[0] = (uint64_t) sessionRef;
		}
			break;
		
		case kAllocateIRMBandwidth:
			result = ((IOFireWireUserClient*) targetObject)->allocateIRMBandwidthInGeneration(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;
		
		case kReleaseIRMBandwidth:
			result = ((IOFireWireUserClient*) targetObject)->releaseIRMBandwidthInGeneration(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;
	
		case kAllocateIRMChannel:
			result = ((IOFireWireUserClient*) targetObject)->allocateIRMChannelInGeneration(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;

		case kReleaseIRMChannel:
			result = ((IOFireWireUserClient*) targetObject)->releaseIRMChannelInGeneration(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;
		
		case kAsyncStreamListener_Allocate:
			result = ((IOFireWireUserClient*) targetObject)->
								asyncStreamListener_Create((FWUserAsyncStreamListenerCreateParams*) arguments->structureInput,
														(UserObjectHandle*) arguments->structureOutput);
			break;

		case kAsyncStreamListener_ClientCommandIsComplete:
			result = ((IOFireWireUserClient*) targetObject)->asyncStreamListener_ClientCommandIsComplete((UserObjectHandle)arguments->scalarInput[0],
																			(FWClientCommandID)arguments->scalarInput[1]);
			break;

		case kAsyncStreamListener_GetOverrunCounter:
		{
			UInt32 counter = 0;
			result = ((IOFireWireUserClient*) targetObject)->
									asyncStreamListener_GetOverrunCounter((UserObjectHandle)arguments->scalarInput[0], &counter);
			arguments->scalarOutput[0] = counter;
		}
			break;

		case kAsyncStreamListener_SetFlags:
			result = ((IOFireWireUserClient*) targetObject)->
											asyncStreamListener_SetFlags((UserObjectHandle)arguments->scalarInput[0],
																	(UInt32)arguments->scalarInput[1]);
			break;

		case kAsyncStreamListener_GetFlags:
		{
			UInt32 outFlags;
			result = ((IOFireWireUserClient*) targetObject)->
									asyncStreamListener_GetFlags((UserObjectHandle)arguments->scalarInput[0], &outFlags);
			arguments->scalarOutput[0] = outFlags;
		}
			break;

		case kAsyncStreamListener_TurnOnNotification:
			result = ((IOFireWireUserClient*) targetObject)->asyncStreamListener_TurnOnNotification((UserObjectHandle)arguments->scalarInput[0]);
			break;

		case kAsyncStreamListener_TurnOffNotification:
			result = ((IOFireWireUserClient*) targetObject)->asyncStreamListener_TurnOffNotification((UserObjectHandle)arguments->scalarInput[0]);
			break;
		
		case kSetAsyncRef_BusReset:
		{
			result = ((IOFireWireUserClient*) targetObject)->
												setAsyncRef_BusReset(arguments->asyncReference,
																	(mach_vm_address_t)arguments->scalarInput[0],
																	(io_user_reference_t)arguments->scalarInput[1],
																	NULL,NULL,NULL,NULL);
		}
			break;
		
		case kSetAsyncRef_BusResetDone:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_BusResetDone(arguments->asyncReference,
																(mach_vm_address_t)arguments->scalarInput[0],
																(io_user_reference_t)arguments->scalarInput[1],
																NULL,NULL,NULL,NULL);
		}
			break;

		case kSetAsyncRef_Packet:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_Packet(arguments->asyncReference,
															(UserObjectHandle)arguments->scalarInput[0],
															(mach_vm_address_t)arguments->scalarInput[1],
															(io_user_reference_t)arguments->scalarInput[2],
															NULL,NULL,NULL);
		}
			break;


		case kSetAsyncRef_SkippedPacket:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_SkippedPacket(arguments->asyncReference,
																(UserObjectHandle)arguments->scalarInput[0],
																(mach_vm_address_t)arguments->scalarInput[1],
																(io_user_reference_t)arguments->scalarInput[2],
																NULL,NULL,NULL);
		}
			break;
		
		case kSetAsyncRef_Read:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_Read(arguments->asyncReference,
														(UserObjectHandle)arguments->scalarInput[0],
														(mach_vm_address_t)arguments->scalarInput[1],
														(io_user_reference_t)arguments->scalarInput[2],
														NULL,NULL,NULL);
		}
			break;
		
		case kCommand_Submit:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										userAsyncCommand_Submit(arguments->asyncReference,
																(CommandSubmitParams*)arguments->structureInput,
																(CommandSubmitResult*)arguments->structureOutput,
																(IOByteCount) arguments->structureInputSize,
																(IOByteCount*) &arguments->structureOutputSize);
			}
			break;
		
		case kSetAsyncRef_IsochChannelForceStop:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_IsochChannelForceStop(arguments->asyncReference,
																		(UserObjectHandle)arguments->scalarInput[0]);
		}
		break;
		
		case kSetAsyncRef_DCLCallProc:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncRef_DCLCallProc(arguments->asyncReference,
																(UserObjectHandle)arguments->scalarInput[0]);
		}
		break;

		case kSetAsyncStreamRef_Packet:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncStreamRef_Packet(arguments->asyncReference,
																(UserObjectHandle)arguments->scalarInput[0],
																(mach_vm_address_t)arguments->scalarInput[1],
																(io_user_reference_t)arguments->scalarInput[2],
																NULL,NULL,NULL);
		}
			break;

		case kSetAsyncStreamRef_SkippedPacket:
		{
			result = ((IOFireWireUserClient*) targetObject)->
										setAsyncStreamRef_SkippedPacket(arguments->asyncReference,
																(UserObjectHandle)arguments->scalarInput[0],
																(mach_vm_address_t)arguments->scalarInput[1],
																(io_user_reference_t)arguments->scalarInput[2],
																NULL,NULL,NULL);
		}
			break;
		
		
		case kIRMAllocation_Allocate:
		{
			UserObjectHandle outDataHandle;
			result = ((IOFireWireUserClient*) targetObject)->irmAllocation_Create(arguments->scalarInput[0],&outDataHandle);
			arguments->scalarOutput[0] = (uint64_t) outDataHandle;
		}
			break;
			
		case kIRMAllocation_AllocateResources:
			result = ((IOFireWireUserClient*) targetObject)->irmAllocation_AllocateResources((UserObjectHandle)arguments->scalarInput[0],
																							 arguments->scalarInput[1],
																							 arguments->scalarInput[2]);
			break;
	

		case kIRMAllocation_DeallocateResources:
			result = ((IOFireWireUserClient*) targetObject)->irmAllocation_DeallocateResources((UserObjectHandle)arguments->scalarInput[0]);
			break;

		case kIRMAllocation_areResourcesAllocated:
		{
			UInt8 isochChannel;
			UInt32 bandwidthUnits;
			result = ((IOFireWireUserClient*) targetObject)->irmAllocation_areResourcesAllocated((UserObjectHandle)arguments->scalarInput[0], &isochChannel , &bandwidthUnits);
			arguments->scalarOutput[1] = (uint64_t) isochChannel;
			arguments->scalarOutput[2] = (uint64_t) bandwidthUnits;
		}
			break;

		case kIRMAllocation_setDeallocateOnRelease:
			((IOFireWireUserClient*) targetObject)->irmAllocation_setDeallocateOnRelease((UserObjectHandle)arguments->scalarInput[0],arguments->scalarInput[1]);
			result = kIOReturnSuccess;
			break;

		case kIRMAllocation_SetRef:
			result = ((IOFireWireUserClient*) targetObject)->irmAllocation_setRef(arguments->asyncReference,
																				  (UserObjectHandle)arguments->scalarInput[0],
																				  arguments->scalarInput[1],
																				  arguments->scalarInput[2]);
			break;

		case kCommandCreateAsync:
			{
				result = ((IOFireWireUserClient*) targetObject)->
										createAsyncCommand( arguments->asyncReference,
															(CommandSubmitParams*)arguments->structureInput,
															(UserObjectHandle*)arguments->structureOutput );
			}
			break;
				

		case kVectorCommandCreate:
			result = ((IOFireWireUserClient*) targetObject)->createVectorCommand( (UserObjectHandle*)arguments->structureOutput );
			break;
			
		case kVectorCommandSubmit:
			result = ((IOFWUserVectorCommand*)targetObject)->submit(	arguments->asyncReference, 
																		(mach_vm_address_t)arguments->scalarInput[0], 
																		(io_user_reference_t)arguments->scalarInput[1] );
			break;

		case kVectorCommandSetBuffers:
			result = ((IOFWUserVectorCommand*)targetObject)->setBuffers(	(mach_vm_address_t)arguments->scalarInput[0], 
																			(mach_vm_size_t)arguments->scalarInput[1],
																			(mach_vm_address_t)arguments->scalarInput[2], 
																			(mach_vm_size_t)arguments->scalarInput[3]  );
			break;

		case kPHYPacketListenerCreate:
			{
				UserObjectHandle kernel_ref;
				result = ((IOFireWireUserClient*) targetObject)->createPHYPacketListener( (mach_vm_address_t)arguments->scalarInput[0],
																						  &kernel_ref );
				arguments->scalarOutput[0] = (uint64_t)kernel_ref;
			}
			break;

		case kPHYPacketListenerSetPacketCallback:
			result = ((IOFWUserPHYPacketListener*)targetObject)->setPacketCallback(	arguments->asyncReference, 
																					(mach_vm_address_t)arguments->scalarInput[0], 
																					(io_user_reference_t)arguments->scalarInput[1] );
			break;
			
		case kPHYPacketListenerSetSkippedCallback:
			result = ((IOFWUserPHYPacketListener*)targetObject)->setSkippedCallback(	arguments->asyncReference, 
																						(mach_vm_address_t)arguments->scalarInput[0], 
																						(io_user_reference_t)arguments->scalarInput[1] );
			break;
			
		case kPHYPacketListenerActivate:
			result = ((IOFWUserPHYPacketListener*)targetObject)->activate();
			break;
			
		case kPHYPacketListenerDeactivate:
			((IOFWUserPHYPacketListener*)targetObject)->deactivate();
			result = kIOReturnSuccess;
			break;
		
		case kPHYPacketListenerClientCommandIsComplete:
			((IOFWUserPHYPacketListener*)targetObject)->clientCommandIsComplete( (FWClientCommandID)arguments->scalarInput[0] );
			result = kIOReturnSuccess;
			break;
			
		default:
			// NONE OF THE ABOVE :(
			break;
	};		
	
	return result;
}

IOReturn
IOFireWireUserClient::registerNotificationPort(
	mach_port_t port,
	UInt32		/* type */,
	UInt32		refCon)
{
	fNotificationPort = port ;
	fNotificationRefCon = refCon ;

	return( kIOReturnUnsupported);
}

#pragma mark -

IOReturn
IOFireWireUserClient::
copyUserData (
		mach_vm_address_t		userBuffer, 
		mach_vm_address_t		kernBuffer, 
		mach_vm_size_t 			bytes ) const
{
	if( (userBuffer == NULL) || (kernBuffer == NULL) )
	{
		return kIOReturnNoMemory;
	}
	
	if( bytes == 0 )
	{
		return kIOReturnSuccess;
	}
	
	IOMemoryDescriptor *	desc 	= IOMemoryDescriptor::withAddressRange (	userBuffer, bytes, kIODirectionOut, 
																			fTask ) ;
	if ( ! desc )
	{
		ErrorLog ( "Couldn't create descriptor\n" ) ;
		return kIOReturnNoMemory ;
	}
	
	// copy user space range list to in kernel list
	IOReturn error = desc->prepare () ;

	if ( ! error )
	{
		if ( bytes != desc->readBytes ( 0, (void*)kernBuffer, bytes ) )
			error = kIOReturnVMError ;

		desc->complete() ;
		desc->release() ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::copyToUserBuffer (
	IOVirtualAddress 		kernelBuffer,
	mach_vm_address_t		userBuffer,
	IOByteCount 			bytes,
	IOByteCount & 			bytesCopied )
{
	if( (userBuffer == NULL) || (kernelBuffer == NULL) )
	{
		return kIOReturnNoMemory;
	}

	if( bytes == 0 )
	{
		return kIOReturnSuccess;
	}

	IOMemoryDescriptor *	mem = IOMemoryDescriptor::withAddressRange( userBuffer, bytes, kIODirectionIn, fTask ) ;
	if ( ! mem )
		return kIOReturnNoMemory ;

	IOReturn error = mem->prepare () ;
	
	if ( !error )
	{
		bytesCopied = mem->writeBytes ( 0, (void*)kernelBuffer, bytes ) ;
		mem->complete() ;
	}
	
	mem->release() ;
	
	return error ;
}

#pragma mark -
#pragma mark OPEN/CLOSE

IOReturn
IOFireWireUserClient::userOpen ()
{
	IOReturn error = kIOReturnSuccess ;		
		
	// do the open...
	
	IOFireWireNub * provider = OSDynamicCast( IOFireWireNub, getOwner() ) ;
	if ( ! provider )
	{
		ErrorLog( "Couldn't find provider!\b" ) ;
		return kIOReturnError ;
	}
	
	if ( getOwner()->open( this ) )
	{
		IOFWUserObjectExporter * exporter = fOwner->getController()->getSessionRefExporter();
		error = exporter->addObject( this, NULL, &fSessionRef );		
		if( error == kIOReturnSuccess )
		{
			fSelfOpenCount = 1 ;
			fOpenClient = this ;
		}
	}
	else
	{
		ErrorLog( "couldn't open provider\n" ) ;
		error = kIOReturnExclusiveAccess ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::userOpenWithSessionRef ( IOFireWireLib::UserObjectHandle sessionRef )
{
	IOReturn	result = kIOReturnSuccess ;
	
	if (getOwner ()->isOpen())
	{
		IOFWUserObjectExporter * exporter = fOwner->getController()->getSessionRefExporter();
		IOService * open_client = (IOService*) exporter->lookupObjectForType( sessionRef, OSTypeID(IOService) );
		IOService * client = open_client;
		
		if (!client)
			result = kIOReturnBadArgument ;
		else
		{
			while (client != NULL)
			{
				if (client == getOwner ())
				{
					fOpenClient = open_client ;	// sessionRef is the originally passed in user object
				
					if ( fOpenClient == this )
					{
						++fSelfOpenCount ;
					} 
				
					break ;
				}
				
				client = client->getProvider() ;
			}
		}
		
		if( open_client )
		{
			open_client->release();
			open_client = NULL;
		}
	}
	else
		result = kIOReturnNotOpen ;
		
	return result ;
}

IOReturn
IOFireWireUserClient::userClose ()
{
	IOReturn result = kIOReturnSuccess ;

	if ( getProvider() == NULL )
		return kIOReturnSuccess ;
	
	if ( fOpenClient == this )
	{
		if ( !fOwner->isOpen( this ) )
		{
			result = kIOReturnNotOpen ;
		}
		else
		{
			if ( fSelfOpenCount > 0 )
			{
				if ( --fSelfOpenCount == 0 )
				{
					IOFWUserObjectExporter * exporter = fOwner->getController()->getSessionRefExporter();
					exporter->removeObject( fSessionRef );	
					fSessionRef = 0;
					fOwner->close(this) ;
					fOpenClient = NULL ;
				}
			}
			else
			{
				return kIOReturnNotOpen ;
			}
		}		
	}
	
	return result ;
}

#pragma mark -
#pragma mark GENERAL

IOReturn
IOFireWireUserClient::readQuad ( const ReadQuadParams* params, UInt32* outVal )
{
	IOReturn 				err ;
	IOFWReadQuadCommand*	cmd ;

	if ( params->isAbs )
		cmd = this->createReadQuadCommand( params->generation, params->addr, outVal, 1, NULL, NULL ) ;
	else
	{
		if ( (cmd = getOwner ()->createReadQuadCommand( params->addr, outVal, 1, NULL, NULL, params->failOnReset )) )
			cmd->setGeneration( params->generation ) ;
	}
			
	if(!cmd)
		return kIOReturnNoMemory;

	err = cmd->submit();        // We block here until the command finishes

	if( !err )
		err = cmd->getStatus();

	cmd->release();

	return err;
}

IOReturn
IOFireWireUserClient::read ( const ReadParams* params, IOByteCount* outBytesTransferred )
{
	IOReturn 					err ;
	IOMemoryDescriptor *		mem ;
	IOFWReadCommand*			cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddressRange(params->buf, params->size, kIODirectionIn, fTask);
	if(!mem)
	{
		return kIOReturnNoMemory;
	}
	
	{
		IOReturn error = mem->prepare() ;
		if ( kIOReturnSuccess != error )
		{
			mem->release() ;
			return error ;
		}
	}
	
	if ( params->isAbs )
		cmd = this->createReadCommand( params->generation, params->addr, mem, NULL, NULL ) ;
	else
	{
		if ( (cmd = getOwner ()->createReadCommand( params->addr, mem, NULL, NULL, params->failOnReset )) )
			cmd->setGeneration(params->generation) ;
	}
	
	if(!cmd)
	{
		mem->complete() ;
		mem->release() ;
		return kIOReturnNoMemory;
	}
	
	err = mem->prepare() ;

	if ( err )
		IOLog("%s %u: IOFireWireUserClient::read: prepare failed\n", __FILE__, __LINE__) ;
	else		
	{
		err = cmd->submit();	// We block here until the command finishes
		mem->complete() ;
	}
	
	if( !err )
		err = cmd->getStatus();
		
	*outBytesTransferred = cmd->getBytesTransferred() ;

	cmd->release();
	mem->release();

	return err;
}

IOReturn
IOFireWireUserClient::writeQuad( const WriteQuadParams* params)
{
	IOReturn 				err;
	IOFWWriteQuadCommand*	cmd ;

	if ( params->isAbs )
		cmd = this->createWriteQuadCommand( params->generation, params->addr, (UInt32*) & params->val, 1, NULL, NULL ) ;
	else
	{
		cmd = getOwner ()->createWriteQuadCommand( params->addr, (UInt32*)&params->val, 1, NULL, NULL, params->failOnReset ) ;
		cmd->setGeneration( params->generation ) ;
	}
	
	if(!cmd)
		return kIOReturnNoMemory;


	err = cmd->submit();	// We block here until the command finishes

	if( err )
		err = cmd->getStatus();
	
	cmd->release();

	return err;
}

IOReturn
IOFireWireUserClient::write( const WriteParams* params, IOByteCount* outBytesTransferred )
{
	IOMemoryDescriptor *	mem ;
	IOFWWriteCommand*		cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddressRange(params->buf, params->size, kIODirectionOut, fTask);
	if(!mem)
	{
		return kIOReturnNoMemory;
	}
	
	{
		IOReturn error = mem->prepare() ;
		if ( kIOReturnSuccess != error )
		{
			mem->release() ;
			return error ;
		}
	}
	
	if ( params->isAbs )
	{
		cmd = this->createWriteCommand( params->generation, params->addr, mem, NULL, NULL ) ;
	}
	else
	{
		if ( (cmd = getOwner ()->createWriteCommand( params->addr, mem, NULL, NULL, params->failOnReset )) )
			cmd->setGeneration( params->generation ) ;
	}
	
	if( !cmd )
	{
		mem->complete() ;
		mem->release() ;
		return kIOReturnNoMemory;
	}

	IOReturn error ;
	error = cmd->submit();

	// We block here until the command finishes
	if( !error )
		error = cmd->getStatus();

	if ( !error )
		*outBytesTransferred = cmd->getBytesTransferred() ;
	
	cmd->release();
	
	mem->complete() ;
	mem->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient::compareSwap( const CompareSwapParams* params, UInt32* oldVal )
{
	IOReturn 							err ;
	IOFWCompareAndSwapCommand*			cmd ;
	
	if ( params->size > 2 )
		return kIOReturnBadArgument ;
	
	if ( params->isAbs )
	{
        cmd = this->createCompareAndSwapCommand( params->generation, params->addr, (UInt32*)& params->cmpVal, 
				(UInt32*)& params->swapVal, params->size, NULL, NULL ) ;
	}
	else
	{
        if ( (cmd = getOwner ()->createCompareAndSwapCommand( params->addr, (UInt32*)& params->cmpVal, (UInt32*)& params->swapVal, 
				params->size, NULL, NULL, params->failOnReset )) )
		{
			cmd->setGeneration( params->generation ) ;
		}
	}
	
	if(!cmd)
		return kIOReturnNoMemory;

	err = cmd->submit();

	// We block here until the command finishes
	if( !err )
	{
		cmd->locked(oldVal) ;
		err = cmd->getStatus();
//		if(kIOReturnSuccess == err && !cmd->locked((UInt32*)oldVal))
//			err = kIOReturnCannotLock;
	}

	cmd->release();

	return err ;
}

IOReturn 
IOFireWireUserClient::busReset()
{
	FWTrace( kFWTUserClient, kTPUserClientBusReset, (uintptr_t)(getOwner()->getController()->getLink()), fUnsafeResets , 0, 0);
	
	if ( fUnsafeResets )
		return getOwner ()->getController()->getLink()->resetBus();

	return getOwner ()->getController()->resetBus();
}

IOReturn
IOFireWireUserClient::getGenerationAndNodeID(
	UInt32*					outGeneration,
	UInt32*					outNodeID) const
{
	UInt16	nodeID ;

	getOwner ()->getNodeIDGeneration(*outGeneration, nodeID);
	if (!getOwner ()->getController()->checkGeneration(*outGeneration))
		return kIOReturnNotFound ;	// nodeID we got was stale...
	
	*outNodeID = (UInt32)nodeID ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getLocalNodeID(
	UInt32*					outLocalNodeID) const
{
	*outLocalNodeID = (UInt32)(getOwner ()->getController()->getLocalNodeID()) ;	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getResetTime(
	AbsoluteTime*	outResetTime) const
{
	*outResetTime = *getOwner ()->getController()->getResetTime() ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::releaseUserObject (
	UserObjectHandle		obj )
{
	fExporter->removeObject( obj ) ;

	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark CONVERSION HELPERS

IOReturn
IOFireWireUserClient::getOSStringData ( 
	UserObjectHandle 			stringHandle, 
	UInt32 						stringLen, 
	mach_vm_address_t 			stringBuffer,
	UInt32 * 					outTextLength )
{
	*outTextLength = 0 ;

	const OSObject * object = fExporter->lookupObject( stringHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	OSString * string = OSDynamicCast ( OSString, object ) ;
	if ( ! string )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOByteCount len = min( stringLen, string->getLength() ) ;
	IOByteCount outLen = 0;
	IOReturn error = copyToUserBuffer (	(IOVirtualAddress) string->getCStringNoCopy(), 
										stringBuffer, 
										len, 
										outLen ) ;
	*outTextLength = (UInt32)outLen;
	
	fExporter->removeObject( stringHandle ) ;
	string->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::getOSDataData (
	UserObjectHandle		dataHandle,
	IOByteCount				dataLen,
	mach_vm_address_t		dataBuffer,
	IOByteCount*			outDataLen)
{
	const OSObject * object = fExporter->lookupObject( dataHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	OSData * data = OSDynamicCast( OSData, object ) ;
	if ( !data )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = copyToUserBuffer (	(IOVirtualAddress) data->getBytesNoCopy(), 
										dataBuffer, 
										min( data->getLength(), dataLen ), 
										*outDataLen ) ;
	fExporter->removeObject( dataHandle ) ;
		
	object->release() ;		// this is retained when we call lookupObject; "I must release you"
	
	return error ;
}

#pragma mark -
#pragma mark LOCAL CONFIG DIRECTORY

//	Config Directory methods

IOReturn
IOFireWireUserClient::localConfigDirectory_Create ( UserObjectHandle* outDir )
{
	IOLocalConfigDirectory * dir = IOLocalConfigDirectory::create () ;

	if ( ! dir )
	{
		DebugLog( "IOLocalConfigDirectory::create returned NULL\n" ) ;
		return kIOReturnNoMemory ;
	}

	IOReturn error = fExporter->addObject ( dir, (IOFWUserObjectExporter::CleanupFunction)&IOLocalConfigDirectory::exporterCleanup, outDir ) ;
	
	dir->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_addEntry_Buffer ( 
	UserObjectHandle		dirHandle, 
	int 					key, 
	char * 					buffer, 
	UInt32 					kr_size,
	const char *			descCString,
	UInt32					descLen ) const
{
	
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = kIOReturnSuccess ;
	OSData * data = OSData::withCapacity( kr_size ) ;
	if ( !data )
	{
		error = kIOReturnNoMemory ;
	}
	else
	{
		data->appendBytes( NULL, kr_size );  // force internal buffer allocation
		copyUserData( (IOVirtualAddress)buffer, (IOVirtualAddress)data->getBytesNoCopy(), kr_size ) ;
		
		OSString * desc = NULL ;
		if ( descCString )
		{
			char cStr[ descLen ] ;
			copyUserData( (IOVirtualAddress)descCString, (IOVirtualAddress)cStr, descLen ) ;
			
			cStr[ descLen ] = 0 ;
			desc = OSString::withCString( cStr ) ;
		}
		
		error = dir->addEntry ( key, data, desc ) ;

		data->release() ;
	}
	
	dir->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_addEntry_UInt32 ( 
	UserObjectHandle 		dirHandle, 
	int 					key, 
	UInt32 					value,
	const char *			descCString,
	UInt32					descLen ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSString * desc = NULL ;
	if ( descCString )
	{
		char cStr[ descLen ] ;
		copyUserData( (IOVirtualAddress)descCString, (IOVirtualAddress)cStr, descLen ) ;
		
		cStr[ descLen ] = 0 ;
		desc = OSString::withCString( cStr ) ;
	}

	IOReturn error = dir->addEntry(key, value, desc) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_addEntry_FWAddr ( 
	UserObjectHandle	dirHandle, 
	int					key, 
	const char *		descCString,
	UInt32				descLen,
	FWAddress *			value ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}

	OSString * desc = NULL ;
	if ( descCString )
	{
		char cStr[ descLen ] ;
		copyUserData( (IOVirtualAddress)descCString, (IOVirtualAddress)cStr, descLen ) ;

		cStr[ descLen ] = 0 ;
		
		desc = OSString::withCString( cStr ) ;
	}

	IOReturn error = dir->addEntry( key, *value, desc ) ;

	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_addEntry_UnitDir ( 
	UserObjectHandle	dirHandle, 
	int 				key, 
	UserObjectHandle 	valueHandle,
	const char *		descCString,
	UInt32				descLen ) const
{
	IOLocalConfigDirectory * dir ;
	{
		const OSObject * object = fExporter->lookupObject( dirHandle ) ;
		if ( !object )
		{
			return kIOReturnBadArgument ;
		}
		
		dir = OSDynamicCast( IOLocalConfigDirectory, object ) ;
		if ( ! dir )
		{
			object->release() ;
			return kIOReturnBadArgument ;
		}
	}
	
	IOReturn error ;
	IOLocalConfigDirectory * value ;

	{
		const OSObject * object = fExporter->lookupObject( valueHandle ) ;
		if ( !object )
		{
			return kIOReturnBadArgument ;
		}
		
		value = OSDynamicCast( IOLocalConfigDirectory, object ) ;

		if ( ! value )
		{
			object->release() ;
			error = kIOReturnBadArgument ;
		}
		else
		{
			OSString * desc = NULL ;
			
			if ( descCString )
			{
				char cStr[ descLen ] ;
				copyUserData( (IOVirtualAddress)descCString, (IOVirtualAddress)cStr, descLen ) ;
	
				cStr[ descLen ] = 0 ;
				
				desc = OSString::withCString( cStr ) ;
			}
		
			error = dir->addEntry( key, value, desc ) ;
			
			value->release() ;
		}
	}
	
	dir->release() ;	// lookupObject retains the object
	
	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_Publish ( UserObjectHandle dirHandle ) const
{	
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = getOwner ()->getController()->AddUnitDirectory( dir ) ;
	
	dir->release() ;	// lookupObject retains result; "I must release you"
	
	return error ;
}

IOReturn
IOFireWireUserClient::localConfigDirectory_Unpublish ( UserObjectHandle dirHandle ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}

	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = getOwner ()->getBus()->RemoveUnitDirectory ( dir ) ;
	
	dir->release() ;	// lookupObject retains result; "I must release you"
	
	return error ;
}

#pragma mark -
#pragma mark ADDRESS SPACES

//
// Address Space methods
//

IOReturn
IOFireWireUserClient::addressSpace_Create ( 
	AddressSpaceCreateParams *	params, 
	UserObjectHandle *			outAddressSpaceHandle )
{
	IOFWUserPseudoAddressSpace * addressSpace = OSTypeAlloc( IOFWUserPseudoAddressSpace );

	if ( addressSpace && 
				( params->isInitialUnits ? 
				!addressSpace->initFixed( this, params ) : !addressSpace->initPseudo( this, params ) ) )
	{
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = addressSpace->activate() ;
	
	if ( !error )
		error = fExporter->addObject ( addressSpace, 
										&IOFWUserPseudoAddressSpace::exporterCleanup, 
									   outAddressSpaceHandle ) ;		// nnn needs cleanup function?

	if ( error )
		addressSpace->deactivate() ;
	
	addressSpace->release() ;
		
	return error ;
}

IOReturn
IOFireWireUserClient::addressSpace_GetInfo (
	UserObjectHandle		addressSpaceHandle,
	AddressSpaceInfo *		outInfo )
{
	const OSObject *  object = fExporter->lookupObject( addressSpaceHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOFWUserPseudoAddressSpace *	me 		= OSDynamicCast( IOFWUserPseudoAddressSpace, object ) ;
	if (!me)
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	outInfo->address = me->getBase() ;

	me->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::addressSpace_ClientCommandIsComplete (
	UserObjectHandle		addressSpaceHandle,
	FWClientCommandID		inCommandID,
	IOReturn				inResult)
{
	const OSObject * object = fExporter->lookupObject( addressSpaceHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}

	IOReturn	result = kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace *	me	= OSDynamicCast( IOFWUserPseudoAddressSpace, object ) ;
	if (!me)
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	me->clientCommandIsComplete ( inCommandID, inResult ) ;
	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_Packet (
	OSAsyncReference64		asyncRef,
	UserObjectHandle		addressSpaceHandle,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inUserRefCon,
	void*,
	void*,
	void*)
{
	const OSObject * object = fExporter->lookupObject ( addressSpaceHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, object ) ;
	if ( ! me )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	if ( inCallback )
		super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
	else
		asyncRef[0] = 0 ;

	me->setAsyncRef_Packet(asyncRef) ;
	me->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_SkippedPacket(
	OSAsyncReference64		asyncRef,
	UserObjectHandle		inAddrSpaceRef,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inUserRefCon,
	void*,
	void*,
	void*)
{
	const OSObject * object = fExporter->lookupObject ( inAddrSpaceRef ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, object ) ;

	if ( !me )
	{
		object->release() ;
		result = kIOReturnBadArgument ;
	}
	
	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], (mach_vm_address_t) inCallback, (io_user_reference_t)inUserRefCon ) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_SkippedPacket(asyncRef) ;
	}
	
	me->release();
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_Read(
	OSAsyncReference64		asyncRef,
	UserObjectHandle		inAddrSpaceRef,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inUserRefCon,
	void*,
	void*,
	void*)
{
	const OSObject * object = fExporter->lookupObject ( inAddrSpaceRef ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, object ) ;

	if ( !me )
	{
		object->release() ;
		result = kIOReturnBadArgument ;
	}
	
	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_Read(asyncRef) ;
	}
	
	me->release();
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_BusReset(
	OSAsyncReference64		asyncRef,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], (mach_vm_address_t)inCallback, (io_user_reference_t)inRefCon ) ;
	
	bcopy(asyncRef, fBusResetAsyncNotificationRef, sizeof(OSAsyncReference64)) ;

	return kIOReturnSuccess ;
}
	
IOReturn
IOFireWireUserClient::setAsyncRef_BusResetDone(
	OSAsyncReference64		inAsyncRef,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	super::setAsyncReference64 ( inAsyncRef, (mach_port_t) inAsyncRef[0], inCallback, (io_user_reference_t)inRefCon ) ;

	bcopy(inAsyncRef, fBusResetDoneAsyncNotificationRef, sizeof(OSAsyncReference64)) ;

	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark PHYSICAL ADDRESS SPACES
//
// --- physical addr spaces ----------
//

IOReturn
IOFireWireUserClient::physicalAddressSpace_Create ( 
//	PhysicalAddressSpaceCreateParams *	params, 
	mach_vm_size_t				size,
	mach_vm_address_t			backingStore,
	UInt32						flags,
	UserObjectHandle * 			outAddressSpaceHandle )
{
	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddressRange( backingStore, size, kIODirectionOutIn, fTask ) ;
	if ( ! mem )
	{
		DebugLog("couldn't get memory descriptor for physical address space memory\n") ;
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = mem->prepare( kIODirectionPrepareToPhys32 );
	if ( error )
	{
		DebugLog("couldn't prepare address space memory descriptor\n") ;
		mem->release() ;
		return error ;
	}
	
	IOFWUserPhysicalAddressSpace *	addrSpace = OSTypeAlloc( IOFWUserPhysicalAddressSpace );
	if ( addrSpace && !addrSpace->initWithDesc( getOwner()->getController(), mem ) )
	{
		addrSpace->release() ;
		addrSpace = NULL ;
	}

	if ( ! addrSpace )
	{
		error = kIOReturnNoMemory ;
	}
	else
	{
		error = addrSpace->activate() ;

		if ( ! error )
			error = fExporter->addObject( addrSpace, 
										  &IOFWUserPhysicalAddressSpace::exporterCleanup, 
										  outAddressSpaceHandle );
										 
		addrSpace->release () ;	// fExporter will retain this
	}

	mem->release() ;		// address space will retain this if it needs it.. in any case
							// we're done with it.

	return error ;
	
}

IOReturn
IOFireWireUserClient::physicalAddressSpace_GetSegments (
	UserObjectHandle					addressSpaceHandle,
	UInt32								inSegmentCount,
	mach_vm_address_t					outSegments,
	UInt32*								outSegmentCount)
{
	const OSObject * object = fExporter->lookupObject( addressSpaceHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOFWUserPhysicalAddressSpace* addressSpace = OSDynamicCast(	IOFWUserPhysicalAddressSpace, object ) ;
																	
	if ( ! addressSpace )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	UInt32 segmentCount	;
	IOReturn error = addressSpace->getSegmentCount( &segmentCount ) ;
	if ( error == kIOReturnSuccess )
	{
		segmentCount = min( segmentCount, inSegmentCount ) ;
	
		FWPhysicalSegment32 * segments = new FWPhysicalSegment32[ segmentCount ] ;
		
		if ( !segments )
		{
			error = kIOReturnNoMemory ;
		}
		
		if ( !error )
		{
			error = addressSpace->getSegments( & segmentCount, segments ) ;
		}
		
		if ( ! error )
		{
			IOByteCount bytesCopied ;
			error = copyToUserBuffer( (IOVirtualAddress)segments, outSegments, sizeof( FWPhysicalSegment32 ) * segmentCount, bytesCopied ) ;
	
			*outSegmentCount = bytesCopied / sizeof( FWPhysicalSegment32 ) ;
		}

		delete[] segments ;
	}
	
	addressSpace->release() ; // retained by call to lookupObject()
	
	return error ;
}

#pragma mark
#pragma mark CONFIG DIRECTORY

IOReturn
IOFireWireUserClient::configDirectory_Create ( UserObjectHandle * outDirRef )
{
	IOConfigDirectory * configDir ;
	IOReturn error = getOwner ()->getConfigDirectoryRef ( configDir );

	if ( error )
		return error ;
		
	if ( !configDir )
		error = kIOReturnNoMemory ;
	else
	{
		error = fExporter->addObject ( configDir, NULL, outDirRef ) ;
		configDir->release () ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeyType (
	UserObjectHandle	dirHandle,
	int					key,
	IOConfigKeyType *	outType) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, object ) ;
	if ( !dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getKeyType(key, *outType) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeyValue_UInt32 (
	UserObjectHandle 		dirHandle, 
	int 					key,
	UInt32 					wantText, 
	UInt32 * 				outValue, 
	UserObjectHandle * 		outTextHandle, 
	UInt32 * 				outTextLength ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSString * outString = NULL ;
	IOReturn error = dir->getKeyValue( key, *outValue, ((bool)wantText) ? & outString : NULL ) ;

	if ( outString && !error )
	{
		error = fExporter->addObject( outString, NULL, outTextHandle ) ;
		
		outString->release () ;
		
		if ( ! error )
			*outTextLength = outString->getLength() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeyValue_Data (
	UserObjectHandle			dirHandle, 
	int 						key,
	UInt32 						wantText, 
	GetKeyValueDataResults *	results ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSData * outData = NULL ;
	OSString * outText = NULL ;
	IOReturn error = dir->getKeyValue ( key, outData, ((bool)wantText) ? & outText : NULL ) ;

	if ( outText )
	{
		if ( ! error )
		{
			error = fExporter->addObject( outText, NULL, &results->text ) ;
			results->textLength = outText->getLength() ;
		}
		
		outText->release() ;
	}
	
	if ( outData )
	{
		if ( ! error )
		{
			error = fExporter->addObject( outData, NULL, &results->data ) ;
			results->dataLength = outData->getLength() ;
		}
		
		outData->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeyValue_ConfigDirectory( 
		UserObjectHandle 		dirHandle, 
		int 						key,
		UInt32 						wantText, 
		UserObjectHandle * 	outDirHandle, 
		UserObjectHandle * 			outTextHandle, 
		UInt32 * 					outTextLength ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}

	OSString * outText = NULL ;
	IOConfigDirectory * outDir = NULL ;
	IOReturn error = dir->getKeyValue ( key, outDir, ((bool)wantText) ? & outText : NULL ) ;

	if ( outText )
	{
		if ( ! error )
		{
			*outTextLength = outText->getLength() ;
			error = fExporter->addObject( outText, NULL, outTextHandle ) ;
		}
		
		outText->release() ;
	}
	
	if ( outDir )
	{
		if ( ! error )
			error = fExporter->addObject( outDir, NULL, outDirHandle ) ;

		outDir->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeyOffset_FWAddress ( 
	UserObjectHandle	dirHandle, 
	int 					key, 
	UInt32 					wantText,
	GetKeyOffsetResults * 	results ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSString * outText = NULL ;
	IOReturn error = dir->getKeyOffset ( key, results->address, ((bool)wantText) ? & outText : NULL) ;

	if ( outText )
	{
		if ( ! error )
		{
			results->length = outText->getLength() ;
			error = fExporter->addObject( outText, NULL, &results->text ) ;
		}
		
		outText->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexType (
	UserObjectHandle	dirHandle,
	int					index,
	IOConfigKeyType*	outType ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getIndexType(index, *outType) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexKey (
	UserObjectHandle	dirHandle,
	int					index,
	int *				outKey ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getIndexKey(index, *outKey) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexValue_UInt32 (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outKey ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getIndexValue(index, *outKey) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexValue_Data (
	UserObjectHandle		dirHandle,
	int						index,
	UserObjectHandle *		outDataHandle,
	IOByteCount *			outDataLen ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSData * outData = NULL ;
	IOReturn error = dir->getIndexValue( index, outData ) ;
	
	if ( !error && outData )
	{
		error = fExporter->addObject( outData, NULL, outDataHandle ) ;
		*outDataLen = outData->getLength() ;
		
		outData->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexValue_String ( 
	UserObjectHandle 		dirHandle, 
	int 					index,
	UserObjectHandle * 		outTextHandle, 
	UInt32 * 				outTextLength ) const
{
	InfoLog("IOFireWireUserClient<%p>::configDirectory_GetIndexValue_String, index=%d, outTextHandle=%p\n", this ) ;

	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		DebugLog("IOFireWireUserClient<%p>::configDirectory_GetIndexValue_String() -- dir handle is invalid\n", this ) ;
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		DebugLog("IOFireWireUserClient<%p>::configDirectory_GetIndexValue_String() -- dir handle is not a config directory object!\n", this ) ;

		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSString * outText = NULL ;
	IOReturn error = dir->getIndexValue( index, outText ) ;

	InfoLog("a. IOFireWireUserClient<%p>::configDirectory_GetIndexValue_String error=%x\n", this, error ) ;

	if ( !error && outText )
	{
		*outTextLength = outText->getLength() ;
		
		error = fExporter->addObject( outText, NULL, outTextHandle ) ;
		
		outText->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexValue_ConfigDirectory (
	UserObjectHandle		dirHandle,
	int						index,
	UserObjectHandle *		outDirHandle ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * outDir = NULL ;
	IOReturn error = dir->getIndexValue( index, outDir ) ;
	
	if ( ! error && outDir )
	{
		error = fExporter->addObject ( outDir, NULL, outDirHandle ) ;
		outDir->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexOffset_FWAddress (
	UserObjectHandle	dirHandle,
	int					index,
	FWAddress *			outAddress ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn result = dir->getIndexOffset( index, *outAddress ) ;
	
	dir->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexOffset_UInt32 (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outValue) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getIndexOffset(index, *outValue) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetIndexEntry (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outValue) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = dir->getIndexEntry(index, *outValue) ;

	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetSubdirectories (
	UserObjectHandle		dirHandle,
	UserObjectHandle*		outIteratorHandle ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSIterator * outIterator = NULL ;
	IOReturn error = dir->getSubdirectories( outIterator ) ;

	if ( outIterator )
	{
		if ( ! error )
			error = fExporter->addObject( outIterator, NULL, outIteratorHandle ) ;

		outIterator->release () ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetKeySubdirectories (
	UserObjectHandle		dirHandle ,
	int							key,
	UserObjectHandle*			outIteratorHandle ) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	OSIterator * outIterator = NULL ;
	IOReturn error = dir->getKeySubdirectories( key, outIterator ) ;
	
	if ( outIterator )
	{
		if ( ! error )
			error = fExporter->addObject( outIterator, NULL, outIteratorHandle ) ;
		
		outIterator->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetType (
	UserObjectHandle		dirHandle,
	int*					outType) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	*outType = dir->getType();
	
	dir->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::configDirectory_GetNumEntries (
	UserObjectHandle		dirHandle,
	int*					outNumEntries) const
{
	const OSObject * object = fExporter->lookupObject( dirHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, object ) ;
	if ( ! dir )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	*outNumEntries = dir->getNumEntries() ;
	dir->release() ;
	
	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark LOCAL ISOCH PORT

IOReturn
IOFireWireUserClient::localIsochPort_GetSupported(
	UserObjectHandle		portHandle,
	IOFWSpeed*				outMaxSpeed,
	UInt32*					outChanSupportedHi,
	UInt32*					outChanSupportedLo) const
{
	const OSObject * object = fExporter->lookupObject( portHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOFWUserLocalIsochPort * port = OSDynamicCast ( IOFWUserLocalIsochPort, object ) ;
	if ( ! port )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	UInt64		chanSupported ;
	IOReturn	result			= kIOReturnSuccess ;

	result = port->getSupported( *outMaxSpeed, chanSupported ) ;

	*outChanSupportedHi = (UInt32)( chanSupported >> 32 ) ;
	*outChanSupportedLo	= (UInt32)( chanSupported & 0xFFFFFFFF ) ;
	
	port->release() ;
	
	return result ;
}


IOReturn
IOFireWireUserClient::localIsochPort_Create (
	LocalIsochPortAllocateParams*	params,
	UserObjectHandle*				outPortHandle )
{
	IOFWUserLocalIsochPort * port = OSTypeAlloc( IOFWUserLocalIsochPort );
	if ( port && ! port->initWithUserDCLProgram( params, *this, *getOwner()->getController() ) )
	{
		port->release() ;
		port = NULL ;
	}

	if ( !port )
	{
		DebugLog( "couldn't create local isoch port\n" ) ;
		return kIOReturnError ;
	}

	IOReturn error = fExporter->addObject( port,
			&IOFWUserLocalIsochPort::exporterCleanup, 
			outPortHandle ) ;
	
	port->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient::localIsochPort_SetChannel (
	UserObjectHandle	portHandle,
	UserObjectHandle	channelHandle )
{
	InfoLog("IOFireWireUserClient<%p>::localIsochPort_SetChannel\n", this ) ;

	const OSObject * object1 = fExporter->lookupObject( portHandle ) ;
	IOFWUserLocalIsochPort * port = OSDynamicCast ( IOFWUserLocalIsochPort, object1 ) ;
	IOReturn error = kIOReturnSuccess ;

	if ( !port )
	{
		error = kIOReturnBadArgument ;
	}

	const OSObject * object2 = NULL ;
	IOFWUserIsochChannel * channel = NULL ;
	
	if ( !error )
	{
		object2 = fExporter->lookupObject( channelHandle ) ;
				
		channel = OSDynamicCast( IOFWUserIsochChannel, object2 ) ;

		if ( !channel )
		{
			error = kIOReturnBadArgument ;
		}
	}
	
	if ( !error )
	{
		IODCLProgram * program = port->getProgramRef() ;
		if ( program )
		{
			program->setForceStopProc( (IOFWIsochChannel::ForceStopNotificationProc*)&IOFWUserIsochChannel::isochChannel_ForceStopHandler, channel, channel ) ;
			
			program->release() ;
		}
	}

	if ( object1 )
	{
		object1->release() ;
	}
	
	if ( object2 )
	{
		object2->release() ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_DCLCallProc ( 
		OSAsyncReference64 			asyncRef,
		UserObjectHandle 			portHandle )
{
	InfoLog("IOFireWireUserClient<%p>::setAsyncRef_DCLCallProc\n", this ) ;
	
	const OSObject * object =  fExporter->lookupObject( portHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}
	
	IOFWUserLocalIsochPort * port = OSDynamicCast( IOFWUserLocalIsochPort, object ) ;
	if ( ! port )
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	IOReturn error = port->setAsyncRef_DCLCallProc( asyncRef ) ;
	
	port->release() ;	// loopkupObject retains the return value for thread safety
	
	return error ;
}

#pragma mark -
#pragma mark ISOCH CHANNEL

IOReturn
IOFireWireUserClient::isochChannel_Create (
	bool					inDoIRM,
	UInt32					inPacketSize,
	IOFWSpeed				inPrefSpeed,
	UserObjectHandle *	outChannelHandle )
{
	// this code the same as IOFireWireController::createIsochChannel
	// must update this code when controller changes. We do this because
	// we are making IOFWUserIsochChannel objects, not IOFWIsochChannel
	// objects

	IOReturn error = kIOReturnSuccess ;
	IOFWUserIsochChannel * channel = OSTypeAlloc( IOFWUserIsochChannel );
	if ( channel )
	{
		if ( channel->init(	getOwner()->getController(), inDoIRM, inPacketSize, inPrefSpeed ) )
		{
			fExporter->addObject( channel, 
					(IOFWUserObjectExporter::CleanupFunction) & IOFWUserIsochChannel::s_exporterCleanup, 
					outChannelHandle ) ;
		}
		
		channel->release() ;	// addObject retains the object
	}
	else
	{
		error = kIOReturnNoMemory ;
	}	

	return error ;
}

IOReturn
IOFireWireUserClient::isochChannel_AllocateChannelBegin(
	UserObjectHandle		channelRef,
	UInt32 					speed,
	UInt32 					chansHi,
	UInt32 					chansLo,
	UInt32 * 				outSpeed,
	UInt32 * 				outChannel )
{
	InfoLog("IOFireWireUserClient<%p>::isochChannel_AllocateChannelBegin\n", this ) ;

	IOReturn error = kIOReturnSuccess ;
	const OSObject * object = fExporter->lookupObject( channelRef ) ;

	IOFWUserIsochChannel * channel = OSDynamicCast( IOFWUserIsochChannel, object ) ;

	if ( ! channel )
	{
		error = kIOReturnBadArgument ;
	}

	if ( !error )
	{
		UInt64 allowedChans = ((UInt64)chansHi << 32) | (UInt64)chansLo ;
		error = channel->allocateChannelBegin( (IOFWSpeed)speed, allowedChans, outChannel ) ;
		*outSpeed = speed;
	}
	
	if ( object )
	{
		object->release() ;	// lookup retains object, so we have to release it.	
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_IsochChannelForceStop(
	OSAsyncReference64		inAsyncRef,
	UserObjectHandle		channelRef )
{
	
	IOReturn error = kIOReturnSuccess;
	
	const OSObject * object = fExporter->lookupObject( channelRef ) ;
	
	IOFWUserIsochChannel * channel = OSDynamicCast( IOFWUserIsochChannel, object ) ;

	if ( !channel )
	{
#if __LP64__	
		DebugLog("IOFireWireUserClient<%p>::setAsyncRef_IsochChannelForceStop() -- invalid channel ref %llx\n", this, (UInt64)channelRef ) ;
#else
		DebugLog("IOFireWireUserClient<%p>::setAsyncRef_IsochChannelForceStop() -- invalid channel ref %lx\n", this, (UInt32)channelRef ) ;
#endif
		error = kIOReturnBadArgument ;
	}
	
	io_user_reference_t * asyncRef = NULL;
	if( error == kIOReturnSuccess )
	{
		asyncRef = new io_user_reference_t[ kOSAsyncRefCount ] ;
	
		if ( !asyncRef )
		{
			error = kIOReturnNoMemory ;
		}
	}
	
	if( error == kIOReturnSuccess )
	{
		bcopy( inAsyncRef, asyncRef, sizeof( OSAsyncReference64 ) ) ;
		
		io_user_reference_t * oldAsyncRef = channel->getUserAsyncRef() ;
		
		channel->setUserAsyncRef( asyncRef ) ;
		
		delete [] oldAsyncRef ;
	}
	
	if( channel != NULL )
	{
		channel->release();
	}

	return error;
}

#pragma mark -
#pragma mark COMMAND OBJECTS

// createAsyncCommand
//
//

IOReturn
IOFireWireUserClient::createAsyncCommand(	OSAsyncReference64 asyncRef,
											CommandSubmitParams * params,
											UserObjectHandle * kernel_ref )
{
	IOReturn status = kIOReturnSuccess;

	IOFWUserCommand * cmd = NULL;
	if( status == kIOReturnSuccess )
	{
		cmd = IOFWUserCommand::withSubmitParams( params, this );
		if( !cmd )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fExporter->addObject( cmd, NULL, kernel_ref );
	}

	if( status == kIOReturnSuccess )
	{
		cmd->setRefCon( (mach_vm_address_t)params->refCon);
	}
	
	if( status == kIOReturnSuccess )
	{
		setAsyncReference64( asyncRef, (mach_port_t) asyncRef[0], (mach_vm_address_t)params->callback, (io_user_reference_t)params->refCon);
	
		cmd->setAsyncReference64( asyncRef );
	}
	
	if( cmd )
	{
		cmd->release();		// we need to release this in all cases
		cmd = NULL;
	}
	
	return status;
}
															
															
IOReturn
IOFireWireUserClient::userAsyncCommand_Submit(
	OSAsyncReference64			asyncRef,
	CommandSubmitParams *		params,
	CommandSubmitResult *		outResult,
	IOByteCount					paramsSize,
	IOByteCount *				outResultSize)
{
	IOFWUserCommand * cmd = NULL ;
	
	IOReturn error = kIOReturnSuccess ;
	
	if ( params->kernCommandRef )
	{
		const OSObject * object = fExporter->lookupObject( params->kernCommandRef ) ;
		if ( !object )
		{
			error = kIOReturnBadArgument ;
		}
		else
		{
			cmd = OSDynamicCast( IOFWUserCommand, object ) ;
			if ( ! cmd )
			{
				object->release() ;
				error = kIOReturnBadArgument ;
			}
		}
	}
	else
	{
		cmd = IOFWUserCommand::withSubmitParams( params, this ) ;

		if ( ! cmd )
			error = kIOReturnNoMemory ;
		else
		{
			UserObjectHandle command_ref;
			error = fExporter->addObject( cmd, NULL, &command_ref );
			outResult->kernCommandRef = command_ref;
		}
	}
	
	if ( cmd )
	{
		if ( !error )
		{
			super::setAsyncReference64( asyncRef, (mach_port_t) asyncRef[0], (mach_vm_address_t)params->callback, (io_user_reference_t)params->refCon) ;
	
			cmd->setAsyncReference64( asyncRef ) ;
			cmd->setRefCon( (mach_vm_address_t)params->refCon);

			error = cmd->submit( params, outResult ) ;
		}
		
		cmd->release() ;		// we need to release this in all cases
	}

	return error ;
}

//
// --- absolute address firewire commands ----------
//
IOFWReadCommand*
IOFireWireUserClient::createReadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback	 		completion,
	void*					refcon ) const
{
	IOFWReadCommand* result = OSTypeAlloc( IOFWReadCommand );
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		result = NULL ;
	}
	
	return result ;
}

IOFWReadQuadCommand*
IOFireWireUserClient::createReadQuadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWReadQuadCommand* result = OSTypeAlloc( IOFWReadQuadCommand );
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteCommand*
IOFireWireUserClient::createWriteCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback 		completion,
	void*					refcon ) const
{
	IOFWWriteCommand* result = OSTypeAlloc( IOFWWriteCommand );
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteQuadCommand*
IOFireWireUserClient::createWriteQuadCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWWriteQuadCommand* result = OSTypeAlloc( IOFWWriteQuadCommand );
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

	// size is 1 for 32 bit compare, 2 for 64 bit.
IOFWCompareAndSwapCommand*
IOFireWireUserClient::createCompareAndSwapCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress,
	const UInt32 *			cmpVal, 
	const UInt32 *			newVal, 
	int 					size,
	FWDeviceCallback 		completion, 
	void *					refcon ) const
{
	IOFWCompareAndSwapCommand* result = OSTypeAlloc( IOFWCompareAndSwapCommand );
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, cmpVal, newVal, size, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

#pragma mark -

IOReturn
IOFireWireUserClient::firelog( const char* string, IOByteCount bufSize ) const
{

#if FIRELOG
	FireLog( string ) ;
#endif

	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::getBusGeneration( UInt32* outGeneration )
{
	*outGeneration = getOwner () -> getController () -> getGeneration () ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getLocalNodeIDWithGeneration( UInt32 testGeneration, UInt32* outLocalNodeID )
{
	if ( ! getOwner () -> getController () -> checkGeneration ( testGeneration ) )
		return kIOFireWireBusReset ;
	
	*outLocalNodeID = (UInt32) getOwner () -> getController () -> getLocalNodeID () ;

	// did generation change when we weren't looking?
	if ( ! getOwner () -> getController () -> checkGeneration ( testGeneration ) )
		return kIOFireWireBusReset ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getRemoteNodeID( UInt32 testGeneration, UInt32* outRemoteNodeID )
{
	UInt32 generation ;
	UInt16 nodeID ;
	
	IOReturn error = getOwner () -> getNodeIDGeneration ( generation, nodeID ) ;
	if (error)
		return error ;
	if ( generation != testGeneration )
		return kIOFireWireBusReset ;
	
	*outRemoteNodeID = (UInt32)nodeID ;

	// did generation change when we weren't looking?
	if ( ! getOwner () -> getController () -> checkGeneration ( generation ) )
		return kIOFireWireBusReset ;

	return error ;
}

IOReturn
IOFireWireUserClient::getSpeedToNode( UInt32 generation, UInt32* outSpeed )
{
	if ( ! getOwner () -> getController () -> checkGeneration ( generation ) )
		return kIOFireWireBusReset ;
	
	UInt16 nodeID ;
	getOwner () -> getNodeIDGeneration( generation, nodeID ) ;
	
	*outSpeed = (UInt32) getOwner () -> getController () -> FWSpeed ( nodeID ) ;

	// did generation change when we weren't looking?
	if ( ! getOwner () -> getController () -> checkGeneration ( generation ) )
		return kIOFireWireBusReset ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getSpeedBetweenNodes( UInt32 generation, UInt32 fromNode, UInt32 toNode, UInt32* outSpeed )
{
	if ( ! getOwner () -> getController () -> checkGeneration ( generation ) )
		return kIOFireWireBusReset ;	
	
	*outSpeed = (UInt32)getOwner ()->getController()->FWSpeed( (UInt16)fromNode, (UInt16)toNode ) ;
	// did generation change when we weren't looking?
	if (!getOwner ()->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getIRMNodeID( UInt32 generation, UInt32* irmNodeID )
{
	UInt16 tempNodeID ;
	UInt32 tempGeneration ;
	
	IOReturn error = (UInt32)getOwner ()->getController()->getIRMNodeID( tempGeneration, tempNodeID ) ;
	if (error)
		return error ;
		
	if ( tempGeneration != generation )
		return kIOFireWireBusReset ;
	
	*irmNodeID = (UInt32)tempNodeID ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::clipMaxRec2K( Boolean clipMaxRec )
{
	//IOLog("IOFireWireUserClient::clipMaxRec2K\n");
	
	if( fClippedMaxRec == clipMaxRec )
		return kIOReturnSuccess;		// Already set the way we want, no need to do it again
	
	IOReturn error = (UInt32)fOwner->getController()->clipMaxRec2K( clipMaxRec ) ;
	if (error)
		return error ;
	
	fClippedMaxRec = clipMaxRec;
			
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::seize( IOOptionBits inFlags )
{
	if ( ! OSDynamicCast(IOFireWireDevice, getOwner ()))
		return kIOReturnUnsupported ;

	if ( kIOReturnSuccess != clientHasPrivilege( fTask, kIOClientPrivilegeAdministrator ) )
		return kIOReturnNotPrivileged ;

	// message all clients that have the device open that
	// the device is going away. It's not really going away, but we
	// want them to think so...
	if ( getOwner ()->isOpen() )
	{
		OSIterator*			clientIterator =  getOwner ()->getOpenClientIterator() ;
		if (!clientIterator)
		{
			DebugLog("IOFireWireUserClient::seize: couldn't make owner client iterator\n") ;
			return kIOReturnError ;
		}	

		{
			IOService*			client = (IOService*)clientIterator->getNextObject() ;
	
			while ( client )
			{
				if ( client != this )
				{
					client->message( kIOFWMessageServiceIsRequestingClose, getOwner () ) ;
					client->message( kIOMessageServiceIsRequestingClose, getOwner () ) ;

					client->terminate() ;
				}
				
				client = (IOService*)clientIterator->getNextObject() ;
			}
		}

		clientIterator->release() ;
	}
	
	if ( getOwner ()->isOpen() )
	{
		OSIterator*			clientIterator =  getOwner ()->getClientIterator() ;
		if (!clientIterator)
		{
			DebugLog("IOFireWireUserClient::seize: couldn't make owner client iterator\n") ;
			return kIOReturnError ;
		}	

		{
			IOService*			client = (IOService*)clientIterator->getNextObject() ;
	
			while ( client )
			{
				if ( client != this )
				{
					client->message( kIOFWMessageServiceIsRequestingClose, getOwner () ) ;
					client->message( kIOMessageServiceIsRequestingClose, getOwner () ) ;

					client->terminate() ;
				}
				
				client = (IOService*)clientIterator->getNextObject() ;
			}
		}

		clientIterator->release() ;
	}

	return kIOReturnSuccess ;
}

IOReturn IOFireWireUserClient::message( UInt32 type, IOService * provider, void * arg )
{
	switch(type)
	{
		case kIOMessageServiceIsSuspended:
			if (fBusResetAsyncNotificationRef[0])
				sendAsyncResult64(fBusResetAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOMessageServiceIsResumed:
			if (fBusResetDoneAsyncNotificationRef[0])
				sendAsyncResult64(fBusResetDoneAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOFWMessageServiceIsRequestingClose:
			getOwner ()->messageClients(kIOMessageServiceIsRequestingClose) ;
			break;
		
	}
	
	super::message ( type, provider ) ;
	
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::getSessionRef( IOFireWireSessionRef * sessionRef )
{
    IOReturn error = kIOReturnSuccess;

    if( !fOwner->isOpen( this ) ) 
	{
		return kIOReturnNotOpen ;
	}
	
	*sessionRef = (IOFireWireSessionRef)fSessionRef;
    
	return error ;
}

#pragma mark -
#pragma mark ASYNC STREAM LISTENER

//
// Async Stream Listener
//

IOReturn
IOFireWireUserClient::setAsyncStreamRef_Packet (
	OSAsyncReference64		asyncRef,
	UserObjectHandle		asyncStreamListenerHandle,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inUserRefCon,
	void*,
	void*,
	void*
)
{
	IOReturn result = kIOReturnBadArgument;

	const OSObject * object = fExporter->lookupObject ( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}
	
	IOFWUserAsyncStreamListener * me = OSDynamicCast ( IOFWUserAsyncStreamListener, object ) ;
	if ( ! me )
	{
		object->release() ;
		return result ;
	}
	
	if ( inCallback )
		super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
	else
		asyncRef[0] = 0 ;

	me->setAsyncStreamRef_Packet(asyncRef) ;
	me->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::setAsyncStreamRef_SkippedPacket (
	OSAsyncReference64		asyncRef,
	UserObjectHandle		inAsyncStreamListenerRef,
	mach_vm_address_t		inCallback,
	io_user_reference_t		inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn result = kIOReturnBadArgument;
	
	const OSObject * object = fExporter->lookupObject ( inAsyncStreamListenerRef ) ;
	if ( !object )
	{
		return result ;
	}
	
	result	= kIOReturnSuccess ;
	IOFWUserAsyncStreamListener * me = OSDynamicCast ( IOFWUserAsyncStreamListener, object ) ;

	if ( !me )
	{
		object->release() ;
		result = kIOReturnBadArgument ;
	}
	
	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			super::setAsyncReference64 ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncStreamRef_SkippedPacket(asyncRef) ;
	}
	
	me->release();
	
	return result ;
}

IOReturn
IOFireWireUserClient::asyncStreamListener_Create ( 
	FWUserAsyncStreamListenerCreateParams*	params, 
	UserObjectHandle*						outAsyncStreamListenerHandle )
{
	IOFWUserAsyncStreamListener * asyncStreamListener = OSTypeAlloc( IOFWUserAsyncStreamListener );

	if ( asyncStreamListener == NULL )
		return kIOReturnNoMemory ;

	if ( not asyncStreamListener->initAsyncStreamListener( this, params ) )
	{
		asyncStreamListener->release();
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = fExporter->addObject ( asyncStreamListener, 
									(IOFWUserObjectExporter::CleanupFunction)&IOFWUserAsyncStreamListener::exporterCleanup, 
									outAsyncStreamListenerHandle ) ;		// nnn needs cleanup function?

	if ( error )
		asyncStreamListener->deactivate() ;
	
	asyncStreamListener->release() ;
		
	return error ;
}

IOReturn
IOFireWireUserClient::asyncStreamListener_ClientCommandIsComplete (
	UserObjectHandle		asyncStreamListenerHandle,
	FWClientCommandID		inCommandID )
{
	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return kIOReturnBadArgument ;
	}

	IOReturn	result = kIOReturnSuccess ;
	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if (!me)
	{
		object->release() ;
		return kIOReturnBadArgument ;
	}
	
	me->clientCommandIsComplete ( inCommandID ) ;
	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::asyncStreamListener_GetOverrunCounter (
	UserObjectHandle		asyncStreamListenerHandle,
	UInt32					*overrunCounter )
{
	IOReturn	result = kIOReturnBadArgument ;

	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}

	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if ( !me )
	{
		object->release() ;
		return result ;
	}
	
	*overrunCounter = me->getOverrunCounter( ) ;
	me->release() ;

	result = kIOReturnSuccess;
	
	return result ;

}

IOReturn
IOFireWireUserClient::asyncStreamListener_SetFlags (
	UserObjectHandle		asyncStreamListenerHandle,
	UInt32					flags )
{
	IOReturn	result = kIOReturnBadArgument ;

	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}

	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if (!me)
	{
		object->release() ;
		return result;
	}
	
	result = kIOReturnSuccess;
	me->setFlags( flags ) ;
	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::asyncStreamListener_GetFlags (
	UserObjectHandle		asyncStreamListenerHandle,
	UInt32					*flags )
{
	IOReturn	result = kIOReturnBadArgument ;

	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}

	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	result = kIOReturnSuccess;
	*flags = me->getFlags( ) ;
	me->release() ;
	
	return result ;
}

IOReturn	
IOFireWireUserClient::asyncStreamListener_TurnOnNotification (
	UserObjectHandle		asyncStreamListenerHandle )
{
	IOReturn	result = kIOReturnBadArgument ;

	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}

	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	result = kIOReturnSuccess;
	me->TurnOnNotification() ;
	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::asyncStreamListener_TurnOffNotification (
	UserObjectHandle		asyncStreamListenerHandle )
{
	IOReturn	result = kIOReturnBadArgument ;

	const OSObject * object = fExporter->lookupObject( asyncStreamListenerHandle ) ;
	if ( !object )
	{
		return result ;
	}

	IOFWUserAsyncStreamListener *me	= OSDynamicCast( IOFWUserAsyncStreamListener, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	result = kIOReturnSuccess;
	me->TurnOffNotification() ;
	me->release() ;
	
	return result ;
}

#pragma mark -
#pragma mark IRM ALLOCATIONS

IOReturn
IOFireWireUserClient::allocateIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation)
{
	return getOwner () -> getController () -> allocateIRMBandwidthInGeneration(bandwidthUnits, generation);
}

IOReturn
IOFireWireUserClient::releaseIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation)
{
	return getOwner () -> getController () -> releaseIRMBandwidthInGeneration(bandwidthUnits, generation);
}

IOReturn
IOFireWireUserClient::allocateIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation)
{
	return getOwner () -> getController () -> allocateIRMChannelInGeneration(isochChannel, generation);
}

IOReturn
IOFireWireUserClient::releaseIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation)
{
	return getOwner () -> getController () -> releaseIRMChannelInGeneration(isochChannel, generation);
}

typedef struct UserIRMAllocationParamsStruct
{
	io_user_reference_t asyncRef[kOSAsyncRef64Count];
	Boolean userNotificationEnabled;
	IOFireWireUserClient *pUserClient;
} UserIRMAllocationParams;

static void UserIRMAllocationCleanupFunction( const OSObject * obj )
{
	IOFireWireIRMAllocation * pIrmAllocation = (IOFireWireIRMAllocation *) obj;
	
	UserIRMAllocationParams *pUserIRMAllocationParams = (UserIRMAllocationParams*) pIrmAllocation->GetRefCon();
	delete pUserIRMAllocationParams;
}

static IOReturn UserIRMAllocationLostNotification(void* refCon, class IOFireWireIRMAllocation* allocation)
{
	UserIRMAllocationParams *pUserIRMAllocationParams = (UserIRMAllocationParams*) refCon;
	if (pUserIRMAllocationParams->userNotificationEnabled)
		pUserIRMAllocationParams->pUserClient->sendAsyncResult64(pUserIRMAllocationParams->asyncRef, kIOReturnSuccess, NULL, 0) ;
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::irmAllocation_Create(Boolean releaseIRMResourcesOnFree, UserObjectHandle* outIRMAllocationHandle)
{
	UserIRMAllocationParams *pUserIRMAllocationParams = new UserIRMAllocationParams;
	if (!pUserIRMAllocationParams)
		return kIOReturnNoMemory ;
	else
	{
		pUserIRMAllocationParams->userNotificationEnabled = false;
		pUserIRMAllocationParams->pUserClient = this;
	}

	IOFireWireIRMAllocation * irmAllocation = OSTypeAlloc( IOFireWireIRMAllocation );
	if (!irmAllocation)
	{
		delete pUserIRMAllocationParams;
		return kIOReturnNoMemory ;
	}
	
	if (!irmAllocation->init(getOwner()->getController(), releaseIRMResourcesOnFree, UserIRMAllocationLostNotification, pUserIRMAllocationParams))
	{
		irmAllocation->release();
		delete pUserIRMAllocationParams;
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = fExporter->addObject ( irmAllocation, UserIRMAllocationCleanupFunction, outIRMAllocationHandle ) ;
	irmAllocation->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::irmAllocation_AllocateResources(UserObjectHandle irmAllocationHandle, UInt8 isochChannel, UInt32 bandwidthUnits)
{
	IOReturn	result = kIOReturnBadArgument ;
	
	const OSObject * object = fExporter->lookupObject( irmAllocationHandle ) ;
	if ( !object )
	{
		return result ;
	}
	
	IOFireWireIRMAllocation *me	= OSDynamicCast( IOFireWireIRMAllocation, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	result = me->allocateIsochResources(isochChannel, bandwidthUnits) ;

	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::irmAllocation_DeallocateResources(UserObjectHandle irmAllocationHandle)
{
	IOReturn	result = kIOReturnBadArgument ;
	
	const OSObject * object = fExporter->lookupObject( irmAllocationHandle ) ;
	if ( !object )
	{
		return result ;
	}
	
	IOFireWireIRMAllocation *me	= OSDynamicCast( IOFireWireIRMAllocation, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	result = me->deallocateIsochResources();

	me->release() ;
	
	return result ;
}

Boolean
IOFireWireUserClient::irmAllocation_areResourcesAllocated(UserObjectHandle irmAllocationHandle, UInt8 *pIsochChannel, UInt32 *pBandwidthUnits)
{
	Boolean	result = false ;
	
	const OSObject * object = fExporter->lookupObject( irmAllocationHandle ) ;
	if ( !object )
	{
		return false ;
	}
	
	IOFireWireIRMAllocation *me	= OSDynamicCast( IOFireWireIRMAllocation, object ) ;
	if (!me)
	{
		object->release() ;
		return false ;
	}
	
	result = me->areIsochResourcesAllocated(pIsochChannel, pBandwidthUnits);
	
	me->release() ;
	
	return result ;
}

void
IOFireWireUserClient::irmAllocation_setDeallocateOnRelease(UserObjectHandle irmAllocationHandle, Boolean doDeallocationOnRelease)
{
	const OSObject * object = fExporter->lookupObject( irmAllocationHandle ) ;
	if ( !object )
	{
		return;
	}
	
	IOFireWireIRMAllocation *me	= OSDynamicCast( IOFireWireIRMAllocation, object ) ;
	if (!me)
	{
		object->release() ;
		return;
	}
	
	me->setReleaseIRMResourcesOnFree(doDeallocationOnRelease);
	
	me->release() ;
	
	return;
}

IOReturn
IOFireWireUserClient::irmAllocation_setRef(OSAsyncReference64 asyncRef,
											 UserObjectHandle irmAllocationHandle,
											 io_user_reference_t inCallback,
											 io_user_reference_t inUserRefCon)
{
	IOReturn	result = kIOReturnBadArgument ;
	
	const OSObject * object = fExporter->lookupObject( irmAllocationHandle ) ;
	if ( !object )
	{
		return result ;
	}
	
	IOFireWireIRMAllocation *me	= OSDynamicCast( IOFireWireIRMAllocation, object ) ;
	if (!me)
	{
		object->release() ;
		return result ;
	}
	
	UserIRMAllocationParams *pUserIRMAllocationParams = (UserIRMAllocationParams*) me->GetRefCon();
	
	for (UInt32 i=0;i<kOSAsyncRef64Count;i++)
		pUserIRMAllocationParams->asyncRef[i] = asyncRef[i];
	pUserIRMAllocationParams->asyncRef[ kIOAsyncCalloutFuncIndex ] = (mach_vm_address_t)inCallback ;
	pUserIRMAllocationParams->asyncRef[ kIOAsyncCalloutRefconIndex ] = inUserRefCon ;
	if (inCallback)
		pUserIRMAllocationParams->userNotificationEnabled = true;
	else
		pUserIRMAllocationParams->userNotificationEnabled = false;
	
	me->release() ;
	
	return result ;
}																				  
																				  
// createVectorCommand
//
//

IOReturn
IOFireWireUserClient::createVectorCommand( UserObjectHandle * kernel_ref )
{
	IOReturn status = kIOReturnSuccess;

	IOFWUserVectorCommand * cmd = NULL;
	if( status == kIOReturnSuccess )
	{
		cmd = IOFWUserVectorCommand::withUserClient( this );
		if( !cmd )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fExporter->addObject( cmd, NULL, kernel_ref );
	}
	
	if( cmd )
	{
		cmd->release();		// we need to release this in all cases
		cmd = NULL;
	}
	
	return status;
}

// createPHYPacketListener
//
//

IOReturn
IOFireWireUserClient::createPHYPacketListener( UInt32 queue_count, UserObjectHandle * kernel_ref )
{
	IOReturn status = kIOReturnSuccess;

	IOFWUserPHYPacketListener * listener = NULL;
	if( status == kIOReturnSuccess )
	{
		listener = IOFWUserPHYPacketListener::withUserClient( this, queue_count );
		if( !listener )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fExporter->addObject( listener, (IOFWUserObjectExporter::CleanupFunction)&IOFWUserPHYPacketListener::exporterCleanup, kernel_ref );
	}
	
	if( listener )
	{
		listener->release();		// we need to release this in all cases
		listener = NULL;
	}
	
	return status;
}
