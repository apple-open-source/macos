/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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

//
// tdclient - Security tokend client interface library
//
#include "tdtransit.h"
#include <security_utilities/debugging.h>

using MachPlusPlus::check;
using MachPlusPlus::Bootstrap;


namespace Security {
namespace Tokend {


//
// Construct a client session
//
ClientSession::ClientSession(Allocator &std, Allocator &rtn)
	: ClientCommon(std, rtn)
{
}


//
// Destroy a session
//
ClientSession::~ClientSession()
{ }


//
// The default fault() notifier does nothing
//
void ClientSession::fault()
{
}


//
// Administrativa
//
void ClientSession::servicePort(Port p)
{
	// record service port
	assert(!mServicePort);	// no overwrite
	mServicePort = p;
	
	// come back if the service port dies (usually a tokend crash)
	mServicePort.requestNotify(mReplyPort);
}


} // end namespace Tokend
} // end namespace Security
