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
 * LUNIDomain.m
 *
 * NetInfo client for lookupd
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "LUNIDomain.h"
#import "LUGlobal.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import "Controller.h"
#import "Config.h"
#import <NetInfo/dsutil.h>
#ifdef RPC_SUCCESS
#undef RPC_SUCCESS
#endif
#import <netinfo/ni.h>
#import <stdio.h>
#import <sys/param.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <sys/types.h>
#import <net/if.h>
#import <netinet/if_ether.h>
#import <string.h>
#import <unistd.h>
#import <stdlib.h>
#import <libc.h>

extern char *nettoa(unsigned long);
extern unsigned long sys_address(void);

#define IAmLocal 0
#define IAmNotLocal 1
#define IDontKnow 2

static int did_init = 0;
static unsigned long ni_connect_timeout = 300;

char *pathForCategory[] =
{
	"/users",
	"/groups",
	"/machines",
	"/networks",
	"/services",
	"/protocols",
	"/rpcs",
	"/mounts",
	"/printers",
	"/machines",
	"/machines",
	"/aliases",
	"/netdomains",
	NULL,
	NULL,
	NULL,
	NULL
};

@implementation LUNIDomain

+ (void)initStatic
{
	LUDictionary *config;

	if (did_init != 0) return;
	
	did_init++;

	config = [configManager configForAgent:"NIAgent"];
	if (config == nil) return;
	
	ni_connect_timeout = [configManager intForKey:"ConnectTimeout" dict:config default:ni_connect_timeout];
	[config release];
}

- (void)initKeys
{
	userKeys = NULL;
	userKeys = appendString("name", userKeys);
	userKeys = appendString("passwd", userKeys);
	userKeys = appendString("uid", userKeys);
	userKeys = appendString("gid", userKeys);
	userKeys = appendString("realname", userKeys);
	userKeys = appendString("home", userKeys);
	userKeys = appendString("shell", userKeys);
#ifdef _UNIX_BSD_44_
	userKeys = appendString("class", userKeys);
	userKeys = appendString("change", userKeys);
	userKeys = appendString("expire", userKeys);
#endif

	groupKeys = NULL;
	groupKeys = appendString("name", groupKeys);
	groupKeys = appendString("passwd", groupKeys);
	groupKeys = appendString("gid", groupKeys);
	groupKeys = appendString("users", groupKeys);

	hostKeys = NULL;
	hostKeys = appendString("name", hostKeys);
	hostKeys = appendString("ip_address", hostKeys);
	hostKeys = appendString("en_address", hostKeys);
	hostKeys = appendString("bootfile", hostKeys);
	hostKeys = appendString("bootparams", hostKeys);

	bootparamKeys = NULL;
	bootparamKeys = appendString("name", bootparamKeys);
	bootparamKeys = appendString("bootparams", bootparamKeys);

	networkKeys = NULL;
	networkKeys = appendString("name", networkKeys);
	networkKeys = appendString("address", networkKeys);

	serviceKeys = NULL;
	serviceKeys = appendString("name", serviceKeys);
	serviceKeys = appendString("port", serviceKeys);
	serviceKeys = appendString("protocol", serviceKeys);

	protocolKeys = NULL;
	protocolKeys = appendString("name", protocolKeys);
	protocolKeys = appendString("number", protocolKeys);

	rpcKeys = NULL;
	rpcKeys = appendString("name", rpcKeys);
	rpcKeys = appendString("number", rpcKeys);

	mountKeys = NULL;
	mountKeys = appendString("name", mountKeys);
	mountKeys = appendString("dir", mountKeys);
	mountKeys = appendString("opts", mountKeys);
#ifdef _UNIX_BSD_44_
	mountKeys = appendString("vfstype", mountKeys);
	mountKeys = appendString("freq", mountKeys);
	mountKeys = appendString("passno", mountKeys);
#endif

	aliasKeys = NULL;
	aliasKeys = appendString("name", aliasKeys);
	aliasKeys = appendString("members", aliasKeys);
}

- (void)freeKeys
{
	freeList(userKeys);
	userKeys = NULL;
	freeList(groupKeys);
	groupKeys = NULL;
	freeList(hostKeys);
	hostKeys = NULL;
	freeList(networkKeys);
	networkKeys = NULL;
	freeList(serviceKeys);
	serviceKeys = NULL;
	freeList(protocolKeys);
	protocolKeys = NULL;
	freeList(rpcKeys);
	rpcKeys = NULL;
	freeList(mountKeys);
	mountKeys = NULL;
	freeList(bootparamKeys);
	bootparamKeys = NULL;
	freeList(aliasKeys);
	aliasKeys = NULL;
}

