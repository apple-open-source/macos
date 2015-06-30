/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibIsochPort.cpp
 *  IOFireWireFamily
 *
 *  Created on Mon Mar 12 2001.
 *  Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.54  2009/01/15 01:40:02  collin
 * <rdar://problem/6400981> BRB-VERIFY: 10A222: Trying to record a movie through QT X, getting error message.
 *
 * Revision 1.53  2008/05/06 00:14:48  collin
 * more k64 changes
 *
 * Revision 1.52  2007/03/14 01:01:14  collin
 * *** empty log message ***
 *
 * Revision 1.51  2007/02/15 19:42:08  ayanowit
 * For 4369537, eliminated support for legacy DCL SendPacketWithHeader, since it didn't work anyway, and NuDCL does support it.
 *
 * Revision 1.50  2007/02/15 17:18:54  ayanowit
 * Fixed a panic found with ProIO DCL program in FCP. Related to recent changes for 64-bit app support.
 *
 * Revision 1.49  2007/02/07 06:35:22  collin
 * *** empty log message ***
 *
 * Revision 1.48  2007/01/26 20:52:31  ayanowit
 * changes to user-space isoch stuff to support 64-bit apps.
 *
 * Revision 1.47  2007/01/11 04:28:05  collin
 * *** empty log message ***
 *
 * Revision 1.46  2007/01/08 18:47:20  ayanowit
 * More 64-bit changes for isoch.
 *
 * Revision 1.45  2007/01/02 18:14:12  ayanowit
 * Enabled building the plug-in lib 4-way FAT. Also, fixed compile problems for 64-bit.
 *
 * Revision 1.44  2006/12/21 21:17:46  ayanowit
 * More changes necessary to eventually get support for 64-bit apps working (4222965).
 *
 * Revision 1.43  2006/12/16 00:07:48  ayanowit
 * fixed some of the leopard user-lib changes. was failing on ppc systems.
 *
 * Revision 1.42  2006/12/13 21:34:24  ayanowit
 * For 4222965, replaced all io async method calls with new Leopard API version.
 *
 * Revision 1.41  2006/12/13 01:11:23  ayanowit
 * For 4222969, replaced the remaining calls to IOConnectMethod... struct variants.
 *
 * Revision 1.40  2006/12/12 22:39:05  ayanowit
 * For radar 4222965, changed all scalar in, scalar out calls to IOConnectMethod... to use new Leopard IOConnectCall... API.
 *
 * Revision 1.39  2006/08/21 22:41:11  collin
 * *** empty log message ***
 *
 * Revision 1.38  2006/02/09 00:21:55  niels
 * merge chardonnay branch to tot
 *
 * Revision 1.37  2005/04/02 02:43:46  niels
 * exporter works outside IOFireWireFamily
 *
 * Revision 1.36.4.6  2006/01/31 04:49:57  collin
 * *** empty log message ***
 *
 * Revision 1.36.4.4  2006/01/17 00:35:00  niels
 * <rdar://problem/4399365> FireWire NuDCL APIs need Rosetta support
 *
 * Revision 1.36.4.3  2006/01/04 00:45:54  collin
 * *** empty log message ***
 *
 * Revision 1.36.4.2  2005/08/06 01:31:31  collin
 * *** empty log message ***
 *
 * Revision 1.36.4.1  2005/07/23 00:30:46  collin
 * *** empty log message ***
 *
 * Revision 1.36  2005/03/12 03:27:52  collin
 * *** empty log message ***
 *
 * Revision 1.35  2005/02/18 03:19:05  niels
 * fix isight
 *
 * Revision 1.34  2004/05/04 22:52:20  niels
 * *** empty log message ***
 *
 * Revision 1.33  2004/02/17 23:13:23  niels
 * *** empty log message ***
 *
 * Revision 1.32  2004/02/17 23:12:27  niels
 * *** empty log message ***
 *
 * Revision 1.31  2004/02/17 20:23:58  niels
 * keep track if local isoch port has started or not to avoid leaking when stop is called on an unstarted port
 *
 * Revision 1.30  2003/12/19 22:07:46  niels
 * send force stop when channel dies/system sleeps
 *
 * Revision 1.29  2003/08/25 08:39:17  niels
 * *** empty log message ***
 *
 * Revision 1.28  2003/08/20 23:33:37  niels
 * *** empty log message ***
 *
 * Revision 1.27  2003/08/20 18:48:45  niels
 * *** empty log message ***
 *
 * Revision 1.26  2003/08/18 23:18:15  niels
 * *** empty log message ***
 *
 * Revision 1.25  2003/08/14 17:47:33  niels
 * *** empty log message ***
 *
 * Revision 1.24  2003/07/29 22:49:25  niels
 * *** empty log message ***
 *
 * Revision 1.23  2003/07/24 20:49:50  collin
 * *** empty log message ***
 *
 * Revision 1.22  2003/07/24 06:30:59  collin
 * *** empty log message ***
 *
 * Revision 1.21  2003/07/21 06:53:10  niels
 * merge isoch to TOT
 *
 * Revision 1.20.6.6  2003/07/21 06:44:48  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.5  2003/07/18 00:17:47  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.4  2003/07/10 00:11:58  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.3  2003/07/09 21:24:07  niels
 * *** empty log message ***
 *
 * Revision 1.20.6.2  2003/07/03 22:10:26  niels
 * fix iidc/dv rcv
 *
 * Revision 1.20.6.1  2003/07/01 20:54:24  niels
 * isoch merge
 *
 */

#import "IOFireWireLibIsochPort.h"
#import "IOFireWireLibDevice.h"
#import "IOFireWireLibNuDCLPool.h"
#import "IOFireWireLibNuDCL.h"
#import "IOFireWireLibCoalesceTree.h"

#import <IOKit/iokitmig.h>
#import <mach/mach.h>
#import <System/libkern/OSCrossEndian.h>

#define IOFIREWIREISOCHPORTIMP_INTERFACE	\
	& IsochPortCOM::SGetSupported,	\
	& IsochPortCOM::SAllocatePort,	\
	& IsochPortCOM::SReleasePort,	\
	& IsochPortCOM::SStart,	\
	& IsochPortCOM::SStop,	\
	& IsochPortCOM::SSetRefCon,	\
	& IsochPortCOM::SGetRefCon

namespace IOFireWireLib {
	RemoteIsochPort::Interface	RemoteIsochPortCOM::sInterface =
	{
		INTERFACEIMP_INTERFACE,
		1, 0,
		
		IOFIREWIREISOCHPORTIMP_INTERFACE,
		& RemoteIsochPortCOM::SSetGetSupportedHandler,
		& RemoteIsochPortCOM::SSetAllocatePortHandler,
		& RemoteIsochPortCOM::SSetReleasePortHandler,
		& RemoteIsochPortCOM::SSetStartHandler,
		& RemoteIsochPortCOM::SSetStopHandler,
	} ;
	
	// ============================================================
	// utility functions
	// ============================================================
	
	Boolean
	GetDCLDataBuffer(
		DCLCommand*	dcl,
		IOVirtualAddress*	outDataBuffer,
		IOByteCount*		outDataLength)
	{
		Boolean	result = false ;
	
		switch(dcl->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			//case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				*outDataBuffer		= (IOVirtualAddress)((DCLTransferPacket*)dcl)->buffer ;
				*outDataLength		= ((DCLTransferPacket*)dcl)->size ;
				result = true ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				//zzz what should I do here?
				break ;
	
			case kDCLPtrTimeStampOp:
				*outDataBuffer		= (IOVirtualAddress)((DCLPtrTimeStamp*)dcl)->timeStampPtr ;
				*outDataLength		= sizeof( *( ((DCLPtrTimeStamp*)dcl)->timeStampPtr) ) ;
				result = true ;
				break ;
			
			default:
				break ;
		}
		
		return result ;
	}
	
