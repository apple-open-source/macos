/*
* Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
* Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
*
* HISTORY
* 8 June 1999 wgulland created.
*
*/
/*
	$Log: IOFireWireUserClient.cpp,v $
	Revision 1.90  2003/09/20 00:54:17  collin
	*** empty log message ***
	
	Revision 1.89  2003/09/16 21:40:51  collin
	*** empty log message ***
	
	Revision 1.88  2003/09/11 20:59:46  collin
	*** empty log message ***
	
	Revision 1.87  2003/09/04 19:43:34  collin
	*** empty log message ***
	
	Revision 1.86  2003/09/02 23:48:12  collin
	*** empty log message ***
	
	Revision 1.85  2003/09/02 22:58:55  collin
	*** empty log message ***
	
	Revision 1.84  2003/08/30 00:16:45  collin
	*** empty log message ***
	
	Revision 1.83  2003/08/26 05:23:34  niels
	*** empty log message ***
	
	Revision 1.82  2003/08/26 05:11:21  niels
	*** empty log message ***
	
	Revision 1.81  2003/08/25 08:39:16  niels
	*** empty log message ***
	
	Revision 1.80  2003/08/20 18:48:43  niels
	*** empty log message ***
	
	Revision 1.79  2003/08/19 01:48:54  niels
	*** empty log message ***
	
	Revision 1.78  2003/08/14 19:46:06  niels
	*** empty log message ***
	
	Revision 1.77  2003/08/14 17:47:33  niels
	*** empty log message ***
	
	Revision 1.76  2003/08/08 22:30:32  niels
	*** empty log message ***
	
	Revision 1.75  2003/08/08 21:03:27  gecko1
	Merge max-rec clipping code into TOT
	
	Revision 1.74  2003/07/26 04:47:24  collin
	*** empty log message ***
	
	Revision 1.73  2003/07/24 20:49:48  collin
	*** empty log message ***
	
	Revision 1.72  2003/07/24 06:30:58  collin
	*** empty log message ***
	
	Revision 1.71  2003/07/24 03:06:27  collin
	*** empty log message ***
	
	Revision 1.70  2003/07/22 10:49:47  niels
	*** empty log message ***
	
	Revision 1.69  2003/07/21 07:29:48  niels
	*** empty log message ***
	
	Revision 1.68  2003/07/21 06:52:59  niels
	merge isoch to TOT
	
	Revision 1.66.2.5  2003/07/21 06:44:45  niels
	*** empty log message ***
	
	Revision 1.66.2.4  2003/07/18 00:17:42  niels
	*** empty log message ***
	
	Revision 1.66.2.3  2003/07/11 18:15:34  niels
	*** empty log message ***
	
	Revision 1.66.2.2  2003/07/09 21:24:01  niels
	*** empty log message ***
	
	Revision 1.66.2.1  2003/07/01 20:54:07  niels
	isoch merge
	
	Revision 1.66  2003/06/12 21:27:14  collin
	*** empty log message ***
	
	Revision 1.65  2003/06/07 01:30:40  collin
	*** empty log message ***
	
	Revision 1.64  2003/06/05 01:19:31  niels
	fix crash on close
	
	Revision 1.63  2003/04/22 02:45:28  collin
	*** empty log message ***
	
	Revision 1.62  2003/03/17 01:05:22  collin
	*** empty log message ***
	
	Revision 1.61  2003/03/01 00:10:07  collin
	*** empty log message ***
	
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
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOLocalConfigDirectory.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOFireWireDevice.h>
#import <sys/proc.h>
#import <IOKit/IOMessage.h>

#if FIRELOG
#import <IOKit/firewire/IOFireLog.h>
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

#if IOFIREWIREUSERCLIENTDEBUG > 0

#undef super
#define super OSObject

OSDefineMetaClassAndStructors( IOFWUserDebugInfo, OSObject )

bool
IOFWUserDebugInfo :: init( IOFireWireUserClient & userClient )
{
	if ( ! super :: init() )
		return false ;

	fUserClient = & userClient ;
//	fIsochCallbacks = OSNumber :: withNumber( (long long unsigned)0, 64 ) ;
//	if ( !fIsochCallbacks )
//		return false ;
	
	return true ;
}

bool
IOFWUserDebugInfo :: serialize ( 
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
IOFWUserDebugInfo :: free ()
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

IOFireWireUserClient *
IOFireWireUserClient :: withTask ( task_t owningTask )
{
	IOFireWireUserClient*	me				= new IOFireWireUserClient;

	if (me)
		if (me->init())
		{
			me->fTask = owningTask;
			me->mappings = 0 ;
			
		}
		else
		{
			me->release();
			return NULL;
		}
	
	return me;
}

bool
IOFireWireUserClient :: start( IOService * provider )
{
	if (!OSDynamicCast(IOFireWireNub, provider))
		return false ;

	if ( ! super::start ( provider ) )
		return false;

	fOwner = (IOFireWireNub *)provider;
	fOwner->retain();
	

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

	fExporter = new IOFWUserObjectExporter ;
	if ( fExporter && ! fExporter->init() ) 
	{
		fExporter->release() ;
		fExporter = NULL ;
	}
	
	if ( ! fExporter )
		result = false ;
		
#if IOFIREWIREUSERCLIENTDEBUG > 0
	if (result)
	{
		fDebugInfo = new IOFWUserDebugInfo ;
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

	return result ;
}

void
IOFireWireUserClient :: free()
{
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
	
	super :: free () ;
}

IOReturn
IOFireWireUserClient :: clientClose ()
{
	clipMaxRec2K( false );	// Make sure maxRec isn't clipped

	IOReturn	result = userClose() ;

	if ( result == kIOReturnSuccess )
	{
		DebugLog("IOFireWireUserClient :: clientClose(): client left user client open, should call close. Closing...\n") ;
	}
	else if ( result == kIOReturnNotOpen )
		result = kIOReturnSuccess ;

	if ( !terminate() )
	{
		IOLog("IOFireWireUserClient :: clientClose: terminate failed!, getOwner()->isOpen( this ) returned %u\n", getOwner()->isOpen(this)) ;
	}
	
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient :: clientDied ()
{
	if ( fOwner )
	{
		fOwner->getBus()->resetBus() ;
	}

	IOReturn error = clientClose () ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: setProperties (
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
			result = super :: setProperties ( properties ) ;
		}
	}
	else
		result = super :: setProperties ( properties ) ;
	
	return result ;
}

#pragma mark -

const IOFireWireUserClient::ExternalMethod IOFireWireUserClient::sMethods[ kNumMethods ] = 
{
	// --- open/close ---------------------------- 0
	{ 1, (IOMethod) & IOFireWireUserClient :: userOpen, kIOUCScalarIScalarO, 0, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: userOpenWithSessionRef, kIOUCScalarIScalarO, 1, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: userClose, kIOUCScalarIScalarO, 0, 0 }
	
	// --- user client general methods ----------- 3
	, { 1, (IOMethod) & IOFireWireUserClient :: readQuad, kIOUCStructIStructO, sizeof( ReadQuadParams ), sizeof( UInt32 ) }
	, { 1, (IOMethod) & IOFireWireUserClient :: read, kIOUCStructIStructO, sizeof( ReadParams ), sizeof( IOByteCount ) }
	, { 1, (IOMethod) & IOFireWireUserClient :: writeQuad, kIOUCStructIStructO, sizeof( WriteQuadParams ), 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: write, kIOUCStructIStructO, sizeof( WriteParams ), sizeof( IOByteCount ) }
	, { 1, (IOMethod) & IOFireWireUserClient :: compareSwap, kIOUCStructIStructO, sizeof ( CompareSwapParams ), sizeof ( UInt64 ) }
	, { 1, (IOMethod) & IOFireWireUserClient :: busReset, kIOUCScalarIScalarO, 0, 0 }
	, { 2, (IOMethod) & IOFireWireBus :: getCycleTime, kIOUCScalarIScalarO, 0, 1 }	
	, { 1, (IOMethod) & IOFireWireUserClient :: getGenerationAndNodeID, kIOUCScalarIScalarO, 0, 2 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getLocalNodeID, kIOUCScalarIScalarO, 0, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getResetTime, kIOUCStructIStructO, 0, sizeof(AbsoluteTime) }
	, { 1, (IOMethod) & IOFireWireUserClient :: releaseUserObject, kIOUCScalarIScalarO, 1, 0 }
	
	// --- conversion helpers -------------------- 14

	, { 1, (IOMethod) & IOFireWireUserClient :: getOSStringData, kIOUCScalarIScalarO, 3, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getOSDataData, kIOUCScalarIScalarO, 3, 1 }
	
	// -- Config ROM Methods ---------- 16
	
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_Create, kIOUCScalarIScalarO, 0, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_addEntry_Buffer, kIOUCScalarIScalarO, 6, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_addEntry_UInt32, kIOUCScalarIScalarO, 5, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_addEntry_FWAddr, kIOUCScalarIStructI, 4, sizeof(FWAddress) }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_addEntry_UnitDir, kIOUCScalarIStructI, 3, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_Publish, kIOUCScalarIScalarO, 1, 0 }
	, { 1, (IOMethod) & IOFireWireUserClient :: localConfigDirectory_Unpublish, kIOUCScalarIScalarO, 1, 0 }
	
	// --- Pseudo Address Space Methods --------- 23
	
	, { 1, (IOMethod) & IOFireWireUserClient :: addressSpace_Create, kIOUCStructIStructO, sizeof(AddressSpaceCreateParams), sizeof(UserObjectHandle) }
	, { 1, (IOMethod) & IOFireWireUserClient :: addressSpace_GetInfo, kIOUCScalarIStructO, 1, sizeof(AddressSpaceInfo) }
	, { 1, (IOMethod) & IOFireWireUserClient :: addressSpace_ClientCommandIsComplete, kIOUCScalarIScalarO, 3, 0 }
	
	// --- Physical Address Space Methods --------- 26
	
	, { 1, (IOMethod) & IOFireWireUserClient :: physicalAddressSpace_Create, kIOUCStructIStructO, sizeof( PhysicalAddressSpaceCreateParams ), sizeof( UserObjectHandle ) }
	, { 0, (IOMethod) & IOFWUserPhysicalAddressSpace :: getSegmentCount, kIOUCScalarIScalarO, 0, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: physicalAddressSpace_GetSegments, kIOUCScalarIScalarO, 4, 1 }
	
	// --- config directory ---------------------- 29

	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_Create, kIOUCScalarIScalarO, 0, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeyType, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeyValue_UInt32, kIOUCScalarIScalarO, 3, 3 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeyValue_Data, kIOUCScalarIStructO, 3, sizeof(GetKeyValueDataResults) }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeyValue_ConfigDirectory, kIOUCScalarIScalarO, 3, 3 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeyOffset_FWAddress, kIOUCScalarIStructO, 3, sizeof(GetKeyOffsetResults) }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexType, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexKey, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexValue_UInt32, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexValue_Data, kIOUCScalarIScalarO, 2, 2 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexValue_String, kIOUCScalarIScalarO, 2, 2 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexValue_ConfigDirectory, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexOffset_FWAddress, kIOUCScalarIStructO, 2, sizeof(FWAddress) }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexOffset_UInt32, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetIndexEntry, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetSubdirectories, kIOUCScalarIScalarO, 1, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetKeySubdirectories, kIOUCScalarIScalarO, 2, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetType, kIOUCScalarIScalarO, 1, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: configDirectory_GetNumEntries, kIOUCScalarIScalarO, 1, 1 }

	// --- isoch port methods ---------------------------- 48

	, { 1, (IOMethod) & IOFireWireUserClient :: localIsochPort_GetSupported, kIOUCScalarIScalarO, 1, 3 }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: allocatePort, kIOUCScalarIScalarO, 2, 0 }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: releasePort, kIOUCScalarIScalarO, 0, 0 }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: start, kIOUCScalarIScalarO, 0, 0 }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: stop, kIOUCScalarIScalarO, 0, 0 }

	// --- local isoch port methods ---------------------- 53
	, { 1, (IOMethod) & IOFireWireUserClient :: localIsochPort_Create, kIOUCStructIStructO, sizeof(LocalIsochPortAllocateParams), sizeof(UserObjectHandle) }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: modifyJumpDCL, kIOUCScalarIScalarO, 2, 0 }
	, { 0, (IOMethod) & IOFWUserLocalIsochPort :: userNotify, kIOUCScalarIStructI, 2, 0xFFFFFFFF /*variable size*/ }

	// --- isoch channel methods ------------------------- 56
	
	, { 1, (IOMethod) & IOFireWireUserClient :: isochChannel_Create, kIOUCScalarIScalarO, 3, 1 }
	, { 0, (IOMethod) & IOFWUserIsochChannel :: userAllocateChannelBegin, kIOUCScalarIScalarO, 3, 2 }
	, { 0, (IOMethod) & IOFWUserIsochChannel :: userReleaseChannelComplete, kIOUCScalarIScalarO, 0, 0 }

	// --- command objects ---------------------- 59
	
	, { 0, (IOMethod) & IOFWCommand :: cancel, kIOUCScalarIScalarO, 1, 0 }

	// --- seize service ---------- 60

	, { 1, (IOMethod) & IOFireWireUserClient :: seize, kIOUCScalarIScalarO, 1, 0 }
	
	// --- firelog ---------- 61
	, { 1, (IOMethod) & IOFireWireUserClient :: firelog, kIOUCStructIStructO, 0xFFFFFFFF, 0 }

	// v3 62
	
	, { 2, (IOMethod) & IOFireWireBus :: getBusCycleTime, kIOUCScalarIScalarO, 0, 2 }

	// v4 63
	
	, { 1, (IOMethod) & IOFireWireUserClient :: getBusGeneration, kIOUCScalarIScalarO, 0, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getLocalNodeIDWithGeneration, kIOUCScalarIScalarO, 1, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getRemoteNodeID, kIOUCScalarIScalarO, 1, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getSpeedToNode, kIOUCScalarIScalarO, 1, 1 }
	, { 1, (IOMethod) & IOFireWireUserClient :: getSpeedBetweenNodes, kIOUCScalarIScalarO, 3, 1 }

	// v5 68

	, { 1, (IOMethod) & IOFireWireUserClient :: getIRMNodeID, kIOUCScalarIScalarO, 1, 1 }

	// v6 69
	
	, { 1, (IOMethod) & IOFireWireUserClient :: clipMaxRec2K, kIOUCScalarIScalarO, 1, 0 }
	, { 0, (IOMethod) & IOFWLocalIsochPort :: setIsochResourceFlags, kIOUCScalarIScalarO, 1, 0 }
} ;