+ (void *)handleForTag:(char *)tag address:(struct sockaddr_in *)addr
{
	void *domain;
	int a;
	unsigned long t;
	ni_status status;
	ni_id root;

	if (tag == NULL) return NULL;
	if (addr == NULL) return NULL;
	if (addr->sin_addr.s_addr == -1) return NULL;

	syslock_lock(rpcLock);
	domain = ni_connect(addr, tag);
	syslock_unlock(rpcLock);

	if (domain == NULL) return NULL;

	a = 1;
	t = ni_connect_timeout;

	if (streq(tag, "local") && (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK)))
	{
		a = 0;
		t = 0;
	}

	ni_setabort(domain, a);
	ni_setreadtimeout(domain, t);

	root.nii_object = 0;

	syslock_lock(rpcLock);	
	status = ni_self(domain, &root);
	syslock_unlock(rpcLock);

	if (status != NI_OK) 
	{
		system_log(LOG_ALERT,
			"NetInfo open failed for %s@%s (%s)",
			tag, inet_ntoa(addr->sin_addr), ni_error(status));
		ni_free(domain);
		return NULL;
	}

	ni_setabort(domain, 1);
	ni_setreadtimeout(domain, ni_connect_timeout);

	return domain;
}

+ (void *)handleForPath:(char *)path address:(struct sockaddr_in *)addr
{
	void *d0, *d1;
	ni_status status;
	char **parts;
	int a, i;
	unsigned long t;
	ni_id root;

	if (path == NULL) return NULL;
	if (addr == NULL) return NULL;
	if (addr->sin_addr.s_addr == -1) return NULL;

	root.nii_object = 0;

	syslock_lock(rpcLock);
	d0 = ni_connect(addr, "local");
	syslock_unlock(rpcLock);

	if (d0 == NULL) return NULL;

	a = 1;
	t = ni_connect_timeout;

	if (streq(path, ".") && (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK)))
	{
		a = 0;
		t = 0;
	}

	ni_setabort(d0, a);
	ni_setreadtimeout(d0, t);

	syslock_lock(rpcLock);	
	status = ni_self(d0, &root);
	syslock_unlock(rpcLock);

	if (status != NI_OK) 
	{
		system_log(LOG_ALERT,
			"NetInfo open failed for local@%s (%s)",
			inet_ntoa(addr->sin_addr), ni_error(status));
		ni_free(d0);
		return NULL;
	}
		
	ni_setabort(d0, 1);
	ni_setreadtimeout(d0, ni_connect_timeout);

	if (streq(path, ".")) return d0;

	status = NI_OK;
	while (status == NI_OK)
	{
		syslock_lock(rpcLock);
		status = ni_open(d0, "..", &d1);
		if (status == NI_OK) status = ni_self(d1, &root);
		syslock_unlock(rpcLock);

		if (status == NI_OK)
		{
			ni_free(d0);
			d0 = d1;
		}
		if (streq(path, "..")) return d0;
	}

	if (streq(path, "/")) return d0;

	parts = explode(path+1, "/");

	for (i = 0; parts[i] != NULL; i++)
	{
		syslock_lock(rpcLock);
		status = ni_open(d0, parts[i], &d1);
		if (status == NI_OK) status = ni_self(d1, &root);
		syslock_unlock(rpcLock);

		if (status != NI_OK)
		{
			system_log(LOG_ALERT,
				"NetInfo open failed for domain component %s at address %s (%s)",
				parts[i], inet_ntoa(addr->sin_addr), ni_error(status));
			freeList(parts);
			return NULL;
		}
		ni_free(d0);
		d0 = d1;
	}

	freeList(parts);
	return d0;
}

+ (void *)handleForName:(char *)name
{
	void *domain;
	char *p_colon, *p_at;
	struct sockaddr_in server;

	if (name == NULL) return NULL;
	if (did_init == 0) [LUNIDomain initStatic];

	/*
	 * names may be of the following formats:
	 *
	 * path -> domain with given pathname
	 * tag@address -> connect by tag to given address
	 * path@address -> path relative to local domain at host
	 *
	 * niserver:tag -> connect by tag, localhost
	 * niserver:tag@address -> connect by tag
	 * nidomain:path -> domain with given pathname
	 * nidomain:path@address -> path relative to local domain at host
	 */

	domain = NULL;

	p_colon = strchr(name, ':');
	p_at = strchr(name, '@');

	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (p_at != NULL)
	{
		*p_at = '\0';
		server.sin_addr.s_addr = inet_addr(p_at + 1);
	}

	if (p_colon != NULL)
	{
		if (!strncmp(name, "niserver:", 9))
			domain = [LUNIDomain handleForTag:name+9 address:&server];

		if (!strncmp(name, "nidomain:", 9))
			domain = [LUNIDomain handleForPath:name+9 address:&server];
	}
	else
	{
		if ((name[0] == '/') || (name[0] == '.'))
			domain = [LUNIDomain handleForPath:name address:&server];

		else domain = [LUNIDomain handleForTag:name address:&server];
	}

	if (domain == NULL)
	{
		system_log(LOG_ALERT, "NetInfo open failed for %s", name);
	}

	if (p_at != NULL) *p_at = '@';

	return domain;
}

/*
 * Initialize a client for a domain given an open handle
 */