	IOByteCount
	GetDCLSize(
		DCLCommand*	dcl)
	{
		IOByteCount result = 0 ;
	
		switch(dcl->opcode & ~kFWDCLOpFlagMask)
		{
			case kDCLSendPacketStartOp:
			//case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
				result = sizeof(UserExportDCLTransferPacket) ;
				break ;
				
			case kDCLSendBufferOp:
			case kDCLReceiveBufferOp:
				result = sizeof(UserExportDCLTransferBuffer) ;
				break ;
	
			case kDCLCallProcOp:
				result = sizeof(UserExportDCLCallProc) ;
				break ;
				
			case kDCLLabelOp:
				result = sizeof(UserExportDCLLabel) ;
				break ;
				
			case kDCLJumpOp:
				result = sizeof(UserExportDCLJump) ;
				break ;
				
			case kDCLSetTagSyncBitsOp:
				result = sizeof(UserExportDCLSetTagSyncBits) ;
				break ;
				
			case kDCLUpdateDCLListOp:
				result = sizeof(UserExportDCLUpdateDCLList) ;
				break ;
	
			case kDCLPtrTimeStampOp:
				result = sizeof(UserExportDCLPtrTimeStamp) ;
				break;

			case kDCLSkipCycleOp:
				result = sizeof(UserExportDCLCommand) ;
				break;
		}
		
		return result ;
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPort
	//
	// ============================================================
	
	IsochPort::IsochPort( const IUnknownVTbl & interface, Device & device, bool talking, bool allocateKernPort )
	: IOFireWireIUnknown( interface ),
	  mDevice( device ),
	  mKernPortRef( 0 ),
	  mTalking( talking )
	{
		mDevice.AddRef() ;
	}
	
	IsochPort::~IsochPort()
	{
		if ( mKernPortRef )
		{
			IOReturn error = kIOReturnSuccess;
			
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernPortRef};
			error = IOConnectCallScalarMethod(mDevice.GetUserClientConnection(), 
											  kReleaseUserObject,
											  inputs,1,NULL,&outputCnt);

			DebugLogCond( error, "Couldn't release kernel port" ) ;
		}
		
		mDevice.Release() ;
	}
	
	IOReturn
	IsochPort::GetSupported(
		IOFWSpeed&					maxSpeed, 
		UInt64& 					chanSupported )
	{
		uint32_t outputCnt = 3;
		uint64_t outputVal[3];
		const uint64_t inputs[1]={(const uint64_t)mKernPortRef};
		IOReturn result = IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
													kIsochPort_GetSupported,
													inputs,1,
													outputVal,&outputCnt);
		maxSpeed = (IOFWSpeed)(outputVal[0] & 0xFFFFFFFF);
		chanSupported = ((outputVal[1] & 0xFFFFFFFF)<<32)+(outputVal[2] & 0xFFFFFFFF);
		return result;
	}
	
	IOReturn
	IsochPort::AllocatePort( IOFWSpeed speed, UInt32 chan )
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[2] = {speed, chan};
		return IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
										 mDevice.MakeSelectorWithObject( kIsochPort_AllocatePort_d, mKernPortRef ),
										 inputs,2,
										 NULL,&outputCnt);
	}
	
	IOReturn
	IsochPort::ReleasePort()
	{
		uint32_t outputCnt = 0;
		return IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
										 mDevice.MakeSelectorWithObject( kIsochPort_ReleasePort_d, mKernPortRef ), 
										 NULL,0,
										 NULL,&outputCnt);
	}
	
	IOReturn
	IsochPort::Start()
	{
		uint32_t outputCnt = 0;
		return IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
										 mDevice.MakeSelectorWithObject( kIsochPort_Start_d, mKernPortRef ),
										 NULL,0,
										 NULL,&outputCnt);
	}
	
	IOReturn
	IsochPort::Stop()
	{
		uint32_t outputCnt = 0;
		return IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
										 mDevice.MakeSelectorWithObject( kIsochPort_Stop_d, mKernPortRef ), 
										 NULL,0,
										 NULL,&outputCnt);
	}
	
#pragma mark -
	// ============================================================
	//
	// IsochPortCOM
	//
	// ============================================================
	
	IsochPortCOM::IsochPortCOM( const IUnknownVTbl & interface, Device& userclient, bool talking, bool allocateKernPort )
	:	IsochPort( interface, userclient, talking, allocateKernPort )
	{
	}
	
	IsochPortCOM::~IsochPortCOM()
	{
	}
	
	IOReturn
	IsochPortCOM::SGetSupported(
		IOFireWireLibIsochPortRef	self, 
		IOFWSpeed* 					maxSpeed, 
		UInt64* 					chanSupported )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetSupported(*maxSpeed, *chanSupported) ;
	}
	
	IOReturn
	IsochPortCOM::SAllocatePort(
		IOFireWireLibIsochPortRef	self, 
		IOFWSpeed 					speed, 
		UInt32 						chan )
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->AllocatePort(speed, chan) ;
	}
	
	IOReturn
	IsochPortCOM::SReleasePort(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->ReleasePort() ;
	}
	
	IOReturn
	IsochPortCOM::SStart(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->Start() ;
	}
	
	IOReturn
	IsochPortCOM::SStop(
		IOFireWireLibIsochPortRef	self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->Stop() ;
	}
	
	void
	IsochPortCOM::SSetRefCon(
		IOFireWireLibIsochPortRef		self,
		void*				inRefCon)
	{
		IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->SetRefCon(inRefCon) ;
	}
	
	void*
	IsochPortCOM::SGetRefCon(
		IOFireWireLibIsochPortRef		self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetRefCon() ;
	}
	
	Boolean
	IsochPortCOM::SGetTalking(
		IOFireWireLibIsochPortRef		self)
	{
		return IOFireWireIUnknown::InterfaceMap<IsochPortCOM>::GetThis(self)->GetTalking() ;
	}

