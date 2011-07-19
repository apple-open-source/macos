/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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
// kq++ - kqueue/kevent interface
//
#include <security_utilities/kq++.h>

namespace Security {
namespace UnixPlusPlus {


KQueue::KQueue()
{
	UnixError::check(mQueue = ::kqueue());
}

KQueue::~KQueue()
{
	UnixError::check(::close(mQueue));
}


unsigned KQueue::operator () (const KEvent *updates, unsigned updateCount,
	KEvent *events, unsigned eventCount, const timespec *timeout)
{
	int rc = ::kevent64(mQueue, updates, updateCount, events, eventCount, 0, timeout);
	UnixError::check(rc);
	assert(rc >= 0);
	return rc;
}


void KQueue::update(const KEvent &event, unsigned flags)
{
	KEvent ev = event;
	ev.flags = flags;
	(*this)(&event, 1, NULL, NULL);
}

bool KQueue::receive(KEvent &event, const timespec *timeout)
{
	return (*this)(&event, 1, timeout) > 0;
}


} // end namespace UnixPlusPlus
} // end namespace Security
