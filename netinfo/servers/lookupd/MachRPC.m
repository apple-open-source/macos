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
 * MachRPC.m
 *
 * Custom procedure calls on top of Mach IPC for lookupd
 * Libinfo uses this to talk to lookupd
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "MachRPC.h"
#import "CacheAgent.h"
#import "LUGlobal.h"
#import "Config.h"
#import "Thread.h"
#import "LUPrivate.h"
#import "MemoryWatchdog.h"
#import <mach/message.h>
#import <mach/mach_error.h>
#import <mach/mig_errors.h>
#import <netinfo/lookup_types.h>
#import "_lu_types.h"
#import <netdb.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <strings.h>
#import <NetInfo/dsutil.h>
#import <stdio.h>

/* 2 second timeout on sends */
#define TIMEOUT_MSECONDS (2000)

#define valNull			0
#define valInt			1
#define valString		2
#define valIPAddr		3
#define valIPV6Addr		4
#define valIPNet			5
#define valENAddr		6

extern boolean_t lookup_server(mach_msg_header_t *, mach_msg_header_t *);
extern char *proc_name(int);

extern char *nettoa(u_int32_t net);

@implementation MachRPC

- (MachRPC *)init:(id)sender
{
	int i;

	[super init];

	for (i = 0; i < NPROCS; i++)
	{
		proc_helper[i].type = nonStandardProc;
		proc_helper[i].encoder = @selector(encodeDictionary:intoXdr:);
	}

	/*
	 * getpwent (returns BSD4.3 data)
	 */
	i = PROC_GETPWENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwent-A (returns BSD4.4 data)
	 */
	i = PROC_GETPWENT_A;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwuid (returns BSD4.3 data)
	 */
	i = PROC_GETPWUID;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "uid";
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwuid_A (returns BSD4.4 data)
	 */
	i = PROC_GETPWUID_A;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "uid";
	proc_helper[i].cat = LUCategoryUser;

	/*
	 * getpwname (returns BSD4.3 data)
	 */
	i = PROC_GETPWNAM;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwname_A (returns BSD4.4 data)
	 */
	i = PROC_GETPWNAM_A;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryUser;

	/*
	 * getgrent
	 */
	i = PROC_GETGRENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryGroup;
	
	/*
	 * getgrgid
	 */
	i = PROC_GETGRGID;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "gid";
	proc_helper[i].cat = LUCategoryGroup;

	/*
	 * getgrnam
	 */
	i = PROC_GETGRNAM;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryGroup;

	/*
	 * gethostent
	 */
	i = PROC_GETHOSTENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryHost;
	proc_helper[i].encoder = @selector(encodeHost:intoXdr:);
	
	/*
	 * gethostbyname
	 */
	i = PROC_GETHOSTBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryHost;

	/*
	 * gethostbyaddr
	 */
	i = PROC_GETHOSTBYADDR;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valIPAddr;
	proc_helper[i].key = "ip_address";
	proc_helper[i].cat = LUCategoryHost;

	/*
	 * getipv6nodebyaddr
	 */
	i = PROC_GETIPV6NODEBYADDR;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valIPV6Addr;
	proc_helper[i].key = "ipv6_address";
	proc_helper[i].cat = LUCategoryHost;

	/*
	 * getnetent
	 */
	i = PROC_GETNETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryNetwork;
	
	/*
	 * getnetbyname
	 */
	i = PROC_GETNETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryNetwork;

	/*
	 * getnetbyaddr
	 */
	i = PROC_GETNETBYADDR;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valIPNet;
	proc_helper[i].key = "address";
	proc_helper[i].cat = LUCategoryNetwork;

	/*
	 * getservent
	 */
	i = PROC_GETSERVENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryService;
	
	/*
	 * getprotoent
	 */
	i = PROC_GETPROTOENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryProtocol;
	
	/*
	 * getprotobyname
	 */
	i = PROC_GETPROTOBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryProtocol;

	/*
	 * getprotobynumber
	 */
	i = PROC_GETPROTOBYNUMBER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "number";
	proc_helper[i].cat = LUCategoryProtocol;

	/*
	 * getrpcent
	 */
	i = PROC_GETRPCENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryRpc;
	
	/*
	 * getrpcbyname
	 */
	i = PROC_GETRPCBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryRpc;

	/*
	 * getrpcbynumber
	 */
	i = PROC_GETRPCBYNUMBER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "number";
	proc_helper[i].cat = LUCategoryRpc;

	/*
	 * getfsent
	 */
	i = PROC_GETFSENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeFS:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryMount;
	
	/*
	 * getfsbyname
	 */
	i = PROC_GETFSBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeFS:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryMount;

	/*
	 * prdb_get
	 */
	i = PROC_PRDB_GET;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryPrinter;
	
	/*
	 * grdb_getbyname
	 */
	i = PROC_PRDB_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryPrinter;

	/*
	 * bootparams_getent
	 */
	i = PROC_BOOTPARAMS_GETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryBootparam;
	
	/*
	 * bootparams_getbyname
	 */
	i = PROC_BOOTPARAMS_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryBootparam;
	
	/*
	 * bootp_getbyip
	 */
	i = PROC_BOOTP_GETBYIP;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valIPAddr;
	proc_helper[i].key = "ip_address";
	proc_helper[i].cat = LUCategoryBootp;

	/*
	 * bootp_getbyether
	 */
	i = PROC_BOOTP_GETBYETHER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valENAddr;
	proc_helper[i].key = "en_address";
	proc_helper[i].cat = LUCategoryBootp;

	/*
	 * alias_getbyname
	 */
	i = PROC_ALIAS_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryAlias;
	
	/*
	 * alias_getent
	 */
	i = PROC_ALIAS_GETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryAlias;
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)process
{
	kern_return_t status;
	BOOL lustatus;
	Thread *t;
	LUServer *server;
	lookup_reply_msg reply;
	lookup_request_msg *request;
	vm_address_t vm_buf;
	u_int32_t vm_len;

	t = [Thread currentThread];
	request = (lookup_request_msg *)[t data];
	[t setData:NULL];
	
	/*
	 * Use the MIG server to dispatch messages.
	 * Server functions for the MIG interface are in lookup_proc.m
	 */ 
	lustatus = lookup_server(&request->head, &reply.head);
	free(request);
	if (lustatus == NO)
	{
		system_log(LOG_DEBUG, "MachRPC process request: no reply");
		return;
	}

	status = sys_send_message(&reply.head, TIMEOUT_MSECONDS);

	if (status != KERN_SUCCESS)
	{
		if (status == MACH_SEND_INVALID_DEST)
		{
			sys_port_free(reply.head.msgh_remote_port);
		}
		else
		{
			system_log(LOG_ERR, "msg_send failed (%s)", sys_strerror(status));
		}
	}

	server = (LUServer *)[t server];
	[t setServer:NULL];

	vm_len = [t dataLen];
	if (vm_len > 0)
	{
		vm_buf = (vm_address_t)[t data];
		vm_deallocate(mach_task_self(), vm_buf, vm_len);
	}

	[t setData:NULL];
	[t setDataLen:0];

	if (!shutting_down) [controller checkInServer:server];
}

