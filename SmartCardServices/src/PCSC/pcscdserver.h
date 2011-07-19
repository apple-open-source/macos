/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// pcscdserver - stripped down securityd main server object
//
#ifndef _H_PCSCDSERVER
#define _H_PCSCDSERVER

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <security_utilities/machserver.h>
#include <security_utilities/powerwatch.h>
#include <map>

#if defined(__cplusplus)

namespace PCSCD {
//
// The server object itself. This is the "go to" object for anyone who wants
// to access the server's global state. It runs the show.
// There is only one Server, and its name is Server::active().
//

class Server : public MachPlusPlus::MachServer
{
public:
	Server(const char *bootstrapName);
	~Server();
		
    // run the server until it shuts down
	void run();
	
    //
    // Retrieve pieces of the Server's object web.
    // These are all static methods that use the active() Server of this thread.
    //
	static Server &active() { return safer_cast<Server &>(MachServer::active()); }
	static const char *bootstrapName() { return active().mBootstrapName.c_str(); }

protected:
    // implementation methods of MachServer
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	void notifyDeadName(MachPlusPlus::Port port);
	void notifyNoSenders(MachPlusPlus::Port port, mach_port_mscount_t);
	void threadLimitReached(UInt32 count);
	// request port status notifications (override virtual methods below to receive)
	virtual void notifyIfDead(MachPlusPlus::Port port, bool doNotify = true) const;
	virtual void notifyIfUnused(MachPlusPlus::Port port, bool doNotify = true) const;

private:
	class SleepWatcher : public MachPlusPlus::PortPowerWatcher
	{
	public:
		void systemWillSleep();
		void systemIsWaking();
		
		void add(PowerWatcher *client);
		void remove(PowerWatcher *client);

	private:
		set<PowerWatcher *> mPowerClients;
	};

	SleepWatcher sleepWatcher;
	
public:
	using MachServer::add;
	using MachServer::remove;
	void add(MachPlusPlus::PowerWatcher *client)	{ StLock<Mutex> _(mLock); sleepWatcher.add(client); }
	void remove(MachPlusPlus::PowerWatcher *client)	{ StLock<Mutex> _(mLock); sleepWatcher.remove(client); }
    
private:
	// mach bootstrap registration name
	std::string mBootstrapName;
	mutable Mutex mLock;	
};

} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif //_H_PCSCDSERVER
