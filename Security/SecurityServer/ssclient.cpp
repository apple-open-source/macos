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
// ssclient - SecurityServer client interface library
//
#include "sstransit.h"
#include <servers/netname.h>
#include <Security/debugging.h>

using MachPlusPlus::check;
using MachPlusPlus::Bootstrap;
using CodeSigning::OSXCode;


namespace Security
{

namespace SecurityServer
{

//
// The process-global object
//
ModuleNexus<ClientSession::Global> ClientSession::mGlobal;


//
// Construct a client session
//
ClientSession::ClientSession(CssmAllocator &std, CssmAllocator &rtn)
: internalAllocator(std), returnAllocator(rtn)
{ }


//
// Destroy a session
//
ClientSession::~ClientSession()
{ }


//
// Activate a session: This connects to the SecurityServer and executes
// application authentication
//
void ClientSession::activate()
{
	Global &global = mGlobal();
    Thread &thread = global.thread();
    if (!thread) {
		// first time for this thread - use abbreviated registration
		IPCN(ucsp_client_setup(UCSP_ARGS, mach_task_self(), ""));
        thread.registered = true;
        global.serverPort.requestNotify(thread.replyPort, MACH_NOTIFY_DEAD_NAME, true);
        debug("SSclnt", "Thread registered with SecurityServer");
	}
}

// Caution: you can't use mGlobal() inside Global::Global (deadlock)
ClientSession::Global::Global()
{
    debug("SSclnt", "Initial process setup");

    // find server port
    serverPort = Bootstrap().lookup("SecurityServer");
    
    // send identification/setup message
    string extForm;
    try {
        myself = OSXCode::main();
        extForm = myself->encode();
        debug("SSclnt", "my OSXCode extForm=%s", extForm.c_str());
    } catch (...) {
        myself = NULL;
        // leave extForm empty
        debug("SSclnt", "failed to obtain my own OSXCode");
    }
    // cannot use UCSP_ARGS here because it uses mGlobal()
    IPCN(ucsp_client_setup(serverPort, mig_get_reply_port(), &rcode,
        mach_task_self(), extForm.c_str()));
    Thread &thread = this->thread();
    thread.registered = true;	// as a side-effect of setup call above
	serverPort.requestNotify(thread.replyPort, MACH_NOTIFY_DEAD_NAME, true);
    debug("SSclnt", "Process registered with SecurityServer");
}


//
// Terminate a session. This is called by the session destructor, or explicitly.
//
void ClientSession::terminate()
{
	// currently defunct
	debug("SSclnt", "ClientSession::terminate() call ignored");
}


} // end namespace SecurityServer

} // end namespace Security
