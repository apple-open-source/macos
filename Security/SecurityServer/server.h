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
// server - the actual Server object
//
#ifndef _H_SERVER
#define _H_SERVER

#include "securityserver.h"
#include <Security/machserver.h>
#include <Security/powerwatch.h>
#include <Security/cssmclient.h>
#include <Security/cspclient.h>
#include <Security/osxsigner.h>
#include <Security/devrandom.h>
#include <Security/uniformrandom.h>
#include "codesigdb.h"
#include "connection.h"
#include "key.h"
#include "xdatabase.h"
#include "authority.h"
#include <map>

#define EQUIVALENCEDBPATH "/var/db/CodeEquivalenceDatabase"


class Server : public MachPlusPlus::MachServer,
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
	
	static Connection &connection(mach_port_t replyPort);
	static Connection &connection(bool tolerant = false);
	static void requestComplete();
	
	static Key &key(KeyHandle key)
	{ return findHandle<Key>(key, CSSMERR_CSP_INVALID_KEY_REFERENCE); }
    static Key *optionalKey(KeyHandle k) { return (k == noKey) ? NULL : &key(k); }
	static Database &database(DbHandle db)
	{ return findHandle<Database>(db, CSSMERR_DL_INVALID_DB_HANDLE); }
	static Database *optionalDatabase(DbHandle db) { return db ? &database(db) : NULL; }
    static Authority &authority() { return active().mAuthority; }
	static CodeSignatures &codeSignatures() { return active().mCodeSignatures; }
	static SecurityServerAcl &aclBearer(AclKind kind, CSSM_HANDLE handle);
	static CssmClient::CSP &csp() { return active().getCsp(); }

	void loadCssm();
	
public:
	// set up a new connection
	enum ConnectLevel {
		connectNewSession,
		connectNewProcess,
		connectNewThread
	};
	void setupConnection(ConnectLevel type, Port servicePort, Port replyPort, Port taskPort,
        const security_token_t &securityToken,
		const ClientSetupInfo *info = NULL, const char *executablePath = NULL);
		
	void endConnection(Port replyPort);
	
	static void releaseWhenDone(CssmAllocator &alloc, void *memory)
	{ MachServer::active().releaseWhenDone(alloc, memory); }
	static void releaseWhenDone(void *memory)
	{ releaseWhenDone(CssmAllocator::standard(), memory); }
    
protected:
    // implementation methods of MachServer
	boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
	void notifyDeadName(Port port);
	void notifyNoSenders(Port port, mach_port_mscount_t);
    
private:
    class SleepWatcher : public MachPlusPlus::PortPowerWatcher {
    public:
        void systemWillSleep();
    };
    SleepWatcher sleepWatcher;
	
private:
	Mutex lock;					// master lock
	
	// mach bootstrap registration name
	std::string mBootstrapName;

	// map of connections (by client reply port)
	typedef map<mach_port_t, Connection *> ConnectionMap;
	ConnectionMap connections;
	
	// map of processes (by process task port)
	typedef map<mach_port_t, Process *> ProcessMap;
	ProcessMap processes;
	
	// Current connection, if any (per thread).
	// Set as a side effect of calling connection(mach_port_t)
	// and returned by connection(bool).
	PerThreadPointer<Connection> mCurrentConnection;
	
    // CSSM components
    CssmClient::Cssm mCssm;				// CSSM instance
    CssmClient::Module mCSPModule;		// CSP module
	CssmClient::CSP mCSP;				// CSP attachment
    CssmClient::CSP &getCsp();			// lazily initialize, then return CSP attachment
    
	Authority &mAuthority;
	CodeSignatures &mCodeSignatures;
};

#endif //_H_SERVER
