/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Controller.m
 *
 * Controller for lookupd
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/config.h>
#import <NetInfo/system_log.h>
#import <NetInfo/project_version.h>
#import "Controller.h"
#import "Thread.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import "Config.h"
#import "LUGlobal.h"
#import "MachRPC.h"
#import "LUServer.h"
#import "CacheAgent.h"
#import "NIAgent.h"
#import "LUNIDomain.h"
#import "DNSAgent.h"
#import "FFAgent.h"
#import "YPAgent.h"
#import "LDAPAgent.h"
#import "NILAgent.h"
#import <NetInfo/dsutil.h>
#import "sys.h"
#import <sys/types.h>
#import <sys/param.h>
#import <unistd.h>
#import <string.h>
#import <libc.h>

#define forever for(;;)

extern int gethostname(char *, int);
extern sys_port_type server_port;
extern sys_port_type _lookupd_port(sys_port_type);
#ifdef _SHADOW_
extern sys_port_type _lookupd_port1(sys_port_type);
extern sys_port_type server_port_privileged;
extern sys_port_type server_port_unprivileged;
#endif

extern int _lookup_link();

@implementation Controller

- (void)serviceRequest:(lookup_request_msg *)request
{
	Thread *worker;

	/*
	 * Deal with the client's request
	 */
	worker = [[Thread alloc] init];
	[worker setName:"Work Thread"];
	[worker setData:(void *)request];
	[worker setState:ThreadStateActive];
	[worker shouldTerminate:YES];
	[worker run:@selector(process) context:machRPC];
}

/*
 * Server runs in this loop to answer requests
 */
- (void)serverLoop
{
	kern_return_t status;
	lookup_request_msg *request;
	Thread *t;

	t = [Thread currentThread];

	forever
	{
		/* Receive and service a request */
		[t setState:ThreadStateIdle];
		request = (lookup_request_msg *)calloc(1, sizeof(lookup_request_msg));
		status = sys_receive_message(&request->head, sizeof(lookup_request_msg), server_port, 0);
		[t setState:ThreadStateActive];

		if (shutting_down) break;
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Server status = %s (%d)", sys_strerror(status), status);
				continue;
		}
		
		/* request is now owned by the thread, which needs to free it when done */
		[self serviceRequest:request];
		if (shutting_down) break;
	}
}

- (void)newAgent:(id)agent name:(char *)name
{
	if (agentCount == 0) agents = (id *)malloc(sizeof(id));
	else agents = (id *)realloc(agents, (agentCount + 1) * sizeof(id));
	agents[agentCount] = agent;
	agentNames = appendString(name, agentNames);
	agentCount++;
}

- (id)agentNamed:(char *)name
{
	int i;
	char cname[256];

	i = listIndex(name, agentNames);
	if (i != IndexNull) return agents[i];

	sprintf(cname, "%sAgent", name);
	i = listIndex(cname, agentNames);
	if (i != IndexNull) return agents[i];

	return nil;
}

- (void)setLookupOrder:(char **)order forCategory:(LUCategory)cat
{
	int i;
	id agent;

	if (order == NULL) return;

	for (i = 0; order[i] != NULL; i++)
	{
		agent = [self agentNamed:order[i]];
		if (agent == nil)
		{
			system_log(LOG_ALERT, "Can't initialize agent: %s", order[i]);
			continue;
		}

		[lookupOrder[(int)cat] addObject:agent];		
	}
}

