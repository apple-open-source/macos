/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_CFMACHPP
#define _H_CFMACHPP

#include <security_utilities/mach++.h>
#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFRunLoop.h>


namespace Security {
namespace MachPlusPlus {


//
// A self-receiving MachPlusPlus::Message.
// Data is delivered through the standard CFRunLoop of the current thread.
// Note that CFAutoPort does NOT own the Port; you must release it yourself
// if you're done with it.
//
class CFAutoPort : public Port {
public:
	CFAutoPort();	// lazily allocates port later
	CFAutoPort(mach_port_t port); // use this port (must have receive right)
	virtual ~CFAutoPort();
	
	void enable();
	void disable();
	
	virtual void receive(const Message &msg) = 0;
	
private:
	CFRef<CFMachPortRef> mPort;
	CFRef<CFRunLoopSourceRef> mSource;
	bool mEnabled;
	
	static void cfCallback(CFMachPortRef cfPort, void *msg, CFIndex size, void *context);
};



} // end namespace MachPlusPlus
} // end namespace Security

#endif //_H_MACHPP
