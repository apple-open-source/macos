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
// server - pcscd main server object
//
#include "pcscdserver.h"
#include <mach/mach_error.h>

using namespace Security;
using namespace MachPlusPlus;

namespace PCSCD {

Server::Server(const char *bootstrapName) : MachServer(bootstrapName),
    mBootstrapName(bootstrapName)
{
	// Construct the server object
    // engage the subsidiary port handler for sleep notifications
	add(sleepWatcher);
}

Server::~Server()
{
	// Clean up the server object
}

void Server::run()
{
	// Run the server. This will not return until the server is forced to exit.
	MachServer::run(0x10000,
        MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
        MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT));
}

//
// Handle thread overflow. MachServer will call this if it has hit its thread
// limit and yet still needs another thread.
//
void Server::threadLimitReached(UInt32 limit)
{
//	Syslog::notice("pcscd has reached its thread limit (%ld) - service deadlock is possible",
//		limit);
}

void Server::notifyDeadName(Port port)
{
	// Handling dead-port notifications.
	// This receives DPNs for all kinds of ports we're interested in.
	StLock<Mutex> _(mLock);
	secdebug("SSports", "port %d is dead", port.port());

	// well, what IS IT?!
	secdebug("server", "spurious dead port notification for port %d", port.port());
}

//
// Handling no-senders notifications.
// This is currently only used for (subsidiary) service ports
//
void Server::notifyNoSenders(Port port, mach_port_mscount_t)
{
	secdebug("SSports", "port %d no senders", port.port());
//	Session::destroy(port);
}

void Server::notifyIfDead(MachPlusPlus::Port port, bool doNotify) const
{
	secdebug("SSports", "port %d is dead", port.port());
	MachServer::notifyIfDead(port, doNotify);
}

void Server::notifyIfUnused(MachPlusPlus::Port port, bool doNotify) const
{
	secdebug("SSports", "port %d is dead", port.port());
	MachServer::notifyIfUnused(port, doNotify);
}

void Server::SleepWatcher::systemWillSleep()
{
	// Notifier for system sleep events
    secdebug("SS", "sleep notification received");
//    Session::processSystemSleep();
	secdebug("server", "distributing sleep event to %ld clients", mPowerClients.size());
	for (set<PowerWatcher *>::const_iterator it = mPowerClients.begin(); it != mPowerClients.end(); it++)
		(*it)->systemWillSleep();
}

void Server::SleepWatcher::systemIsWaking()
{
	secdebug("server", "distributing wakeup event to %ld clients", mPowerClients.size());
	for (set<PowerWatcher *>::const_iterator it = mPowerClients.begin(); it != mPowerClients.end(); it++)
		(*it)->systemIsWaking();
}

void Server::SleepWatcher::add(PowerWatcher *client)
{
	assert(mPowerClients.find(client) == mPowerClients.end());
	mPowerClients.insert(client);
}

void Server::SleepWatcher::remove(PowerWatcher *client)
{
	assert(mPowerClients.find(client) != mPowerClients.end());
	mPowerClients.erase(client);
}

boolean_t Server::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	// The primary server run-loop function
    secdebug("SSreq", "Server::handle(mach_msg_header_t *in, mach_msg_header_t *out)");
	return false;
}


} // end namespace PCSCD