#pragma mark -
	// ============================================================
	//
	// RemoteIsochPort
	//
	// ============================================================
	
	RemoteIsochPort::RemoteIsochPort( const IUnknownVTbl & interface, Device& userclient, bool talking )
	:	IsochPortCOM( interface, userclient, talking ),
		mGetSupportedHandler(0),
		mAllocatePortHandler(0),
		mReleasePortHandler(0),
		mStartHandler(0),
		mStopHandler(0),
		mRefInterface( reinterpret_cast<IOFireWireIsochPortInterface**>(& GetInterface()) )
	{
	}
	
	IOReturn
	RemoteIsochPort::GetSupported(
		IOFWSpeed&					maxSpeed,
		UInt64&						chanSupported)
	{
		if (mGetSupportedHandler)
			return (*mGetSupportedHandler)(mRefInterface, & maxSpeed, & chanSupported) ;
		else
			return kIOReturnUnsupported ;	// should we return unsupported if user proc doesn't answer?
	}
	
	IOReturn
	RemoteIsochPort::AllocatePort(
		IOFWSpeed 					speed, 
		UInt32 						chan )
	{
		if (mAllocatePortHandler)
			return (*mAllocatePortHandler)(mRefInterface, speed,  chan) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::ReleasePort()
	{
		if (mReleasePortHandler)
			return (*mReleasePortHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::Start()
	{
		if (mStartHandler)
			return (*mStartHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	IOReturn
	RemoteIsochPort::Stop()
	{
		if (mStopHandler)
			return (*mStopHandler)(mRefInterface) ;
		else
			return kIOReturnSuccess ;
	}
	
	
	IOFireWireLibIsochPortGetSupportedCallback
	RemoteIsochPort::SetGetSupportedHandler(
		IOFireWireLibIsochPortGetSupportedCallback	inHandler)
	{
		IOFireWireLibIsochPortGetSupportedCallback oldHandler = mGetSupportedHandler ;
		mGetSupportedHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortAllocateCallback
	RemoteIsochPort::SetAllocatePortHandler(
		IOFireWireLibIsochPortAllocateCallback	inHandler)
	{
		IOFireWireLibIsochPortAllocateCallback	oldHandler	= mAllocatePortHandler ;
		mAllocatePortHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetReleasePortHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mReleasePortHandler ;
		mReleasePortHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetStartHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mStartHandler ;
		mStartHandler = inHandler ;
		
		return oldHandler ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPort::SetStopHandler(
		IOFireWireLibIsochPortCallback	inHandler)
	{
		IOFireWireLibIsochPortCallback	oldHandler	= mStopHandler ;
		mStopHandler = inHandler ;
		
		return oldHandler ;
	}
								
#pragma mark -
	// ============================================================
	//
	// RemoteIsochPortCOM
	//
	// ============================================================
	RemoteIsochPortCOM::RemoteIsochPortCOM( Device& userclient, bool talking )
	: RemoteIsochPort( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, talking )
	{
	}
								
	RemoteIsochPortCOM::~RemoteIsochPortCOM()
	{
	}
	
	IUnknownVTbl**
	RemoteIsochPortCOM::Alloc( Device& userclient, bool talking )
	{
		RemoteIsochPortCOM*	me = nil ;

		try {
			me = new RemoteIsochPortCOM( userclient, talking ) ;
		} catch(...) {
		}
			
		return (nil==me) ? nil : reinterpret_cast<IUnknownVTbl**>(& me->GetInterface()) ;
	}
	
	
	HRESULT
	RemoteIsochPortCOM::QueryInterface(REFIID iid, void ** ppv )
	{
		HRESULT		result = S_OK ;
		*ppv = nil ;
	
		CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) ||  CFEqual(interfaceID, kIOFireWireRemoteIsochPortInterfaceID) )
		{
			*ppv = & GetInterface() ;
			AddRef() ;
		}
		else
		{
			*ppv = nil ;
			result = E_NOINTERFACE ;
		}	
		
		CFRelease(interfaceID) ;
		return result ;
	}
	
	IOFireWireLibIsochPortGetSupportedCallback
	RemoteIsochPortCOM::SSetGetSupportedHandler(
		PortRef				self,
		IOFireWireLibIsochPortGetSupportedCallback	inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetGetSupportedHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortAllocateCallback
	RemoteIsochPortCOM::SSetAllocatePortHandler(
		PortRef		self,
		IOFireWireLibIsochPortAllocateCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetAllocatePortHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetReleasePortHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetReleasePortHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetStartHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetStartHandler(inHandler) ;
	}
	
	IOFireWireLibIsochPortCallback
	RemoteIsochPortCOM::SSetStopHandler(
		PortRef		self,
		IOFireWireLibIsochPortCallback		inHandler)
	{
		return IOFireWireIUnknown::InterfaceMap<RemoteIsochPortCOM>::GetThis(self)->SetStopHandler(inHandler) ;
	}
								
#pragma mark -
	LocalIsochPortCOM::Interface	LocalIsochPortCOM::sInterface = 
	{
		INTERFACEIMP_INTERFACE
		,4,0
		
		,IOFIREWIREISOCHPORTIMP_INTERFACE
		,& LocalIsochPortCOM::SModifyJumpDCL
		,& LocalIsochPortCOM::SPrintDCLProgram
		,& LocalIsochPortCOM::SModifyTransferPacketDCLSize
		,& LocalIsochPortCOM::SModifyTransferPacketDCLBuffer
		,& LocalIsochPortCOM::SModifyTransferPacketDCL
		,& LocalIsochPortCOM::S_SetFinalizeCallback
		, & LocalIsochPortCOM::S_SetResourceUsageFlags
		, & LocalIsochPortCOM::S_Notify
	} ;

	LocalIsochPort::LocalIsochPort( const IUnknownVTbl & interface, Device & userclient, bool talking,
			DCLCommand* program, UInt32 startEvent, UInt32 startState, UInt32 startMask,
			IOVirtualRange userProgramRanges[], UInt32 userProgramRangeCount, 
			IOVirtualRange userBufferRanges[], UInt32 userBufferRangeCount, IOFWIsochPortOptions options )
	: IsochPortCOM( interface, userclient, talking, false )
	, mDCLProgram( program )
	, mExpectedStopTokens(0)
	, mDeferredReleaseCount(0)
	, mFinalizeCallback(nil)
	, mBufferRanges( nil )
	, mBufferAddressRanges( nil )
	, mStarted( false )
	{
		// sorry about the spaghetti.. hope you're hungry:
		
		if ( !program )
		{
			DebugLog( "no DCL program!\n" ) ;
			throw kIOReturnBadArgument ;
		}

//		DeviceCOM::SPrintDCLProgram ( nil, program, 0 ) ;

		IOReturn	error = kIOReturnSuccess ;

		// trees used to coalesce program/data ranges
		CoalesceTree		bufferTree ;

		// check if user passed in any virtual memory ranges to start with...
		if ( userBufferRanges )
		{
			for( unsigned index=0; index < userBufferRangeCount; ++index )
				bufferTree.CoalesceRange( userBufferRanges[index]) ;
		}		

		IOByteCount			programExportBytes = 0 ;
		IOVirtualAddress	programData = 0 ;

		LocalIsochPortAllocateParams params ;
		{
			params.programExportBytes = 0 ;
			params.programData = 0 ;
		}

		if ( program->opcode == kDCLNuDCLLeaderOp )
		{
			if( !error )
			{
				NuDCLPool*	pool	= reinterpret_cast<NuDCLPool*>( reinterpret_cast< DCLNuDCLLeader* >( program )->program ) ;

				params.version 				= kDCLExportDataNuDCLRosettaVersion ;		// new-style DCL program

				pool->CoalesceBuffers( bufferTree ) ;
				
				mBufferRangeCount = bufferTree.GetCount() ;
				mBufferRanges = new IOVirtualRange[ mBufferRangeCount ] ;
				if ( !mBufferRanges )
				{
					error = kIOReturnNoMemory ;
				}
				else
				{
					bufferTree.GetCoalesceList( mBufferRanges ) ;
				
					mBufferAddressRanges = new FWVirtualAddressRange[ mBufferRangeCount ] ;
					if ( !mBufferAddressRanges )
					{
						error = kIOReturnNoMemory ;
					}
					else
						for (unsigned int i=0;i<mBufferRangeCount;i++)
						{
							mBufferAddressRanges[i].address = mBufferRanges[i].address;
							mBufferAddressRanges[i].length = mBufferRanges[i].length;
						}
					
					programExportBytes 	= pool->Export( &programData, mBufferRanges, mBufferRangeCount ) ;				
					params.programExportBytes = programExportBytes ;
					params.programData = programData;
				}
			}
		}
		else
		{	
			unsigned programCount = 0 ;
			
			params.version = kDCLExportDataLegacyVersion ;					// old-style DCL program
			
			// count DCLs in program and coalesce buffers:
			for( DCLCommand * dcl = mDCLProgram; dcl != nil; dcl = dcl->pNextDCLCommand )
			{
				IOVirtualRange tempRange ;
				if ( GetDCLDataBuffer ( dcl, & tempRange.address, & tempRange.length ) )
				{
					bufferTree.CoalesceRange ( tempRange ) ;
				}

				++programCount ;
			}
			
			InfoLog("program count is %d\n", programCount) ;
			
			if ( !error )
			{
				error = ExportDCLs( &programData, &programExportBytes ) ;
				params.programData = programData;
				params.programExportBytes = programExportBytes;
			}

			if ( !error )
			{
				mBufferRangeCount = bufferTree.GetCount() ;
				mBufferRanges = new IOVirtualRange[ mBufferRangeCount ] ;
	
				if ( !mBufferRanges )
				{
					error = kIOReturnNoMemory ;
				}
				else
				{
					bufferTree.GetCoalesceList( mBufferRanges ) ;
					
					mBufferAddressRanges = new FWVirtualAddressRange[ mBufferRangeCount ] ;
					if ( !mBufferAddressRanges )
					{
						error = kIOReturnNoMemory ;
					}
					else
						for (unsigned int i=0;i<mBufferRangeCount;i++)
						{
							mBufferAddressRanges[i].address = mBufferRanges[i].address;
							mBufferAddressRanges[i].length = mBufferRanges[i].length;
						}
				}
			}
		}
		
		if ( error )
		{
			throw error ;
		}
	
//		// allocate lists to store buffer ranges
//		// and get coalesced buffer lists
//
//		UInt32			bufferRangeCount					= bufferTree.GetCount() ;
//		IOVirtualRange	bufferRanges[ bufferRangeCount ] ;
//
//		bufferTree.GetCoalesceList ( bufferRanges ) ;
	
		// fill out param struct and submit to kernel
		params.bufferRanges					= (mach_vm_address_t)mBufferAddressRanges ;
		params.bufferRangeCount				= mBufferRangeCount ;
		params.talking						= mTalking ;	
		params.startEvent					= startEvent ;
		params.startState					= startState ;
		params.startMask					= startMask ;
		params.userObj						= (mach_vm_address_t) this ;
		params.options						= options ;

#if 0
		params.options |= kFWIsochEnableRobustness;
#endif
		
		InfoLog("startEvent=%x, startState=%x, startMask=%x\n", params.startEvent, params.startState, params.startMask) ;

#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				params.version = OSSwapInt32( params.version );
				params.talking = params.talking; // byte
				params.startEvent = OSSwapInt32( params.startEvent );
				params.startState = OSSwapInt32( params.startState );
				params.startMask = OSSwapInt32( params.startMask );
				params.programExportBytes = OSSwapInt32( params.programExportBytes );
				params.programData = OSSwapInt64( params.programData );
				
				for( UInt32 i = 0; i < params.bufferRangeCount; i++ )
				{
					IOAddressRange *pAddressRange = (IOAddressRange*) (params.bufferRanges+(i*sizeof(IOAddressRange)));
					
					pAddressRange->address = OSSwapInt64( pAddressRange->address );
					pAddressRange->length = OSSwapInt64( pAddressRange->length );
					
				}
				
				params.bufferRangeCount = OSSwapInt32( params.bufferRangeCount );
				params.bufferRanges = OSSwapInt64(params.bufferRanges );
				params.options = (IOFWIsochPortOptions)OSSwapInt32( params.options | kFWIsochBigEndianUpdates );
				params.userObj = (mach_vm_address_t)OSSwapInt64((mach_vm_address_t)params.userObj );
			}
		);
#endif
		
		uint32_t outputCnt = 0;
		size_t outputStructSize = sizeof( UserObjectHandle ) ;
		error =  IOConnectCallMethod(mDevice.GetUserClientConnection(),
									 kLocalIsochPort_Allocate,
									 NULL,0,
									 & params,sizeof (params),
									 NULL,&outputCnt,
									 &mKernPortRef,&outputStructSize);
#ifndef __LP64__		
		ROSETTA_ONLY(
			{
				mKernPortRef = (UserObjectHandle)OSSwapInt32( (UInt32)mKernPortRef );
			}
		);
#endif
		
		if (error)
		{
			DebugLog ( "Couldn't create local isoch port (error=%x)\nCheck your buffers!\n", error ) ;
			DebugLog ( "Found buffers:\n" ) ;

#if IOFIREWIRELIBDEBUG
			for( unsigned index=0; index < mBufferRangeCount; ++index )
			{
#ifdef __LP64__
				DebugLog (	"\%u: <0x%x>-<0x%x>\n", index, (unsigned)mBufferRanges[index].address, 
							(unsigned)mBufferRanges[index].address + mBufferRanges[index].length ) ;
#else
				DebugLog (	"\%u: <0x%x>-<0x%lx>\n", index, (unsigned)mBufferRanges[index].address, 
							(unsigned)mBufferRanges[index].address + mBufferRanges[index].length ) ;
#endif
			}
#endif

			throw error ;
		}
		
		{
			uint64_t refrncData[kOSAsyncRef64Count];
			refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t) & LocalIsochPort::s_DCLStopTokenCallProcHandler;
			refrncData[kIOAsyncCalloutRefconIndex] = (unsigned long)this;
			uint32_t outputCnt = 0;
			const uint64_t inputs[1]={(const uint64_t)mKernPortRef};

			error = IOConnectCallAsyncScalarMethod(mDevice.GetUserClientConnection(),
												   kSetAsyncRef_DCLCallProc,
												   mDevice.GetIsochAsyncPort(), 
												   refrncData,kOSAsyncRef64Count,
												   inputs,1,
												   NULL,&outputCnt);

			if( error )
			{
				throw error ;
			}
		}
		
		if ( params.programData )
		{
			vm_deallocate( mach_task_self (), (vm_address_t) programData, programExportBytes ) ;		// this is temporary storage
		}
	
		// make our mutex
		pthread_mutex_init ( & mMutex, nil ) ;	
	}
	
	LocalIsochPort::~LocalIsochPort ()
	{
		delete[] mBufferRanges ;
		delete[] mBufferAddressRanges;
		
		pthread_mutex_destroy( & mMutex ) ;
	}
	
	ULONG
	LocalIsochPort::Release ()
	{
		Lock () ;
		
		if ( mExpectedStopTokens > 0 )
		{
			Unlock () ;
			++ mDeferredReleaseCount ;
			return mRefCount ;
		}
	
		Unlock () ;

//		while( true )
//		{
//			Lock() ;
//			bool run = ( mExpectedStopTokens > 0 ) ;
//			Unlock() ;
//			
//			if ( !run )
//			{
//				break ;
//			}
//			
//			::CFRunLoopRunInMode( kCFRunLoopDefaultMode, 1, true ) ;
//		}

		return IsochPortCOM::Release() ;
	}

	IOReturn
	LocalIsochPort::Start()
	{
		IOReturn error = IsochPort::Start() ;
		if ( !error )
		{
			Lock() ;
			mStarted = true ;
			Unlock() ;
		}
		
		return error ;
	}
	
	IOReturn
	LocalIsochPort::Stop ()
	{
		Lock() ;
		if ( mStarted )
		{
			mStarted = false ;
			++mExpectedStopTokens ;
			InfoLog("waiting for %lu stop tokens\n", mExpectedStopTokens) ;
		}
		Unlock() ;
						
		return IsochPortCOM::Stop() ;	// call superclass Stop()
	}
	
	IOReturn
	LocalIsochPort::ModifyJumpDCL ( DCLJump* inJump, DCLLabel* inLabel )
	{		
		inJump->pJumpDCLLabel = inLabel ;
	
		uint32_t outputCnt = 0;
		const uint64_t inputs[2] = {inJump->compilerData, inLabel->compilerData};
		IOReturn result =  IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
													 mDevice.MakeSelectorWithObject( kLocalIsochPort_ModifyJumpDCL_d, mKernPortRef ), 
													 inputs,2,
													 NULL,&outputCnt);
		return result ;
	}
	
	IOReturn
	LocalIsochPort::ModifyTransferPacketDCLSize ( DCLTransferPacket* dcl, IOByteCount newSize )
	{
		//kLocalIsochPort_ModifyTransferPacketDCLSize, 
		return kIOReturnUnsupported ;
	}

	void
	LocalIsochPort::s_DCLStopTokenCallProcHandler ( void * self, IOReturn e )
	{
		((LocalIsochPort*)self)->DCLStopTokenCallProcHandler(e) ;
	}

	void
	LocalIsochPort::DCLStopTokenCallProcHandler( IOReturn )
	{
		if ( mExpectedStopTokens > 0 )
		{
			Lock() ;
			mExpectedStopTokens-- ;
			Unlock() ;

			if ( mExpectedStopTokens == 0 )
			{
				if ( mFinalizeCallback )
					(*mFinalizeCallback)(mRefCon) ;

				// use a local so we don't touch "this" after release
				UInt32 release_count = mDeferredReleaseCount;
				while ( release_count > 0 )
				{
					--release_count ;
					Release() ;
				}
			}
		}		
	}
#if 0	
	void
	LocalIsochPort::S_DCLKernelCallout( DCLCallProc * dcl )
	{
		(*dcl->proc)(dcl->procData) ;
		
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)dcl->compilerData};

		IOReturn result =  IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
													 Device::MakeSelectorWithObject( kLocalIsochPort_RunDCLUpdateList_d, port->mKernPortRef ), 
													 inputs,1,
													 NULL,&outputCnt);
	}

	void
	LocalIsochPort::S_NuDCLKernelCallout ( NuDCL * dcl )
	{
		(*dcl->fData.callback)(dcl->fData.refcon, (NuDCLRef)dcl) ;
		
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)dcl->fExportIndex};

		IOReturn result =  IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
													 Device::MakeSelectorWithObject( kLocalIsochPort_RunNuDCLUpdateList_d, mKernPortRef ), 
													 inputs,1,
													 NULL,&outputCnt);
	}
