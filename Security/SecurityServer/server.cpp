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
// server - the actual SecurityServer server object
//
#include "server.h"
#include "session.h"
#include "acls.h"
#include <mach/mach_error.h>

using namespace MachPlusPlus;


//
// Construct the server object
//
Server::Server(Authority &myAuthority, const char *bootstrapName)
  : MachServer(bootstrapName),
    mCurrentConnection(false),
    mCSPModule(gGuidAppleCSP, mCssm), mCSP(mCSPModule),
    mAuthority(myAuthority)
{
    // engage the subsidiary port handler for sleep notifications
    add(sleepWatcher);
}


//
// Clean up the server object
//
Server::~Server()
{
    //@@@ more later
}


//
// Locate a connection by reply port and make it the current connection
// of this thread. The connection will be marked busy, and can be accessed
// by calling Server::connection() [no argument] until it is released by
// calling Connection::endWork().
//
Connection &Server::connection(mach_port_t port)
{
	Server &server = active();
	StLock<Mutex> _(server.lock);
	if (Connection *conn = server.connections[port]) {
		active().mCurrentConnection = conn;
		conn->beginWork();
		return *conn;
	}
	// unknown client port -- could be a hack attempt
	CssmError::throwMe(CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
}

Connection &Server::connection(bool tolerant)
{
	Connection *conn = active().mCurrentConnection;
	assert(conn);	// have to have one
	if (!tolerant)
		conn->checkWork();
	return *conn;
}

void Server::requestComplete()
{
	// note: there may not be an active connection if connection setup failed
	if (Connection *conn = active().mCurrentConnection) {
		if (conn->endWork())
			delete conn;
		active().mCurrentConnection = NULL;
	}
}


//
// Locate an ACL bearer (database or key) by handle
//
SecurityServerAcl &Server::aclBearer(AclKind kind, CSSM_HANDLE handle)
{
	SecurityServerAcl &bearer = findHandle<SecurityServerAcl>(handle);
	if (kind != bearer.kind())
		CssmError::throwMe(CSSMERR_CSSM_INVALID_HANDLE_USAGE);
	return bearer;
}


//
// Run the server. This will not return until the server is forced to exit.
//
void Server::run()
{
	MachServer::run(0x10000,
        MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
        MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER));
}


//
// The server run-loop function
//
boolean_t ucsp_server(mach_msg_header_t *, mach_msg_header_t *);

boolean_t Server::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	return ucsp_server(in, out);
}


//
// Set up a new Connection. This establishes the environment (process et al) as needed
// and registers a properly initialized Connection object to run with.
//
void Server::setupConnection(Port replyPort, Port taskPort,
    const security_token_t &securityToken, const char *identity)
{
	// first, make or find the process based on task port
	StLock<Mutex> _(lock);
	Process * &proc = processes[taskPort];
	if (proc == NULL) {
		proc = new Process(taskPort, identity, securityToken.val[0], securityToken.val[1]);
		notifyIfDead(taskPort);
	}

	// now, establish a connection and register it in the server
	Connection *connection = new Connection(*proc, replyPort);
	if (connections[replyPort])	// malicious re-entry attempt?
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ error code? (client error)
	connections[replyPort] = connection;
	notifyIfDead(replyPort);
}


//
// Synchronously end a Connection.
// This is due to a request from the client, so no thread races are possible.
//
void Server::endConnection(Port replyPort)
{
	StLock<Mutex> _(lock);
	Connection *connection = connections[replyPort];
	assert(connection);
	connections.erase(replyPort);
	connection->terminate();
	delete connection;
}


//
// Take an existing Connection/Process combo. Tear them down even though
// the client-side thread/process is still alive and construct new ones in their place.
// This is a high-wire act with a frayed net. We use it ONLY to deal with clients
// who change their Session (by changing their bootstrap subset port) in mid-stream.
// In other words, this is a hack that the client would be well advised to avoid.
// (Avoid it by calling SessionCreate before calling any other Security interfaces in
// the process's life.)
//
Process *Server::resetConnection()
{
    Connection *oldConnection = mCurrentConnection;
    Process *oldProcess = &oldConnection->process;
    debug("SS", "reset process %p connection %p for session switch",
        oldProcess, oldConnection);

    Port replyPort = oldConnection->clientPort();
    
    oldConnection->endWork();
    oldConnection->abort(true);
    delete oldConnection;
    
    oldProcess->kill();
    
    Process * &proc = processes[oldProcess->taskPort()];
    proc = new Process(*oldProcess);
    delete oldProcess;
    
    Connection *connection = new Connection(*proc, replyPort);
    connections[replyPort] = connection;
    mCurrentConnection = connection;
    connection->beginWork();
    
    return proc;
}


//
// Handling dead-port notifications.
// This receives DPNs for all kinds of ports we're interested in.
//
void Server::notifyDeadName(Port port)
{
	StLock<Mutex> _(lock);
    
    // is it a connection?
    ConnectionMap::iterator conIt = connections.find(port);
    if (conIt != connections.end()) {
        Connection *connection = conIt->second;
		if (connection->abort())
			delete connection;
		connections.erase(conIt);
        return;
    }
    
    // is it a process?
    ProcessMap::iterator procIt = processes.find(port);
    if (procIt != processes.end()) {
        Process *process = procIt->second;
		if (process->kill())
			delete process;
		processes.erase(procIt);
        return;
    }
    
    // well, it better be a session
    Session::eliminate(Bootstrap(port));
}


//
// Notifier for system sleep events
//
void Server::SleepWatcher::systemWillSleep()
{
    debug("SS", "sleep notification received");
    Database::lockAllDatabases(true);
}


//
// Return the primary Cryptographic Service Provider.
// This will be lazily loaded when it is first requested.
//
CssmClient::CSP &Server::getCsp()
{
	//@@@ not officially pthread-kosher. Use a ModuleNexus here?
    if (!mCssm->isActive()) {
        // first time load
        //@@@ should we abort the server if this fails? What point continuing?
		StLock<Mutex> _(lock);
        debug("SS", "CSSM initializing");
        mCssm->init();
        mCSP->attach();
        char guids[Guid::stringRepLength+1];
        IFDEBUG(debug("SS", "CSSM ready with CSP %s", mCSP->guid().toString(guids)));
    }
    return mCSP;
}