- (LUNIDomain *)initWithHandle:(void *)handle
{
	[super init];

	parent = nil;
	iAmRoot = NO;
	mustSetChecksumPassword = YES;
	isLocal = IDontKnow;
	masterHostName = NULL;
	masterTag = NULL;
	currentServerHostName = NULL;
	currentServerAddress = NULL;
	currentServerTag = NULL;
	mustSetMaxChecksumAge = YES;
	lastChecksum = (unsigned int)-1;
	lastChecksumFetch.tv_sec = 0;
	lastChecksumFetch.tv_usec = 0;
	maxChecksumAge = 15;
	[self initKeys];
	ni = handle;
	return self;
}

/*
 * Initialize a client for a domain by name
 */
- (LUNIDomain *)initWithDomainNamed:(char *)domainName
{
	void *d;
	char str[256];

	d = [LUNIDomain handleForName:domainName];
	if (d == NULL) return nil;

	[self initWithHandle:d];
	sprintf(str, "LUNIDomain %s", domainName);
	[self setBanner:str];
	return self;
}

- (void)dealloc
{
	freeString(myDomainName);
	myDomainName = NULL;
	freeString(masterHostName);
	masterHostName = NULL;
	freeString(masterTag);
	masterTag = NULL;
	freeString(currentServer);
	currentServer = NULL;
	freeString(currentServerHostName);
	currentServerHostName = NULL;
	freeString(currentServerAddress);
	currentServerAddress = NULL;
	freeString(currentServerTag);
	currentServerTag = NULL;
	[self freeKeys];
	ni_free(ni);
	if (parent != nil) [parent release];
	[super dealloc];
}

- (void)setTimeout:(unsigned long)t
{
	ni_setabort(ni, 1);
	ni_setreadtimeout(ni, t);
}

- (void)setMaxChecksumAge:(time_t)age
{
	maxChecksumAge = age;
}

/*
 * Create a client for a domain's parent domain.
 * Returns nil if the domain is root.
 */
- (LUNIDomain *)parent
{
	ni_status status;
	void *handle;
	char str[512];
	ni_name dn;
	ni_id root;

	if (iAmRoot) return nil;
	if (parent != nil) return parent;

	root.nii_object = 0;

	syslock_lock(rpcLock);
	status = ni_open(ni, "..", &handle);
	if (status == NI_OK) status = ni_self(handle, &root);

	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		iAmRoot = YES;
		return nil;
	}

	parent = [[LUNIDomain alloc] initWithHandle:handle];

	syslock_lock(rpcLock);
	ni_pwdomain(handle, &dn);
	syslock_unlock(rpcLock);

	sprintf(str, "LUNIDomain %s", dn);
	free(dn);
	[parent setBanner:str];

	return parent;
}

/*
 * Is this domain the root domain?
 */
- (BOOL)isRootDomain
{
	return ([self parent] == nil);
}

/*
 * Is this a "local" domain?
 */
- (BOOL)isLocalDomain
{
	if (isLocal == IDontKnow)
	{
		if (strcmp([self masterTag], "local") == 0)
			isLocal = IAmLocal;
		else
			isLocal = IAmNotLocal;
	}
	if (isLocal == IAmLocal) return YES;
	return NO;
}

/*
 * Get a child's domain's name relative to this domain.
 */
- (char *)nameForChild:(LUNIDomain *)child
{
	char mtag[1024], str[64], *name, *p;
	ni_status status;
	ni_id dir;
	ni_namelist nl;
	int i, len;
	BOOL searching;

	if (child == nil) return NULL;

	sprintf(mtag, "%s", [child masterTag]);
	if (strcmp(mtag, "local") == 0)
	{
		/* look for child's ip_address */
		sprintf(str, "/machines/ip_address=%s", [child currentServerAddress]);
	}
	else
	{
		/* look for the child's master */
		sprintf(str, "/machines/%s", [child masterHostName]);
	}

	syslock_lock(rpcLock);
	status = ni_pathsearch(ni, &dir, str);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return NULL;

	/* get the "serves" property namelist */
	NI_INIT(&nl);

	syslock_lock(rpcLock);
	status = ni_lookupprop(ni, &dir, "serves", &nl);
	syslock_unlock(rpcLock);

	if (status != NI_OK || nl.ni_namelist_len == 0) return NULL;

	/* walk through the serves property values */
	/* looking for <name>/<tag> */
	searching = YES;
	p = NULL;
	len = nl.ni_namelist_len;
	for (i = 0; i < len && searching; i++)
	{
		p = index(nl.ni_namelist_val[i], '/');

		if (strcmp(mtag, p+1) == 0)
		{
			/* BINGO - found the child domain */
			searching = NO;
		}
	}

	/* return nil if not found */
	if (searching)
	{
		ni_namelist_free(&nl);
		return NULL;
	}

	/* copy out the domain name */
	p[0] = '\0';
	i--; /* we went around the loop one extra time */

	name = copyString(nl.ni_namelist_val[i]);
	ni_namelist_free(&nl);
	return name;
}

