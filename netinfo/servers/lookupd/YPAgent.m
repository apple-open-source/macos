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
 * YPAgent.m
 *
 * NIS lookup agent for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "YPAgent.h"
#import "Config.h"
#import "LUGlobal.h"
#import "LUPrivate.h"
#import "Controller.h"
#import "LUArray.h"
#import "LUCachedDictionary.h"
#ifdef RPC_SUCCESS
#undef RPC_SUCCESS
#endif
#import <rpc/rpc.h>
#import <rpcsvc/yp_prot.h>
#import <rpcsvc/ypclnt.h>
#import <string.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <sys/types.h>
#import <net/if.h>
#import <netinet/if_ether.h>
#import <stdio.h>
#import <stdlib.h>
#import <NetInfo/dsutil.h>

#include <NetInfo/config.h>
#ifdef _UNIX_BSD_43_
#define xdr_domainname xdr_ypdomain_wrap_string
#endif

#ifdef _UNIX_BSD_44_
extern int _yplib_timeout;
extern void _yp_unbind();
#endif

@interface FFParser (FFParserPrivate)
- (char **)tokensFromLine:(const char *)data separator:(const char *)sep;
@end

extern int close(int);
extern char *nettoa(unsigned long);

extern unsigned long sys_address(void);
#define BUFSIZE 8192

typedef char *domainname;
typedef char *mapname;
typedef struct {
	u_int keydat_len;
	char *keydat_val;
} keydat;

typedef struct {
	u_int valdat_len;
	char *valdat_val;
} valdat;

struct _my_ypreq_key {
	domainname domain;
	mapname map;
	keydat key;
};

struct _my_ypresp_val {
	long status;
	valdat val;
};

typedef struct _my_ypreq_key _my_ypreq_key;
typedef struct _my_ypresp_val _my_ypresp_val;

bool_t
_my_xdr_valdat(xdrs, objp)
	XDR *xdrs;
	valdat *objp;
{

	if (!xdr_bytes(xdrs, (char **)&objp->valdat_val, (u_int *)&objp->valdat_len, YPMAXRECORD))
		return (FALSE);
	return (TRUE);
}


bool_t
_my_xdr_ypresp_val(xdrs, objp)
	XDR *xdrs;
	_my_ypresp_val *objp;
{

	if (!xdr_long(xdrs, &objp->status))
		return (FALSE);
	if (!_my_xdr_valdat(xdrs, &objp->val))
		return (FALSE);
	return (TRUE);
}


bool_t
_my_xdr_domainname(xdrs, objp)
	XDR *xdrs;
	domainname *objp;
{

	if (!xdr_string(xdrs, objp, YPMAXDOMAIN))
		return (FALSE);
	return (TRUE);
}

bool_t
_my_xdr_mapname(xdrs, objp)
	XDR *xdrs;
	mapname *objp;
{

	if (!xdr_string(xdrs, objp, YPMAXMAP))
		return (FALSE);
	return (TRUE);
}

bool_t
_my_xdr_keydat(xdrs, objp)
	XDR *xdrs;
	keydat *objp;
{

	if (!xdr_bytes(xdrs, (char **)&objp->keydat_val, (u_int *)&objp->keydat_len, YPMAXRECORD))
		return (FALSE);
	return (TRUE);
}

bool_t
_my_xdr_ypreq_key(xdrs, objp)
	XDR *xdrs;
	_my_ypreq_key *objp;
{

	if (!_my_xdr_domainname(xdrs, &objp->domain))
		return (FALSE);
	if (!_my_xdr_mapname(xdrs, &objp->map))
		return (FALSE);
	if (!_my_xdr_keydat(xdrs, &objp->key))
		return (FALSE);
	return (TRUE);
}

@implementation YPAgent

