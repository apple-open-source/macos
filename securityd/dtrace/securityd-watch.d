#!/usr/sbin/dtrace -q -s



/*
 * Tracking state
 */
typedef uint32_t DTPort;
typedef uint64_t DTHandle;

DTHandle portmap[DTPort];				/* map client reply ports to connections */

struct connection {
	DTPort replyport;		/* reply port for client thread */
	uint32_t client;		/* client object for this connection */
};
struct connection connection[DTHandle];	/* indexed by connection handle */

/* should be a single self struct, but that doesn't work right... */
self string reqName;		/* request name */
self DTHandle reqConnection; /* associated connection */
self DTHandle reqClient;	/* associated client */

struct client {
	pid_t pid;				/* original client pid */
	DTHandle session;		/* session handle */
	string name;			/* abbreviated name */
	string path;			/* path to client process (regardless of guests) */
	DTPort  taskport;		/* process task port */
};
struct client client[DTHandle];			/* indexed by client handle */

struct keychain {
	string name;			/* keychain path */
};
struct keychain keychain[DTHandle];		/* indexed by DbCommon handle */


/*
 * Script management
 */
:::BEGIN
{
	/* fake data for unknown processes */
	client[0].pid = 0;
	client[0].session = 0;
	client[0].name = "*UNKNOWN*";
	client[0].path = "*UNKNOWN*";

	printf("Ready...\n");
}


/*
 * Translate thread id
 */
uint32_t threads[DTHandle];	/* map tids to simple thread numbers */
uint32_t nextThread;		/* next unused thread number */
self uint32_t mytid;		/* translated tid */

securityd*::: /!threads[tid]/ { threads[tid] = ++nextThread; }
security_debug*::: /!threads[tid]/ { threads[tid] = ++nextThread; }

securityd*::: { self->mytid = threads[tid]; }
security_debug*::: { self->mytid = threads[tid]; }


/*
 * Principal events
 */
securityd*:::installmode
{
	printf("%u SYSTEM INSTALLATION MODE SELECTED\n", timestamp);
}

securityd*:::initialized
{
	printf("%u COMMENCING SERVICE as %s\n", timestamp, copyinstr(arg0));
}


/*
 * Client management
 */
securityd*:::client-connection-new
{
	replymap[arg1] = arg0;
	self->reqClient = arg2;
	connection[arg0].client = self->reqClient;
	self->reqConnection = arg0;
	@total["Connections"] = count();
	printf("%u T%d:connection-new(<%x>,port=%d,client=<%x>/%s(%d))\n",
		timestamp, self->mytid, arg0, arg1,
		arg2, client[arg2].name, client[arg2].pid);
}

securityd*:::client-connection-release
/connection[arg0].client/
{
	printf("%u T%d:connection-release(<%x>,client=<%x>/%s(%d))\n",
		timestamp, self->mytid, arg0,
		connection[arg0].client,
		client[connection[arg0].client].name,
		client[connection[arg0].client].pid);
	replymap[connection[arg0].replyport] = 0;		/* clear from port map */
	connection[arg0].replyport = 0;
	connection[arg0].client = 0;
}

securityd*:::client-new
{
	client[arg0].pid = arg1;
	client[arg0].session = arg2;
    client[arg0].path = copyinstr(arg3);
	client[arg0].name = basename(client[arg0].path);
	client[arg0].taskport = arg4;
	self->reqClient = arg0;
	@total["Processes"] = count();
    printf("%u T%d:client-new(<%x>,%s(%d),session=<%x>,task=%d)\n",
        timestamp, self->mytid, arg0,
		client[arg0].path, client[arg0].pid,
		client[arg0].session, client[arg0].taskport);
}

securityd*:::client-release
{
    printf("%u T%d:client-release(<%x>,%s(%d))\n",
		timestamp, self->mytid, arg0, client[arg0].path, arg1);
	client[arg0].pid = 0;
}

