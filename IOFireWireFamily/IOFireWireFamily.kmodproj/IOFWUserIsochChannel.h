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
 *  IOFWUserIsochChannel.h
 *  IOFireWireFamily
 *
 *  Created by noggin on Tue May 15 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

// public
#import <IOKit/firewire/IOFWIsochChannel.h>

class IOFWUserIsochChannel: public IOFWIsochChannel
{
	typedef IOFWIsochChannel super ;
	
	OSDeclareDefaultStructors(IOFWUserIsochChannel)
	
	public :
	
		virtual bool					init(	IOFireWireController *		control, 
													bool 						doIRM,
													UInt32 						packetSize, 
													IOFWSpeed 					prefSpeed ) ;

		// IOFWIsochChannel
		virtual IOReturn 				allocateChannel(void) APPLE_KEXT_OVERRIDE;
		virtual IOReturn 				releaseChannel(void) APPLE_KEXT_OVERRIDE;
		virtual IOReturn 				start(void) APPLE_KEXT_OVERRIDE;
		virtual IOReturn 				stop(void) APPLE_KEXT_OVERRIDE;
		
		// me
		IOReturn						allocateChannelBegin(
												IOFWSpeed		speed,
												UInt64			allowedChans,
												UInt32 *		outChannel )				{ return IOFWIsochChannel::allocateChannelBegin( speed, allowedChans, outChannel ) ; }
		IOReturn						releaseChannelComplete()							{ return IOFWIsochChannel::releaseChannelComplete() ; }
		IOReturn						allocateListenerPorts() ;
		IOReturn						allocateTalkerPort() ;
		static void						s_exporterCleanup( IOFWUserIsochChannel * channel ) ;
		
		inline io_user_reference_t *	getUserAsyncRef()									{ return fAsyncRef ; }
		inline void						setUserAsyncRef( OSAsyncReference64 asyncRef )		{ fAsyncRef = asyncRef ; }
		
	protected:
	
		bool					fBandwidthAllocated ;
		io_user_reference_t *   fAsyncRef ;

	public :

		static IOReturn						isochChannel_ForceStopHandler( void * self, IOFWIsochChannel*, UInt32 stopCondition ) ;
	
} ;