/*
 * Domain name
 */
- (const char *)name
{
	char *myName;
	const char *parentName;

	if (myDomainName != NULL) return myDomainName;

	if ([self isRootDomain])
	{
		myDomainName = copyString("/");
		return myDomainName;
	}

	myName = [parent nameForChild:self];
	if (myName == NULL)
	{
		myDomainName = copyString("<?>");
		return myDomainName;
	}

	if ([parent isRootDomain])
	{
		myDomainName = malloc(strlen(myName) + 2);
		sprintf(myDomainName, "/%s", myName);	
		freeString(myName);
		return myDomainName;
	}

	parentName = [parent name];
	if (parentName == NULL)
	{
		myDomainName = malloc(strlen(myName) + 5);
		sprintf(myDomainName, "<?>/%s", myName);
		freeString(myName);
		return myDomainName;
	}

	myDomainName = malloc(strlen(parentName) + strlen(myName) + 2);
	sprintf(myDomainName, "%s/%s", parentName, myName);	
	freeString(myName);
	return myDomainName;
}

- (char *)currentServerAddress
{
	if (currentServerAddress == NULL) [self currentServer];
	return currentServerAddress;
}

/*
 * Look up the master's hostname from the master property
 */
- (char *)masterHostName
{
	ni_id dir;
	ni_namelist val;
	ni_status status;
	char *p;

	if (masterHostName != NULL) return masterHostName;
	dir.nii_object = 0;
	NI_INIT(&val);

	syslock_lock(rpcLock);
	status = ni_lookupprop(ni, &dir, "master", &val);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		system_log(LOG_ALERT, "Domain <?>: can't get master property");
		return NULL;
	}
	if (val.ni_namelist_len == 0)
	{
		system_log(LOG_ALERT, "Domain <?>: master property has no value");
		return NULL;
	}

	p = (char *)val.ni_namelist_val[0];
	while ((p[0] != '/') && (p[0] != '\0')) p++;
	if (p[0] != '/')
	{
		system_log(LOG_ALERT, "Domain <?>: malformend master property");
		ni_namelist_free(&val);
		return NULL;
	}

	p[0] = '\0';
	p++;

	freeString(masterHostName);
	masterHostName = copyString(val.ni_namelist_val[0]);

	freeString(masterTag);
	masterTag = copyString(p);

	ni_namelist_free(&val);
	return masterHostName;
}

/*
 * Get up the master's tag
 * The real work is done in -masterHostName
 */
- (char *)masterTag
{
	if (masterTag == NULL)
	{
		[self masterHostName];
	}

	return masterTag;
}

/*
 * Get the current server's address, tag, and host name.
 */
- (char *)currentServer
{
	struct sockaddr_in addr;
	ni_name tag;
	ni_status status;
	LUDictionary *host;
	char *hName;
	char str[MAXHOSTNAMELEN];

	syslock_lock(rpcLock);
	status = ni_addrtag(ni, &addr, &tag);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		system_log(LOG_ALERT, "Domain %s: can't get address and tag of current server",
				[self name]);
		return NULL;
	}

	if ((addr.sin_addr.s_addr == currentServerIPAddr) &&
		(streq(tag, currentServerTag)))
	{
		ni_name_free(&tag);
		return currentServer;
	}

	if (addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
		addr.sin_addr.s_addr = sys_address();

	freeString(currentServer);
	currentServer = NULL;

	freeString(currentServerTag);
	currentServerTag = copyString(tag);
	ni_name_free(&tag);

	freeString(currentServerAddress);
	currentServerAddress = copyString(inet_ntoa(addr.sin_addr));

	freeString(currentServerHostName);
	currentServerHostName = NULL;

	currentServerIPAddr = addr.sin_addr.s_addr;

	host = [self entityForCategory:LUCategoryHost
		key:"ip_address" value:currentServerAddress selectedKeys:NULL];

	if (host != nil)
	{
		hName = [host valueForKey:"name"];
		if (hName != NULL) currentServerHostName = copyString(hName);
	}
	else if (addr.sin_addr.s_addr == sys_address())
	{
		if (gethostname(str, MAXHOSTNAMELEN) >= 0)
			currentServerHostName = copyString(str);
	}

	if (currentServerHostName != NULL)
	{
		currentServer = malloc(strlen(currentServerHostName) + 
			strlen(currentServerTag) + 2);
		sprintf(currentServer, "%s/%s",
			currentServerHostName, currentServerTag);
	}
	else
	{
		currentServer = malloc(strlen(currentServerAddress) + 
			strlen(currentServerTag) + 2);
		sprintf(currentServer, "%s/%s",
			currentServerAddress, currentServerTag);
	}

	[host release];
	return currentServer;
}

/*
 * Get the current server's host name.
 */
- (char *)currentServerHostName
{

	if (currentServerAddress == NULL)
	{
		[self currentServerAddress];
	}

	return currentServerHostName;
}

/*
 * Get the current server's tag.
 */
