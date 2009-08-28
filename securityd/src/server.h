/*
 * Copyright (c) 2000-2004,2008-2009 Apple Inc. All Rights Reserved.
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
#ifndef _H_SERVER
#define _H_SERVER

#include "structure.h"
#include <security_utilities/machserver.h>
#include <security_utilities/powerwatch.h>
#include <security_utilities/ccaudit.h>
#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_client/cspclient.h>
#include <security_utilities/devrandom.h>
#include <security_cdsa_utilities/uniformrandom.h>
#include <security_utilities/vproc++.h>
#include "codesigdb.h"
#include "connection.h"
#include "key.h"
#include "database.h"
#include "localdatabase.h"
#include "kcdatabase.h"
#include "authority.h"
#include "AuthorizationEngine.h"
#include <map>

#define EQUIVALENCEDBPATH "/var/db/CodeEquivalenceDatabase"


//
// The authority itself. You will usually only have one of these.
//
class Authority : public Authorization::Engine {
public:
	Authority(const char *configFile);
	~Authority();
};

//
// The server object itself. This is the "go to" object for anyone who wants
// to access the server's global state. It runs the show.
// There is only one Server, and its name is Server::active().
//
// Server also acts as the global-scope nexus of securityd's object mesh.
// Sessions have Server as their parent, and global-scope objects have it
// as their referent. The Server is never kill()ed; though kill(globalObject)
// may make sense. Also, we can search for global-scope objects by using the
// findFirst/allReferences feature of Node<>.
//
class Server : public PerGlobal,
			   public MachPlusPlus::MachServer,
               public UniformRandomBlobs<DevRandomGenerator> {
public:
	Server(Authority &myAuthority, CodeSignatures &signatures, const char *bootstrapName);
	~Server();
		
    // run the server until it shuts down
	void run();
	
    //
    // Retrieve pieces of the Server's object web.
    // These are all static methods that use the active() Server of this thread.
    //
	static Server &active() { return safer_cast<Server &>(MachServer::active()); }
	static const char *bootstrapName() { return active().mBootstrapName.c_str(); }
	static unsigned int verbosity() { return active().mVerbosity; }

	//
	// Each thread has at most one "active connection". If the server is currently
	// servicing a request received through a Connection, that's it. Otherwise
	// there is none.
	//
	static Connection &connection(mach_port_t replyPort, audit_token_t &auditToken);	// find by reply port and make active
	static Connection &connection(bool tolerant = false);	// return active (or fail unless tolerant)
	static void requestComplete(CSSM_RETURN &rcode);		// de-activate active connection
	
	//
	// Process and session of the active Connection
	//
	static Process &process();
	static Session &session();
	
	//
	// Find objects from their client handles.
	// These will all throw on invalid handles, and the RefPointer<> results are always non-NULL.
	//
	static RefPointer<Key> key(KeyHandle key);
    static RefPointer<Key> optionalKey(KeyHandle k) { return (k == noKey) ? NULL : key(k); }
	static RefPointer<Database> database(DbHandle db);
	static RefPointer<KeychainDatabase> keychain(DbHandle db);
	static RefPointer<Database> optionalDatabase(DbHandle db, bool persistent = true);
	static AclSource &aclBearer(AclKind kind, U32HandleObject::Handle handle);
	
	// Generic version of handle lookup
	template <class ProcessBearer>
    static RefPointer<ProcessBearer> find(uint32_t handle, CSSM_RETURN notFoundError)
	{
		RefPointer<ProcessBearer> object = 
			U32HandleObject::findRef<ProcessBearer>(handle, notFoundError);
		if (object->process() != Server::process())
			CssmError::throwMe(notFoundError);
		return object;
	}

	//
	// publicly accessible components of the active server
	//
    static Authority &authority() { return active().mAuthority; }
	static CodeSignatures &codeSignatures() { return active().mCodeSignatures; }
	static CssmClient::CSP &csp() { return active().mCSP; }

public:
	//
	// Initialize CSSM and MDS
	//
	void loadCssm(bool mdsIsInstalled);
	
public:
	// set up a new connection
	enum ConnectLevel {
		connectNewSession,
		connectNewProcess,
		connectNewThread
	};
	void setupConnection(ConnectLevel type, Port servicePort, Port replyPort, Port taskPort,
        const audit_token_t &auditToken,
		const ClientSetupInfo *info = NULL, const char *executablePath = NULL);
		
	void endConnection(Port replyPort);
	
	static void releaseWhenDone(Allocator &alloc, void *memory)
	{ MachServer::active().releaseWhenDone(alloc, memory); }
	static void releaseWhenDone(void *memory)
	{ releaseWhenDone(Allocator::standard(), memory); }
    
protected:
    // implementation methods of MachServer
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	void notifyDeadName(Port port);
	void notifyNoSenders(Port port, mach_port_mscount_t);
	void threadLimitReached(UInt32 count);
	void eventDone();

private:
	class SleepWatcher : public MachPlusPlus::PortPowerWatcher {
	public:
		void systemWillSleep();
		void systemIsWaking();
		void systemWillPowerOn();
		
		void add(PowerWatcher *client);
		void remove(PowerWatcher *client);

	private:
		set<PowerWatcher *> mPowerClients;
	};

	SleepWatcher sleepWatcher;
	
public:
	using MachServer::add;
	using MachServer::remove;
	void add(MachPlusPlus::PowerWatcher *client)	{ StLock<Mutex> _(*this); sleepWatcher.add(client); }
	void remove(MachPlusPlus::PowerWatcher *client)	{ StLock<Mutex> _(*this); sleepWatcher.remove(client); }
	
public:
	Process *findPid(pid_t pid) const;

	void verbosity(unsigned int v) { mVerbosity = v; }
	void waitForClients(bool waiting);				// set waiting behavior
	void beginShutdown();							// start delayed shutdown if configured
	bool shuttingDown() const { return mShuttingDown; }
	void shutdownSnitch();							// report lingering clients
    
private:
	// mach bootstrap registration name
	std::string mBootstrapName;
	
	// connection map (by client reply port)
	PortMap<Connection> mConnections;

	// process map (by process task port)
	typedef std::map<pid_t, Process *> PidMap;
	PortMap<Process> mProcesses;					// strong reference
	PidMap mPids;									// weak reference (subsidiary to mProcesses)
	
	// Current connection, if any (per thread).
	// Set as a side effect of calling connection(mach_port_t)
	// and returned by connection(bool).
	ThreadNexus<RefPointer<Connection> > mCurrentConnection;
	
    // CSSM components
    CssmClient::Cssm mCssm;				// CSSM instance
    CssmClient::Module mCSPModule;		// CSP module
	CssmClient::CSP mCSP;				// CSP attachment
    
	Authority &mAuthority;
	CodeSignatures &mCodeSignatures;
    
    // Per-process audit initialization
    CommonCriteria::AuditSession mAudit;
	
	// busy state for primary state authority
	unsigned int mVerbosity;
	bool mWaitForClients;
	bool mShuttingDown;
};


//
// A StLock that (also) declares a longTermActivity (only) once it's been entered.
//
class LongtermStLock : public StLock<Mutex> {
public:
	LongtermStLock(Mutex &lck);
	// destructor inherited
};

#endif //_H_SERVER