- (char *)matchKey:(char *)key map:(char *)map
{
	char *outval;
	int outlen;
	struct dom_binding *ysd;
	struct timeval tv;
	_my_ypreq_key yprk;
	_my_ypresp_val yprv;
	int tries, status;

	syslock_lock(rpcLock);

	for (tries = 0; tries < 3; tries++)
	{
		if (_yp_dobind(domainName, &ysd) != 0) return NULL;

		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		yprk.domain = domainName;
		yprk.map = map;
		yprk.key.keydat_val = key;
		yprk.key.keydat_len = strlen(key);

		memset(&yprv, 0, sizeof(yprv));

		outval = NULL;
		outlen = 0;

		status = clnt_call(ysd->dom_client, YPPROC_MATCH, _my_xdr_ypreq_key, &yprk, _my_xdr_ypresp_val, &yprv, tv);
		if (status == RPC_SUCCESS) break;

		system_log(LOG_ERR, clnt_sperror(ysd->dom_client, "clnt_call"));
		ysd->dom_vers = -1;
	}

	if (status == RPC_SUCCESS)
	{
		if (ypprot_err(yprv.status) == 0)
		{
			outlen = yprv.val.valdat_len;
			if (outlen > 0)
			{
				outval = malloc(outlen + 1);
				memmove(outval, yprv.val.valdat_val, outlen);
				outval[outlen] = '\0';
			}
		}
	}

	xdr_free(xdr_ypresp_val, (char *)&yprv);
#ifdef _UNIX_BSD_44_
	_yp_unbind(ysd);
#endif

	syslock_unlock(rpcLock);

	return outval;
}

- (char *)currentServerName
{
	struct in_addr server;
	struct sockaddr_in query;
	CLIENT *client;
	int sock = RPC_ANYSOCK;
	enum clnt_stat rpc_stat;
	struct ypbind_resp response;
	struct timeval tv = { 10, 0 };
	char *key, *buf;
	int buflen, len;
	char **tokens = NULL;

	query.sin_family = AF_INET;
	query.sin_port = 0;
	query.sin_addr.s_addr = sys_address();
	bzero(query.sin_zero, 8);

	syslock_lock(rpcLock);

	client = clntudp_create(&query, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client == NULL)
	{
		system_log(LOG_ERR, clnt_spcreateerror("clntudp_create"));
		syslock_unlock(rpcLock);
		return NULL;
	}

	buflen = strlen(domainName);

	rpc_stat = clnt_call(client, YPBINDPROC_DOMAIN,
	    xdr_domainname, &domainName,
		xdr_ypbind_resp, &response, tv);

	if (rpc_stat != RPC_SUCCESS)
	{
		system_log(LOG_ERR, clnt_sperror(client, "clnt_call"));
		clnt_destroy(client);
		close(sock);
		syslock_unlock(rpcLock);
		return NULL;
	}

	server = response.ypbind_respbody.ypbind_bindinfo.ypbind_binding_addr;
	clnt_destroy(client);
	close(sock);

	syslock_unlock(rpcLock);

	key = inet_ntoa(server);
	buf = NULL;

	freeString(currentServerName);
	currentServerName = NULL;

	buf = [self matchKey:key map:"hosts.byaddr"];

	if (buf == NULL)
	{
		currentServerName = copyString(key);
	}
	else
	{
		/* pull out the host name */
		tokens = [parser tokensFromLine:buf separator:" \t"];
		if (tokens == NULL) len = 0;
		else len = listLength(tokens);

		if (len < 2)
		{
			currentServerName = copyString(key);
		}
		else
		{
			currentServerName = copyString(tokens[1]);
		}

		freeList(tokens);
		freeString(buf);
	}

	return currentServerName;
}

