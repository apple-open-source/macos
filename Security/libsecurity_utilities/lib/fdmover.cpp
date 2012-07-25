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
// fdmover - send/receive file descriptors over a UNIX domain socket connection
//
#include "fdmover.h"
#include <iterator>
#include <security_utilities/debugging.h>


namespace Security {
namespace IPPlusPlus {


void *FdMover::Element::operator new (size_t base, size_t more)
{
	Element *element = (Element *)::malloc(CMSG_SPACE(more));
	element->cmsg_len = CMSG_LEN(more);
	return element;
}

void FdMover::Element::operator delete (void *data, size_t base)
{
	::free(data);
}

FdMover::Element::Element(int level, int type)
{
	cmsg_level = level;
	cmsg_type = type;
}


FdMover::Message::Message(const void *data, size_t length)
	: iovec(data, length)
{
	msg_name = NULL;
	msg_namelen = 0;
	msg_iov = &iovec;
	msg_iovlen = 1;
	msg_control = NULL;
	msg_controllen = 0;
	msg_flags = 0;
}

void FdMover::Message::set(Element *elem)
{
	msg_control = (caddr_t)elem;
	msg_controllen = elem->cmsg_len;
}


size_t FdMover::send(const void *data, size_t length, const FdVector &fds)
{
	auto_ptr<Element> elem(new (fds.size() * sizeof(int)) Element (SOL_SOCKET, SCM_RIGHTS));
	copy(fds.begin(), fds.end(), &elem.get()->payload<int>());
	Message msg(data, length);
	msg.set(elem.get());
	ssize_t rc = ::sendmsg(fd(), &msg, 0);
	checkError(rc);
	return rc;
}


size_t FdMover::receive(void *data, size_t length, FdVector &fds)
{
	static const int maxFds = 20;	// arbitrary limit
	Message msg(data, length);
	auto_ptr<Element> elem(new (maxFds * sizeof(int)) Element);
	msg.set(elem.get());
	ssize_t rc = ::recvmsg(fd(), &msg, 0);
	checkError(rc);
	unsigned count = elem.get()->payloadSize() / sizeof(int);
	FdVector result;
	copy(&elem.get()->payload<int>(), &elem.get()->payload<int>() + count, back_inserter(result));
	swap(fds, result);
	return rc;
}


}	// end namespace IPPlusPlus
}	// end namespace Security