const IOFireWireUserClient::ExternalAsyncMethod IOFireWireUserClient :: sAsyncMethods[ kNumAsyncMethods ] =
{
	{ 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_BusReset, kIOUCScalarIScalarO, 2, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_BusResetDone, kIOUCScalarIScalarO, 2, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_Packet, kIOUCScalarIScalarO, 3, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_SkippedPacket, kIOUCScalarIScalarO, 3, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_Read, kIOUCScalarIScalarO, 3, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: userAsyncCommand_Submit, kIOUCStructIStructO, 0xFFFFFFFF, 0xFFFFFFFF }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_IsochChannelForceStop, kIOUCScalarIScalarO, 1, 0 }
	, { 1, (IOAsyncMethod) & IOFireWireUserClient :: setAsyncRef_DCLCallProc, kIOUCScalarIScalarO, 1, 0 }
} ;

IOExternalMethod* 
IOFireWireUserClient :: getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
	UInt32 actualIndex = index & 0xFFFF ;
	
	if ( actualIndex >= kNumMethods )
		return NULL;

	*target = fObjectTable[ sMethods[ actualIndex ].objectTableLookupIndex ] ;
	
	if ( !*target )
	{
		const OSObject * userObject = fExporter->lookupObject( (UserObjectHandle)( index >> 16 ) ) ;
//		DebugLog("using inline object %lx; found object %p, type %s, index=%lx\n", index>>16, userObject, userObject ? userObject->getMetaClass()->getClassName() : "(no object)", index ) ;

		*target = (IOService*)userObject ;		// "interesting code" note:
												// when we don't have an object set in our method table,
												// the object handle is encoded in the upper 16 bits of the 
												// method index...
												// We extract the object handle here to get the object to call...
		if ( *target )
		{
			(*target)->release() ;	// exporter retains returned objects, we have to release it here.. 
									// hopefully no one will release the object until we're done..
		}
		else
		{
			return NULL ;
		}
	}
		
	return (IOExternalMethod *) & sMethods[ actualIndex ] ;
}

