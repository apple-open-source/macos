/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// cfmach++ - a marriage of CoreFoundation with Mach/C++
//
#include <security_utilities/cfmach++.h>


namespace Security {
namespace MachPlusPlus {


//
// Construct CFAutoPorts
//
CFAutoPort::CFAutoPort()
	: mEnabled(false)
{ }

CFAutoPort::CFAutoPort(mach_port_t p)
	: Port(p), mEnabled(false)
{ }


//
// On destruction, make sure we're disengaged from the CFRunLoop
//
CFAutoPort::~CFAutoPort()
{
	disable();
}


//
// enable() will lazily allocate needed resources, then click into the runloop
//
void CFAutoPort::enable()
{
	if (!mEnabled) {
		if (!*this)
			allocate();
		if (!mPort) {
			// first-time creation of CF resources
			CFMachPortContext ctx = { 1, this, NULL, NULL, NULL };
			mPort = CFMachPortCreateWithPort(NULL, port(), cfCallback, &ctx, NULL);
			mSource = CFMachPortCreateRunLoopSource(NULL, mPort, 10);
			if (!mSource || !mPort)
				CFError::throwMe();		// CF won't tell us why...
		}
		CFRunLoopAddSource(CFRunLoopGetCurrent(), mSource, kCFRunLoopDefaultMode);
		mEnabled = true;
		secdebug("autoport", "%p enabled", this);
	}
}


//
// Disable() just removes us from the runloop. All the other resources stay
// around, ready to be re-enable()d.
//
void CFAutoPort::disable()
{
	if (mEnabled) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mSource, kCFRunLoopDefaultMode);
		mEnabled = false;
		secdebug("autoport", "%p disabled", this);
	}
}


//
// The CF-sponsored port callback.
// We pass this to our receive() virtual and eat all exceptions.
//
void CFAutoPort::cfCallback(CFMachPortRef cfPort, void *msg, CFIndex size, void *context)
{
	Message message(msg, size);
	try {
		reinterpret_cast<CFAutoPort *>(context)->receive(message);
	} catch (...) {
		secdebug("autoport", "%p receive handler failed with exception", context);
	}
}


} // end namespace MachPlusPlus
} // end namespace Security