- (YPAgent *)init
{
	char *dn, *str;
	LUDictionary *config;
	int status;

	if (didInit) return self;

	[super init];

	yp_get_default_domain(&dn);	
	if (dn == NULL)
	{
		[self release];
		return nil;
	}

	status = yp_bind(dn);
	if (status != 0)
	{
		system_log(LOG_ERR, yperr_string(status));
		[self release];
		return nil;
	}

	str = malloc(strlen(dn) + 16);
	sprintf(str, "YPAgent (%s)", dn);
	[self setBanner:str];
	free(str);

	domainName = copyString(dn);

	mapValidationTable = [[LUDictionary alloc] init];
	[mapValidationTable setBanner:"YP Map Validation Table"];

	parser = [[FFParser alloc] init];

	config = [configManager configGlobal:configurationArray];
	timeout = [configManager intForKey:"Timeout" dict:config default:30];
	validationLatency = [configManager intForKey:"ValidationLatency"dict:config default:15];

	config = [configManager configForAgent:"YPAgent" fromConfig:configurationArray];
	timeout = [configManager intForKey:"Timeout" dict:config default:timeout];
	validationLatency = [configManager intForKey:"ValidationLatency" dict:config default:validationLatency];

#ifdef _UNIX_BSD_44_
	if (timeout != 0)
	{
		_yplib_timeout = timeout;
	}
#endif

	return self;
}

- (LUAgent *)initWithArg:(char *)arg
{
	return [self init];
}

+ (YPAgent *)alloc
{
	YPAgent *agent;

	agent = [super alloc];
	system_log(LOG_DEBUG, "Allocated YPAgent 0x%08x\n", (int)agent);
	return agent;
}

- (const char *)shortName
{
	return "YP";
}

- (void)dealloc
{
	freeString(currentServerName);
	currentServerName = NULL;

	freeString(domainName);
	domainName = NULL;

	if (mapValidationTable != nil) [mapValidationTable release];
	if (parser != nil) [parser release];

	system_log(LOG_DEBUG, "Deallocated YPAgent 0x%08x\n", (int)self);

	[super dealloc];
}

- (char *)orderNumberForMap:(char *)map
{
	char *val;
	unsigned long lastOrder;
	char *out;
	struct timeval now;
	time_t lastTime;
	time_t age;
	char *mapEntry;
	char scratch[256];

	gettimeofday(&now, (struct timezone *)NULL);

	/*
	 * Each map entry is a string of the form "time order#"
	 * each is an unsigned long.
	 */
	mapEntry = [mapValidationTable valueForKey:map];

	if (mapEntry != NULL)
	{
		sscanf(mapEntry, "%lu %lu", &lastTime, &lastOrder);
		age = now.tv_sec - lastTime;
		if (age <= validationLatency)
		{
			sprintf(scratch, "%lu", lastOrder);
			out = copyString(scratch);
			return out;
		}

		[mapValidationTable removeKey:map];
	}

	val = [self matchKey:"YP_LAST_MODIFIED" map:map];
	if (val == NULL) return copyString("");

#ifdef _UNIX_BSD_43_
	sprintf(scratch, "%lu %s", now.tv_sec, val);
#else
	sprintf(scratch, "%u %s", now.tv_sec, val);
#endif

	[mapValidationTable setValue:scratch forKey:map];

	return val;
}

- (LUDictionary *)stamp:(LUDictionary *)item
	map:(char *)map
	server:(char *)curr
	order:(char *)order
{
	if (item == nil) return nil;

	[item setAgent:self];
	[item setValue:"NIS" forKey:"_lookup_info_system"];
	[item setValue:domainName forKey:"_lookup_NIS_domain"];
	[item setValue:curr forKey:"_lookup_NIS_server"];
	[item setValue:map forKey:"_lookup_NIS_map"];
	[item setValue:order forKey:"_lookup_NIS_order"];

	return item;
}

- (LUDictionary *)parse:(char *)buf
	map:(char *)map
	category:(LUCategory)cat
	server:(char *)name
	order:(char *)order
{
	LUDictionary *item;
	char scratch[256];
	
	if (buf == NULL) return nil;
	item = [parser parse:buf category:cat];
	sprintf(scratch, "YPAgent: %s %s", [LUAgent categoryName:cat], [item valueForKey:"name"]);
	[item setBanner:scratch];

	return [self stamp:item map:map server:name order:order];
}