IOExternalAsyncMethod* 
IOFireWireUserClient :: getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
	if( index >= kNumAsyncMethods )
		return NULL;

	*target = fObjectTable[ sAsyncMethods[index].objectTableLookupIndex ]  ;
	return (IOExternalAsyncMethod *) & sAsyncMethods[index] ;
}

IOReturn
IOFireWireUserClient :: registerNotificationPort(
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
IOFireWireUserClient :: 
copyUserData (
		IOVirtualAddress		userBuffer, 
		IOVirtualAddress		kernBuffer, 
		IOByteCount 			bytes ) const
{
	IOMemoryDescriptor *	desc 	= IOMemoryDescriptor :: withAddress (	userBuffer, bytes, kIODirectionOut, 
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
IOFireWireUserClient :: copyToUserBuffer (
	IOVirtualAddress 		kernelBuffer,
	IOVirtualAddress		userBuffer,
	IOByteCount 			bytes,
	IOByteCount & 			bytesCopied )
{
	IOMemoryDescriptor *	mem = IOMemoryDescriptor::withAddress( userBuffer, bytes, kIODirectionIn, fTask ) ;
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
IOFireWireUserClient :: userOpen ()
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
		fOpenClient = this ;
		error = kIOReturnSuccess ;
	}
	else
	{
		ErrorLog( "couldn't open provider\n" ) ;
		error = kIOReturnExclusiveAccess ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient :: userOpenWithSessionRef ( IOService * sessionRef )
{
	IOReturn	result = kIOReturnSuccess ;
	
	if (getOwner ()->isOpen())
	{
		IOService*	client = OSDynamicCast(IOService, sessionRef) ;
	
		if (!client)
			result = kIOReturnBadArgument ;
		else
		{
			while (client != NULL)
			{
				if (client == getOwner ())
				{
					fOpenClient = sessionRef ;	// sessionRef is the originally passed in user object
				
					break ;
				}
				
				client = client->getProvider() ;
			}
		}
	}
	else
		result = kIOReturnNotOpen ;
		
	return result ;
}

IOReturn
IOFireWireUserClient :: userClose ()
{
	IOReturn result = kIOReturnSuccess ;

	if ( getProvider() == NULL )
		return kIOReturnSuccess ;
			
	if (!fOwner->isOpen( this ))
	{
		result = kIOReturnNotOpen ;
	}
	else
	{
		if (fOpenClient == this)
		{
			fOwner->close(this) ;
		}
		
		fOpenClient = NULL ;
	}		
	
	return result ;
}

#pragma mark -
#pragma mark GENERAL

IOReturn
IOFireWireUserClient :: readQuad ( const ReadQuadParams* params, UInt32* outVal )
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
IOFireWireUserClient :: read ( const ReadParams* params, IOByteCount* outBytesTransferred )
{
	IOReturn 					err ;
	IOMemoryDescriptor *		mem ;
	IOFWReadCommand*			cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddress((vm_address_t)params->buf, (IOByteCount)params->size, kIODirectionIn, fTask);
	if(!mem)
	{
		return kIOReturnNoMemory;
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
		mem->release() ;
		return kIOReturnNoMemory;
	}
	
	err = mem->prepare() ;

	if ( err )
		IOLog("%s %u: IOFireWireUserClient :: read: prepare failed\n", __FILE__, __LINE__) ;
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
IOFireWireUserClient :: writeQuad( const WriteQuadParams* params)
{
	IOReturn 				err;
	IOFWWriteQuadCommand*	cmd ;

	if ( params->isAbs )
		cmd = this->createWriteQuadCommand( params->generation, params->addr, & (UInt32)params->val, 1, NULL, NULL ) ;
	else
	{
		cmd = getOwner ()->createWriteQuadCommand( params->addr, & (UInt32)params->val, 1, NULL, NULL, params->failOnReset ) ;
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
IOFireWireUserClient :: write( const WriteParams* params, IOByteCount* outBytesTransferred )
{
	IOMemoryDescriptor *	mem ;
	IOFWWriteCommand*		cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddress((vm_address_t) params->buf, params->size, kIODirectionOut, fTask);
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

	*outBytesTransferred = cmd->getBytesTransferred() ;
	
	cmd->release();
	
	mem->complete() ;
	mem->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient :: compareSwap( const CompareSwapParams* params, UInt64* oldVal )
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
		cmd->locked((UInt32*)oldVal) ;
		err = cmd->getStatus();
//		if(kIOReturnSuccess == err && !cmd->locked((UInt32*)oldVal))
//			err = kIOReturnCannotLock;
	}

	cmd->release();

	return err ;
}

IOReturn 
IOFireWireUserClient :: busReset()
{
	if ( fUnsafeResets )
		return getOwner ()->getController()->getLink()->resetBus();

	return getOwner ()->getController()->resetBus();
}

IOReturn
IOFireWireUserClient :: getGenerationAndNodeID(
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
IOFireWireUserClient :: getLocalNodeID(
	UInt32*					outLocalNodeID) const
{
	*outLocalNodeID = (UInt32)(getOwner ()->getController()->getLocalNodeID()) ;	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: getResetTime(
	AbsoluteTime*	outResetTime) const
{
	*outResetTime = *getOwner ()->getController()->getResetTime() ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: releaseUserObject (
	UserObjectHandle		obj )
{
	DebugLog("+IOFireWireUserClient :: releaseUserObject\n") ;
	fExporter->removeObject( obj ) ;

	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark CONVERSION HELPERS

IOReturn
IOFireWireUserClient :: getOSStringData ( 
	UserObjectHandle 			stringHandle, 
	UInt32 						stringLen, 
	char * 						stringBuffer,
	UInt32 * 					outTextLength )
{
	*outTextLength = 0 ;

	OSString * string = OSDynamicCast ( OSString, fExporter->lookupObject( stringHandle ) ) ;
	if ( ! string )
		return kIOReturnBadArgument ;
	
	UInt32 len = stringLen <? string->getLength() ;

	IOReturn error = copyToUserBuffer (	(IOVirtualAddress) string->getCStringNoCopy(), 
										(IOVirtualAddress) stringBuffer, 
										len, 
										*outTextLength ) ;

	fExporter->removeObject( stringHandle ) ;
	string->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: getOSDataData (
	UserObjectHandle			dataHandle,
	IOByteCount				dataLen,
	char*					dataBuffer,
	IOByteCount*			outDataLen)
{
	OSData * data = OSDynamicCast( OSData, fExporter->lookupObject( dataHandle ) ) ;
	if ( ! data )
		return kIOReturnBadArgument ;
	
	IOReturn error = copyToUserBuffer (	(IOVirtualAddress) data->getBytesNoCopy(), 
										(IOVirtualAddress) dataBuffer, 
										( data->getLength() <? dataLen ), 
										*outDataLen ) ;
	fExporter->removeObject( dataHandle ) ;
	data->release() ;		// this is retained when we call lookupObject; "I must release you"
	
	return error ;
}

#pragma mark -
#pragma mark LOCAL CONFIG DIRECTORY

//	Config Directory methods

IOReturn
IOFireWireUserClient :: localConfigDirectory_Create ( UserObjectHandle* outDir )
{
	IOLocalConfigDirectory * dir = IOLocalConfigDirectory :: create () ;

	if ( ! dir )
	{
		DebugLog ( "IOLocalConfigDirectory::create returned NULL\n" ) ;
		return kIOReturnNoMemory ;
	}

	IOReturn error = fExporter->addObject ( *dir, NULL, *outDir ) ;		// must make a cleanup function that calls unpublish()
																		// and pass it to addObject
	
	dir->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient :: localConfigDirectory_addEntry_Buffer ( 
	UserObjectHandle		dirHandle, 
	int 					key, 
	char * 					buffer, 
	UInt32 					kr_size,
	const char *			descCString,
	UInt32					descLen ) const
{
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, fExporter->lookupObject ( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;

	IOReturn error = kIOReturnSuccess ;
	OSData * data = OSData :: withBytes ( buffer, kr_size ) ;
	if ( !data )
		error = kIOReturnNoMemory ;
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
		
		error = dir->addEntry ( key, data, desc ) ;

		data->release() ;
	}
	
	dir->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient :: localConfigDirectory_addEntry_UInt32 ( 
	UserObjectHandle 		dirHandle, 
	int 					key, 
	UInt32 					value,
	const char *			descCString,
	UInt32					descLen ) const
{
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, fExporter->lookupObject( dirHandle) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;

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
IOFireWireUserClient :: localConfigDirectory_addEntry_FWAddr ( 
	UserObjectHandle	dirHandle, 
	int					key, 
	const char *		descCString,
	UInt32				descLen,
	FWAddress *			value ) const
{
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;

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
IOFireWireUserClient :: localConfigDirectory_addEntry_UnitDir ( 
	UserObjectHandle	dirHandle, 
	int 				key, 
	UserObjectHandle 	valueHandle,
	const char *		descCString,
	UInt32				descLen ) const
{
	IOLocalConfigDirectory * dir = OSDynamicCast( IOLocalConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn error ;
	IOLocalConfigDirectory * value = OSDynamicCast( IOLocalConfigDirectory, fExporter->lookupObject( valueHandle ) ) ;
	if ( ! value )
		error = kIOReturnBadArgument ;
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

	dir->release() ;	// lookupObject retains the object
	
	return error ;
}

IOReturn
IOFireWireUserClient :: localConfigDirectory_Publish ( UserObjectHandle dirHandle ) const
{	
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;

	IOReturn error = getOwner ()->getController()->AddUnitDirectory( dir ) ;
	
	dir->release() ;	// lookupObject retains result; "I must release you"
	
	return error ;
}

IOReturn
IOFireWireUserClient :: localConfigDirectory_Unpublish ( UserObjectHandle dirHandle ) const
{
	IOLocalConfigDirectory * dir = OSDynamicCast ( IOLocalConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
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
IOFireWireUserClient :: addressSpace_Create ( 
	AddressSpaceCreateParams *	params, 
	UserObjectHandle *			outAddressSpaceHandle )
{
	IOFWUserPseudoAddressSpace * addressSpace = new IOFWUserPseudoAddressSpace;

	if ( addressSpace && 
				( params->isInitialUnits ? 
				!addressSpace->initFixed( this, params ) : !addressSpace->initPseudo( this, params ) ) )
	{
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = addressSpace->activate() ;
	
	if ( !error )
		error = fExporter->addObject ( * addressSpace, 
									   (IOFWUserObjectExporter::CleanupFunction) & IOFWUserPseudoAddressSpace::exporterCleanup, 
									   * outAddressSpaceHandle ) ;		// nnn needs cleanup function?

	if ( error )
		addressSpace->deactivate() ;
	
	addressSpace->release() ;
		
	return error ;
}

IOReturn
IOFireWireUserClient :: addressSpace_GetInfo (
	UserObjectHandle		addressSpaceHandle,
	AddressSpaceInfo *		outInfo )
{
	IOFWUserPseudoAddressSpace *	me 		= OSDynamicCast(	IOFWUserPseudoAddressSpace, 
																fExporter->lookupObject( addressSpaceHandle ) ) ;
	if (!me)
		return kIOReturnBadArgument ;

	outInfo->address = me->getBase() ;

	me->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: addressSpace_ClientCommandIsComplete (
	UserObjectHandle		addressSpaceHandle,
	FWClientCommandID		inCommandID,
	IOReturn				inResult)
{
	IOReturn	result = kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace *	me	= OSDynamicCast( IOFWUserPseudoAddressSpace, fExporter->lookupObject( addressSpaceHandle ) ) ;
	if (!me)
		return kIOReturnBadArgument ;

	me->clientCommandIsComplete ( inCommandID, inResult ) ;
	me->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient :: setAsyncRef_Packet (
	OSAsyncReference		asyncRef,
	UserObjectHandle		addressSpaceHandle,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, fExporter->lookupObject ( addressSpaceHandle ) ) ;
	if ( ! me )
		return kIOReturnBadArgument ;

	if ( inCallback )
		super :: setAsyncReference ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
	else
		asyncRef[0] = 0 ;

	me->setAsyncRef_Packet(asyncRef) ;
	me->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: setAsyncRef_SkippedPacket(
	OSAsyncReference		asyncRef,
	UserObjectHandle		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, fExporter->lookupObject ( inAddrSpaceRef ) ) ;

	if ( NULL == me)
		result = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			super :: setAsyncReference ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_SkippedPacket(asyncRef) ;
	}
	
	me->release();
	
	return result ;
}

IOReturn
IOFireWireUserClient :: setAsyncRef_Read(
	OSAsyncReference		asyncRef,
	UserObjectHandle		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace * me = OSDynamicCast ( IOFWUserPseudoAddressSpace, fExporter->lookupObject ( inAddrSpaceRef ) ) ;

	if ( NULL == me)
		result = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			super :: setAsyncReference ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon ) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_Read(asyncRef) ;
	}
	
	me->release();
	
	return result ;
}

IOReturn
IOFireWireUserClient :: setAsyncRef_BusReset(
	OSAsyncReference		asyncRef,
	void*					inCallback,
	void*					inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	super :: setAsyncReference ( asyncRef, (mach_port_t) asyncRef[0], inCallback, inRefCon ) ;
	
	bcopy(asyncRef, fBusResetAsyncNotificationRef, sizeof(OSAsyncReference)) ;

	return kIOReturnSuccess ;
}
	
IOReturn
IOFireWireUserClient :: setAsyncRef_BusResetDone(
	OSAsyncReference		inAsyncRef,
	void*					inCallback,
	void*					inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	super :: setAsyncReference ( inAsyncRef, (mach_port_t) inAsyncRef[0], inCallback, inRefCon ) ;

	bcopy(inAsyncRef, fBusResetDoneAsyncNotificationRef, sizeof(OSAsyncReference)) ;

	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark PHYSICAL ADDRESS SPACES
//
// --- physical addr spaces ----------
//

IOReturn
IOFireWireUserClient :: physicalAddressSpace_Create ( 
	PhysicalAddressSpaceCreateParams *	params, 
	UserObjectHandle * 			outAddressSpaceHandle )
{
	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddress((vm_address_t)params->backingStore, params->size, kIODirectionOutIn, fTask) ;
	if ( ! mem )
	{
		return kIOReturnNoMemory ;
	}
	
	IOReturn error = mem->prepare() ;
	if ( error )
	{
		mem->release() ;
		return error ;
	}
	
	IOFWUserPhysicalAddressSpace *	addrSpace = new IOFWUserPhysicalAddressSpace ;
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
			error = fExporter->addObject( *addrSpace, 
										  (IOFWUserObjectExporter::CleanupFunction) &IOFWUserPhysicalAddressSpace::exporterCleanup, 
										  *outAddressSpaceHandle );
										 
		addrSpace->release () ;	// fExporter will retain this
	}

	mem->release() ;		// address space will retain this if it needs it.. in any case
							// we're done with it.

	return error ;
	
}

IOReturn
IOFireWireUserClient :: physicalAddressSpace_GetSegments (
	UserObjectHandle			addressSpaceHandle,
	UInt32								inSegmentCount,
	IOMemoryCursor::IOPhysicalSegment *	outSegments,
	UInt32*								outSegmentCount)
{
	IOFWUserPhysicalAddressSpace* addressSpace = OSDynamicCast(	IOFWUserPhysicalAddressSpace, 
																		fExporter->lookupObject( addressSpaceHandle )) ;
																	
	if ( ! addressSpace )
		return kIOReturnBadArgument ;
	
	UInt32 segmentCount	;
	IOReturn error = addressSpace->getSegmentCount( &segmentCount ) ;
	if ( error == kIOReturnSuccess )
	{
		
		segmentCount = segmentCount <? inSegmentCount ;
	
		IOPhysicalSegment	segments[ segmentCount ] ;
			
		error = addressSpace->getSegments( & segmentCount, segments ) ;
		
		if ( ! error )
		{
			IOByteCount bytesCopied ;
			error = copyToUserBuffer( (IOVirtualAddress)segments, (IOVirtualAddress)outSegments, sizeof( IOPhysicalAddress ) * segmentCount, bytesCopied ) ;
	
			*outSegmentCount = bytesCopied / sizeof( IOPhysicalSegment ) ;
		}
	}
	
	addressSpace->release() ; // retained by call to lookupObject()
	
	return error ;
}

#pragma mark
#pragma mark CONFIG DIRECTORY

IOReturn
IOFireWireUserClient :: configDirectory_Create ( UserObjectHandle * outDirRef )
{
	IOConfigDirectory * configDir ;
	IOReturn error = getOwner ()->getConfigDirectoryRef ( configDir );

	if ( error )
		return error ;
		
	if ( !configDir )
		error = kIOReturnNoMemory ;
	else
	{
		error = fExporter->addObject ( *configDir, NULL, *outDirRef ) ;
		configDir->release () ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeyType (
	UserObjectHandle	dirHandle,
	int					key,
	IOConfigKeyType *	outType) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( !dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getKeyType(key, *outType) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeyValue_UInt32 (
	UserObjectHandle 		dirHandle, 
	int 					key,
	UInt32 					wantText, 
	UInt32 * 				outValue, 
	UserObjectHandle * 		outTextHandle, 
	UInt32 * 				outTextLength ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSString * outString = NULL ;
	IOReturn error = dir->getKeyValue( key, *outValue, ((bool)wantText) ? & outString : NULL ) ;

	if ( outString && !error )
	{
		error = fExporter->addObject( *outString, NULL, *outTextHandle ) ;
		
		outString->release () ;
		
		if ( ! error )
			*outTextLength = outString->getLength() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeyValue_Data (
	UserObjectHandle			dirHandle, 
	int 						key,
	UInt32 						wantText, 
	GetKeyValueDataResults *	results ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSData * outData = NULL ;
	OSString * outText = NULL ;
	IOReturn error = dir->getKeyValue ( key, outData, ((bool)wantText) ? & outText : NULL ) ;

	if ( outText )
	{
		if ( ! error )
		{
			error = fExporter->addObject( *outText, NULL, results->text ) ;
			results->textLength = outText->getLength() ;
		}
		
		outText->release() ;
	}
	
	if ( outData )
	{
		if ( ! error )
		{
			error = fExporter->addObject( *outText, NULL, results->data ) ;
			results->dataLength = outText->getLength() ;
		}
		
		outData->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeyValue_ConfigDirectory( 
		UserObjectHandle 		dirHandle, 
		int 						key,
		UInt32 						wantText, 
		UserObjectHandle * 	outDirHandle, 
		UserObjectHandle * 			outTextHandle, 
		UInt32 * 					outTextLength ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;

	OSString * outText = NULL ;
	IOConfigDirectory * outDir = NULL ;
	IOReturn error = dir->getKeyValue ( key, outDir, ((bool)wantText) ? & outText : NULL ) ;

	if ( outText )
	{
		if ( ! error )
		{
			*outTextLength = outText->getLength() ;
			error = fExporter->addObject( *outText, NULL, *outTextHandle ) ;
		}
		
		outText->release() ;
	}
	
	if ( outDir )
	{
		if ( ! error )
			error = fExporter->addObject( *outDir, NULL, *outDirHandle ) ;

		outDir->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeyOffset_FWAddress ( 
	UserObjectHandle	dirHandle, 
	int 					key, 
	UInt32 					wantText,
	GetKeyOffsetResults * 	results ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSString * outText = NULL ;
	IOReturn error = dir->getKeyOffset ( key, results->address, ((bool)wantText) ? & outText : NULL) ;

	if ( outText )
	{
		if ( ! error )
		{
			results->length = outText->getLength() ;
			error = fExporter->addObject( *outText, NULL, results->text ) ;
		}
		
		outText->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexType (
	UserObjectHandle	dirHandle,
	int					index,
	IOConfigKeyType*	outType ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( !dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getIndexType(index, *outType) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexKey (
	UserObjectHandle	dirHandle,
	int					index,
	int *				outKey ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getIndexKey(index, *outKey) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexValue_UInt32 (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outKey ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getIndexValue(index, *outKey) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexValue_Data (
	UserObjectHandle		dirHandle,
	int						index,
	UserObjectHandle *		outDataHandle,
	IOByteCount *			outDataLen ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSData * outData = NULL ;
	IOReturn error = dir->getIndexValue( index, outData ) ;
	
	if ( !error && outData )
	{
		error = fExporter->addObject( *outData, NULL, *outDataHandle ) ;
		*outDataLen = outData->getLength() ;
		
		outData->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexValue_String ( 
	UserObjectHandle 		dirHandle, 
	int 					index,
	UserObjectHandle * 		outTextHandle, 
	UInt32 * 				outTextLength ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSString * outText = NULL ;
	IOReturn error = dir->getIndexValue( index, outText ) ;

	if ( !error && outText )
	{
		*outTextLength = outText->getLength() ;
		error = fExporter->addObject( *outText, NULL, *outTextHandle ) ;
		
		outText->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexValue_ConfigDirectory (
	UserObjectHandle		dirHandle,
	int						index,
	UserObjectHandle *		outDirHandle ) const
{
	IOConfigDirectory * dir = OSDynamicCast ( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOConfigDirectory * outDir = NULL ;
	IOReturn error = dir->getIndexValue( index, outDir ) ;
	
	if ( ! error && outDir )
	{
		error = fExporter->addObject ( *outDir, NULL, *outDirHandle ) ;
		outDir->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexOffset_FWAddress (
	UserObjectHandle	dirHandle,
	int					index,
	FWAddress *			outAddress ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn result = dir->getIndexOffset( index, *outAddress ) ;
	
	dir->release() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexOffset_UInt32 (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outValue) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getIndexOffset(index, *outValue) ;
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetIndexEntry (
	UserObjectHandle	dirHandle,
	int					index,
	UInt32*				outValue) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	IOReturn error = dir->getIndexEntry(index, *outValue) ;

	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetSubdirectories (
	UserObjectHandle		dirHandle,
	UserObjectHandle*		outIteratorHandle ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSIterator * outIterator = NULL ;
	IOReturn error = dir->getSubdirectories( outIterator ) ;

	if ( outIterator )
	{
		if ( ! error )
			error = fExporter->addObject( *outIterator, NULL, *outIteratorHandle ) ;

		outIterator->release () ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetKeySubdirectories (
	UserObjectHandle		dirHandle ,
	int							key,
	UserObjectHandle*			outIteratorHandle ) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	OSIterator * outIterator = NULL ;
	IOReturn error = dir->getKeySubdirectories( key, outIterator ) ;
	
	if ( outIterator )
	{
		if ( ! error )
			error = fExporter->addObject( *outIterator, NULL, *outIteratorHandle ) ;
		
		outIterator->release() ;
	}
	
	dir->release() ;
	
	return error ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetType (
	UserObjectHandle		dirHandle,
	int*					outType) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	*outType = dir->getType();
	
	dir->release() ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: configDirectory_GetNumEntries (
	UserObjectHandle		dirHandle,
	int*					outNumEntries) const
{
	IOConfigDirectory * dir = OSDynamicCast( IOConfigDirectory, fExporter->lookupObject( dirHandle ) ) ;
	if ( ! dir )
		return kIOReturnBadArgument ;
	
	*outNumEntries = dir->getNumEntries() ;
	dir->release() ;
	
	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark LOCAL ISOCH PORT

IOReturn
IOFireWireUserClient :: localIsochPort_GetSupported(
	UserObjectHandle		portHandle,
	IOFWSpeed*				outMaxSpeed,
	UInt32*					outChanSupportedHi,
	UInt32*					outChanSupportedLo) const
{
	IOFWUserLocalIsochPort * port = OSDynamicCast ( IOFWUserLocalIsochPort, fExporter->lookupObject( portHandle ) ) ;
	if ( ! port )
		return kIOReturnBadArgument ;
		
	UInt64		chanSupported ;
	IOReturn	result			= kIOReturnSuccess ;

	result = port->getSupported( *outMaxSpeed, chanSupported ) ;

	*outChanSupportedHi = (UInt32)( chanSupported >> 32 ) ;
	*outChanSupportedLo	= (UInt32)( chanSupported & 0xFFFFFFFF ) ;
	
	port->release() ;
	
	return result ;
}


IOReturn
IOFireWireUserClient :: localIsochPort_Create (
	LocalIsochPortAllocateParams*	params,
	UserObjectHandle*				outPortHandle )
{
	DebugLog("+IOFireWireUserClient :: localIsochPort_Create\n") ;
	
	IOFWUserLocalIsochPort * port = new IOFWUserLocalIsochPort ;
	if ( port && ! port->initWithUserDCLProgram( params, *this, *getOwner()->getController() ) )
	{
		port->release() ;
		port = NULL ;
	}

	DebugLog("port retain is %d\n", getRetainCount()) ;

	if ( !port )
	{
		DebugLog( "couldn't create local isoch port\n" ) ;
		return kIOReturnError ;
	}

	IOReturn error = fExporter->addObject( *port, 
			(IOFWUserObjectExporter::CleanupFunction) & IOFWUserLocalIsochPort::exporterCleanup, 
			*outPortHandle ) ;
	
	port->release() ;

	return error ;
}

IOReturn
IOFireWireUserClient :: setAsyncRef_DCLCallProc ( 
		OSAsyncReference 			asyncRef,
		UserObjectHandle 			portHandle )
{
//	DebugLog("asyncRef\n") ;
//	for( unsigned index=0; index < 8 ; ++index )
//	{
//		DebugLog("asyncRef[ %d ]=%x\n", index, asyncRef[ index ] ) ;
//	}
	
	IOFWUserLocalIsochPort * port = OSDynamicCast( IOFWUserLocalIsochPort, fExporter->lookupObject( portHandle ) ) ;
	if ( ! port )
		return kIOReturnBadArgument ;
	
	IOReturn error = port->setAsyncRef_DCLCallProc ( asyncRef ) ;
	
	port->release() ;	// loopkupObject retains the return value for thread safety
	
	return error ;
}

#pragma mark -
#pragma mark ISOCH CHANNEL
IOReturn
IOFireWireUserClient::s_IsochChannel_ForceStopHandler(
	void*					refCon,
	IOFWIsochChannel*		isochChannelID,
	UInt32					stopCondition)
{
	if ( !refCon )
	{
		return kIOReturnSuccess ;
	}
	
	return IOFireWireUserClient::sendAsyncResult( (natural_t*)refCon, kIOReturnSuccess, (void **) & stopCondition, 1 ) ;
}

IOReturn
IOFireWireUserClient :: isochChannel_Create (
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
	IOFWUserIsochChannel * channel = new IOFWUserIsochChannel ;
	if ( channel )
	{
		if ( channel->init(	getOwner()->getController(), inDoIRM, 
										inPacketSize, inPrefSpeed, 
										s_IsochChannel_ForceStopHandler, 
										NULL ) )
		{
			fExporter->addObject( *channel, 
					(IOFWUserObjectExporter::CleanupFunction) & IOFWUserIsochChannel::s_exporterCleanup, 
					*outChannelHandle ) ;
		}
		
		DebugLog( "made new channel %p, handle=%p, retain count now %d\n", channel, *outChannelHandle, channel->getRetainCount()) ;

		channel->release() ;	// addObject retains the object
	}
	else
	{
		DebugLog( "couldn't make newChannel\n") ;
		error = kIOReturnNoMemory ;
	}	

	return error ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_IsochChannelForceStop(
	OSAsyncReference		inAsyncRef,
	UserObjectHandle		channelRef )
{
	IOReturn result = kIOReturnSuccess;
	
	IOFWUserIsochChannel * channel = OSDynamicCast( IOFWUserIsochChannel, fExporter->lookupObject( channelRef ) ) ;
	if ( ! channel )
	{
		result = kIOReturnBadArgument ;
	}
	
	natural_t * asyncRef = NULL;
	if( result == kIOReturnSuccess )
	{
		asyncRef = new (natural_t)[ kOSAsyncRefCount ] ;
	
		if ( !asyncRef )
		{
			result = kIOReturnNoMemory ;
		}
	}
	
	if( result == kIOReturnSuccess )
	{
		bcopy( inAsyncRef, asyncRef, sizeof( OSAsyncReference ) ) ;
		
		if ( channel->getAsyncRef() )
		{
			delete [] (natural_t*)channel->getAsyncRef() ;
		}
		
		channel->setAsyncRef( asyncRef ) ;
	}
	
	if( channel != NULL )
	{
		channel->release();
	}
	
	return result;
}

#pragma mark -
#pragma mark COMMAND OBJECTS

IOReturn
IOFireWireUserClient :: userAsyncCommand_Submit(
	OSAsyncReference			asyncRef,
	CommandSubmitParams *		params,
	CommandSubmitResult *		outResult,
	IOByteCount					paramsSize,
	IOByteCount *				outResultSize)
{
	IOFWUserCommand * cmd = NULL ;
	
	IOReturn error = kIOReturnSuccess ;
	
	if ( params->kernCommandRef )
	{
		DebugLog("using existing command object\n") ;

		cmd = OSDynamicCast( IOFWUserCommand, fExporter->lookupObject( params->kernCommandRef ) ) ;
		if ( ! cmd )
			error = kIOReturnBadArgument ;
	}
	else
	{
		DebugLog("lazy allocating command object\n") ;
	
		cmd = IOFWUserCommand :: withSubmitParams( params, this ) ;

		if ( ! cmd )
			error = kIOReturnNoMemory ;
		else
			error = fExporter->addObject( *cmd, NULL, outResult->kernCommandRef ) ;
	}

	if ( cmd )
	{
		if ( !error )
		{
			super :: setAsyncReference( asyncRef, (mach_port_t) asyncRef[0], (void*)params->callback, (void*)params->refCon) ;
	
			cmd->setAsyncReference( asyncRef ) ;
			
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
IOFireWireUserClient :: createReadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback	 		completion,
	void*					refcon ) const
{
	IOFWReadCommand* result = new IOFWReadCommand ;
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		result = NULL ;
	}
	
	return result ;
}

IOFWReadQuadCommand*
IOFireWireUserClient :: createReadQuadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWReadQuadCommand* result = new IOFWReadQuadCommand ;
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteCommand*
IOFireWireUserClient :: createWriteCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback 		completion,
	void*					refcon ) const
{
	IOFWWriteCommand* result = new IOFWWriteCommand ;
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteQuadCommand*
IOFireWireUserClient :: createWriteQuadCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWWriteQuadCommand* result = new IOFWWriteQuadCommand ;
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

	// size is 1 for 32 bit compare, 2 for 64 bit.
IOFWCompareAndSwapCommand*
IOFireWireUserClient :: createCompareAndSwapCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress,
	const UInt32 *			cmpVal, 
	const UInt32 *			newVal, 
	int 					size,
	FWDeviceCallback 		completion, 
	void *					refcon ) const
{
	IOFWCompareAndSwapCommand* result = new IOFWCompareAndSwapCommand ;
	if ( result && !result->initAll( getOwner ()->getController(), generation, devAddress, cmpVal, newVal, size, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

#pragma mark -

IOReturn
IOFireWireUserClient :: firelog( const char* string, IOByteCount bufSize ) const
{

#if FIRELOG
	FireLog( string ) ;
#endif

	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient :: getBusGeneration( UInt32* outGeneration )
{
	*outGeneration = getOwner () -> getController () -> getGeneration () ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient :: getLocalNodeIDWithGeneration( UInt32 testGeneration, UInt32* outLocalNodeID )
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
IOFireWireUserClient :: getRemoteNodeID( UInt32 testGeneration, UInt32* outRemoteNodeID )
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
IOFireWireUserClient :: getSpeedToNode( UInt32 generation, UInt32* outSpeed )
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
IOFireWireUserClient :: getSpeedBetweenNodes( UInt32 generation, UInt32 fromNode, UInt32 toNode, UInt32* outSpeed )
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
IOFireWireUserClient :: getIRMNodeID( UInt32 generation, UInt32* irmNodeID )
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
IOFireWireUserClient :: clipMaxRec2K( Boolean clipMaxRec )
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
IOFireWireUserClient :: seize( IOOptionBits inFlags )
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
			DebugLog("IOFireWireUserClient :: seize: couldn't make owner client iterator\n") ;
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
			DebugLog("IOFireWireUserClient :: seize: couldn't make owner client iterator\n") ;
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

IOReturn IOFireWireUserClient :: message( UInt32 type, IOService * provider, void * arg )
{
	switch(type)
	{
		case kIOMessageServiceIsSuspended:
			if (fBusResetAsyncNotificationRef[0])
				sendAsyncResult(fBusResetAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOMessageServiceIsResumed:
			if (fBusResetDoneAsyncNotificationRef[0])
				sendAsyncResult(fBusResetDoneAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOFWMessageServiceIsRequestingClose:
			getOwner ()->messageClients(kIOMessageServiceIsRequestingClose) ;
			break;
		
	}
	
	super :: message ( type, provider ) ;
	
	return kIOReturnSuccess;
}

void
IOFireWireUserClient :: s_userBufferFillPacketProc( 
	IOFWBufferFillIsochPort *   port,
	IOVirtualRange				packets[],
	unsigned					packetCount )
{
}