/*
 * Called by MIG server routines in lookup_proc.m
 */
- (BOOL)process:(int)procno
	inData:(char *)indata
	inLength:(unsigned int)inlen
	outData:(char **)outdata
	outLength:(unsigned int *)outlen
{
	LUDictionary *dict, *item;
	LUArray *list;
	LUServer *server;
	Thread *t;
	int i;
	int cat;
	char *key;
	char *val;
	char *name;
	char *proto;
	char **stuff;
	BOOL test;
	char logString[512];
	SEL aSel;

	t = [Thread currentThread];
	sprintf(logString, "%s", proc_name(procno));

	if (shutting_down) return NO;
	server = [controller checkOutServer];
	if (server == nil)
	{
		system_log(LOG_ERR, "%s: checkOutServer failed", [t name]);
		return NO;
	}

	[t setServer:(void *)server];

	key = NULL;
	val = NULL;
	cat = (LUCategory)-1;

	dict = nil;

	if (proc_helper[procno].type == standardListProc)
	{
		aSel = proc_helper[procno].encoder;
		cat = proc_helper[procno].cat;

		system_log(LOG_DEBUG, "%s", logString);

		list = [server allItemsWithCategory:cat];
		if (list == nil) return NO;

		test = [self xdrList:list method:aSel buffer:outdata length:outlen server:server];
		[list release];

		return test;
	}

	if (proc_helper[procno].type == standardDictionaryProc)
	{
		if ((inlen == 0) || (indata == NULL))
		{
			system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
			return NO;
		}

		aSel = proc_helper[procno].encoder;
		cat = proc_helper[procno].cat;
		key = proc_helper[procno].key;

		switch (proc_helper[procno].decoder)
		{
			case valInt:
				val = [self decodeInt:indata length:inlen];
				break;
			case valString:
				val = [self decodeString:indata length:inlen];
				break;
			case valIPAddr:
				val = [self decodeIPAddr:indata length:inlen];
				break;
			case valIPV6Addr:
				val = [self decodeIPV6Addr:indata length:inlen];
				break;
			case valIPNet:
				val = [self decodeIPNet:indata length:inlen];
				break;
			case valENAddr:
				val = [self decodeENAddr:indata length:inlen];
				break;
			default: val = NULL;
		}

		if ((val != NULL) && (val[0] == '\0'))
		{
			free(val);
			val = NULL;
		}
	
		if (val == NULL)
		{
			system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
			return NO;
		}

		system_log(LOG_DEBUG, "%s %s", logString, val);

		dict = [server itemWithKey:key value:val category:cat];
		free(val);
		val = NULL;

		if (dict == nil) return NO;

		test = [self xdrItem:dict method:aSel buffer:outdata length:outlen];
		[dict release];

		return test;
	}

	switch (procno)
	{
		case PROC_GETIPV6NODEBYNAME: /* NONSTANDARD */
			name = [self decodeString:indata length:inlen];
			if ((name != NULL) && (name[0] == '\0'))
			{
				free(name);
				name = NULL;
			}

			if (name == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			dict = [server ipv6NodeWithName:name];
			freeString(name);
			name = NULL;
			if (dict == nil) return NO;
			test = [self xdrItem:dict method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_SETPWENT:
		case PROC_ALIAS_SETENT:
			*outlen = 0;
			*outdata = NULL;
			system_log(LOG_DEBUG, "%s", logString);
			return YES;

		case PROC_GETSERVBYNAME: /* NONSTANDARD */
			stuff = [self twoStringsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			proto = stuff[1];
			system_log(LOG_DEBUG, "%s %s %s", logString, stuff[0], proto);
			if (proto[0] == '\0') proto = NULL;
			dict = [server serviceWithName:stuff[0] protocol:proto];
			if (proto != NULL) [dict setValue:proto forKey:"_lookup_service_protocol"];
			freeList(stuff);
			stuff = NULL;
			if (dict == nil) return NO;
			test = [self xdrItem:dict method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_GETSERVBYPORT: /* NONSTANDARD */
			stuff = [self intAndStringFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			i = atoi(stuff[0]);
			proto = stuff[1];
			system_log(LOG_DEBUG, "%s %d %s", logString, i, proto);
			if (proto[0] == '\0') proto = NULL;
			dict = [server serviceWithNumber:&i protocol:proto];
			if (proto != NULL) [dict setValue:proto forKey:"_lookup_service_protocol"];
			freeList(stuff);
			stuff = NULL;
			if (dict == nil) return NO;
			test = [self xdrItem:dict method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_FIND: /* NONSTANDARD */
			stuff = [self threeStringsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s category %s key %s value %s", logString, stuff[0], stuff[1], stuff[2]);
			i = [LUAgent categoryWithName:stuff[0]];
			if (i == -1) dict = nil;
			else dict = [server itemWithKey:stuff[1] value:stuff[2] category:i];
			freeList(stuff);
			stuff = NULL;
			if (dict == nil) return NO;
			test = [self xdrItem:dict method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_LIST: /* NONSTANDARD */
			name = [self decodeString:indata length:inlen];
			if ((name != NULL) && (name[0] == '\0'))
			{
				free(name);
				name = NULL;
			}

			if (name == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}
	
			system_log(LOG_DEBUG, "%s %s", logString, name);
			if (streq(name, "config"))
			{
				list = [configManager config];
			}
			else
			{
				i = [LUAgent categoryWithName:name];
				if (i == -1) list = nil;
				else list = [server allItemsWithCategory:i];
			}
			freeString(name);
			name = NULL;
			if (list == nil) return NO;
			test = [self xdrList:list method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen server:server];
			[list release];
			return test;

		case PROC_QUERY: /* NONSTANDARD */
			dict = [self dictionaryFromBuffer:indata length:inlen];
			if (dict == nil)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s", logString);
			name = [dict description];
			system_log(LOG_DEBUG, "%s", name);
			free(name);

			list = [server query:dict];
			[dict release];
			if (list == nil) return NO;
			test = [self xdrList:list method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen server:server];
			[list release];
			return test;

		case PROC_INITGROUPS: /* NONSTANDARD */
			name = [self decodeString:indata length:inlen];
			if ((name != NULL) && (name[0] == '\0'))
			{
				free(name);
				name = NULL;
			}

			if (name == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			dict = [server allGroupsWithUser:name];
			freeString(name);
			name = NULL;
			if (dict == nil) return NO;
			test = [self xdrInitgroups:dict buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_INNETGR: /* NONSTANDARD */
			stuff = [self inNetgroupArgsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s (%s, %s, %s)",
				logString, stuff[0], stuff[1], stuff[2], stuff[3]);
			test = [server inNetgroup:stuff[0]
				host:((stuff[1][0] == '\0') ? NULL : stuff[1])
				user:((stuff[2][0] == '\0') ? NULL : stuff[2])
				domain:((stuff[3][0] == '\0') ? NULL : stuff[3])];
			[self xdrInt:(test ? 1 : 0) buffer:outdata length:outlen];
			freeList(stuff);
			stuff = NULL;
			return YES;

		case PROC_GETNETGRENT: /* NONSTANDARD */
			name = [self decodeString:indata length:inlen];
			if ((name != NULL) && (name[0] == '\0'))
			{
				free(name);
				name = NULL;
			}

			if (name == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			dict = [server itemWithKey:"name" value:name category:LUCategoryNetgroup];
			freeString(name);
			name = NULL;
			if (dict == nil) return NO;
			test = [self xdrNetgroup:dict buffer:outdata length:outlen server:server];
			[dict release];
			return test;

		case PROC_CHECKSECURITYOPT: /* NONSTANDARD */
			name = [self decodeString:indata length:inlen];
			if ((name != NULL) && (name[0] == '\0'))
			{
				free(name);
				name = NULL;
			}

			if (name == NULL)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			test = [server isSecurityEnabledForOption:name];
			freeString(name);
			name = NULL;
			[self xdrInt:(test ? 1 : 0) buffer:outdata length:outlen];
			return YES;
	
		case PROC_CHECKNETWAREENBL: /* NONSTANDARD */
			system_log(LOG_DEBUG, "%s", logString);
			test = [server isNetwareEnabled];
			[self xdrInt:(test ? 1 : 0) buffer:outdata length:outlen];
			return YES;

		case PROC_SETLOGINUSER: /* NONSTANDARD */
			if ((inlen == 0) || (indata == NULL))
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			i = [self intFromBuffer:indata length:inlen];
			system_log(LOG_DEBUG, "%s %d", logString, i);
			if (!shutting_down) [controller setLoginUser:i];
			[self xdrInt:1 buffer:outdata length:outlen];
			return YES;

		case PROC__GETSTATISTICS: /* NONSTANDARD */
			system_log(LOG_DEBUG, "%s", logString);
			if (statistics == NULL) return NO;
			sprintf(logString, "%u", [rover totalMemory]);
			[statistics setValue:logString forKey:"# Total Memory"];
			test = [self xdrItem:statistics method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			return test;

		case PROC__INVALIDATECACHE: /* NONSTANDARD */
			system_log(LOG_DEBUG, "%s", logString);
			if (!shutting_down) [controller flushCache];
			[self xdrInt:0 buffer:outdata length:outlen];
			return YES;

		case PROC__SUSPEND: /* NONSTANDARD */
			system_log(LOG_DEBUG, "%s", logString);
			if (!shutting_down) [controller suspend];
			[self xdrInt:1 buffer:outdata length:outlen];
			return YES;

		case PROC_DNS_PROXY: /* NONSTANDARD */
			dict = [self dictionaryFromBuffer:indata length:inlen];
			if (dict == nil)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s", logString);
			name = [dict description];
			system_log(LOG_DEBUG, "%s", name);
			free(name);

			item = [server dns_proxy:dict];
			[dict release];
			if (item == nil) return NO;
			test = [self xdrItem:item method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[item release];
			return test;

		case PROC_GETADDRINFO: /* NONSTANDARD */
			dict = [self dictionaryFromBuffer:indata length:inlen];
			if (dict == nil)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s", logString);
			name = [dict description];
			system_log(LOG_DEBUG, "%s", name);
			free(name);

			list = [server getaddrinfo:dict];
			[dict release];
			if (list == nil) return NO;
			test = [self xdrList:list method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen server:server];
			[list release];
			return test;

		case PROC_GETNAMEINFO: /* NONSTANDARD */
			dict = [self dictionaryFromBuffer:indata length:inlen];
			if (dict == nil)
			{
				system_log(LOG_DEBUG, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s", logString);
			name = [dict description];
			system_log(LOG_DEBUG, "%s", name);
			free(name);

			item = [server getnameinfo:dict];
			[dict release];
			if (item == nil) return NO;
			test = [self xdrItem:item method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			[item release];
			return test;

		default: 
			system_log(LOG_DEBUG, "%s: unknown proc %d", [t name], procno);
			return NO;
	}

	return NO;
}

- (void)encodeHost:(LUDictionary *)item intoXdr:(lu_xdr_t *)xdrs
{
	u_int32_t count;

	/* Number of keys */
	count = 2;
	lu_xdr_u_int_32(xdrs, &count);

	[self encodeAttribute:"name" from:item intoXdr:xdrs count:(u_int32_t)-1];
	[self encodeAttribute:"ip_address" from:item intoXdr:xdrs count:(u_int32_t)-1];
}

- (void)encodeFS:(LUDictionary *)item intoXdr:(lu_xdr_t *)xdrs
{
	char *opts, *s;
	char type[64];
	char **optsList;
	int i, len;
	u_int32_t count;

	/* Number of keys */
	count = 7;
	lu_xdr_u_int_32(xdrs, &count);

	[self encodeAttribute:"name" from:item intoXdr:xdrs count:1];
	[self encodeAttribute:"dir" from:item intoXdr:xdrs count:1];
	[self encodeAttribute:"vfstype" from:item intoXdr:xdrs count:1];
	[self encodeAttribute:"freq" from:item intoXdr:xdrs count:1];
	[self encodeAttribute:"passno" from:item intoXdr:xdrs count:1];

	optsList = [item valuesForKey:"opts"];
	if (optsList == NULL) count = 0;
	else count = [item countForKey:"opts"];
	if (count < 0) count = 0;

	len = 0;
	for (i = 0; i < count; i++)
	{
		len += strlen(optsList[i]);
		if (i < (count - 1)) len++;
	}

	opts = malloc(len + 1);

	strcpy(type, "rw");

	opts[0] = '\0';
	for (i = 0; i < count; i++)
	{
		strcat(opts, optsList[i]);
		if (i < (count - 1)) strcat(opts, ",");
	
		if ((streq(optsList[i], "rw")) ||
			(streq(optsList[i], "rq")) ||
			(streq(optsList[i], "ro")) ||
			(streq(optsList[i], "sw")) ||
			(streq(optsList[i], "xx")))
		{
			strcpy(type, optsList[i]);
		}
	}

	count = 1;

	s = "opts";
	lu_xdr_string(xdrs, &s);
	lu_xdr_u_int_32(xdrs, &count);
	lu_xdr_string(xdrs, &opts);
	free(opts);

	s = "type";
	lu_xdr_string(xdrs, &s);
	lu_xdr_u_int_32(xdrs, &count);
	s = type;
	lu_xdr_string(xdrs, &s);
}

- (void)encodeDictionary:(LUDictionary *)item intoXdr:(lu_xdr_t *)xdrs
{
	char *key;
	int i, count;

	if (item == NULL) return;

	count = 0;
	for (i = 0; NULL != (key = [item keyAtIndex:i]); i++)
	{
		if (!strncmp(key, "_lookup_", 8)) continue;
		count++;
	}

	lu_xdr_int_32(xdrs, &count);

	for (i = 0; NULL != (key = [item keyAtIndex:i]); i++)
	{
		if (!strncmp(key, "_lookup_", 8)) continue;
		[self encodeAttribute:key from:item intoXdr:xdrs count:-1];
	}
}

- (BOOL)xdrNetgroup:(LUDictionary *)item buffer:(char **)data length:(int *)len server:(LUServer *)server
{
	u_int32_t i, count;
	char **names, *s, *dash;
	lu_xdr_t *outxdr;
	int32_t status;

	if (item == nil) return NO;

	count = 0;
	i = [item countForKey:"hosts"];
	if (i != IndexNull) count += i;
	i = [item countForKey:"users"];
	if (i != IndexNull) count += i;
	i = [item countForKey:"domains"];
	if (i != IndexNull) count += i;

	*len = 0;
	outxdr = lu_xdr_alloc(0, 0);

	status = lu_xdr_u_int_32(outxdr, &count);
	if (status != 0)
	{
		lu_xdr_free(outxdr);
		return NO;
	}

	/* XXX Netgroups as members of other netgroups not supported! */

	dash = "-";

	s = "hosts";
	names = [item valuesForKey:s];
	count = [item countForKey:s];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		lu_xdr_string(outxdr, &(names[i]));
		lu_xdr_string(outxdr, &dash);
		lu_xdr_string(outxdr, &dash);
	}

	s = "users";
	names = [item valuesForKey:s];
	count = [item countForKey:s];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		lu_xdr_string(outxdr, &dash);
		lu_xdr_string(outxdr, &(names[i]));
		lu_xdr_string(outxdr, &dash);
	}

	s = "domains";
	names = [item valuesForKey:s];
	count = [item countForKey:s];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		lu_xdr_string(outxdr, &dash);
		lu_xdr_string(outxdr, &dash);
		lu_xdr_string(outxdr, &(names[i]));
	}

	*data = outxdr->buf;
	*len = outxdr->datalen;

	free(outxdr);

	return YES;
}

- (BOOL)xdrInt:(int)i buffer:(char **)data length:(int *)len
{
	lu_xdr_t *outxdr;
	int32_t status;
	
	outxdr = lu_xdr_alloc(0, 0);

	status = lu_xdr_int_32(outxdr, &i);
	if (status != 0)
	{
		system_log(LOG_ERR, "lu_xdr_int_32 failed");
		lu_xdr_free(outxdr);
		return NO;
	}

	*len = lu_xdr_getpos(outxdr);
	*data = outxdr->buf;
	free(outxdr);

	return YES;
}

- (BOOL)xdrList:(LUArray *)list
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len
	server:(LUServer *)server
{
	u_int32_t i, count;
	static LUDictionary *item;
	lu_xdr_t *outxdr;
	int32_t status;

	if (list == nil) return NO;
	
	outxdr = lu_xdr_alloc(0, 0);
	count = [list count];

	*len = 0;

	status = lu_xdr_u_int_32(outxdr, &count);
	if (status != 0)
	{
		lu_xdr_free(outxdr);
		return NO;
	}

	for (i = 0; i < count; i++)
	{
		item = [list objectAtIndex:i];
		[self perform:method with:item with:(id)outxdr];
	}

	*data = outxdr->buf;
	*len = outxdr->datalen;

	free(outxdr);

	return YES;
}

- (BOOL)xdrItem:(LUDictionary *)item
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len
{
	lu_xdr_t *outxdr;
	int count;

	outxdr = lu_xdr_alloc(0, 0);
	if (outxdr == NULL) return NO;

	count = 0;
	if (item != nil) count = 1;

	lu_xdr_int_32(outxdr, &count);

	if (count == 0)
	{		
		*len = lu_xdr_getpos(outxdr);
		*data = outxdr->buf;
		free(outxdr);
		return YES;
	}

	[self perform:method with:item with:(id)outxdr];

	*len = lu_xdr_getpos(outxdr);
	*data = outxdr->buf;
	free(outxdr);

	return YES;
}

- (BOOL)xdrInitgroups:(LUDictionary *)item buffer:(char **)data length:(int *)len
{
	lu_xdr_t *outxdr;
	char **gids;
	int i, count;
	int n;

	if (item == nil) return NO;
	gids = [item valuesForKey:"gid"];
	if (gids == NULL) return NO;

	outxdr = lu_xdr_alloc(0, 0);

	count = [item countForKey:"gid"];
	if (count < 0) count = 0;
	if (count > NGROUPS) count = NGROUPS;

	lu_xdr_int_32(outxdr, &count);

	for (i = 0; i < count; i++)
	{
		n = atoi(gids[i]);
		lu_xdr_int_32(outxdr, &n);
	}

	*len = lu_xdr_getpos(outxdr);
	*data = outxdr->buf;
	free(outxdr);

	return YES;
}

- (unsigned int)memorySize
{
	unsigned int size, i;

	size = [super memorySize];

	size += 4;

	for (i = 0; i < NPROCS; i++)
	{
		size += 20;
		if (proc_helper[i].key != NULL) size += (strlen(proc_helper[i].key) + 1);
	}

	return size;
}

- (void)encodeAttribute:(char *)key from:(LUDictionary *)item intoXdr:(lu_xdr_t *)xdrs count:(unsigned long)n
{
	u_int32_t i, len;
	char **values;

	if (key == NULL) return;

	values = [item valuesForKey:key];
	len = [item countForKey:key];

	if (len == IndexNull) len = 0;
	if (len > n) len = n;

	for (i = 0; i < len; i++)
	{
		if (values[i] == NULL) return;
	}

	lu_xdr_string(xdrs, &key);
	lu_xdr_u_int_32(xdrs, &len);

	for (i = 0; i < len; i++)
	{
		lu_xdr_string(xdrs, &(values[i]));
	}
}

/* 
 * decode routines
 */
- (char *)decodeString:(char *)buf length:(int)len;
{
	char *str;
	lu_xdr_t *inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	str = NULL;
	lu_xdr_string(inxdr, &str);
	lu_xdr_free(inxdr);

	return str;
}

- (char *)decodeInt:(char *)buf length:(int)len
{
	char *str;
	int32_t i, status;
	lu_xdr_t *inxdr;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_int_32(inxdr, &i);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	str = malloc(16);
	sprintf(str, "%d", i);

	return str;
}

- (char *)decodeIPAddr:(char *)buf length:(int)len
{
	struct in_addr ip;
	char *str;
	int32_t i, status;
	lu_xdr_t *inxdr;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_int_32(inxdr, &i);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	ip.s_addr = i;
	str = malloc(16);
	if (inet_ntop(AF_INET, &ip, str, 16) == NULL)
	{
		free(str);
		return NULL;
	}

	return str;
}

- (char *)decodeIPV6Addr:(char *)buf length:(int)len
{
	struct in6_addr ip;
	char *str;
	int32_t i, j, status;
	lu_xdr_t *inxdr;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	for (j = 0; j < 4; j++)
	{
		status = lu_xdr_int_32(inxdr, &i);
		if (status != 0)
		{
			lu_xdr_free(inxdr);
			return NULL;
		}

		ip.__u6_addr.__u6_addr32[j] = i;
	}

	lu_xdr_free(inxdr);

	str = malloc(64);
	if (inet_ntop(AF_INET6, &ip, str, 64) == NULL)
	{
		free(str);
		return NULL;
	}

	return str;
}

- (char *)decodeIPNet:(char *)buf length:(int)len
{
	char *str;
	int32_t i, status;
	lu_xdr_t *inxdr;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_int_32(inxdr, &i);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	str = malloc(16);
	sprintf(str, "%s", nettoa(i));

	return str;
}

- (char *)decodeENAddr:(char *)buf length:(int)len
{
	char *str, *p;
	struct ether_addr en;
	lu_xdr_t *inxdr;
	u_int32_t size;
	int32_t status;

	size = sizeof(struct ether_addr);
	memset(&en, 0, size);
	p = (char *)&en;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_buffer(inxdr, &p, &size);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	str = malloc(20);
	sprintf(str, "%s", ether_ntoa(&en));

	return str;
}

- (char **)twoStringsFromBuffer:(char *)buf length:(int)len
{
	char *str1, *str2;
	char **l = NULL;
	lu_xdr_t *inxdr;
	int32_t status;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);
	str1 = NULL;
	str2 = NULL;

	status = lu_xdr_string(inxdr, &str1);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &str2);
	if (status != 0)
	{
		free(str1);
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	l = appendString(str1, l);
	l = appendString(str2, l);

	free(str1);
	free(str2);

	return l;
}

- (char **)threeStringsFromBuffer:(char *)buf length:(int)len
{
	char *str1, *str2, *str3;
	char **l = NULL;
	lu_xdr_t *inxdr;
	int32_t status;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);
	str1 = NULL;
	str2 = NULL;
	str3 = NULL;

	status = lu_xdr_string(inxdr, &str1);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &str2);
	if (status != 0)
	{
		free(str1);
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &str3);
	if (status != 0)
	{
		free(str1);
		free(str2);
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	l = appendString(str1, l);
	l = appendString(str2, l);
	l = appendString(str3, l);

	free(str1);
	free(str2);
	free(str3);

	return l;
}

- (int)intFromBuffer:(char *)buf length:(int)len
{
	int32_t i, status;
	lu_xdr_t *inxdr;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);
	status = lu_xdr_int_32(inxdr, &i);
	if (status != 0) i = 0;
	lu_xdr_free(inxdr);

	return i;
}

- (LUDictionary *)dictionaryFromBuffer:(char *)buf length:(int)len
{
	LUDictionary *item;
	char *key, *val, **l;
	int32_t i, j, count, n, status;
	lu_xdr_t *inxdr;

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_int_32(inxdr, &count);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	l = NULL;

	item = [[LUDictionary alloc] init];
	for (i = 0; i < count; i++)
	{
		key = NULL;
		status = lu_xdr_string(inxdr, &key);
		if (status != 0) break;

		status = lu_xdr_int_32(inxdr, &n);
		if (status != 0) break;

		l = NULL;
		for (j = 0; j < n; j++)
		{
			val = NULL;
			status = lu_xdr_string(inxdr, &val);
			if (status != 0) break;

			l = appendString(val, l);
			free(val);
		}

		if (j != n) break;

		[item setValues:l forKey:key count:n];
		free(key);
		key = NULL;
		freeList(l);
		l = NULL;
	}

	if (key != NULL) free(key);
	if (l != NULL) freeList(l);

	lu_xdr_free(inxdr);
	if (i != count)
	{
		[item release];
		return NULL;
	}

	return item;	
}

- (char **)intAndStringFromBuffer:(char *)buf length:(int)len
{
	int32_t i, status;
	char *str;
	char **l = NULL;
	lu_xdr_t *inxdr;
	char num[64];

	if (buf == NULL) return NULL;
	if (len == 0) return NULL;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);
	str = NULL;
	status = lu_xdr_int_32(inxdr, &i);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &str);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	sprintf(num, "%d", i);
	l = appendString(num, l);
	l = appendString(str, l);
	free(str);
	return l;
}

- (char **)inNetgroupArgsFromBuffer:(char *)buf length:(int)len
{
	lu_xdr_t *inxdr;
	char **l = NULL;
	char *group, *host, *user, *domain;
	int32_t status;

	inxdr = lu_xdr_from_buffer(buf, len, LU_XDR_DECODE);

	status = lu_xdr_string(inxdr, &group);
	if (status != 0)
	{
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &user);
	if (status != 0)
	{
		if (group != NULL) free(group);
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &host);
	if (status != 0)
	{
		if (group != NULL) free(group);
		if (user != NULL) free(user);
		lu_xdr_free(inxdr);
		return NULL;
	}

	status = lu_xdr_string(inxdr, &domain);
	if (status != 0)
	{
		if (group != NULL) free(group);
		if (user != NULL) free(user);
		if (host != NULL) free(host);
		lu_xdr_free(inxdr);
		return NULL;
	}

	lu_xdr_free(inxdr);

	if (group == NULL)
	{
		if (user != NULL) free(user);
		if (host != NULL) free(host);
		if (domain != NULL) free(domain);
		return NULL;
	}

	l = appendString(group, l);
	free(group);

	if (host != NULL)
	{
		l = appendString(host, l);
		free(host);
	}
	else l = appendString("", l);

	if (user != NULL)
	{
		l = appendString(user, l);
		free(user);
	}
	else l = appendString("", l);

	if (domain != NULL)
	{
		l = appendString(domain, l);
		free(domain);
	}
	else l = appendString("", l);

	return l;
}

@end
