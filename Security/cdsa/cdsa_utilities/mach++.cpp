/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// mach++ - C++ bindings for useful Mach primitives
//
#include "mach++.h"
#include <mach/mach_error.h>
#include <stdio.h>	// debug
#include <Security/debugging.h>
#include <servers/bootstrap_defs.h>	// debug

namespace Security {
namespace MachPlusPlus {


//
// Mach subsystem exceptions, a subclass of CssmCommonError
//
Error::Error(kern_return_t err) : error(err)
{
}

Error::~Error() throw()
{
}

CSSM_RETURN
Error::cssmError() const
{
	switch (error) {
	case BOOTSTRAP_UNKNOWN_SERVICE:
	case MIG_SERVER_DIED:
		return CSSM_ERRCODE_SERVICE_NOT_AVAILABLE;
	default: 
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
}

OSStatus
Error::osStatus() const
{
	return cssmError();
}

void Error::check(kern_return_t status)
{
	if (status != KERN_SUCCESS)
		Error::throwMe(status);
}

void Error::throwMe(kern_return_t err)
{
	throw Error(err);
}


#if !defined(NDEBUG)
void Error::debugDiagnose(const void *id) const
{
	const char *name;
	switch (error) {
	default:
		name = mach_error_string(error); break;
	case BOOTSTRAP_UNKNOWN_SERVICE:
		name = "BOOTSTRAP_UNKNOWN_SERVICE"; break;
	case BOOTSTRAP_NAME_IN_USE:	
		name = "BOOTSTRAP_NAME_IN_USE"; break;
	case BOOTSTRAP_NOT_PRIVILEGED:
		name = "BOOTSTRAP_NOT_PRIVILEGED"; break;
	case BOOTSTRAP_SERVICE_ACTIVE:
		name = "BOOTSTRAP_SERVICE_ACTIVE"; break;
	}
    secdebug("exception", "%p Mach Error %s (%d) osStatus %ld",
		id, name, error, osStatus());
}
#endif //NDEBUG


//
// Memory management
//
void *allocate(size_t size)
{
	vm_address_t address;
	check(vm_allocate(mach_task_self(), &address, size, true));
	return reinterpret_cast<void *>(address);
}

void deallocate(vm_address_t address, size_t size)
{
	check(vm_deallocate(mach_task_self(), address, size));
}


//
// Port functions
//
mach_port_urefs_t Port::getRefs(mach_port_right_t right)
{
	mach_port_urefs_t count;
	check(::mach_port_get_refs(self(), mPort, right, &count));
	return count;
}

mach_port_t Port::requestNotify(mach_port_t notify, mach_msg_id_t type, mach_port_mscount_t sync)
{
    mach_port_t previous;
    check(mach_port_request_notification(self(), mPort, type, sync, notify,
        MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous));

#if !defined(NDEBUG)
	const char *typeName;
	switch (type) {
	case MACH_NOTIFY_PORT_DELETED:	typeName = "port deleted"; break;
	case MACH_NOTIFY_PORT_DESTROYED:typeName = "port destroyed"; break;
	case MACH_NOTIFY_NO_SENDERS:	typeName = "no senders"; break;
	case MACH_NOTIFY_SEND_ONCE:		typeName = "send once"; break;
	case MACH_NOTIFY_DEAD_NAME:		typeName = "dead name"; break;
	default:						typeName = "???"; break;
	}
	if (notify == MACH_PORT_NULL)
		secdebug("port", "%d cancel notify %s", port(), typeName);
	else
		secdebug("port", "%d request notify %s to %d (sync %d)", port(), typeName, notify, sync);
#endif //!NDEBUG

    return previous;
}

mach_port_t Port::cancelNotify(mach_msg_id_t type)
{
    // Mach won't let us unset the DPN port if we are already dead
    // (EVEN if the DPN has already been sent!) So just ignore that case...
    if (isDead())
        return MACH_PORT_NULL;
	return requestNotify(MACH_PORT_NULL, type);
}


//
// PortSet features
//
set<Port> PortSet::members() const
{
	mach_port_array_t members;
	mach_msg_type_number_t count;
	check(::mach_port_get_set_status(self(), mPort, &members, &count));
	try {
		set<Port> result;
		copy(members, members+count, inserter(result, result.begin()));
		vm_deallocate(self(), vm_address_t(members), count * sizeof(members[0]));
		return result;
	} catch (...) {
		vm_deallocate(self(), vm_address_t(members), count * sizeof(members[0]));
		throw;
	}
}


bool PortSet::contains(Port member) const
{
	set<Port> memberSet = members();
	return memberSet.find(member) != memberSet.end();
}


//
// Task port features
//
pid_t TaskPort::pid() const
{
    pid_t pid;
    check(::pid_for_task(mPort, &pid));
    return pid;
}

TaskPort TaskPort::forPid(pid_t pid)
{
    TaskPort taskPort;
    check(::task_for_pid(self(), pid, &taskPort.port()));
    return taskPort;
}


//
// Bootstrap port management
//
mach_port_t Bootstrap::checkIn(const char *name) const
{
	mach_port_t port;
	check(::bootstrap_check_in(mPort, makeName(name), &port));
	return port;
}

mach_port_t Bootstrap::checkInOptional(const char *name) const
{
	mach_port_t port;
	switch (kern_return_t err = ::bootstrap_check_in(mPort, makeName(name), &port)) {
	case BOOTSTRAP_SERVICE_ACTIVE:
    case BOOTSTRAP_UNKNOWN_SERVICE:
	case BOOTSTRAP_NOT_PRIVILEGED:
        return MACH_PORT_NULL;
	default:
		check(err);
	}
	return port;
}

void Bootstrap::registerAs(mach_port_t port, const char *name) const
{
	secdebug("bootstrap", "creating service port %d in %d:%s", port, this->port(), name);
	check(::bootstrap_register(mPort, makeName(name), port));
}

mach_port_t Bootstrap::lookup(const char *name) const
{
	mach_port_t port;
	check(::bootstrap_look_up(mPort, makeName(name), &port));
	return port;
}

mach_port_t Bootstrap::lookupOptional(const char *name) const
{
	mach_port_t port;
	kern_return_t err = ::bootstrap_look_up(mPort, makeName(name), &port);
    if (err == BOOTSTRAP_UNKNOWN_SERVICE)
        return 0;
    check(err);
	return port;
}


Bootstrap Bootstrap::subset(Port requestor)
{
    mach_port_t sub;
    check(::bootstrap_subset(mPort, requestor, &sub));
    return sub;
}


//
// ReceivePorts
//
ReceivePort::ReceivePort(const char *name, const Bootstrap &bootstrap)
{
	mPort = bootstrap.checkInOptional(name);
	if (!mPort) {
		allocate();
		// Bootstrap registration requires a send right to (copy) send.
		// Make a temporary one, send it, then take it away again, to avoid
		// messing up the caller's send right accounting.
		insertRight(MACH_MSG_TYPE_MAKE_SEND);
		bootstrap.registerAs(mPort, name);
		modRefs(MACH_PORT_RIGHT_SEND, -1);
	}
}


//
// Stack-based bootstrap switcher
//
ModuleNexus<Mutex> StBootstrap::critical;

StBootstrap::StBootstrap(const Bootstrap &newBoot, const TaskPort &task) 
    : mTask(task), locker(critical())
{
    mOldBoot = Bootstrap();
    mTask.bootstrap(newBoot);
    secdebug("StBoot", "bootstrap for %d switched to %d", mTask.port(), newBoot.port());
}

StBootstrap::~StBootstrap()
{
    mTask.bootstrap(mOldBoot);
    secdebug("StBoot", "bootstrap for %d returned to %d", mTask.port(), mOldBoot.port());
}


//
// Mach message buffers
//
Message::Message(void *buffer, size_t size)
    : mBuffer(reinterpret_cast<mig_reply_error_t *>(buffer)), 
    mSize(size), mRelease(false)
{
    assert(size >= sizeof(mach_msg_header_t));
}

Message::Message(size_t size)
{
    mSize = size + MAX_TRAILER_SIZE;
    mBuffer = (mig_reply_error_t *)new char[mSize];
    mRelease = true;
}

Message::~Message()
{
    if (mRelease)
        delete[] mBuffer;
}


bool Message::check(kern_return_t status)
{
    switch (status) {
    case KERN_SUCCESS:
        return true;
    case MACH_RCV_TIMED_OUT:
    case MACH_SEND_TIMED_OUT:
        return false;
    default:
        Error::throwMe(status);
    }
}


bool Message::send(mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    return check(mach_msg_overwrite(*this,
        options | MACH_SEND_MSG,
        length(),
        0, MACH_PORT_NULL,
        timeout, notify,
        NULL, 0));
}

bool Message::receive(mach_port_t receivePort,
    mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    return check(mach_msg_overwrite(*this,
        options | MACH_RCV_MSG,
        length(),
        mSize, receivePort,
        timeout, notify,
        NULL, 0));
}

bool Message::sendReceive(mach_port_t receivePort,
    mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    return check(mach_msg_overwrite(*this,
        options | MACH_SEND_MSG | MACH_RCV_MSG,
        length(),
        mSize, receivePort,
        timeout, notify,
        NULL, 0));
}


//
// Debug dumping of ports etc.
//
#if defined(DEBUGDUMP)

void Port::dump(const char *descr)
{
    if (mPort == MACH_PORT_NULL) {
        Debug::dump("[%s==NULL]\n", descr ? descr : "port");
    } else {
        Debug::dump("[%s(%d)", descr ? descr : "port", mPort);
        mach_port_type_t type;
        if (kern_return_t err = mach_port_type(self(), mPort, &type)) {
            Debug::dump(" !%s", mach_error_string(err));
        } else {
            if (type & MACH_PORT_TYPE_SEND)
                Debug::dump(" send(%d)", getRefs(MACH_PORT_RIGHT_SEND));
            if (type & MACH_PORT_TYPE_RECEIVE)
                Debug::dump(" rcv");
            if (type & MACH_PORT_TYPE_SEND_ONCE)
                Debug::dump(" once(%d)", getRefs(MACH_PORT_RIGHT_SEND));
            if (type & MACH_PORT_TYPE_PORT_SET)
                Debug::dump(" set");
            if (type & MACH_PORT_TYPE_DEAD_NAME)
                Debug::dump(" dead(%d)", getRefs(MACH_PORT_RIGHT_SEND));        
            if (type & MACH_PORT_TYPE_DNREQUEST)
                Debug::dump(" dnreq");
            // handle unknown/unexpected type flags
            if (type & ~(MACH_PORT_TYPE_SEND|MACH_PORT_TYPE_RECEIVE|MACH_PORT_TYPE_SEND_ONCE|
                    MACH_PORT_TYPE_PORT_SET|MACH_PORT_TYPE_DEAD_NAME|MACH_PORT_TYPE_DNREQUEST))
                Debug::dump(" type(0x%x)", type);
        }
        Debug::dump("]\n");
    }
}


void Bootstrap::dump()
{
    name_array_t services, servers;
    bool_array_t active;
    mach_msg_type_number_t nServices, nServers, nActive;
    check(bootstrap_info(mPort, &services, &nServices,
        &servers, &nServers, &active, &nActive));
    Port::dump();
    Debug::dump(" %d services\n", nServices);
    for (mach_msg_type_number_t n = 0; n < nServices; n++)
        Debug::dump("%s\n", services[n]);
}

#endif //DEBUGDUMP


} // end namespace MachPlusPlus
} // end namespace Security