- (BOOL)isValid:(LUDictionary *)item
{
	char *oldOrder;
	char *newOrder;
	char *mapName;
	BOOL ret;

	if (item == nil) return NO;
	if ([self isStale]) return NO;

	mapName = [item valueForKey:"_lookup_NIS_map"];
	if (mapName == NULL) return NO;

	oldOrder = [item valueForKey:"_lookup_NIS_order"];
	if (oldOrder == NULL) return NO;
	if (oldOrder[0] == '\0') return NO;

	newOrder = [self orderNumberForMap:mapName];
	ret = YES;
	if (strcmp(oldOrder, newOrder)) ret = NO;
	freeString(newOrder);
	
	return ret;
}

- (char *)mapForKey:(char *)key category:(LUCategory)cat
{
	switch (cat)
	{
		case LUCategoryUser:
			if (streq(key, "name")) return "passwd.byname";
			if (streq(key, "uid")) return "passwd.byuid";
			return NULL;

		case LUCategoryGroup:
			if (streq(key, "name")) return "group.byname";
			if (streq(key, "gid")) return "group.bygid";
			return NULL;

		case LUCategoryHost:
			if (streq(key, "name")) return "hosts.byname";
			if (streq(key, "ip_address")) return "hosts.byaddr";
			return NULL;

		case LUCategoryNetwork:
			if (streq(key, "name")) return "networks.byname";
			if (streq(key, "address")) return "networks.byaddr";
			return NULL;

		case LUCategoryProtocol:
			if (streq(key, "name")) return "protocols.byname";
			if (streq(key, "number")) return "protocols.bynumber";
			return NULL;
		
		case LUCategoryRpc:
			if (streq(key, "name")) return "rpc.byname";
			if (streq(key, "number")) return "rpc.bynumber";
			return NULL;

		case LUCategoryMount:
			if (streq(key, "name")) return "mounts.byname";
			return NULL;

		case LUCategoryPrinter:
			if (streq(key, "name")) return "printcap.byname";
			return NULL;

		case LUCategoryBootparam:
			if (streq(key, "name")) return "bootparams.byname";
			return NULL;

		case LUCategoryBootp:
			if (streq(key, "ip_address")) return "bootptab.byaddr";
			if (streq(key, "en_address")) return "bootptab.byether";
			return NULL;

		case LUCategoryAlias:
			if (streq(key, "name")) return "mail.aliases";
			return NULL;

		case LUCategoryNetgroup: return "netgroup";

		default: return NULL;
	}

	return NULL;
}

/*
 * These methods do NIS lookups on behalf of all calls
 */

- (LUDictionary *)itemWithName:(char *)name
	map:(char *)map
	category:(LUCategory)cat
{
	LUDictionary *item;
	char *val = NULL;
	int vallen, keylen;
	char scratch[4096];
	char *curr;
	char *order;

	curr = [self currentServerName];
	order = [self orderNumberForMap:map];
	
	keylen = strlen(name);

	val = [self matchKey:name map:map];
	if (val == NULL)
	{
		freeString(order);
		return nil;
	}

	vallen = strlen(val);
	if (cat == LUCategoryNetgroup)
	{
		bcopy(name, scratch, keylen);
		scratch[keylen] = ' ';
		bcopy(val, scratch+keylen+1, vallen);
		scratch[keylen + vallen + 1] = '\0';
	}
	else
	{
		bcopy(val, scratch, vallen);
		scratch[vallen] = '\0';
	}

	freeString(val);

	item = [self parse:scratch map:map category:cat server:curr order:order];
	freeString(order);
	return item;
}