securityd*:::client-change_session
{
    printf("%u T%d:client-change_session(<%x>,new session=<%x>)\n",
		timestamp, self->mytid, arg0, arg1);
	client[arg0].pid = 0;
}


/*
 * Client requests
 */
uint32_t connections[DTHandle];
uint32_t nextConnection;
self uint32_t myConnection;

securityd*:::request-entry
/!connections[arg1]/
{ connections[arg1] = ++nextConnection; }

securityd*:::request-entry
{
	self->reqName = copyinstr(arg0);
	self->reqConnection = arg1;
	self->myConnection = connections[arg1];
	self->reqClient = arg2;
	this->client = client[self->reqClient];
}

securityd*:::request-entry
/this->client.pid/
{
	printf("%u T%d:C%d:%s(%d)%s\n",
		timestamp, self->mytid, self->myConnection, this->client.name, this->client.pid, self->reqName);
	@request[client[self->reqClient].name, self->reqName] = count();
}

securityd*:::request-entry
/!this->client.pid/
{
	printf("%u T%d:C%d:%s\n",
		timestamp, self->mytid, self->myConnection, self->reqName);
}

securityd*:::request-entry
{
	@requests[self->reqName] = count();
	@total["Requests"] = count();
}

securityd*:::request-return
/self->reqConnection && arg0 == 0/
{
	printf("%u T%d:C%d:return\n",
		timestamp, self->mytid, self->myConnection);
}

securityd*:::request-return
/self->reqConnection && arg0 != 0/
{
	printf("%u T%d:C%d:FAIL(%d)\n",
		timestamp, self->mytid, self->myConnection, arg0);
}

securityd*:::request-return
{
	self->reqConnection = 0;
	self->reqClient = 0;
}


/*
 * Sessions
 */
typedef uint32_t SessionId;

struct Session {
	DTHandle handle;
	SessionId sessionid;
};
struct Session session[SessionId];

struct xauditinfo {
	uint32_t	ai_auid;		/* audit user id */
	struct {
		unsigned int low;
		unsigned int high;
	} ai_mask;
	struct {
		uint32_t dev;
		uint32_t type;
		uint32_t addr[4];
	} ai_termid;
	au_asid_t ai_asid;		/* audit session id */
	au_asflgs_t ai_flags;	/* audit session flags */
};
self struct xauditinfo *ai;

securityd*:::session-create
{
	session[arg1].handle = arg0;
	session[arg1].sessionid = arg1;
	self->ai = copyin(arg2, sizeof(struct xauditinfo));
	printf("%u T%d:%s(<%x>,id=%d,uid=%d,flags=%#x)\n", timestamp, self->mytid, probename,
		arg0, arg1, self->ai->ai_auid, self->ai->ai_flags);
}

securityd*:::session-kill
{
	printf("%u T%d:%s(<%x>,id=%d)\n", timestamp, self->mytid, probename, arg0, arg1);
}

securityd*:::session-destroy
{
	printf("%u T%d:%s(<%x>,id=%d)\n", timestamp, self->mytid, probename, arg0, arg1);
}

securityd*:::session-notify
{
	printf("%u T%d:%s(<%x>,id=%d,events=0x%x,uid=%d)\n", timestamp, self->mytid, probename,
		session[arg0].handle, arg0, arg1, arg2);
}


/*
 * Keychains
 */
securityd*:::keychain-*
{
	this->path = copyinstr(arg1);
	printf("%u T%d:%s(<%x>,%s)\n", timestamp, self->mytid, probename, arg0, this->path);
	@keychain[this->path, probename] = count();
}


/*
 * Low-level port events
 */
securityd*:::ports-*
{
	printf("%u T%d:%s(%d)\n", timestamp, self->mytid, probename, arg0);
}


/*
 * Code signing
 */