- (char *)currentServerTag
{
	if (currentServerAddress == NULL)
	{
		[self currentServerAddress];
	}
	return currentServerTag;
}

/*
 * Get the server's checksum (can lag real checksum)
 */
- (unsigned long)checksum
{
	struct timeval now;
	time_t age;
	LUDictionary *config;
	BOOL globalHasAge;
	BOOL agentHasAge;
	time_t agentAge;
	time_t globalAge;

	if (mustSetMaxChecksumAge)
	{
		agentAge = 0;
		agentHasAge = NO;
		config = [configManager configForAgent:"NIAgent"];
		if (config != nil)
		{
			if ([config valueForKey:"ValidationLatency"] != NULL)
			{
				agentAge = [config unsignedLongForKey:"ValidationLatency"];
				agentHasAge = YES;
			}
			[config release];
		}

		globalAge = 0;
		globalHasAge = NO;
		config = [configManager configForAgent:NULL];
		if (config != nil)
		{
			if ([config valueForKey:"ValidationLatency"] != NULL)
			{
				globalAge = [config unsignedLongForKey:"ValidationLatency"];
				globalHasAge = YES;
			}
			[config release];
		}

		if (agentHasAge) maxChecksumAge = agentAge;
		else if (globalHasAge) maxChecksumAge = globalAge;

		mustSetMaxChecksumAge = NO;
	}

	if ((maxChecksumAge == 0) || (lastChecksumFetch.tv_sec == 0))
		return [self currentChecksum];

	gettimeofday(&now, (struct timezone *)NULL);
	age = now.tv_sec - lastChecksumFetch.tv_sec;
	if (age > maxChecksumAge) return [self currentChecksum];

	return lastChecksum;
}

/*
 * Get the current server's checksum.
 */
- (unsigned long)currentChecksum
{
	ni_status status;
	ni_proplist pl;
	unsigned long sum;
	ni_index where;

	if (mustSetChecksumPassword)
	{
		/* Special hack to only lookup the checksum */
		ni_setpassword(ni, "checksum");
		mustSetChecksumPassword = NO;
	}

	NI_INIT(&pl);

	syslock_lock(rpcLock);
	status = ni_statistics(ni, &pl);
	syslock_unlock(rpcLock);

	/* checksum should be first (and only!) property */
	where = NI_INDEX_NULL;
	if (pl.ni_proplist_len > 0)
	{
		if (strcmp(pl.ni_proplist_val[0].nip_name, "checksum"))
			where = 0;
		else
			where = ni_proplist_match(pl, "checksum", NULL);
	}

	if (where == NI_INDEX_NULL)
	{
		system_log(LOG_ERR, "Domain %s: can't get checksum", [self name]);
		ni_proplist_free(&pl);
		return (unsigned long)-1;
	}

	sscanf(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], "%lu", &sum);
	ni_proplist_free(&pl);

	lastChecksum = sum;
	gettimeofday(&lastChecksumFetch, (struct timezone *)NULL);

	return sum;
}

/*
 * Read a directory and turn it into a dictionary.
 * Coalesce duplicate keys.
 */
- (LUDictionary *)readDirectory:(unsigned long)d
	selectedKeys:(char **)keyList
{
	ni_id dir;
	ni_proplist pl;
	ni_status status;
	ni_property *p;
	int i, len;
	LUDictionary *dict;

	NI_INIT(&pl);
	dir.nii_object = d;

	syslock_lock(rpcLock);
	status = ni_read(ni, &dir, &pl);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return nil;

	dict = [[LUDictionary alloc] init];
	len = pl.ni_proplist_len;

	/* split this way to take the test out of the loop */
	if (keyList == NULL)
	{
		for (i = 0; i < len; i++)
		{
			p = &(pl.ni_proplist_val[i]);
			[dict addValues:p->nip_val.ni_namelist_val
				forKey:p->nip_name count:p->nip_val.ni_namelist_len];
		}
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			p = &(pl.ni_proplist_val[i]);
			if (listIndex(p->nip_name, keyList) == IndexNull) continue;
			[dict addValues:p->nip_val.ni_namelist_val
				forKey:p->nip_name count:p->nip_val.ni_namelist_len];

		}
	}
	ni_proplist_free(&pl);
	return dict;
}

- (LUDictionary *)readDirectoryName:(char *)name
	selectedKeys:(char **)keyList
{
	ni_id dir;
	ni_status status;

	syslock_lock(rpcLock);
	status = ni_pathsearch(ni, &dir, name);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return nil;
	return [self readDirectory:dir.nii_object selectedKeys:keyList];
}

/*
 * Look up a directory given a key and a value, within a 
 * given category of objects (users, hosts, etc).
 *
 * Searches for the first directory with key=value.
 * Returns a dictionary, with all the directory's keys as
 * dictionary keys.  Values are always arrays, which may be
 * empty for keys with no values.  If the directory has 
 * duplicate keys, all values are coalesced in the array.
 */