- (LUDictionary *)userWithRealName:(char *)name
{
	LUDictionary *anObject;
	char *key, *val, *lastkey;
	int status, keylen, vallen, lastlen;
	char scratch[4096];
	char *curr;
	char *order;

	curr = [self currentServerName];
	order = [self orderNumberForMap:"passwd.byname"];

	key = NULL;
	val = NULL;
	lastkey = NULL;

	/* NIS client doesn't support multi-threaded access */
	syslock_lock(rpcLock); // locked
	status = yp_first(domainName, "passwd.byname", &key, &keylen, &val, &vallen);
	if (status != 0)
	{
		syslock_unlock(rpcLock); // unlocked
		freeString(order);
		return nil;
	}

	while (status == 0)
	{
		bcopy(val, scratch, vallen);
		scratch[vallen] = '\0';

		freeString(val);
		val = NULL;

		anObject = [self parse:scratch map:"passwd.byname" category:LUCategoryUser server:curr order:order];

		freeString(lastkey);

		if (anObject != nil)
		{
			/* Check the user's real name */
			if ([anObject hasValue:name forKey:"realname"])
			{
				syslock_unlock(rpcLock); // unlocked
				freeString(order);
				return anObject;
			}

			[anObject release];
		}

		lastkey = key;
		lastlen = keylen;

		status = yp_next(domainName, "passwd.byname",
		    lastkey, lastlen, &key, &keylen, &val, &vallen);
	}

	syslock_unlock(rpcLock); // unlocked

	freeString(lastkey);
	freeString(order);
	
	return nil;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	char *map;
	
	if ((cat == LUCategoryUser) && (streq(key, "realname")))
	{
		return [self userWithRealName:val];
	}

	map = [self mapForKey:key category:cat];
	if (map == NULL) return nil;
	
	return [self itemWithName:val map:map category:cat];
}

- (LUArray *)allItemsInMap:(char *)map
	category:(LUCategory)cat
{
	LUArray *all;
	LUDictionary *anObject;
	LUDictionary *vstamp;
	char *key, *val, *lastkey;
	int status, keylen, vallen, lastlen;
	char scratch[4096];
	char *curr;
	char *order;

	all = [[LUArray alloc] init];
	sprintf(scratch, "YPAgent: all %s", [LUAgent categoryName:cat]);
	[all setBanner:scratch];

	curr = [self currentServerName];
	order = [self orderNumberForMap:map];

	key = NULL;
	val = NULL;
	lastkey = NULL;

	/* NIS client doesn't support multi-threaded access */
	syslock_lock(rpcLock); // locked {[
	status = yp_first(domainName, map, &key, &keylen, &val, &vallen);
	if (status != 0)
	{
		syslock_unlock(rpcLock); // ] unlocked
		[all release];
		freeString(order);
		return nil;
	}

	vstamp = [[LUDictionary alloc] init];
	sprintf(scratch, "YPAgent validation %s %s %s", curr, map, order);
	[vstamp setBanner:scratch];
	[self stamp:vstamp map:map server:curr order:order];
	[all addValidationStamp:vstamp];
	[vstamp release];

	while (status == 0)
	{
		switch (cat)
		{
			case LUCategoryNetgroup:
				bcopy(key, scratch, keylen);
				scratch[keylen] = ' ';
				bcopy(val, scratch+keylen+1, vallen);
				scratch[keylen + vallen + 1] = '\0';
				break;
			case LUCategoryAlias:
				bcopy(key, scratch, keylen);
				scratch[keylen] = ':';
				scratch[keylen + 1] = ' ';
				bcopy(val, scratch+keylen+2, vallen);
				scratch[keylen + vallen + 2] = '\0';
				break;
			default:
				bcopy(val, scratch, vallen);
				scratch[vallen] = '\0';
		}

		freeString(val);
		val = NULL;

		anObject = [self parse:scratch map:map category:cat server:curr order:order];
		if (anObject != nil)
		{
			[all addObject:anObject];
			[anObject release];
		}

		freeString(lastkey);
		lastkey = key;
		lastlen = keylen;

		status = yp_next(domainName, map,
		    lastkey, lastlen, &key, &keylen, &val, &vallen);
	}

	syslock_unlock(rpcLock); // } unlocked

	freeString(lastkey);
	freeString(order);

	return all;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	char *map;
	
	map = [self mapForKey:"name" category:cat];
	if (map == NULL) return nil;
	
	return [self allItemsInMap:map category:cat];
}

@end