securityd*:::guest-create
{
	printf("%u T%d:guest-create(<%x>,host=<%x>,guest=<%x>,status=0x%x,flags=0x%x,path=%s)\n",
		timestamp, self->mytid, arg0, arg1, arg2, arg3, arg4, copyinstr(arg5));
	@total["Guests"] = count();
}

securityd*:::guest-change
{
	printf("%u T%d:guest-change(<%x>,<%x>,status=0x%x)\n", timestamp, self->mytid, arg0, arg1, arg2);
}

securityd*:::guest-destroy
{
	printf("%u T%d:guest-destroy(<%x>,<%x>)\n", timestamp, self->mytid, arg0, arg1);
}

securityd*:::host-register,
securityd*:::host-proxy
{
	printf("%u T%d:%s(<%x>,port=%d)\n", timestamp, self->mytid, probename, arg0, arg1);
	@total["Hosts"] = count();
}

securityd*:::host-unregister
{
	printf("%u T%d:host-unregister(<%x>)\n", timestamp, self->mytid, arg0);
}


/*
 * Child management
 */
securityd*:::child-*
{
	printf("%u T%d:%s(%d,%d)\n", timestamp, self->mytid, probename, arg0, arg1);
}


/*
 * Power events
 */
securityd*:::power-*
{
	printf("%u T%d:POWER(%s)\n", timestamp, self->mytid, probename);
}


/*
 * Authorization
 */
securityd*:::auth-create
{
    printf("%u T%d:%s ref(%#x) session(%#x)\n", timestamp, self->mytid, probename, arg1, arg0);
}

securityd*:::auth-allow,
securityd*:::auth-deny,
securityd*:::auth-user,
securityd*:::auth-rules,
securityd*:::auth-kofn,
securityd*:::auth-mechrule
{
    printf("%u T%d:%s ref(%#x) rule(%s)\n", timestamp, self->mytid, probename, arg0, copyinstr(arg1));
}

securityd*:::auth-mech
{
    printf("%u T%d:%s ref(%#x) (%s)\n", timestamp, self->mytid, probename, arg0, copyinstr(arg1));
}

securityd*:::auth-user-allowroot,
securityd*:::auth-user-allowsessionowner
{
    printf("%u T%d:%s ref(%#x)\n", timestamp, self->mytid, probename, arg0);
}

securityd*:::auth-evalright
{
    printf("%u T%d:%s ref(%#x) %s (%d)\n", timestamp, self->mytid, probename, arg0, copyinstr(arg1), arg2);
}


/*
 * Miscellanea
 */
securityd*:::entropy-collect
{
	printf("%u T%d:entropy-collect()\n", timestamp, tid);
}

securityd*:::entropy-seed
{
	printf("%u T%d:entropy-seed(%d)\n", timestamp, self->mytid, arg0);
}

securityd*:::entropy-save
{
	printf("%u T%d:entropy-save(%s)\n", timestamp, self->mytid, copyinstr(arg0));
}

securityd*:::signal-*
{
	printf("%u T%d:%s(%d)\n", timestamp, self->mytid, probename, arg0);
}


/*
 * Integrate secdebug logs
 */
security_debug*:::log
/execname == "securityd"/
{
	printf("%u T%d:[%s]%s\n", timestamp, threads[tid],
		copyinstr(arg0), copyinstr(arg1));
}

security_exception*:::throw-*
/execname == "securityd"/
{
	printf("%u T%d:EXCEPTION(%p) THROWN %s(%d)\n", timestamp, threads[tid],
		arg0, probename, arg1);
}


/*
 * Wrapup
 */
:::END
{
	printa("%@8u %s\n", @total);
	printf("\n         Requests:\n");
	printa("%@8u %s\n", @requests);
	printf("\n         Requests by client:\n");
	printa("%@8u %s:%s\n", @request);
	printf("\n         Keychains by path and operation:\n");
	printa("%@8u %s(%s)\n", @keychain);
}