#endif	
	
	IOReturn
	LocalIsochPort::SetResourceUsageFlags (
				IOFWIsochResourceFlags 			flags )
	{
		uint32_t outputCnt = 0;
		const uint64_t inputs[1]={(const uint64_t)flags};

		return IOConnectCallScalarMethod(mDevice.GetUserClientConnection(),
										 mDevice.MakeSelectorWithObject( kIsochPort_SetIsochResourceFlags_d, mKernPortRef ), 
										 inputs,1,
										 NULL,&outputCnt);
	}

	IOReturn
	LocalIsochPort::ExportDCLs( IOVirtualAddress * exportBuffer, IOByteCount * exportBytes )
	{
		IOReturn error = kIOReturnSuccess ;
	
		// see how much space we need for serialization...
		
//		unsigned byteCount = 0 ;
		*exportBytes = 0 ;
		for( DCLCommand * dcl = mDCLProgram; dcl != NULL; dcl = dcl->pNextDCLCommand )
		{
			*exportBytes += GetDCLSize( dcl ) ;
			
			switch ( dcl->opcode & ~kFWDCLOpFlagMask )
			{
				case kDCLUpdateDCLListOp :
				{
					// update DCLs store a copy of their update list in the export buffer
					*exportBytes += sizeof( mach_vm_address_t ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ;
					break ;
				}
				case kDCLCallProcOp :
				{
					*exportBytes += sizeof( uint64_t[kOSAsyncRef64Count]) ;
					break ;
				}
			}
		}

		// buffer to hold copy of DCLs in program
		error = vm_allocate( mach_task_self (), (vm_address_t*)exportBuffer, *exportBytes, true /*anywhere*/ ) ;

		if ( !*exportBuffer && !error )
		{
			error = kIOReturnNoMemory ;
		}

		// start from beginning
		{
			unsigned offset = 0 ;
			IOVirtualAddress buffer = *exportBuffer ;
			
			InfoLog("exporting DCLs, pass 1...\n") ;

			for( DCLCommand * dcl = mDCLProgram; dcl != NULL ; dcl = dcl->pNextDCLCommand )
			{

				unsigned size = GetDCLSize( dcl ) ;
		
				dcl->compilerData = offset ;		// save for later.

				// Copy the DCLs into the buffer using the ExportDCL structs
				switch(dcl->opcode & ~kFWDCLOpFlagMask)
				{
					case kDCLSendPacketStartOp:
					//case kDCLSendPacketWithHeaderStartOp:
					case kDCLSendPacketOp:
					case kDCLReceivePacketStartOp:
					case kDCLReceivePacketOp:
						{
							UserExportDCLTransferPacket *pUserExportDCLTransferPacket = (UserExportDCLTransferPacket*) buffer; 
							pUserExportDCLTransferPacket->pClientDCLStruct = (mach_vm_address_t) dcl; 
							pUserExportDCLTransferPacket->pNextDCLCommand = (mach_vm_address_t) ((DCLTransferPacket*)dcl)->pNextDCLCommand;
							pUserExportDCLTransferPacket->compilerData = ((DCLTransferPacket*)dcl)->compilerData;
							pUserExportDCLTransferPacket->opcode = ((DCLTransferPacket*)dcl)->opcode;
							pUserExportDCLTransferPacket->buffer = (mach_vm_address_t) ((DCLTransferPacket*)dcl)->buffer;
							pUserExportDCLTransferPacket->size = ((DCLTransferPacket*)dcl)->size;
						}
						break ;
					
					case kDCLSendBufferOp:
					case kDCLReceiveBufferOp:
						{
							UserExportDCLTransferBuffer *pUserExportDCLTransferBuffer = (UserExportDCLTransferBuffer*) buffer; 
							pUserExportDCLTransferBuffer->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLTransferBuffer->pNextDCLCommand = (mach_vm_address_t) ((DCLTransferBuffer*)dcl)->pNextDCLCommand;
							pUserExportDCLTransferBuffer->compilerData = ((DCLTransferBuffer*)dcl)->compilerData;
							pUserExportDCLTransferBuffer->opcode = ((DCLTransferBuffer*)dcl)->opcode;
							pUserExportDCLTransferBuffer->buffer = (mach_vm_address_t) ((DCLTransferBuffer*)dcl)->buffer;
							pUserExportDCLTransferBuffer->size = ((DCLTransferBuffer*)dcl)->size;
							pUserExportDCLTransferBuffer->packetSize = ((DCLTransferBuffer*)dcl)->packetSize;
							pUserExportDCLTransferBuffer->reserved = ((DCLTransferBuffer*)dcl)->reserved;
							pUserExportDCLTransferBuffer->bufferOffset = ((DCLTransferBuffer*)dcl)->bufferOffset;
						}
						break ;
					
					case kDCLCallProcOp:
						{
							UserExportDCLCallProc *pUserExportDCLCallProc = (UserExportDCLCallProc*) buffer; 
							pUserExportDCLCallProc->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLCallProc->pNextDCLCommand = (mach_vm_address_t) ((DCLCallProc*)dcl)->pNextDCLCommand;
							pUserExportDCLCallProc->compilerData = ((DCLCallProc*)dcl)->compilerData;
							pUserExportDCLCallProc->opcode = ((DCLCallProc*)dcl)->opcode;
							pUserExportDCLCallProc->proc = (mach_vm_address_t) ((DCLCallProc*)dcl)->proc;
							//pUserExportDCLCallProc->procData = (uint64_t) ((DCLCallProc*)dcl)->procData;
							pUserExportDCLCallProc->procData = (uint64_t)dcl ;
							size += sizeof( uint64_t[kOSAsyncRef64Count]) ;
						}
						break ;
					
					case kDCLLabelOp:
						{
							UserExportDCLLabel *pUserExportDCLLabel = (UserExportDCLLabel*) buffer; 
							pUserExportDCLLabel->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLLabel->pNextDCLCommand = (mach_vm_address_t) ((DCLLabel*)dcl)->pNextDCLCommand;
							pUserExportDCLLabel->compilerData = ((DCLLabel*)dcl)->compilerData;
							pUserExportDCLLabel->opcode = ((DCLLabel*)dcl)->opcode;
						}
						break ;
					
					case kDCLJumpOp:
						{
							UserExportDCLJump *pUserExportDCLJump = (UserExportDCLJump*) buffer; 
							pUserExportDCLJump->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLJump->pNextDCLCommand = (mach_vm_address_t) ((DCLJump*)dcl)->pNextDCLCommand;
							pUserExportDCLJump->compilerData = ((DCLJump*)dcl)->compilerData;
							pUserExportDCLJump->opcode = ((DCLJump*)dcl)->opcode;
							pUserExportDCLJump->pJumpDCLLabel = (mach_vm_address_t) ((DCLJump*)dcl)->pJumpDCLLabel;
						}
						break ;
					
					case kDCLSetTagSyncBitsOp:
						{
							UserExportDCLSetTagSyncBits *pUserExportDCLSetTagSyncBits = (UserExportDCLSetTagSyncBits*) buffer; 
							pUserExportDCLSetTagSyncBits->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLSetTagSyncBits->pNextDCLCommand = (mach_vm_address_t) ((DCLSetTagSyncBits*)dcl)->pNextDCLCommand;
							pUserExportDCLSetTagSyncBits->compilerData = ((DCLSetTagSyncBits*)dcl)->compilerData;
							pUserExportDCLSetTagSyncBits->opcode = ((DCLSetTagSyncBits*)dcl)->opcode;
							pUserExportDCLSetTagSyncBits->tagBits = ((DCLSetTagSyncBits*)dcl)->tagBits;
							pUserExportDCLSetTagSyncBits->syncBits = ((DCLSetTagSyncBits*)dcl)->syncBits;
						}
						break ;
					
					case kDCLUpdateDCLListOp:
						{
							UserExportDCLUpdateDCLList *pUserExportDCLUpdateDCLList = (UserExportDCLUpdateDCLList*) buffer; 
							pUserExportDCLUpdateDCLList->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLUpdateDCLList->pNextDCLCommand = (mach_vm_address_t) ((DCLUpdateDCLList*)dcl)->pNextDCLCommand;
							pUserExportDCLUpdateDCLList->compilerData = ((DCLUpdateDCLList*)dcl)->compilerData;
							pUserExportDCLUpdateDCLList->opcode = ((DCLUpdateDCLList*)dcl)->opcode;
							pUserExportDCLUpdateDCLList->dclCommandList = (mach_vm_address_t) ((DCLUpdateDCLList*)dcl)->dclCommandList;
							pUserExportDCLUpdateDCLList->numDCLCommands = ((DCLUpdateDCLList*)dcl)->numDCLCommands;
							size += ( sizeof( mach_vm_address_t ) * ((DCLUpdateDCLList*)dcl)->numDCLCommands ) ;
						}
						break ;
					
					case kDCLPtrTimeStampOp:
						{
							UserExportDCLPtrTimeStamp *pUserExportDCLPtrTimeStamp = (UserExportDCLPtrTimeStamp*) buffer; 
							pUserExportDCLPtrTimeStamp->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLPtrTimeStamp->pNextDCLCommand = (mach_vm_address_t) ((DCLPtrTimeStamp*)dcl)->pNextDCLCommand;
							pUserExportDCLPtrTimeStamp->compilerData = ((DCLPtrTimeStamp*)dcl)->compilerData;
							pUserExportDCLPtrTimeStamp->opcode = ((DCLPtrTimeStamp*)dcl)->opcode;
							pUserExportDCLPtrTimeStamp->timeStampPtr = (mach_vm_address_t) ((DCLPtrTimeStamp*)dcl)->timeStampPtr;
						}
						break ;
					
					case kDCLSkipCycleOp:
						{
							UserExportDCLCommand *pUserExportDCLCommand = (UserExportDCLCommand*) buffer; 
							pUserExportDCLCommand->pClientDCLStruct = (mach_vm_address_t) dcl;
							pUserExportDCLCommand->pNextDCLCommand = (mach_vm_address_t) ((DCLCommand*)dcl)->pNextDCLCommand;
							pUserExportDCLCommand->compilerData = ((DCLCommand*)dcl)->compilerData;
							pUserExportDCLCommand->opcode = ((DCLCommand*)dcl)->opcode;
							pUserExportDCLCommand->operands[0] = ((DCLCommand*)dcl)->operands[0];
						}
						break ;
				}
				
				// Account for this DCL's exported bytes
				buffer += size ;
				offset += size ;
			}

			InfoLog("...done\n") ;
		}
		
		// some DCLs (jumps and update list DCLs) refer to other DCLs with 
		// user space pointers which makes translation a bit harder.
		// The 'compilerData' field of all the DCLs in our export data block 
		// contain an offset in bytes from the beginning of the exported data
		// block..
		// We now replace any user space pointers with offsets for kernel use..
		{
			unsigned offset = 0 ;

			InfoLog("exporting DCLs, pass 2... export size=%d bytes\n", (int)*exportBytes ) ;

			while( offset < *exportBytes )
			{
				UserExportDCLCommand * dcl = (UserExportDCLCommand*)(*exportBuffer + offset ) ;
				DCLCommand *pClientDCL = (DCLCommand *) dcl->pClientDCLStruct;
				
//				DebugLog("DCL=%p, offset=%x, opcode=%x\n", dcl, offset, dcl->opcode) ;
	
				{
					unsigned opcode = dcl->opcode & ~kFWDCLOpFlagMask ;
					assert( opcode <= 15 || opcode == 20 ) ;
				}
	
				unsigned size = GetDCLSize( pClientDCL ) ;
				
				switch ( dcl->opcode & ~kFWDCLOpFlagMask )
				{
					case kDCLUpdateDCLListOp :
					{
						// make list of offsets from list of user space DCL pointers
						// List starts after DCL in question on export buffer
						mach_vm_address_t* list = (mach_vm_address_t*)( ((UserExportDCLUpdateDCLList*)dcl) + 1 )  ;
						for( unsigned index=0; index < ((UserExportDCLUpdateDCLList*)dcl)->numDCLCommands; ++index )
						{
							list[ index ] = (mach_vm_address_t) ((DCLUpdateDCLList*)pClientDCL)->dclCommandList[ index ]->compilerData ;
						}
	
						size += sizeof( mach_vm_address_t ) * ((DCLUpdateDCLList*)pClientDCL)->numDCLCommands ;
						
						break ;
					}
					
					case kDCLJumpOp :
					{
						((UserExportDCLJump*)dcl)->pJumpDCLLabel = (mach_vm_address_t) ((DCLJump*)pClientDCL)->pJumpDCLLabel->compilerData ;
						break ;
					}
					
					case kDCLCallProcOp :
					{
						size += sizeof( uint64_t[kOSAsyncRef64Count]) ;
						break ;
					}
						
					default :
					
						break ;
				}

#ifndef __LP64__		
				ROSETTA_ONLY(
					{	
						switch(dcl->opcode & ~kFWDCLOpFlagMask)
						{
							case kDCLSendPacketStartOp:
							//case kDCLSendPacketWithHeaderStartOp:
							case kDCLSendPacketOp:
							case kDCLReceivePacketStartOp:
							case kDCLReceivePacketOp:
								((UserExportDCLTransferPacket*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLTransferPacket*)dcl)->pNextDCLCommand );
								((UserExportDCLTransferPacket*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLTransferPacket*)dcl)->compilerData );
								((UserExportDCLTransferPacket*)dcl)->opcode = OSSwapInt32( ((UserExportDCLTransferPacket*)dcl)->opcode );
								((UserExportDCLTransferPacket*)dcl)->buffer = (mach_vm_address_t)OSSwapInt64( ((UserExportDCLTransferPacket*)dcl)->buffer );
								((UserExportDCLTransferPacket*)dcl)->size = OSSwapInt32( ((UserExportDCLTransferPacket*)dcl)->size );
								break ;
								
							case kDCLSendBufferOp:
							case kDCLReceiveBufferOp:
								((UserExportDCLTransferBuffer*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLTransferBuffer*)dcl)->pNextDCLCommand );
								((UserExportDCLTransferBuffer*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLTransferBuffer*)dcl)->compilerData );
								((UserExportDCLTransferBuffer*)dcl)->opcode = OSSwapInt32( ((UserExportDCLTransferBuffer*)dcl)->opcode );
								((UserExportDCLTransferBuffer*)dcl)->buffer = (mach_vm_address_t)OSSwapInt64(((UserExportDCLTransferBuffer*)dcl)->buffer );
								((UserExportDCLTransferBuffer*)dcl)->size = OSSwapInt32( ((UserExportDCLTransferBuffer*)dcl)->size );
								((UserExportDCLTransferBuffer*)dcl)->packetSize = OSSwapInt16( ((UserExportDCLTransferBuffer*)dcl)->packetSize );
								((UserExportDCLTransferBuffer*)dcl)->reserved = OSSwapInt16( ((UserExportDCLTransferBuffer*)dcl)->reserved );
								((UserExportDCLTransferBuffer*)dcl)->bufferOffset = OSSwapInt32( ((UserExportDCLTransferBuffer*)dcl)->bufferOffset );
								break ;
					
							case kDCLCallProcOp:
								((UserExportDCLCallProc*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLCallProc*)dcl)->pNextDCLCommand );
								((UserExportDCLCallProc*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLCallProc*)dcl)->compilerData );
								((UserExportDCLCallProc*)dcl)->opcode = OSSwapInt32( ((UserExportDCLCallProc*)dcl)->opcode );
								((UserExportDCLCallProc*)dcl)->proc = (mach_vm_address_t)OSSwapInt64(((UserExportDCLCallProc*)dcl)->proc );
								((UserExportDCLCallProc*)dcl)->procData = OSSwapInt64( ((UserExportDCLCallProc*)dcl)->procData );
								break ;
								
							case kDCLLabelOp:
								((UserExportDCLLabel*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLLabel*)dcl)->pNextDCLCommand );
								((UserExportDCLLabel*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLLabel*)dcl)->compilerData );
								((UserExportDCLLabel*)dcl)->opcode = OSSwapInt32( ((UserExportDCLLabel*)dcl)->opcode );
								break ;
								
							case kDCLJumpOp:
								((UserExportDCLJump*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64( ((UserExportDCLJump*)dcl)->pNextDCLCommand );
								((UserExportDCLJump*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLJump*)dcl)->compilerData );
								((UserExportDCLJump*)dcl)->opcode = OSSwapInt32( ((UserExportDCLJump*)dcl)->opcode );
								((UserExportDCLJump*)dcl)->pJumpDCLLabel = (mach_vm_address_t)OSSwapInt64(((UserExportDCLJump*)dcl)->pJumpDCLLabel );
								break ;
								
							case kDCLSetTagSyncBitsOp:
								((UserExportDCLSetTagSyncBits*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLSetTagSyncBits*)dcl)->pNextDCLCommand );
								((UserExportDCLSetTagSyncBits*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLSetTagSyncBits*)dcl)->compilerData );
								((UserExportDCLSetTagSyncBits*)dcl)->opcode = OSSwapInt32( ((UserExportDCLSetTagSyncBits*)dcl)->opcode );
								((UserExportDCLSetTagSyncBits*)dcl)->tagBits = OSSwapInt16( ((UserExportDCLSetTagSyncBits*)dcl)->tagBits );
								((UserExportDCLSetTagSyncBits*)dcl)->syncBits = OSSwapInt16( ((UserExportDCLSetTagSyncBits*)dcl)->syncBits );
								break ;
								
							case kDCLUpdateDCLListOp:
								((UserExportDCLUpdateDCLList*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLUpdateDCLList*)dcl)->pNextDCLCommand );
								((UserExportDCLUpdateDCLList*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLUpdateDCLList*)dcl)->compilerData );
								((UserExportDCLUpdateDCLList*)dcl)->opcode = OSSwapInt32( ((UserExportDCLUpdateDCLList*)dcl)->opcode );
								
								{
									mach_vm_address_t * list = (mach_vm_address_t *)( ((UserExportDCLUpdateDCLList*)dcl) + 1 )  ;
									for( unsigned index=0; index < ((UserExportDCLUpdateDCLList*)dcl)->numDCLCommands; ++index )
									{
										list[ index ] = (mach_vm_address_t)OSSwapInt64(list[ index ] );
									}
								}
								
								((UserExportDCLUpdateDCLList*)dcl)->dclCommandList = (mach_vm_address_t)OSSwapInt64(((UserExportDCLUpdateDCLList*)dcl)->dclCommandList );
								((UserExportDCLUpdateDCLList*)dcl)->numDCLCommands = OSSwapInt32( ((UserExportDCLUpdateDCLList*)dcl)->numDCLCommands );
								break ;
					
							case kDCLPtrTimeStampOp:
								((UserExportDCLPtrTimeStamp*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLPtrTimeStamp*)dcl)->pNextDCLCommand );
								((UserExportDCLPtrTimeStamp*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLPtrTimeStamp*)dcl)->compilerData );
								((UserExportDCLPtrTimeStamp*)dcl)->opcode = OSSwapInt32( ((UserExportDCLPtrTimeStamp*)dcl)->opcode );
								((UserExportDCLPtrTimeStamp*)dcl)->timeStampPtr = (mach_vm_address_t)OSSwapInt64(((UserExportDCLPtrTimeStamp*)dcl)->timeStampPtr );
								break;
								
							case kDCLSkipCycleOp:
								((UserExportDCLCommand*)dcl)->pNextDCLCommand = (mach_vm_address_t)OSSwapInt64(((UserExportDCLCommand*)dcl)->pNextDCLCommand );
								((UserExportDCLCommand*)dcl)->compilerData = OSSwapInt32( ((UserExportDCLCommand*)dcl)->compilerData );
								((UserExportDCLCommand*)dcl)->opcode = OSSwapInt32( ((UserExportDCLCommand*)dcl)->opcode );
								((UserExportDCLCommand*)dcl)->operands[0] = OSSwapInt32( ((UserExportDCLCommand*)dcl)->operands[0] );
								break;
						}
					}
				);
