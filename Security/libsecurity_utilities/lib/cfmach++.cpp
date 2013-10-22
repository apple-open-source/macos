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
	
	// invalidate everything
	if (mPort)
	{
		CFMachPortInvalidate(mPort);
		CFRunLoopSourceInvalidate(mSource);
	}
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
			CFMachPortRef machPort = CFMachPortCreateWithPort(NULL, port(), cfCallback, &ctx, NULL);
			if (machPort != NULL)
			{
				// using take here because "assignment" causes an extra retain, which will make the
				// CF objects leak when this data structure goes away.
				mPort.take(machPort);
				
				CFRunLoopSourceRef sr = CFMachPortCreateRunLoopSource(NULL, mPort, 10);
				mSource.take(sr);
			}
			if (!mPort || !mSource)
				CFError::throwMe();		// CF won't tell us why...
		}
		CFRunLoopAddSource(CFRunLoopGetCurrent(), mSource, kCFRunLoopCommonModes);
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
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mSource, kCFRunLoopCommonModes);
		mEnabled = false;
		secdebug("autoport", "%p disabled", this);
	}
}


//
// The CF-sponsored port callback.
// We pass this to our receive() virtual and eat all exceptions.
//
static int gNumTimesCalled = 0;

void CFAutoPort::cfCallback(CFMachPortRef cfPort, void *msg, CFIndex size, void *context)
{
	++gNumTimesCalled;
	secdebug("adhoc", "Callback was called %d times.", gNumTimesCalled);

#warning Cast to mach_msg_size_t may loose precision
	Message message(msg, (mach_msg_size_t)size);
	try {
		reinterpret_cast<CFAutoPort *>(context)->receive(message);
	} catch (...) {
		secdebug("autoport", "%p receive handler failed with exception", context);
	}
}


} // end namespace MachPlusPlus
} // end namespace Security