- (LUDictionary *)entityForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal
{
	return [self entityForCategory:cat key:aKey value:aVal selectedKeys:NULL];
}

/*
 * Look up a directory given a key and a value, within a 
 * given category of objects (users, hosts, etc).
 *
 * Searches for the first directory with key=value.
 * Returns a dictionary, with the directory's keys as
 * dictionary keys.  Only those keys in keyList are
 * included.  nil means that all keys are included in the
 * output.  Values are always arrays, which may be
 * empty for keys with no values.  If the directory has 
 * duplicate keys, all values are coalesced in the array.
 */
- (LUDictionary *)entityForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal
	selectedKeys:(char **)keyList
{
	ni_id parent_dir;
	ni_status status;
	LUDictionary *dict;
	ni_idlist idl;
	char str[256], *path;

	path = pathForCategory[cat];
	if (path == NULL)
	{
		system_log(LOG_ERR, "Domain %s: unsupported lookup category %d", [self name], cat);
		return nil;
	}	

	syslock_lock(rpcLock);
	status = ni_pathsearch(ni, &parent_dir, path);
	syslock_unlock(rpcLock);
	if (status != NI_OK) return nil;

	NI_INIT(&idl);

	syslock_lock(rpcLock);
	status = ni_lookup(ni, &parent_dir, aKey, aVal, &idl);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		/* No match */
		return nil;
	}

	if (idl.ni_idlist_len == 0)
	{
		/* No match */
		return nil;
	}

	dict = [self readDirectory:idl.ni_idlist_val[0] selectedKeys:keyList];
	ni_idlist_free(&idl);

	if (dict != nil)
	{
		sprintf(str, "NIAgent: %s %s", [LUAgent categoryName:cat], aVal);
		[dict setBanner:str];
		if ((cat == LUCategoryAlias) && ([self isLocalDomain]))
			[dict addValue:"1" forKey:"alias_local"];
	}
	
	return dict;
}

/*
 * Look up all directory within a given category of objects
 * (users, hosts, etc).
 *
 * Returns an array of dictionaries.  Dictionaries are the same
 * as those returned by -entityForCategory:key:value:
 */
- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	return [self allEntitiesForCategory:cat selectedKeys:NULL];
}

/*
 * Look up all directory within a given category of objects
 * (users, hosts, etc).
 *
 * Returns an array of dictionaries.  Dictionaries are the same
 * as those returned by -entityForCategory:key:value:selectedKeys:
 */
- (LUArray *)allEntitiesForCategory:(LUCategory)cat
	selectedKeys:(char **)keyList
{
	return [self allEntitiesForCategory:cat 
		key:NULL value:NULL selectedKeys:keyList];
}

- (LUArray *)allEntitiesForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal
	selectedKeys:(char **)keyList
{
	ni_id parent_dir;
	ni_entrylist all;
	ni_idlist kids;
	ni_status status;
	LUDictionary *dict;
	LUArray *list;
	int i, j, len, nkeys;
	char *path;
	BOOL localAlias;

	path = pathForCategory[cat];
	if (path == NULL)
	{
		system_log(LOG_ERR, "Domain %s: unsupported lookup category %d", [self name], cat);
		return nil;
	}	

	syslock_lock(rpcLock);
	status = ni_pathsearch(ni, &parent_dir, path);
	syslock_unlock(rpcLock);
	if (status != NI_OK) return nil;

	localAlias = NO;
	if ((cat == LUCategoryAlias) && [self isLocalDomain]) localAlias = YES;

	/*
	 * If the keyList is NULL, we interate through all directories.
	 * We need to do this for printers, where keys are variable.
	 * We also use this code when given keys and values, since
	 * ni_lookup can be used to find just those directories.
	 */
	if ((keyList == NULL) || (aKey != NULL) || (aVal != NULL))
	{
		NI_INIT(&kids);

		syslock_lock(rpcLock);
		if ((aKey == NULL) || (aVal == NULL))
		{
			status = ni_children(ni, &parent_dir, &kids);
		}
		else
		{
			status = ni_lookup(ni, &parent_dir, aKey, aVal, &kids);
		}
		syslock_unlock(rpcLock);

		if (status != NI_OK) return nil;

		list = [[LUArray alloc] init];

		for (i = 0; i < kids.ni_idlist_len; i++)
		{
			dict = [self readDirectory:kids.ni_idlist_val[i]
				selectedKeys:keyList];
			if (dict != nil)
			{
				if (localAlias) [dict addValue:"1" forKey:"alias_local"];
				[list addObject:dict];
				[dict release];
			}
		}
		ni_idlist_free(&kids);
		return list;
	}

	nkeys = listLength(keyList);
	if (nkeys == 0) return nil;

	/* get all directories */
	len = 0;
	list = [[LUArray alloc] init];

	for (i = 0; i < nkeys; i++)
	{
		NI_INIT(&all);

		syslock_lock(rpcLock);
		status = ni_list(ni, &parent_dir, keyList[i], &all);
		syslock_unlock(rpcLock);

		if (status != NI_OK)
		{
			[list release];
			return nil;
		}

		len = all.ni_entrylist_len;
		for (j = 0; j < len; j++)
		{
			if (i == 0)
			{
				/*
				 * Must check if ids in the list we just got
				 * match existing ids.  Need to store id in dict
				 * quick hack: set cacheHits = id 
				 */

				dict = [[LUDictionary alloc] init];
				if (localAlias) [dict addValue:"1" forKey:"alias_local"];
				[dict setCacheHits:all.ni_entrylist_val[j].id];
				[list addObject:dict];
				[dict release];
			}
			else
			{
				dict = [list objectAtIndex:j];
				if ([dict cacheHits] != all.ni_entrylist_val[j].id)
				{
					/*
					 * Yikes! Someone added or deleted directories!
					 * Try again, but just iterate through the
					 * child dirs.  This is slower, but safer.
					 */
					ni_entrylist_free(&all);
					[list releaseObjects];

					NI_INIT(&kids);

					syslock_lock(rpcLock);
					status = ni_children(ni, &parent_dir, &kids);
					syslock_unlock(rpcLock);

					if (status != NI_OK) 
					{
						[list release];
						return nil;
					}
					for (i = 0; i < kids.ni_idlist_len; i++)
					{
						dict = [self readDirectory:kids.ni_idlist_val[i]
							selectedKeys:keyList];
						if (dict != nil)
						{
							if (localAlias) [dict addValue:"1" forKey:"alias_local"];
							[list addObject:dict];
							[dict release];
						}
					}
					ni_idlist_free(&kids);
					return list;
				}
			}
			if (all.ni_entrylist_val[j].names != NULL)
			{
				if (all.ni_entrylist_val[j].names->ni_namelist_len > 0)
					[dict
						setValues:
							all.ni_entrylist_val[j].names->ni_namelist_val
						forKey:
							keyList[i]
						count:
							all.ni_entrylist_val[j].names->ni_namelist_len];
			}
		}

		ni_entrylist_free(&all);
	}

	if (len > 0)
	{
		/* clean up from cacheHits hack */
		len = [list count];
		for (j = 0; j < len; j++) [[list objectAtIndex:j] setCacheHits:0];
	}

	return list;
}

