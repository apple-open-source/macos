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
// tqueue.h -- timer queues
//
#ifndef _H_TQUEUE
#define _H_TQUEUE

#include <Security/utilities.h>
#include <Security/cssmalloc.h>
#include <Security/debugging.h>

#ifdef _CPP_CDSA_UTILITIES_TQUEUE
#pragma export on
#endif


namespace Security
{

//
// A TimerQueue is a container of elements that have relative "timer" positions.
// TimerQueues are concerned with shuffling these elements around as their "times"
// change, and with processing elements that fall off the front of the queue as
// "time" passes.
// We put "time" into quotes because nothing here really cares what kind of time
// you are playing with. It could be seconds, points scored, etc. The only requirement
// is that "time" doesn't ever flow backwards...
//
template <class Time>
class ScheduleQueue {
public:
	ScheduleQueue()					{ first.fwd = first.back = &first; }
	virtual ~ScheduleQueue()		{ }
	
public:
	class Event {
		friend class ScheduleQueue;
	public:
		Event() : mScheduled(false) { }
		~Event() { if (scheduled()) unschedule(); }
		
		void unschedule();
		
		Time when() const			{ return fireTime; }
		bool scheduled() const		{ return mScheduled; }
		
	private:
		Time fireTime;				// when will it happen?
		bool mScheduled;			// are we scheduled?
		Event *back, *fwd;			// doubly-linked interior list
		
		void putBefore(Event *ev)
		{ back = ev->back; fwd = ev; ev->back = back->fwd = this; mScheduled = true; }
	};
	
public:
	void schedule(Event *event, Time when);
	void unschedule(Event *event)
	{ event->unschedule(); }
	
	bool empty() const	{ return first.fwd == &first; }
	Time next() const	{ assert(!empty()); return first.fwd->fireTime; }
	
	Event *pop(Time now);

private:
	Event first;					// root of active timers list
};

template <class Time>
void ScheduleQueue<Time>::Event::unschedule()
{
	assert(mScheduled);
	back->fwd = fwd; fwd->back = back; 
	mScheduled = false;
	secdebug("schedq", "event %p unscheduled", this);
}

template <class Time>
inline void ScheduleQueue<Time>::schedule(Event *event, Time when)
{
	Event *ev = first.fwd;
	if (event->scheduled()) {
		if (when == event->fireTime)	// no change
			return;
		else if (when > event->fireTime)	// forward move
			ev = event->back;
		event->unschedule();
	}
	event->fireTime = when;
	// newly schedule the event
	for (; ev != &first; ev = ev->fwd)
		if (ev->fireTime > when) {
			event->putBefore(ev);
			return;
		}
	// hit the end-of-queue; put at end
	event->putBefore(&first);
	secdebug("schedq", "event %p set for %.3f", event, double(when));
}

template <class Time>
inline typename ScheduleQueue<Time>::Event *ScheduleQueue<Time>::pop(Time now)
{
	if (!empty()) {
		Event *top = first.fwd;
		if (top->fireTime <= now) {
			top->unschedule();
			secdebug("schedq", "event %p delivered at %.3f", top, double(now));
			return top;
		}
	}
	return NULL;
}

} // end namespace Security

#ifdef _CPP_CDSA_UTILITIES_TQUEUE
#pragma export off
#endif

#endif // _H_TQUEUE
