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
#ifndef _H_KQPP
#define _H_KQPP

#include <security_utilities/unix++.h>
#include <sys/event.h>

namespace Security {
namespace UnixPlusPlus {


class KEvent;


class KQueue {
public:
	KQueue();
	virtual ~KQueue();

	unsigned operator () (const KEvent *updates, unsigned updateCount, KEvent *events, unsigned eventCount,
		const timespec *timeout = NULL);
	unsigned operator () (KEvent *events, unsigned eventCount, const timespec *timeout = NULL)
		{ return operator () (NULL, NULL, events, eventCount, timeout); }
	
	void update(const KEvent &event, unsigned flags = EV_ADD);
	bool receive(KEvent &event, const timespec *timeout = NULL);

private:
	int mQueue;
};


class KEvent : public PodWrapper<KEvent, kevent64_s> {
public:
	KEvent() { clearPod(); }
	KEvent(int16_t filt) { clearPod(); this->filter = filt; }
	KEvent(int16_t filt, uint64_t id, uint32_t ffl = 0)
//		{ clearPod(); this->ident = id; this->filter = filt; this->fflags = ffl; }
		{ EV_SET64(this, id, filt, 0, ffl, 0, 0, 0, 0); }

	void addTo(KQueue &kq, unsigned flags = 0)
		{ this->flags = EV_ADD | flags; kq.update(*this); }
	void removeFrom(KQueue &kq, unsigned flags = 0)
		{ this->flags = EV_DELETE | flags; kq.update(*this); }
	void enable(KQueue &kq, unsigned flags = 0)
		{ this->flags = EV_ENABLE | flags; kq.update(*this); }
	void disable(KQueue &kq, unsigned flags = 0)
		{ this->flags = EV_DISABLE | flags; kq.update(*this); }
};


namespace Event {


class Vnode : public KEvent {
public:
	Vnode() : KEvent(EVFILT_VNODE) { }
	Vnode(int fd, uint32_t flags) : KEvent(EVFILT_VNODE, fd, flags) { }
	
	int fd() const { return (int)this->ident; }
};

} // namespace Event


} // end namespace UnixPlusPlus
} // end namespace Security

#endif //_H_KQPP
