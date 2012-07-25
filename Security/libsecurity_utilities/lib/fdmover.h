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
// An FdMover object is a very specialized Socket:
// It must be bound to a UNIX domain address
//
#ifndef _H_FDMOVER
#define _H_FDMOVER

#include "ip++.h"
#include <vector>

using namespace UnixPlusPlus;


namespace Security {
namespace IPPlusPlus {


// an ordered list of file descriptors
typedef std::vector<FileDesc> FdVector;


//
// An FdMover - a specialized Socket for transferring file descriptors
// across UNIX domain sockets.
//
class FdMover : public Socket {
private:
	class Element : public cmsghdr {
	public:
		void *operator new (size_t base, size_t more);
		void operator delete (void *addr, size_t size);
		
		Element() { }
		Element(int level, int type);
		
		template <class T> T &payload()	{ return *reinterpret_cast<T *>(CMSG_DATA(this)); }
		size_t payloadSize() const		{ return cmsg_len - ((caddr_t)CMSG_DATA(this) - (caddr_t)this); }
	};

	class Message : public msghdr {
	public:
		Message(const void *data, size_t length);
		void set(Element *elem);
		Element *element() const			{ return (Element *)msg_control; }
		Element *next(Element *elem) const	{ return (Element *)CMSG_NXTHDR(this, elem); }
	
	public:
		IOVec iovec;
	};
	
public:
	FdMover() { }
	FdMover(Socket s) : Socket(s) { }

	size_t send(const void *data, size_t length, const FdVector &fds);
	size_t receive(void *data, size_t length, FdVector &fds);
	
private:
	
};


}	// end namespace IPPlusPlus
}	// end namespace Security


#endif //_H_FDMOVER