- (void)initConfig
{
	char **gLUorder, **cLUorder;
	int i;
	LUDictionary *global, *config;
	BOOL gValidation, cValidation;
	char *logFileName;
	char *logFacilityName;
	unsigned int gMax, gFreq, cMax, cFreq;
	time_t now, gTTL, cTTL, gDelta, cDelta;
	char str[64];
	FILE *fp;

	global = [configManager configGlobal];

	gLUorder = [global valuesForKey:"LookupOrder"];

	logFileName = [configManager stringForKey:"LogFile" dict:global default:NULL];
	fp = fopen(logFileName, "a");
 	system_log_set_logfile(fp);
	freeString(logFileName);

	logFacilityName = [configManager stringForKey:"LogFacility" dict:global default:"LOG_NETINFO"];
	freeString(logFacilityName);

	now = time(0);
	sprintf(str, "lookupd (version %s) starting - %s", _PROJECT_VERSION_, ctime(&now));

	/* remove ctime trailing newline */
	str[strlen(str) - 1] = '\0';
	system_log(LOG_DEBUG, str);

	maxThreads = [configManager intForKey:"MaxThreads" dict:global default:16];
	maxIdleThreads = [configManager intForKey:"MaxIdleThreads" dict:global default:16];
	maxIdleServers = [configManager intForKey:"MaxIdleServers" dict:global default:16];

	gValidation = [configManager boolForKey:"ValidateCache" dict:global default:YES];
	gMax = [configManager intForKey:"CacheCapacity" dict:global default:-1];
	if (gMax == 0) gMax = (unsigned int)-1;
	gTTL = (time_t)[configManager intForKey:"TimeToLive" dict:global default:43200];
	gDelta = (time_t)[configManager intForKey:"TimeToLiveDelta" dict:global default:0];
	gFreq = [configManager intForKey:"TimeToLiveFreq" dict:global default:0];

	for (i = 0; i < NCATEGORIES; i++)
	{
		lookupOrder[i] = [[LUArray alloc] init];
		sprintf(str, "Controller lookup order for category %s", [LUAgent categoryName:i]);
		[lookupOrder[i] setBanner:str];

		cLUorder = gLUorder;
		config = [configManager configForCategory:(LUCategory)i];
		if (config != nil)
		{
			cLUorder = [config valuesForKey:"LookupOrder"];
			if (cLUorder == NULL) cLUorder = gLUorder;
		}

		cValidation = [configManager boolForKey:"ValidateCache" dict:config default:gValidation];
		cMax = [configManager intForKey:"CacheCapacity" dict:config default:gMax];
		if (cMax == 0) cMax = (unsigned int)-1;
		cTTL = (time_t)[configManager intForKey:"TimeToLive" dict:config default:gTTL];
		cDelta = (time_t)[configManager intForKey:"TimeToLiveDelta" dict:config default:gDelta];
		cFreq = [configManager intForKey:"TimeToLiveFreq" dict:config default:gFreq];

		[cacheAgent setCacheIsValidated:cValidation forCategory:(LUCategory)i];
		[cacheAgent setCapacity:cMax forCategory:(LUCategory)i];
		[cacheAgent setTimeToLive:cTTL forCategory:(LUCategory)i];
		[cacheAgent addTimeToLive:cDelta afterCacheHits:cFreq forCategory:(LUCategory)i];
		
		[self setLookupOrder:cLUorder forCategory:(LUCategory)i];
	}

}

- (id)initWithName:(char *)name
{
	char str[128];
	DNSAgent *dns;

	[super init];

	dnsSearchList = NULL;

	controller = self;

 	if (name == NULL) portName = NULL;
	else portName = copyString((char *)name);

	sprintf(str, "Controller");
	[self setBanner:str];

	if (![self registerPort:portName]) return nil;

	serverLock = syslock_new(1);

	serverList = [[LUArray alloc] init];
	[serverList setBanner:"Controller server list"];

	statistics = [[LUDictionary alloc] init];
	[statistics setBanner:"lookupd statistics"];
	

	[statistics setValue:_PROJECT_BUILD_INFO_ forKey:"# build"];
	[statistics setValue:_PROJECT_VERSION_    forKey:"# version"];

	[self newAgent:[CacheAgent class] name:"CacheAgent"];
	[self newAgent:[DNSAgent class] name:"DNSAgent"];
	[self newAgent:[NIAgent class] name:"NIAgent"];
	[self newAgent:[FFAgent class] name:"FFAgent"];
	[self newAgent:[YPAgent class] name:"YPAgent"];
	[self newAgent:[LDAPAgent class] name:"LDAPAgent"];
	[self newAgent:[NILAgent class] name:"NILAgent"];

	[self initConfig];

	loginUser = nil;

	machRPC = [[MachRPC alloc] init:self];

	dns = [[DNSAgent alloc] init];
	if (dns != nil)
	{
		dnsSearchList = [dns searchList];
		[dns release];
	}

	return self;
}

- (char *)portName
{
	return portName;
}

- (char **)dnsSearchList
{
	return dnsSearchList;
}

- (void)dealloc
{
	int i;

	if (dnsSearchList != NULL) freeList(dnsSearchList);
	dnsSearchList = NULL;

	[machRPC release];
	machRPC = nil;

	if (serverList != nil)
		[serverList release];

	[NIAgent releaseDomainStore];

	if (portName != NULL) 
	{
		sys_destroy_service(portName);
		freeString(portName);
	}
	portName = NULL;

#ifdef _SHADOW_
	sys_port_free(server_port_privileged);
	sys_port_free(server_port_unprivileged);
#endif
	sys_port_free(server_port);

	if (loginUser != nil) [loginUser release];

	syslock_free(serverLock);

	for (i = 0; i < NCATEGORIES; i++)
	{
		if (lookupOrder[i] != nil) [lookupOrder[i] release];
	}

	if (statistics != nil) [statistics release];
	freeList(agentNames);
	agentNames = NULL;
	free(agents);

	[super dealloc];
}

