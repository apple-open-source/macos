/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// machrunloopserver - C++ shell for writing Mach 3 servers called by CFRunLoop.
//
// Note that this is a subclass of MachServer and tries to preserve its interface,
// so you can switch back-and-forth between them with a minimum of fuss.
// Timers are not currently implemented; they're not that hard to add if you need them.
//
#ifndef _H_MACHRUNLOOPSERVER
#define _H_MACHRUNLOOPSERVER

#include <security_utilities/machserver.h>
#include <security_utilities/cfmach++.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>


namespace Security {
namespace MachPlusPlus {


//
// A Mach server object variant for use with CFRunLoops
//
// This currently only supports a subset of full MachServer functionality.
//
class MachRunLoopServer : public MachServer, private CFAutoPort {
public:
	MachRunLoopServer();		// anonymous
	MachRunLoopServer(const char *name); // register by name
	MachRunLoopServer(const char *name, const Bootstrap &boot); // register in bootstrap
	virtual ~MachRunLoopServer();

	void run(mach_msg_size_t maxSize = 4096, mach_msg_options_t options = 0);

	static MachRunLoopServer &active()
	{ return safer_cast<MachRunLoopServer &>(MachServer::active()); }
	
	void notifyIfDead(Port port, bool doNotify = true) const;

protected:
	void receive(const Message &request);
	void oneRequest(const Message &request);

private:
	static void cfInvalidate(CFMachPortRef port, void *info);
	
private:
	Message mReplyMessage;
};


} // end namespace MachPlusPlus

} // end namespace Security

#endif //_H_MACHRUNLOOPSERVER