#endif
				
				offset += size ;
				
			}

			InfoLog("...done\n") ;
			
		}
		
		// fill in DCL compiler data fields with ( program index + 1 ) ;
		{
			unsigned count = 0 ;
			for( DCLCommand * dcl = mDCLProgram; dcl != nil; dcl = dcl->pNextDCLCommand )
			{
				dcl->compilerData = ++count ;		// index incremented here..
													// compiler data for DCL should be index + 1
			}
		}
		
		return error ;
	}
	
	IOReturn
	LocalIsochPort::Notify (
		IOFWDCLNotificationType 	notificationType,
		void ** 					inDCLList, 
		UInt32 						numDCLs )
	{
		IOReturn error = kIOReturnSuccess ;
	
		switch( notificationType )
		{
			case kFWNuDCLModifyNotification:
			{
				IOByteCount dataSize = 0 ;
				for( unsigned index=0; index < numDCLs; ++index )
				{
					dataSize += 4 + ((NuDCL**)inDCLList)[ index ]->Export( NULL, NULL, 0 ) ;
				}
				
				UInt8 *data;
				error = vm_allocate ( mach_task_self (), (vm_address_t *) &data, dataSize, true /*anywhere*/ ) ;
				if (error)
					break;

				{
					UInt8 * exportCursor = data ;
					for( unsigned index=0; index < numDCLs; ++index )				
					{
						NuDCL * dcl = ((NuDCL**)inDCLList)[ index ] ;
						*(UInt32*)exportCursor = dcl->GetExportIndex() ;
						
#ifndef __LP64__		
						ROSETTA_ONLY(
							{
								*(UInt32*)exportCursor = OSSwapInt32(*(UInt32*)exportCursor);
							}
						);
#endif
						exportCursor += sizeof( UInt32 ) ;
						
						dcl->Export( (IOVirtualAddress*) & exportCursor, mBufferRanges, mBufferRangeCount ) ;
					}
				}
				
				uint32_t outputCnt = 0;
				const uint64_t inputs[4] = {notificationType, numDCLs, (uint64_t) data, dataSize};
				error = IOConnectCallScalarMethod(mDevice.GetUserClientConnection(), 
												  Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
												  inputs,4,NULL,&outputCnt);
				vm_deallocate( mach_task_self (), (vm_address_t) data, dataSize ) ;
				break ;
			}
			
			case kFWNuDCLModifyJumpNotification:
			{
				unsigned pairCount = numDCLs << 1 ;
				
				unsigned dcls[ pairCount ] ;
				
				{
					unsigned index = 0 ;
					unsigned pairIndex=0; 
					
					while( pairIndex < pairCount )
					{
						NuDCL * theDCL = ((NuDCL**)inDCLList)[ index++ ] ;
						dcls[ pairIndex ] = theDCL->GetExportIndex() ;
#ifndef __LP64__		
						ROSETTA_ONLY(
							{
								dcls[ pairIndex ] = OSSwapInt32(dcls[ pairIndex ]);
							}
						);
#endif
						pairIndex += 1;
						
						
						if (theDCL->GetBranch())
							dcls[ pairIndex ] = theDCL->GetBranch()->GetExportIndex() ;
						else
							dcls[ pairIndex ] = 0;
#ifndef __LP64__		
						ROSETTA_ONLY(
							{
								dcls[ pairIndex ] = OSSwapInt32(dcls[ pairIndex ]);
							}
						);
#endif
						pairIndex += 1;
					}
				}

				uint32_t outputCnt = 0;
				size_t outputStructSize =  0 ;
				const uint64_t inputs[2] = {notificationType, numDCLs};
				error = IOConnectCallMethod(mDevice.GetUserClientConnection(),
											Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
											inputs,2,
											dcls,sizeof( dcls ),
											NULL,&outputCnt,
											NULL,&outputStructSize);
				break ;
			}
			
			case kFWNuDCLUpdateNotification:
			{
				unsigned dcls[ numDCLs ] ;

				for( unsigned index=0; index < numDCLs; ++index )
				{
					dcls[ index ] = ((NuDCL*)inDCLList[ index ])->GetExportIndex() ;
#ifndef __LP64__		
					ROSETTA_ONLY(
						{
							dcls[ index ] = OSSwapInt32(dcls[ index ]);
							}
						);
#endif
				}

				uint32_t outputCnt = 0;
				size_t outputStructSize =  0 ;
				const uint64_t inputs[2] = {notificationType, numDCLs};
				error = IOConnectCallMethod(mDevice.GetUserClientConnection(),
											Device::MakeSelectorWithObject( kLocalIsochPort_Notify_d, mKernPortRef ), 
											inputs,2,
											dcls,sizeof( dcls ),
											NULL,&outputCnt,
											NULL,&outputStructSize);
				break ;
			}
			
			case kFWDCLUpdateNotification:
			case kFWDCLModifyNotification:
			{
				error = kIOReturnUnsupported ;
			}
				
			default:
				error = kIOReturnBadArgument ;
		}		
		
		return error ;
	}

