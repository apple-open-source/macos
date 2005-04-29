/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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

#include "eventlistener.h"

using namespace MachPlusPlus;


namespace Security {
namespace SecurityServer {


//
// Constructing an EventListener immediately enables it for event reception.
//
EventListener::EventListener (NotificationDomain domain, NotificationMask eventMask,
	Allocator &standard, Allocator &returning)
	: SecurityServer::ClientSession (standard, returning)
{
	// behold the power of multiple inheritance...
	CFAutoPort::enable();								// enable port reception
	Port::qlimit(MACH_PORT_QLIMIT_MAX);					// set maximum queue depth
	ClientSession::requestNotification(*this, domain, eventMask); // request notifications there
}


//
// StopNotification() is needed on destruction; everyone else cleans up after themselves.
//
EventListener::~EventListener ()
{
	ClientSession::stopNotification(*this);
}


//
// We simply hand off the incoming raw mach message to ourselves via
// SecurityServer::dispatchNotification. This will call our consume() method,
// which our subclass will have to provide.
//
void EventListener::receive(const Message &message)
{
	ClientSession::dispatchNotification(message, this);
}


} // end namespace SecurityServer
} // end namespace Security
