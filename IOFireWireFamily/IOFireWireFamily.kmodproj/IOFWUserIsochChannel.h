/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
	OSDeclareDefaultStructors(IOFWUserIsochChannel)
	
	public :
	
		// IOFWIsochChannel
		virtual IOReturn 				allocateChannel();
		virtual IOReturn 				releaseChannel();
		virtual IOReturn 				start();
		virtual IOReturn 				stop();
		
		// me
//		IOReturn						userAllocateChannelBegin(
//														IOFWSpeed	inSpeed,
//														UInt32		inAllowedChansHi,
//														UInt32		inAllowedChansLo,
//														IOFWSpeed*	outActualSpeed,
//														UInt32*		outActualChannel) ;
//		IOReturn						userReleaseChannelComplete() ;
		IOReturn						allocateChannelBegin(
												IOFWSpeed		speed,
												UInt64			allowedChans,
												UInt32 *		outChannel )				{ return IOFWIsochChannel::allocateChannelBegin( speed, allowedChans, outChannel ) ; }
		IOReturn						releaseChannelComplete()							{ return IOFWIsochChannel::releaseChannelComplete() ; }
		IOReturn						allocateListenerPorts() ;
		IOReturn						allocateTalkerPort() ;
		static void						s_exporterCleanup( IOFWUserIsochChannel * channel ) ;
		
		inline natural_t *				getAsyncRef()									{ return (natural_t*)fStopRefCon ; }
		inline void						setAsyncRef( OSAsyncReference asyncRef )		{ fStopRefCon = asyncRef ; }
		
	protected:
	
		bool			fBandwidthAllocated ;
} ;