/*
 * Look up a directory with two key/value pairs.
 * This is primarily an optimization for getServiceWithXXX
 */
- (LUDictionary *)entityForCategory:(LUCategory)cat
	key:(char *)key1
	value:(char *)val1
	key:(char *)key2
	value:(char *)val2
	selectedKeys:(char **)keyList
{
	ni_id parent_dir;
	ni_status status;
	LUDictionary *dict;
	ni_idlist idl1;
	ni_idlist idl2;
	int i, len1, j, len2;
	BOOL searching;
	unsigned long id1;
	char *path;

	path = pathForCategory[cat];
	if (path == NULL)
	{
		system_log(LOG_ERR, "Domain %s: unsupported lookup category %d", [self name], cat);
		return nil;
	}	

	syslock_lock(rpcLock);
	status = ni_pathsearch(ni, &parent_dir, path);
	syslock_unlock(rpcLock);
	if (status != NI_OK) return nil;

	NI_INIT(&idl1);
	NI_INIT(&idl2);

	syslock_lock(rpcLock);
	status = ni_lookup(ni, &parent_dir, key1, val1, &idl1);
	if (status == NI_OK) 
		status = ni_lookup(ni, &parent_dir, key2, val2, &idl2);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		/* No match */
		return nil;
	}

	len1 = idl1.ni_idlist_len;
	len2 = idl2.ni_idlist_len;

	if ((len1 == 0) || (len2 == 0))
	{
		/* No match */
		return nil;
	}

	/*
	 * Look for a directory that's in both lists
	 */
	searching = YES;
	id1 = idl1.ni_idlist_val[0];
	for (i = 0; (i < len1) && searching; i++)
	{
		id1 = idl1.ni_idlist_val[i];
		for (j = 0; (j < len2) && searching; j++)
		{
			searching = !(id1 == idl2.ni_idlist_val[j]);
		}
	}

	if (searching)
	{
		dict = nil;
	}
	else
	{
		dict = [self readDirectory:id1 selectedKeys:keyList];
	}

	ni_idlist_free(&idl1);
	ni_idlist_free(&idl2);
	return dict;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	return [self entityForCategory:cat key:key value:val selectedKeys:NULL];
}

/************************* CUSTOM LOOKUP ROUTINES *************************/

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	if (prot == NULL)
	{
		return [self entityForCategory:LUCategoryService
			key:"name" value:name selectedKeys:serviceKeys];
	}

	return [self entityForCategory:LUCategoryService
		key:"name" value:name key:"protocol" value:prot selectedKeys:NULL];
}

- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	char str[32];

	sprintf(str, "%d", *number);

	if (prot == NULL)
	{
		return [self entityForCategory:LUCategoryService
			key:"port" value:str selectedKeys:serviceKeys];
	}

	return [self entityForCategory:LUCategoryService
		key:"port" value:str key:"protocol" value:prot selectedKeys:NULL];
}

