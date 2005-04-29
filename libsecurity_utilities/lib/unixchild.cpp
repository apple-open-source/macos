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
// unixchild - low-level UNIX process child management
//
#include "unixchild.h"
#include <security_utilities/debugging.h>


namespace Security {
namespace UnixPlusPlus {


//
// All our globals are in a ModuleNexus, for that special lazy-init goodness
//
ModuleNexus<Child::Children> Child::mChildren;


//
// Make and break Children
//
Child::Child()
	: mState(unborn), mPid(0), mStatus(0)
{
}


Child::~Child()
{
	assert(mState != alive);	// not allowed by protocol
}


//
// Take a Child object that is not alive (i.e. is either unborn or dead),
// and reset it to unborn, so you can fork() it again.
// This call forgets everything all about the previous process.
//
void Child::reset()
{
	switch (mState) {
	case alive:
		assert(false);		// bad boy; can't do that
	case unborn:
		break;				// s'okay
	default:
		secdebug("unixchild", "%p reset (from state %d)", this, mState);
		mState = unborn;
		mPid = 0;
		mStatus = 0;
		break;
	}
}


//
// Global inquiries and setup
//
void Child::sharedChildren(bool s)
{
	StLock<Mutex> _(mChildren());
	mChildren().shared = s;
}

bool Child::sharedChildren()
{
	StLock<Mutex> _(mChildren());
	return mChildren().shared;
}


//
// Common kill code.
// Requires caller to hold mChildren() lock.
//
void Child::tryKill(int signal)
{
	assert(mState == alive);	// ... or don't bother us
	secdebug("unixchild", "%p (pid %d) sending signal(%d)", this, pid(), signal);
	if (::kill(pid(), signal))
		switch (errno) {
		case ESRCH: // someone else reaped ths child; or things are just wacky
			secdebug("unixchild", "%p (pid %d) has disappeared!", this, pid());
			mState = invalid;
			mChildren().erase(pid());
			// fall through
		default:
			UnixError::throwMe();
		}
}


//
// Send a signal to the Child.
// This will succeed (and do nothing) if the Child is not alive.
//
void Child::kill(int signal)
{
	StLock<Mutex> _(mChildren());
	if (mState == alive)
		tryKill(signal);
	else
		secdebug("unixchild", "%p (pid %d) not alive; cannot send signal %d",
			this, pid(), signal);
}


//
// Kill with prejudice.
// This will make a serious attempt to *synchronously* kill the process before
// returning. If that doesn't work for some reason, we throw(!).
// This is one thing you can do in the destructor of your subclass to legally
// dispose of your Child's process.
//
void Child::kill()
{
	StLock<Mutex> _(mChildren());
	if (mState == alive) {
		secdebug("unixchild", "kill-with-prejudice is not implemented");
		assert(false);
	} else
		secdebug("unixchild", "%p (pid %d) not alive; ignoring request to kill it", this, pid());
}


//
// Take a living child and cut it loose. This sets its state to abandoned
// and removes it from the child registry.
// This is one thing you can do in the destructor of your subclass to legally
// dispose of your child's process.
//
void Child::abandon()
{
	StLock<Mutex> _(mChildren());
	if (mState == alive) {
		secdebug("unixchild", "%p (pid %d) abandoned", this, pid());
		mState = abandoned;
		mChildren().erase(pid());
	} else {
		secdebug("unixchild", "%p (pid %d) is not alive; abandon() ignored",
			this, pid());
	}
}


//
// Forensic examination of the Child's cadaver.
// Not interlocked because you have to check for state() == dead first,
// and these values are const ever after.
//
int Child::waitStatus() const
{
	assert(mState == dead);
	return mStatus;
}

bool Child::bySignal() const
{
	assert(mState == dead);
	return WIFSIGNALED(mStatus);
}

int Child::exitCode() const
{
	assert(mState == dead);
	assert(WIFEXITED(mStatus));
	return WEXITSTATUS(mStatus);
}

int Child::exitSignal() const
{
	assert(mState == dead);
	assert(WIFSIGNALED(mStatus));
	return WTERMSIG(mStatus);
}

bool Child::coreDumped() const
{
	assert(mState == dead);
	assert(WIFSIGNALED(mStatus));
	return WCOREDUMP(mStatus);
}


//
// Find a child in the child map, by pid
// This will only find live children, and return NULL for all others.
//
Child *Child::findGeneric(pid_t pid)
{
	StLock<Mutex> _(mChildren());
	Children::iterator it = mChildren().find(pid);
	if (it == mChildren().end())
		return NULL;
	else
		return it->second;
}


//
// Do the actual fork job.
// At this layer, the client side does nothing but run childAction(). Any plumbing
// or cleanup is up to that function (which runs in the child) and the caller (after
// fork() returns). If childAction() returns at all, we will call exit(1) to get
// rid of the child.
//
void Child::fork()
{
	static const unsigned maxDelay = 30;	// seconds increment, i.e. 5 retries

	assert(mState == unborn);
	for (unsigned delay = 1; ;) {
		StLock<Mutex> _(mChildren());
		switch (pid_t pid = ::fork()) {
		case -1:	// fork failed
			switch (errno) {
			case EINTR:
				secdebug("unixchild", "%p fork EINTR; retrying", this);
				continue;	// no problem
			case EAGAIN:
				if (delay < maxDelay) {
					secdebug("unixchild", "%p fork EAGAIN; delaying %d seconds",
						this, delay);
					sleep(delay);
					delay *= 2;
					continue;
				}
				// fall through
			default:
				UnixError::throwMe();
			}
			assert(false);	// unreached

		case 0:		// child
			//@@@ bother to clean child map?
			secdebug("unixchild", "%p (child pid %d) running child action",
				this, getpid());
			secdelay("/tmp/delay/unixchild");
			try {
				this->childAction();
				secdebug("unixchild", "%p (pid %d) child action returned; exiting",
					this, getpid());
			} catch (...) {
				secdebug("unixchild", "%p (pid %d) child action had uncaught exception",
					this, getpid());
			}
			_exit(1);

		default:	// parent
			mState = alive;
			mPid = pid;
			mChildren().insert(make_pair(pid, this));
			break;
		}
		break;
	}

	// run parent action after releasing the lock
	secdebug("unixchild", "%p (parent) running parent action", this);
	this->parentAction();
}


//
// Perform an idempotent check for dead children, as per the UNIX wait() system calls.
// This can be called at any time, and will reap all children that have died since
// last time. The obvious time to call this is after a SIGCHLD has been received;
// however signal dispatch is so - uh, interesting - in UNIX that we don't even try
// to deal with it at this level. Suffice to say that calling checkChildren directly
// from within a signal handler is NOT generally safe due to locking constraints.
//
// If the shared() flag is on, we explicitly poll each child known to be recently
// alive. That is less efficient than reaping any and all, but leaves any children
// alone that someone else may have created without our knowledge. The default is
// not shared(), which will reap (and discard) any unrelated children without letting
// the caller know about it.
//
void Child::checkChildren()
{
	StLock<Mutex> _(mChildren());
	if (mChildren().shared) {
		for (Children::iterator it = mChildren().begin(); it != mChildren().end(); it++) {
			Child *child = it->second;
			assert(child->state() == alive);
			secdebug("unixchild", "checking %p (pid %d)", child, child->pid());
			int status;
		  again:
			switch (IFDEBUG(pid_t pid =) ::wait4(child->pid(), &status, WNOHANG, NULL)) {
			case pid_t(-1):
				switch (errno) {
				case EINTR:
					goto again;		// retry THIS child
				case ECHILD:
					secdebug("unixchild", "%p (pid=%d) unknown to kernel", child, child->pid());
					child->mState = invalid;
					mChildren().erase(it);
					break;
				default:
					UnixError::throwMe();
					break;	// placebo
				}
				break;
			case 0:
				break;		// not ready
			default:
				assert(pid == child->pid());
				child->bury(status);
				break;
			}
		}
	} else if (!mChildren().empty()) {
		int status;
		while (pid_t pid = ::wait4(0, &status, WNOHANG, NULL)) {
			secdebug("unixchild", "universal child check (%ld children alive)", mChildren().size());
			switch (pid) {
			case pid_t(-1):
				switch (errno) {
				case EINTR:
					secdebug("unixchild", "EINTR on wait4; retrying");
					continue;	// benign, but retry the wait()
				case ECHILD:
					// Should not normally happen (there *is* a child around),
					// but gets returned anyway if the child is stopped in the debugger.
					// Treat like a zero return (no children ready to be buried).
					secdebug("unixchild", "ECHILD with filled nursery (ignored)");
					return;
				default:
					UnixError::throwMe();
				}
				break;
			default:
				if (Child *child = mChildren()[pid])
					child->bury(status);
				else
					secdebug("unixchild", "reaping feral child pid=%d", pid);
				if (mChildren().empty())
					return;		// none left
				break;
			}
		}
	} else {
		secdebug("unixchild", "spurious checkChildren (the nursery is empty)");
	}
}


//
// Perform the canonical last rites for a formerly alive child.
// Requires master lock held throughout.
//
void Child::bury(int status)
{
	assert(mState == alive);
	mState = dead;
	mStatus = status;
	mChildren().erase(mPid);
#if !defined(NDEBUG)
	if (bySignal())
		secdebug("unixchild", "%p (pid %d) died by signal %d%s",
			this, mPid, exitSignal(),
			coreDumped() ? " and dumped core" : "");
	else
		secdebug("unixchild", "%p (pid %d) died by exit(%d)",
			this, mPid, exitCode());
#endif //NDEBUG
	this->dying();
}


//
// Default hooks
//
void Child::parentAction()
{ /* nothing */ }

void Child::dying()
{ /* nothing */ }


}	// end namespace IPPlusPlus
}	// end namespace Security
