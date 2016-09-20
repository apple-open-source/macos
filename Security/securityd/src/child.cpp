/*
 * Copyright (c) 2004,2007 Apple Inc. All Rights Reserved.
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
// child - track a single child process and its belongings
//
#include "child.h"
#include "dtrace.h"
#include <security_utilities/debugging.h>


//
// We use a static Mutex to coordinate checkin
//
Mutex ServerChild::mCheckinLock;


//
// Make and break ServerChildren
//
ServerChild::ServerChild()
	: mCheckinCond(mCheckinLock)
{
}


//
// If the ServerChild is destroyed, kill its process, nice or hard.
//
// In case you wonder about the tango below, it's making sure we
// get to "It's dead, Jim" with the minimum number of checkChildren()
// calls while still working correctly if this is the only thread alive.
//
//@@@ We *could* define a "soft shutdown" MIG message to send to all
//@@@ ServerChildren in this situation.
//
ServerChild::~ServerChild()
{
	mServicePort.destroy();
	
	if (state() == alive) {
		this->kill(SIGTERM);		// shoot it once
		checkChildren();			// check for quick death
		if (state() == alive) {
			usleep(300000);			// give it some grace
			if (state() == alive) {	// could have been reaped by another thread
				checkChildren();	// check again
				if (state() == alive) {	// it... just... won't... die...
					this->kill(SIGKILL); // take THAT!
					checkChildren();
					if (state() == alive) // stuck zombie
						abandon();	// leave the body behind
				}
			}
		}
	}
}


//
// Parent action during fork: wait until ready or dead, then return
//
void ServerChild::parentAction()
{
	// wait for either checkin or (premature) death
	secinfo("serverchild", "%p (pid %d) waiting for checkin", this, pid());
	StLock<Mutex> _(mCheckinLock);
	while (!ready() && state() == alive)
		mCheckinCond.wait();

	// so what happened?
	if (state() == dead) {
		// our child died
		secinfo("serverchild", "%p (pid %d) died before checking in", this, pid());
	} else if (ready()) {
		// child has checked in and is ready for service
		secinfo("serverchild", "%p (pid %d) ready for service on port %d",
			this, pid(), mServicePort.port());
	} else
		assert(false);		// how did we ever get here?!
}


//
// Death action during fork: release the waiting creator thread, if any
//
void ServerChild::dying()
{
	secinfo("serverchild", "%p [%d] is dead; resuming parent thread (if any)", this, this->pid());
	mCheckinCond.signal();
}


void ServerChild::checkIn(Port servicePort, pid_t pid)
{
	if (ServerChild *child = Child::find<ServerChild>(pid)) {
		// Child was alive when last seen. Store service port and signal parent thread
		{
			StLock<Mutex> _(mCheckinLock);
			child->mServicePort = servicePort;
			servicePort.modRefs(MACH_PORT_RIGHT_SEND, +1);	// retain send right
			secinfo("serverchild", "%p (pid %d) checking in; resuming parent thread",
				child, pid);
		}
		child->mCheckinCond.signal();
	} else {
		// Child has died; is wrong kind; or spurious checkin.
		// If it was a proper child, death notifications will wake up the parent thread
		secinfo("serverchild", "pid %d not in child set; checkin ignored", pid);
	}
}