- (BOOL)registerPort:(char *)name
{
	kern_return_t status;

	if (portName == NULL) return YES;

	system_log(LOG_DEBUG, "Registering service \"%s\"", portName);

	if (streq(name, DefaultName))
	{
		/*
		 * If server_port is already set, this is a restart.
		 */
		if (server_port != SYS_PORT_NULL) return YES;

#ifdef _SHADOW_
		status = sys_port_alloc(&server_port_unprivileged);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't allocate unprivileged server port!");
		}

		status = sys_port_alloc(&server_port_privileged);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't allocate privileged server port!");
			return NO;
		}

		status = port_set_allocate(task_self(), &server_port);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't allocate server port set!");
			return NO;
		}

		status = port_set_add(task_self(), server_port, server_port_unprivileged);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't add unprivileged port to port set!");
			return NO;
		}

		status = port_set_add(task_self(), server_port, server_port_privileged);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't add privileged port to port set!");
			return NO;
		}
#else
		if (sys_port_alloc(&server_port) != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't allocate server port!");
			return NO;
		}
#endif

#ifdef _OS_VERSION_MACOS_X_
		status = mach_port_insert_right(mach_task_self(), server_port, server_port, MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS)
		{
			system_log(LOG_ERR, "Can't insert send right for server port!");
			return NO;
		}
#endif

#ifdef _SHADOW_
		/*
		 * _lookupd_port(p) registers the unprivileged lookupd port p.
		 * _lookupd_port1(q) registers the privileged lookupd port q.
		 * Clients get ports with _lookup_port(0) and _lookup_port1(0).
		 */

		if (_lookupd_port(server_port_unprivileged) != server_port_unprivileged)
		{
			system_log(LOG_ERR, "Can't check in unprivileged server port!");
			return NO;
		}

		if (_lookupd_port1(server_port_privileged) != server_port_privileged)
		{
			system_log(LOG_ERR, "Can't check in privileged server port!");
		}
#else
		/* _lookupd_port(p) registers the lookupd port p. */
		if (_lookupd_port(server_port) != server_port)
		{
			system_log(LOG_ERR, "Can't check in server port!");
			return NO;
		}
#endif
		return YES;
	}

	status = sys_create_service(name, &server_port);
	if (status == KERN_SUCCESS) return YES;

	system_log(LOG_ERR, "Can't create service! (error %d)", status);
	return NO;
}

- (void)startServerThread
{
	Thread *t;

	syslock_lock(serverLock);

	/*
	 * Create the thread
	 */
	t = [[Thread alloc] init];
	[t setName:"IPC Server"];
	[t run:@selector(serverLoop) context:self];

	syslock_unlock(serverLock);

	system_log(LOG_DEBUG, "Started IPC Server");
}

/*
 * Get an idle server from the server list
 */
- (LUServer *)checkOutServer
{
	LUServer *server;
	int i, len;

	syslock_lock(serverLock);
	server = nil;

	len = [serverList count];

	for (i = 0; i < len; i++)
	{
		if ([[serverList objectAtIndex:i] isIdle])
		{
			server = [serverList objectAtIndex:i];
			[server setIsIdle:NO];
			break;
		}
	}

	if (server == nil)
	{
		/*
		 * No servers available - create a new server 
		 */
		server = [[LUServer alloc] init];
		[server setIsIdle:NO];

		for (i = 0; i < NCATEGORIES; i++)
			[server setLookupOrder:lookupOrder[i] forCategory:(LUCategory)i];

		[serverList addObject:server];
		[server release];
	}

	syslock_unlock(serverLock);
	return server;
}

- (void)checkInServer:(LUServer *)server
{
	int i, len, idleServerCount;

	[cacheAgent sweepCache];
	syslock_lock(serverLock);

	[server setIsIdle:YES];

	idleServerCount = 0;
	len = [serverList count];
	for (i = 0; i < len; i++)
	{
		if ([[serverList objectAtIndex:i] isIdle]) idleServerCount++;
	}

	if (idleServerCount > maxIdleServers)
	{
		[serverList removeObject:server];
	}

	syslock_unlock(serverLock);
}

- (void)setLoginUser:(int)uid
{
	LUServer *s;
	char scratch[256];

	if (loginUser != nil)
	{
		[cacheAgent removeObject:loginUser];
		[loginUser release];
		loginUser = nil;
	}

	sprintf(scratch, "%d", uid);
	s = [self checkOutServer];
	loginUser = [s itemWithKey:"uid" value:scratch category:LUCategoryUser];
	[self checkInServer:s];

	if (loginUser != nil)
	{
		[cacheAgent addObject:loginUser];
		[loginUser setTimeToLive:(time_t)-1];
		sprintf(scratch, "%s (console user)", [loginUser banner]);
		[loginUser setBanner:scratch];
	}
}

- (void)flushCache
{
	[cacheAgent flushCache];
}

- (void)suspend
{
	/* XXX suspend */
}

/*
 * This is just here to send a message to the server port.
 * This wakes the thread blocked on message receive in serverLoop.
 */
- (void)lookupdMessage
{
	sys_port_type s;
	kern_return_t status;
	int proc;

	s = lookupd_port(portName);
	status = _lookup_link(s, "_getstatistics", &proc);

	[[Thread currentThread] terminateSelf];
}

@end
