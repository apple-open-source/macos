/*
 * Copyright (c) 2000-2004,2009 Apple Inc. All Rights Reserved.
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
// server - securityd main server object
//
#include <securityd_client/ucsp.h>	// MIG ucsp service
#include "self.h"					// MIG self service
#include <security_utilities/logging.h>
#include <security_cdsa_client/mdsclient.h>
#include "server.h"
#include "session.h"
#include "acls.h"
#include "notifications.h"
#include "child.h"
#include <mach/mach_error.h>
#include <security_utilities/ccaudit.h>
#include "pcscmonitor.h"

#include "agentquery.h"


using namespace MachPlusPlus;

//
// Construct an Authority
//
Authority::Authority(const char *configFile)
: Authorization::Engine(configFile)
{
}

Authority::~Authority()
{
}


//
// Construct the server object
//
Server::Server(Authority &authority, CodeSignatures &signatures, const char *bootstrapName)
  : MachServer(bootstrapName),
    mBootstrapName(bootstrapName),
    mCSPModule(gGuidAppleCSP, mCssm), mCSP(mCSPModule),
    mAuthority(authority),
	mCodeSignatures(signatures), 
	mAudit(geteuid(), getpid()),
	mVerbosity(0),
	mWaitForClients(true), mShuttingDown(false)
{
	// make me eternal (in the object mesh)
	ref();

	mAudit.registerSession();

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
Connection &Server::connection(mach_port_t port, audit_token_t &auditToken)
{
	Server &server = active();
	StLock<Mutex> _(server);
	Connection *conn = server.mConnections.get(port, CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
	active().mCurrentConnection() = conn;
	conn->beginWork(auditToken);
	return *conn;
}

Connection &Server::connection(bool tolerant)
{
	Connection *conn = active().mCurrentConnection();
	assert(conn);	// have to have one
	if (!tolerant)
		conn->checkWork();
	return *conn;
}

void Server::requestComplete(CSSM_RETURN &rcode)
{
	// note: there may not be an active connection if connection setup failed
	if (RefPointer<Connection> &conn = active().mCurrentConnection()) {
		conn->endWork(rcode);
		conn = NULL;
	}
	IFDUMPING("state", NodeCore::dumpAll());
}


//
// Shorthand for "current" process and session.
// This is the process and session for the current connection.
//
Process &Server::process()
{
	return connection().process();
}

Session &Server::session()
{
	return connection().process().session();
}

RefPointer<Key> Server::key(KeyHandle key)
{
	return U32HandleObject::findRef<Key>(key, CSSMERR_CSP_INVALID_KEY_REFERENCE);
}

RefPointer<Database> Server::database(DbHandle db)
{
	return find<Database>(db, CSSMERR_DL_INVALID_DB_HANDLE);
}

RefPointer<KeychainDatabase> Server::keychain(DbHandle db)
{
	return find<KeychainDatabase>(db, CSSMERR_DL_INVALID_DB_HANDLE);
}

RefPointer<Database> Server::optionalDatabase(DbHandle db, bool persistent)
{
	if (persistent && db != noDb)
		return database(db);
	else
		return &process().localStore();
}


//
// Locate an ACL bearer (database or key) by handle
// The handle might be used across IPC, so we clamp it accordingly
//
AclSource &Server::aclBearer(AclKind kind, U32HandleObject::Handle handle)
{
	AclSource &bearer = U32HandleObject::find<AclSource>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	if (kind != bearer.acl().aclKind())
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
        MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT));
}


//
// Handle thread overflow. MachServer will call this if it has hit its thread
// limit and yet still needs another thread.
//
void Server::threadLimitReached(UInt32 limit)
{
	Syslog::notice("securityd has reached its thread limit (%ld) - service deadlock is possible",
		limit);
}


//
// The primary server run-loop function.
// Invokes the MIG-generated main dispatch function (ucsp_server), as well
// as the self-send dispatch (self_server).
// For debug builds, look up request names in a MIG-generated table
// for better debug-log messages.
//
boolean_t ucsp_server(mach_msg_header_t *, mach_msg_header_t *);
boolean_t self_server(mach_msg_header_t *, mach_msg_header_t *);


boolean_t Server::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	return ucsp_server(in, out) || self_server(in, out);
}


//
// Set up a new Connection. This establishes the environment (process et al) as needed
// and registers a properly initialized Connection object to run with.
// Type indicates how "deep" we need to initialize (new session, process, or connection).
// Everything at and below that level is constructed. This is straight-forward except
// in the case of session re-initialization (see below).
//
void Server::setupConnection(ConnectLevel type, Port servicePort, Port replyPort, Port taskPort,
    const audit_token_t &auditToken, const ClientSetupInfo *info, const char *identity)
{
	// first, make or find the process based on task port
	StLock<Mutex> _(*this);
	RefPointer<Process> &proc = mProcesses[taskPort];
	if (type == connectNewSession && proc) {
		// The client has talked to us before and now wants to create a new session.
		proc->changeSession(servicePort);
	}
	if (proc && type == connectNewProcess) {
		// the client has amnesia - reset it
		assert(info && identity);
		proc->reset(servicePort, taskPort, info, identity, AuditToken(auditToken));
		proc->changeSession(servicePort);
	}
	if (!proc) {
		if (type == connectNewThread)	// client error (or attack)
			CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
		assert(info && identity);
		proc = new Process(servicePort, taskPort, info, identity, AuditToken(auditToken));
		notifyIfDead(taskPort);
		mPids[proc->pid()] = proc;
	}

	// now, establish a connection and register it in the server
	Connection *connection = new Connection(*proc, replyPort);
	if (mConnections.contains(replyPort))   // malicious re-entry attempt?
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ error code? (client error)
	mConnections[replyPort] = connection;
	notifyIfDead(replyPort);
}


//
// Synchronously end a Connection.
// This is due to a request from the client, so no thread races are possible.
// In practice, this is optional since the DPN for the client thread reply port
// will destroy the connection anyway when the thread dies.
//
void Server::endConnection(Port replyPort)
{
	StLock<Mutex> _(*this);
	PortMap<Connection>::iterator it = mConnections.find(replyPort);
	assert(it != mConnections.end());
	it->second->terminate();
	mConnections.erase(it);
}

//
// Handling dead-port notifications.
// This receives DPNs for all kinds of ports we're interested in.
//
void Server::notifyDeadName(Port port)
{
	// We need the lock to get a proper iterator on mConnections or mProcesses,
	// but must release it before we call abort or kill, as these might also
	// need the server lock
	
	StLock<Mutex> serverLock(*this);
	secdebug("SSports", "port %d is dead", port.port());
    
    // is it a connection?
    PortMap<Connection>::iterator conIt = mConnections.find(port);
    if (conIt != mConnections.end()) {
		SECURITYD_PORTS_DEAD_CONNECTION(port);
        RefPointer<Connection> con = conIt->second;
        mConnections.erase(conIt);
        serverLock.unlock();
		con->abort();        
		return;
    }
    
    // is it a process?
    PortMap<Process>::iterator procIt = mProcesses.find(port);
    if (procIt != mProcesses.end()) {
		SECURITYD_PORTS_DEAD_PROCESS(port);
        RefPointer<Process> proc = procIt->second;
        mPids.erase(proc->pid());
        mProcesses.erase(procIt);
        serverLock.unlock();
		// The kill may take some time; make sure there is a spare thread around
		// to prevent deadlocks
		StLock<MachServer, &Server::busy, &Server::idle> _(*this);
		proc->kill();
        return;
    }
    
	// well, what IS IT?!
	SECURITYD_PORTS_DEAD_ORPHAN(port);
	secdebug("server", "spurious dead port notification for port %d", port.port());
}


//
// Handling no-senders notifications.
// This is currently only used for (subsidiary) service ports
//
void Server::notifyNoSenders(Port port, mach_port_mscount_t)
{
	SECURITYD_PORTS_DEAD_SESSION(port);
	secdebug("SSports", "port %d no senders", port.port());
	Session::destroy(port);
}


//
// Handling signals.
// These are sent as Mach messages from ourselves to escape the limitations of
// the signal handler environment.
//
kern_return_t self_server_handleSignal(mach_port_t sport,
	mach_port_t taskPort, int sig)
{
    try {
		SECURITYD_SIGNAL_HANDLED(sig);
        if (taskPort != mach_task_self()) {
            Syslog::error("handleSignal: received from someone other than myself");
			return KERN_SUCCESS;
		}
		switch (sig) {
		case SIGCHLD:
			ServerChild::checkChildren();
			break;
		case SIGINT:
			SECURITYD_SHUTDOWN_NOW();
			Syslog::notice("securityd terminated due to SIGINT");
			_exit(0);
		case SIGTERM:
			Server::active().beginShutdown();
			break;
		case SIGPIPE:
			fprintf(stderr, "securityd ignoring SIGPIPE received");
			break;

#if defined(DEBUGDUMP)
		case SIGUSR1:
			NodeCore::dumpAll();
			break;
#endif //DEBUGDUMP

		case SIGUSR2:
			{
				extern PCSCMonitor *gPCSC;
				gPCSC->startSoftTokens();
				break;
			}

		default:
			assert(false);
        }
    } catch(...) {
		secdebug("SS", "exception handling a signal (ignored)");
	}
    mach_port_deallocate(mach_task_self(), taskPort);
    return KERN_SUCCESS;
}


//
// Notifier for system sleep events
//
void Server::SleepWatcher::systemWillSleep()
{
	SECURITYD_POWER_SLEEP();
    Session::processSystemSleep();
	for (set<PowerWatcher *>::const_iterator it = mPowerClients.begin(); it != mPowerClients.end(); it++)
		(*it)->systemWillSleep();
}

void Server::SleepWatcher::systemIsWaking()
{
	SECURITYD_POWER_WAKE();
	for (set<PowerWatcher *>::const_iterator it = mPowerClients.begin(); it != mPowerClients.end(); it++)
		(*it)->systemIsWaking();
}

void Server::SleepWatcher::systemWillPowerOn()
{
	SECURITYD_POWER_ON();
	Server::active().longTermActivity();
	for (set<PowerWatcher *>::const_iterator it = mPowerClients.begin(); it != mPowerClients.end(); it++)
		(*it)->systemWillPowerOn();
}

void Server::SleepWatcher::add(PowerWatcher *client)
{
	assert(mPowerClients.find(client) == mPowerClients.end());
	mPowerClients.insert(client);
}

void Server::SleepWatcher::remove(PowerWatcher *client)
{
	assert(mPowerClients.find(client) != mPowerClients.end());
	mPowerClients.erase(client);
}


//
// Expose the process/pid map to the outside
//
Process *Server::findPid(pid_t pid) const
{
	PidMap::const_iterator it = mPids.find(pid);
	return (it == mPids.end()) ? NULL : it->second;
}


//
// Set delayed shutdown mode
//
void Server::waitForClients(bool waiting)
{
	mWaitForClients = waiting;
}


//
// Begin shutdown processing.
// We relinquish our primary state authority. From now on, we'll be
// kept alive (only) by our current clients.
//
static FILE *reportFile;

void Server::beginShutdown()
{
	StLock<Mutex> _(*this);
	if (!mWaitForClients) {
		SECURITYD_SHUTDOWN_NOW();
		_exit(0);
	} else {
		if (!mShuttingDown) {
			mShuttingDown = true;
            Session::invalidateAuthHosts();
			SECURITYD_SHUTDOWN_BEGIN();
			if (verbosity() >= 2) {
				reportFile = fopen("/var/log/securityd-shutdown.log", "w");
				shutdownSnitch();
			}
		}
	}
}


//
// During shutdown, we report residual clients to dtrace, and allow a state dump
// for debugging.
// We don't bother locking for the shuttingDown() check; it's a latching boolean
// and we'll be good enough without a lock.
//
void Server::eventDone()
{
	if (this->shuttingDown()) {
		StLock<Mutex> lazy(*this, false);	// lazy lock acquisition
		if (SECURITYD_SHUTDOWN_COUNT_ENABLED()) {
			lazy.lock();
			SECURITYD_SHUTDOWN_COUNT(mProcesses.size(), VProc::Transaction::debugCount());
		}
		if (verbosity() >= 2) {
			lazy.lock();
			shutdownSnitch();
		}
		IFDUMPING("shutdown", NodeCore::dumpAll());
	}
}


void Server::shutdownSnitch()
{
	time_t now;
	time(&now);
	fprintf(reportFile, "%.24s %d residual clients:\n",	ctime(&now), int(mPids.size()));
	for (PidMap::const_iterator it = mPids.begin(); it != mPids.end(); ++it)
		if (SecCodeRef clientCode = it->second->processCode()) {
			CFRef<CFURLRef> path;
			OSStatus rc = SecCodeCopyPath(clientCode, kSecCSDefaultFlags, &path.aref());
			if (path)
				fprintf(reportFile, " %s (%d)\n", cfString(path).c_str(), it->first);
			else
				fprintf(reportFile,  "pid=%d (error %d)\n", it->first, int32_t(rc));
		}
	fprintf(reportFile, "\n");
	fflush(reportFile);
}


//
// Initialize the CSSM/MDS subsystem.
// This was once done lazily on demand. These days, we are setting up the
// system MDS here, and CSSM is pretty much always needed, so this is called
// early during program startup. Do note that the server may not (yet) be running.
//
void Server::loadCssm(bool mdsIsInstalled)
{
	if (!mCssm->isActive()) {
		StLock<Mutex> _(*this);
		VProc::Transaction xact;
		if (!mCssm->isActive()) {
            if (!mdsIsInstalled) {  // non-system securityd instance should not reinitialize MDS
                secdebug("SS", "Installing MDS");
                IFDEBUG(if (geteuid() == 0))
				MDSClient::mds().install();
            }
			secdebug("SS", "CSSM initializing");
			mCssm->init();
			mCSP->attach();
			secdebug("SS", "CSSM ready with CSP %s", mCSP->guid().toString().c_str());
		}
	}
}


//
// LongtermActivity/lock combo
//
LongtermStLock::LongtermStLock(Mutex &lck)
	: StLock<Mutex>(lck, false)	// don't take the lock yet
{
	if (lck.tryLock()) {	// uncontested
		this->mActive = true;
	} else {				// contested - need backup thread
		Server::active().longTermActivity();
		this->lock();
	}
}
