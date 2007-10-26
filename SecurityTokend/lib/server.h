/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */
#ifndef _H_TOKEND_SERVER
#define _H_TOKEND_SERVER


//
// server - master server loop for tokend
//
#include "SecTokend.h"
#include <security_utilities/logging.h>
#include <security_utilities/pcsc++.h>
#include <security_utilities/machserver.h>

namespace Security {
namespace Tokend {


//
// The server class that drives this tokend
//
class Server : public MachPlusPlus::MachServer, public SecTokendCallbacks {
public:
	int operator() (int argc, const char *argv[], SecTokendCallbackFlags flags);

	const char *readerName() const { return mReaderName; }
	const PCSC::ReaderState &startupReaderState() const { return mStartupReaderState; }
	
	const char *tokenUid() const { return mTokenUid.c_str(); }
	void tokenUid(const char *uid) { mTokenUid = uid; }
	
	void releaseWhenDone(void *) { }	//@@@ unimplemented (share with ss side)
	
	SecTokendCallbacks &callbacks() { return static_cast<SecTokendCallbacks &>(*this); }
	
	void termination(uint32 reason, uint32 options) __attribute__((noreturn));
	
protected:
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	
private:
	const char *mReaderName;
	PCSC::ReaderState mStartupReaderState;
	std::string mTokenUid;
};


//
// The server singleton
//
extern Server *server;


}	// namespace Tokend
}	// namespace Security

#endif //_H_TOKEND_SERVER