#pragma mark -
	// ============================================================
	//
	// LocalIsochPortCOM
	//
	// ============================================================
	
	LocalIsochPortCOM::LocalIsochPortCOM( Device& userclient, bool talking, DCLCommand* program, UInt32 startEvent,
			UInt32 startState, UInt32 startMask, IOVirtualRange programRanges[], UInt32 programRangeCount,
			IOVirtualRange bufferRanges[], UInt32 bufferRangeCount, IOFWIsochPortOptions options )
	: LocalIsochPort( reinterpret_cast<const IUnknownVTbl &>( sInterface ), userclient, talking, program, 
			startEvent, startState, startMask, programRanges, 
			programRangeCount, bufferRanges, bufferRangeCount, options )
	{
	}
	
	LocalIsochPortCOM::~LocalIsochPortCOM()
	{
	}
	
	IUnknownVTbl**
	LocalIsochPortCOM::Alloc( Device & userclient, Boolean talking, DCLCommand * program,
			UInt32 startEvent, UInt32 startState, UInt32 startMask,
			IOVirtualRange programRanges[], UInt32 programRangeCount, 
			IOVirtualRange bufferRanges[], UInt32 bufferRangeCount,
			IOFWIsochPortOptions options )
	{
		LocalIsochPortCOM*	me = nil ;
		
		try
		{
			me = new LocalIsochPortCOM (	userclient, (bool)talking, program, startEvent, startState, startMask,
											programRanges, programRangeCount, bufferRanges, bufferRangeCount, options ) ;
		}
		catch(...)
		{
		}

		return ( nil == me ) ? nil : reinterpret_cast < IUnknownVTbl ** > ( & me->GetInterface () ) ;
	}
	
	HRESULT
	LocalIsochPortCOM::QueryInterface (	REFIID iid, void ** ppv )
	{
		HRESULT		result			= S_OK ;
		CFUUIDRef	interfaceID		= CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid) ;

		*ppv = nil ;
	
		if ( CFEqual(interfaceID, IUnknownUUID) 
				|| CFEqual(interfaceID, kIOFireWireLocalIsochPortInterfaceID ) 
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v2 )
#if 0
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v3 )	// don't support this yet...
#endif
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v4 )
				|| CFEqual( interfaceID, kIOFireWireLocalIsochPortInterfaceID_v5 )
			)
		{
			* ppv = & GetInterface () ;
			AddRef () ;
		}
		else
		{
			DebugLog("unknown local isoch port interface UUID\n") ;
			
			* ppv = nil ;
			result = E_NOINTERFACE ;
		}	
		
		:: CFRelease ( interfaceID ) ;
		return result ;
	}
	
	IOReturn
	LocalIsochPortCOM::SModifyJumpDCL(
				IOFireWireLibLocalIsochPortRef 	self, 
				DCLJump *	 					jump, 
				DCLLabel *		 				label)
	{
		return IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis ( self )->ModifyJumpDCL ( jump, label ) ;
	}
	
	//
	// utility functions
	//
	
	void
	LocalIsochPortCOM::SPrintDCLProgram (
				IOFireWireLibLocalIsochPortRef 	self ,
				const DCLCommand *				program ,
				UInt32							length )
	{
		DeviceCOM::SPrintDCLProgram ( nil, program, length ) ;
	}	

	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCLSize ( 
				PortRef 				self, 
				DCLTransferPacket * 	dcl, 
				IOByteCount 			newSize )
	{
		IOReturn error = kIOReturnBadArgument ;
	
		switch ( dcl->opcode )
		{
			case kDCLSendPacketStartOp:
			//case kDCLSendPacketWithHeaderStartOp:
			case kDCLSendPacketOp:
			case kDCLReceivePacketStartOp:
			case kDCLReceivePacketOp:
			case kDCLReceiveBufferOp:			
				error = IOFireWireIUnknown::InterfaceMap<LocalIsochPortCOM>::GetThis( self )->ModifyTransferPacketDCLSize( dcl, newSize ) ;
		}

		return error ;
	}

	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCLBuffer (
				PortRef 				self, 
				DCLTransferPacket * 	dcl, 
				void * 					newBuffer )
	{
		return kIOReturnUnsupported ;
	}
	
	IOReturn
	LocalIsochPortCOM::SModifyTransferPacketDCL ( PortRef self, DCLTransferPacket * dcl, void * newBuffer, IOByteCount newSize )
	{
		return kIOReturnUnsupported ;
	}

	//
	// v4
	//
	
	IOReturn
	LocalIsochPortCOM::S_SetFinalizeCallback( 
				IOFireWireLibLocalIsochPortRef 				self, 
				IOFireWireLibIsochPortFinalizeCallback	 	finalizeCallback )
	{
		LocalIsochPortCOM * 	me = IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis( self ) ;

		me->mFinalizeCallback = finalizeCallback ;
		
		return kIOReturnSuccess ;
	}

	//
	// v5 (panther)
	//
	
	IOReturn
	LocalIsochPortCOM::S_SetResourceUsageFlags (
				IOFireWireLibLocalIsochPortRef	self, 
				IOFWIsochResourceFlags 			flags )
	{
		return IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis( self )->SetResourceUsageFlags( flags ) ;
	}

	IOReturn
	LocalIsochPortCOM::S_Notify( 
				IOFireWireLibLocalIsochPortRef self, 
				IOFWDCLNotificationType notificationType, 
				void ** inDCLList, 
				UInt32 numDCLs )
	{
		return  IOFireWireIUnknown::InterfaceMap< LocalIsochPortCOM >::GetThis( self )->Notify( notificationType, inDCLList, numDCLs ) ;
	}
}
