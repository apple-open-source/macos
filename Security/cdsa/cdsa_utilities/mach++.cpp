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


Error::Error(kern_return_t err) : error(err)
{
}

Error::~Error()
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
	if (status != KERN_SUCCESS) {
#if !defined(NDEBUG)
		// issue a diagnostic log for any discovered mach-level error
		switch (status) {
		case BOOTSTRAP_UNKNOWN_SERVICE:
			debug("error", "mach error: BOOTSTRAP_UNKNOWN_SERVICE"); break;
		case BOOTSTRAP_NAME_IN_USE:	
			debug("error", "mach error: BOOTSTRAP_NAME_IN_USE"); break;
		case BOOTSTRAP_NOT_PRIVILEGED:
			debug("error", "mach error: BOOTSTRAP_NOT_PRIVILEGED"); break;
		case BOOTSTRAP_SERVICE_ACTIVE:
			debug("error", "mach error: BOOTSTRAP_SERVICE_ACTIVE"); break;
		default:
			debug("error", "mach error: %s (%d)", mach_error_string(status), status); break;
		}
#endif NDEBUG
		Error::throwMe(status);
	}
}

void Error::throwMe(kern_return_t err) { throw Error(err); }

//
// Port functions
//
mach_port_urefs_t Port::getRefs(mach_port_right_t right)
{
	mach_port_urefs_t count;
	check(::mach_port_get_refs(self(), mPort, right, &count));
	return count;
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
	kern_return_t err = ::bootstrap_check_in(mPort, makeName(name), &port);
    if (err == BOOTSTRAP_UNKNOWN_SERVICE || err == BOOTSTRAP_NOT_PRIVILEGED)
        return 0;
    check(err);
	return port;
}

void Bootstrap::registerAs(mach_port_t port, const char *name) const
{
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
	if (!mPort)
	{
		allocate();
		insertRight(MACH_MSG_TYPE_MAKE_SEND);
		bootstrap.registerAs(mPort, name);
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
    debug("StBoot", "bootstrap for %d switched to %d", mTask.port(), newBoot.port());
}

StBootstrap::~StBootstrap()
{
    mTask.bootstrap(mOldBoot);
    debug("StBoot", "bootstrap for %d returned to %d", mTask.port(), mOldBoot.port());
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


void Message::send(mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    check(mach_msg_overwrite_trap(*this,
        options | MACH_SEND_MSG,
        length(),
        0, MACH_PORT_NULL,
        timeout, notify,
        NULL, 0));
}

void Message::receive(mach_port_t receivePort,
    mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    check(mach_msg_overwrite_trap(*this,
        options | MACH_RCV_MSG,
        length(),
        mSize, receivePort,
        timeout, notify,
        NULL, 0));
}

void Message::sendReceive(mach_port_t receivePort,
    mach_msg_option_t options,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify)
{
    check(mach_msg_overwrite_trap(*this,
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
	fprintf(stderr, "[%s(%d)", descr ? descr : "port", mPort);
	mach_port_type_t type;
	kern_return_t err = mach_port_type(self(), mPort, &type);
	if (err != KERN_SUCCESS) {
		fprintf(stderr, " !%s", mach_error_string(err));
	} else {
		if (type & MACH_PORT_TYPE_SEND) fprintf(stderr, " send(%d)", getRefs(MACH_PORT_RIGHT_SEND));
		if (type & MACH_PORT_TYPE_RECEIVE) fprintf(stderr, " rcv");
		if (type & MACH_PORT_TYPE_SEND_ONCE) fprintf(stderr, " once");
		if (type & MACH_PORT_TYPE_PORT_SET) fprintf(stderr, " set");
		if (type & MACH_PORT_TYPE_DEAD_NAME) fprintf(stderr, " dead");
		if (type & MACH_PORT_TYPE_DNREQUEST) fprintf(stderr, " dnreq");
	}
	fprintf(stderr, "]\n");
}


void Bootstrap::dump()
{
    name_array_t services, servers;
    bool_array_t active;
    mach_msg_type_number_t nServices, nServers, nActive;
    check(bootstrap_info(mPort, &services, &nServices,
        &servers, &nServers, &active, &nActive));
    fprintf(stderr, "[port %d] %d services\n", mPort, nServices);
    for (mach_msg_type_number_t n = 0; n < nServices; n++)
        fprintf(stderr, "%s\n", services[n]);
}

#endif //DEBUGDUMP


} // end namespace MachPlusPlus
} // end namespace Security
