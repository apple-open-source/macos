/*
 * DTrace provider for securityd
 */


/*
 * Work around 5194316
 */
#define uint32_t unsigned


/*
 * Types
 */
typedef const void *DTHandle;
typedef uint32_t DTPort;
typedef uint32_t DTGuest;


/*
 * The main static provider for securityd
 */
provider securityd {
	/*
	 * Overall operational events
	 */
	probe installmode();		// configuring for system installation scenario
	probe initialized(const char *bootstrapName);


	/*
	 * Keychain activity (DbCommon status change)
	 */
	probe keychain__create(DTHandle common, const char *name, DTHandle db);
	probe keychain__make(DTHandle common, const char *name, DTHandle db);
	probe keychain__join(DTHandle common, const char *name, DTHandle db);
	probe keychain__unlock(DTHandle id, const char *name);
	probe keychain__lock(DTHandle id, const char *name);
	probe keychain__release(DTHandle id, const char *name);
	
	/*
	 * Client management
	 */
	probe client__new(DTHandle id, int pid, DTHandle session, const char *path, DTPort taskport, int uid, int gid, bool flipped);
	probe client__release(DTHandle id, int pid);
	probe client__connection__new(DTHandle id, DTPort port, DTHandle client);
	probe client__connection__release(DTHandle id);
	
	probe client__change_session(DTHandle id, DTHandle session);
	
	probe request__entry(const char *name, DTHandle connection, DTHandle process);
	probe request__return(uint32_t osstatus);

	/*
	 * Session management
	 */
	probe session__create(DTHandle id, uint32_t attributes, DTPort port);
	probe session__setattr(DTHandle id, uint32_t attributes);
	probe session__destroy(DTHandle id);
	
	/*
	 * Port-related events (internal interest only)
	 */
	probe ports__dead__connection(DTPort port);
	probe ports__dead__process(DTPort port);
	probe ports__dead__session(DTPort port);
	probe ports__dead__orphan(DTPort port);
	
	/*
	 * Power management and tracking
	 */
	probe power__sleep();
	probe power__wake();
	probe power__on();
	
	/*
	 * Code Signing related
	 */
	probe host__register(DTHandle proc, DTPort port);
	probe host__proxy(DTHandle proc, DTPort port);
	probe host__unregister(DTHandle proc);
	probe guest__create(DTHandle proc, DTGuest host, DTGuest guest, uint32_t status, uint32_t flags, const char *path);
	probe guest__cdhash(DTHandle proc, DTGuest guest, const void *hash, uint32_t length);
	probe guest__destroy(DTHandle proc, DTGuest guest);
	probe guest__change(DTHandle proc, DTGuest guest, uint32_t status);
	
	/*
	 * Child management
	 */
	probe child__dying(int pid);
	probe child__checkin(int pid, DTPort servicePort);
	probe child__stillborn(int pid);
	probe child__ready(int pid);
    
    /*
     * Authorization
     */
    /* creation */
    probe auth__create(DTHandle session, void *authref);
    /* rule evaluation types */
    probe auth__allow(DTHandle authref, const char *rule);
    probe auth__deny(DTHandle authref, const char *rule);
    probe auth__user(DTHandle authref, const char *rule);
    probe auth__rules(DTHandle authref, const char *rule);
    probe auth__kofn(DTHandle authref, const char *rule);
    probe auth__mechrule(DTHandle authref, const char *rule);
    probe auth__mech(DTHandle authref, const char *mechanism);
    /* evaluation intermediate results */
    probe auth__user__allowroot(DTHandle authref);
    probe auth__user__allowsessionowner(DTHandle authref);
    /* evaluation final result */
    probe auth__evalright(DTHandle authref, const char *right, int32_t status);
	
	/*
	 * Miscellaneous activity
	 */
	probe shutdown__begin();
	probe shutdown__count(int processesLeft, int dirtyCountLeft);
	probe shutdown__now();

	probe entropy__collect();
	probe entropy__seed(const void *data, uint32_t count);
	probe entropy__save(const char *path);
	
	probe signal__received(int signal);
	probe signal__handled(int signal);
};
