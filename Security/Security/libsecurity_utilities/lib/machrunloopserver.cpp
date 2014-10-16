/*
 * Copyright (c) 2000-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// machrunloopserver - C++ shell for writing Mach 3 servers called by CFRunLoop
//
#include "machrunloopserver.h"
#include <security_utilities/cfutilities.h>
#include <mach/mach_error.h>
#include <security_utilities/debugging.h>


namespace Security {
namespace MachPlusPlus {


//
// Generic Mach server
//
MachRunLoopServer::MachRunLoopServer(const char *name)
	: MachServer(name), CFAutoPort(primaryServicePort())
{
}

MachRunLoopServer::MachRunLoopServer(const char *name, const Bootstrap &boot)
	: MachServer(name, boot), CFAutoPort(primaryServicePort())
{
}

void MachRunLoopServer::run(mach_msg_size_t bufferSize, mach_msg_options_t options)
{
	// allocate reply buffer
	mReplyMessage.setBuffer(bufferSize);
	
	// enable reception
	CFAutoPort::enable();

	// we are it!
	perThread().server = this;
}


MachRunLoopServer::~MachRunLoopServer()
{
    // no longer active on this thread
    perThread().server = NULL;
}


//
// Handle dead-port notifications.
// Since we don't actually run our own runloop here, we can't well use standard
// notifications to our own server port. So we use a CFMachPort facility instead.
//
void MachRunLoopServer::notifyIfDead(Port port, bool doNotify) const
{
	if (CFMachPortRef cfPort = CFMachPortCreateWithPort(NULL, port, NULL, NULL, NULL))
		CFMachPortSetInvalidationCallBack(cfPort, cfInvalidate);
}

void MachRunLoopServer::cfInvalidate(CFMachPortRef cfPort, void *context)
{
	reinterpret_cast<MachRunLoopServer *>(context)->notifyDeadName(CFMachPortGetPort(cfPort));
	//@@@ should we CFRelease cfPort here?
}


//
// Reception callback
//
void MachRunLoopServer::receive(const Message &request)
{
	active().oneRequest(request);
}

void MachRunLoopServer::oneRequest(const Message &request)
{
	if (!handle(request, mReplyMessage)) {	// MIG dispatch failed
		secdebug("machrls", "MachRunLoopServer dispatch failed");
	} else {
		// MIG dispatch handled the call. Send reply back to caller.
		mReplyMessage.send((MACH_MSGH_BITS_REMOTE(mReplyMessage.bits()) == MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
			MACH_SEND_MSG :	MACH_SEND_MSG|MACH_SEND_TIMEOUT);
	}
	active().releaseDeferredAllocations();
}


} // end namespace MachPlusPlus
} // end namespace Security