- (LUDictionary *)netgroupWithName:(char *)name
{
	LUDictionary *item;
	LUDictionary *group;
	LUArray *them;
	int i, tlen;
	int nlen;
	char **keys = NULL;
	BOOL found;

	keys = appendString("name", keys);
	found = NO;

	/* search /users, /machines, and /netdomains for this netgroup */
	group = [[LUDictionary alloc] init];
	[group setValue:name forKey:"name"];

	/* Get hosts with this netgroup */
	them = [self allEntitiesForCategory:LUCategoryHost key:"netgroups"
		value:name selectedKeys:keys];
	tlen = [them count];
	for (i = 0; i < tlen; i++)
	{
		item = [them objectAtIndex:i];
		nlen = [item countForKey:"name"];
		if (nlen > 0)
		{
			found = YES;
			[group addValues:[item valuesForKey:"name"] forKey:"hosts"];	
		}
	}
	[them release];

	/* Get users with this netgroup */
	them = [self allEntitiesForCategory:LUCategoryUser key:"netgroups"
		value:name selectedKeys:keys];
	tlen = [them count];
	for (i = 0; i < tlen; i++)
	{
		item = [them objectAtIndex:i];
		nlen = [item countForKey:"name"];
		if (nlen > 0)
		{
			found = YES;
			[group addValues:[item valuesForKey:"name"] forKey:"users"];	
		}
	}
	[them release];

	/* Get domains with this netgroup */
	them = [self allEntitiesForCategory:LUCategoryNetDomain key:"netgroups"
		value:name selectedKeys:keys];
	tlen = [them count];
	for (i = 0; i < tlen; i++)
	{
		item = [them objectAtIndex:i];
		nlen = [item countForKey:"name"];
		if (nlen > 0)
		{
			found = YES;
			[group addValues:[item valuesForKey:"name"] forKey:"domains"];	
		}
	}
	[them release];


	freeList(keys);
	keys = NULL;

	return group;
}

/*
 * Custom lookup for security options
 *
 * Special case: "all" enables all security options
 */
- (BOOL)isSecurityEnabledForOption:(char *)option
{
	LUDictionary *root;
	char **security;

	root = [self readDirectory:0 selectedKeys:NULL];
	security = [root valuesForKey:"security_options"];
	if (security == NULL)
	{
		[root release];
		return NO;
	}

	if (listIndex("all", security) != IndexNull)
	{
		[root release];
		return YES;
	}
	if (listIndex(option, security) != IndexNull)
	{
		[root release];
		return YES;
	}
	[root release];
	return NO;
}

/*
 * Custom lookup for netware
 */
- (BOOL)checkNetwareEnabled
{
	LUDictionary *nw;
	char **en;

	nw = [self readDirectoryName:"/locations/NetWare" selectedKeys:NULL];
	if (nw == nil) return NO;

	en = [nw valuesForKey:"enabled"];
	if (en == NULL)
	{
		[nw release];
		return NO;
	}

	if (listIndex("YES", en) != IndexNull)
	{
		[nw release];
		return YES;
	}
	[nw release];
	return NO;
}

/*
 * Custom lookup for initgroups()
 *
 * Returns an array of all groups containing a user
 * (including default group)
 */
- (LUArray *)allGroupsWithUser:(char *)name
{
	LUArray *allGroups;
	LUDictionary *user;
	LUDictionary *group;
	char **ga;
	char *gid;
	int i, len, j, ngrps, dgid;
	BOOL new;

	/* get all the groups for which the user is a member */
	allGroups = [self allEntitiesForCategory:LUCategoryGroup
		key:"users" value:name selectedKeys:NULL];

	if (allGroups == nil) allGroups = [[LUArray alloc] init];

	/* add in the user's default group */
	user = [self entityForCategory:LUCategoryUser key:"name" value:name
		selectedKeys:NULL];
	if (user == nil)
	{
		/* User isn't in this domain */
		return allGroups;
	}
	
	ga = [user valuesForKey:"gid"];
	if (ga == NULL)
	{
		/* user has no default group */
		[user release];
		return allGroups;
	}

	len = [user countForKey:"gid"];
	if (len < 0) len = 0;
	for (i = 0; i < len; i++)
	{
		gid = ga[i];
		dgid = atoi(gid);
		group = [self entityForCategory:LUCategoryGroup key:"gid" value:gid
			selectedKeys:NULL];
		if (group == nil) continue;

		/* is this group already in allGroups */
		ngrps = [allGroups count];
		new = YES;
		for (j = 0; j < ngrps; j++)
		{
			if (dgid == atoi([[allGroups objectAtIndex:i] valueForKey:"gid"]))
			{
				new = NO;
				break;
			}
		}
		if (new)
		{
			[allGroups addObject:group];
			[group release];
		}
	}

	[user release];
	return allGroups;
}

@end
