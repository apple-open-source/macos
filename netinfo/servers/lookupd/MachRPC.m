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
 * Custom procedure calls (using XDR!) on top of Mach IPC for lookupd
 * libc uses this goofy idea to talk to lookupd
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
#import "LDAPAgent.h"
#import <mach/message.h>
#import <mach/mach_error.h>
#import <mach/mig_errors.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <netinfo/lookup_types.h>
#import "_lu_types.h"
#import <netdb.h>
#import <arpa/inet.h>
#import <strings.h>
#import <NetInfo/dsutil.h>
#import <stdio.h>

/* 2 second timeout on sends */
#define TIMEOUT_MSECONDS (2000)
#define XDRSIZE 8192

#define valNull   0
#define valInt    1
#define valString 2
#define valIPAddr 3
#define valIPNet  4
#define valENAddr 5

#ifdef _OS_VERSION_MACOS_X_
extern boolean_t lookup_server(mach_msg_header_t *, mach_msg_header_t *);
#else
extern kern_return_t lookup_server(lookup_request_msg *, lookup_reply_msg *);
#endif
extern char *proc_name(int);

@implementation MachRPC

- (MachRPC *)init:(id)sender
{
	int i;

	[super init];

	xdr = [[XDRSerializer alloc] init];

	for (i = 0; i < NPROCS; i++)
		proc_helper[i].type = nonStandardProc;

	/*
	 * getpwent (returns BSD4.3 data)
	 */
	i = PROC_GETPWENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeUser:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwent-A (returns BSD4.4 data)
	 */
	i = PROC_GETPWENT_A;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeUser_A:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwuid (returns BSD4.3 data)
	 */
	i = PROC_GETPWUID;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeUser:intoXdr:);
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "uid";
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwuid_A (returns BSD4.4 data)
	 */
	i = PROC_GETPWUID_A;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeUser_A:intoXdr:);
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "uid";
	proc_helper[i].cat = LUCategoryUser;

	/*
	 * getpwname (returns BSD4.3 data)
	 */
	i = PROC_GETPWNAM;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeUser:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryUser;
	
	/*
	 * getpwname_A (returns BSD4.4 data)
	 */
	i = PROC_GETPWNAM_A;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeUser_A:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryUser;

	/*
	 * getgrent
	 */
	i = PROC_GETGRENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeGroup:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryGroup;
	
	/*
	 * getgrgid
	 */
	i = PROC_GETGRGID;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeGroup:intoXdr:);
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "gid";
	proc_helper[i].cat = LUCategoryGroup;

	/*
	 * getgrnam
	 */
	i = PROC_GETGRNAM;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeGroup:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryGroup;

	/*
	 * gethostent
	 */
	i = PROC_GETHOSTENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeHost:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryHost;
	
	/*
	 * gethostbyname
	 */
	i = PROC_GETHOSTBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeHost:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryHost;

	/*
	 * gethostbyaddr
	 */
	i = PROC_GETHOSTBYADDR;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeHost:intoXdr:);
	proc_helper[i].decoder = valIPAddr;
	proc_helper[i].key = "ip_address";
	proc_helper[i].cat = LUCategoryHost;

	/*
	 * getnetent
	 */
	i = PROC_GETNETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeNetwork:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryNetwork;
	
	/*
	 * getnetbyname
	 */
	i = PROC_GETNETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeNetwork:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryNetwork;

	/*
	 * getnetbyaddr
	 */
	i = PROC_GETNETBYADDR;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeNetwork:intoXdr:);
	proc_helper[i].decoder = valIPNet;
	proc_helper[i].key = "address";
	proc_helper[i].cat = LUCategoryNetwork;

	/*
	 * getservent
	 */
	i = PROC_GETSERVENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeService:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryService;
	
	/*
	 * getprotoent
	 */
	i = PROC_GETPROTOENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeProtocol:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryProtocol;
	
	/*
	 * getprotobyname
	 */
	i = PROC_GETPROTOBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeProtocol:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryProtocol;

	/*
	 * getprotobynumber
	 */
	i = PROC_GETPROTOBYNUMBER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeProtocol:intoXdr:);
	proc_helper[i].decoder = valInt;
	proc_helper[i].key = "number";
	proc_helper[i].cat = LUCategoryProtocol;

	/*
	 * getrpcent
	 */
	i = PROC_GETRPCENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeRpc:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryRpc;
	
	/*
	 * getrpcbyname
	 */
	i = PROC_GETRPCBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeRpc:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryRpc;

	/*
	 * getrpcbynumber
	 */
	i = PROC_GETRPCBYNUMBER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeRpc:intoXdr:);
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
	 * getmntent
	 */
	i = PROC_GETMNTENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeMNT:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryMount;
	
	/*
	 * getmntbyname
	 */
	i = PROC_GETMNTBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeMNT:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryMount;

	/*
	 * prdb_get
	 */
	i = PROC_PRDB_GET;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodePrinter:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryPrinter;
	
	/*
	 * grdb_getbyname
	 */
	i = PROC_PRDB_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodePrinter:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryPrinter;

	/*
	 * bootparams_getent
	 */
	i = PROC_BOOTPARAMS_GETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeBootparams:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryBootparam;
	
	/*
	 * bootparams_getbyname
	 */
	i = PROC_BOOTPARAMS_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeBootparams:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryBootparam;
	
	/*
	 * bootp_getbyip
	 */
	i = PROC_BOOTP_GETBYIP;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeBootp:intoXdr:);
	proc_helper[i].decoder = valIPAddr;
	proc_helper[i].key = "ip_address";
	proc_helper[i].cat = LUCategoryBootp;

	/*
	 * bootp_getbyether
	 */
	i = PROC_BOOTP_GETBYETHER;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeBootp:intoXdr:);
	proc_helper[i].decoder = valENAddr;
	proc_helper[i].key = "en_address";
	proc_helper[i].cat = LUCategoryBootp;

	/*
	 * alias_getbyname
	 */
	i = PROC_ALIAS_GETBYNAME;
	proc_helper[i].type = standardDictionaryProc;
	proc_helper[i].encoder = @selector(encodeAlias:intoXdr:);
	proc_helper[i].decoder = valString;
	proc_helper[i].key = "name";
	proc_helper[i].cat = LUCategoryAlias;
	
	/*
	 * alias_getent
	 */
	i = PROC_ALIAS_GETENT;
	proc_helper[i].type = standardListProc;
	proc_helper[i].encoder = @selector(encodeAlias:intoXdr:);
	proc_helper[i].decoder = valNull;
	proc_helper[i].key = NULL;
	proc_helper[i].cat = LUCategoryAlias;
	
	return self;
}

- (void)dealloc
{
	if (xdr != nil) [xdr release];
	[super dealloc];
}

- (void)process
{
	kern_return_t status;
#ifdef _OS_VERSION_MACOS_X_
	BOOL lustatus;
#else
	kern_return_t lustatus;
#endif
	Thread *t;
	LUServer *server;
	lookup_reply_msg reply;
	lookup_request_msg *request;

	t = [Thread currentThread];
	request = (lookup_request_msg *)[t data];
	[t setData:NULL];
	
	/*
	 * Use the MIG server to dispatch messages.
	 * Server functions for the MIG interface are in lookup_proc.m
	 */ 
#ifdef _OS_VERSION_MACOS_X_
	lustatus = lookup_server(&request->head, &reply.head);
	free(request);
	if (lustatus == NO)
#else
	reply.head.msg_local_port = request->head.msg_local_port;
	lustatus = lookup_server(request, &reply);
	free(request);
	if (lustatus == MIG_NO_REPLY)
#endif
	{
		system_log(LOG_DEBUG, "MachRPC process request: no reply");
		return;
	}

	status = sys_send_message(&reply.head, TIMEOUT_MSECONDS);

	if (status != KERN_SUCCESS)
	{
		system_log(LOG_ERR, "msg_send failed (%s)", sys_strerror(status));
	}

	server = (LUServer *)[t data];
	[t setData:NULL];

	if (!shutting_down) [controller checkInServer:server];
	[t terminateSelf];
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
	LUDictionary *dict;
	LUArray *list;
	LUServer *server;
	Thread *t, *ttest;
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

	/*
	 * Check if any other thread is using this server.
	 * This is not supposed to happen.
	 */
	ttest = [Thread threadWithData:server];
	if (ttest != nil) 
	{
		system_log(LOG_ERR, "%s: server already in use by %s", 
			[t name], [ttest name]);
		return NO;
	}

	[t setData:(void *)server];

	key = NULL;
	val = NULL;
	cat = (LUCategory)-1;

	dict = nil;

	if (proc_helper[procno].type == standardListProc)
	{
		aSel = proc_helper[procno].encoder;
		cat = proc_helper[procno].cat;

		system_log(LOG_DEBUG, logString);

		list = [server allItemsWithCategory:cat];
		test = [self xdrList:list method:aSel buffer:outdata length:outlen server:server];
	
		[list release];

		return test;
	}

	if (proc_helper[procno].type == standardDictionaryProc)
	{
		if ((inlen == 0) || (indata == NULL))
		{
			system_log(LOG_ERR, "%s - can't decode lookup value", logString);
			return NO;
		}

		aSel = proc_helper[procno].encoder;
		cat = proc_helper[procno].cat;
		key = proc_helper[procno].key;

		switch (proc_helper[procno].decoder)
		{
			case valInt:
				val = [xdr decodeInt:indata length:inlen];
				break;
			case valString:
				val = [xdr decodeString:indata length:inlen];
				break;
			case valIPAddr:
				val = [xdr decodeIPAddr:indata length:inlen];
				break;
			case valIPNet:
				val = [xdr decodeIPNet:indata length:inlen];
				break;
			case valENAddr:
				val = [xdr decodeENAddr:indata length:inlen];
				break;
			default: val = NULL;
		}

		if (val == NULL)
		{
			system_log(LOG_ERR, "%s - can't decode lookup value", logString);
			return NO;
		}

		system_log(LOG_DEBUG, "%s %s", logString, val);

		dict = [server itemWithKey:key value:val category:cat];
		test = [self xdrItem:dict method:aSel buffer:outdata length:outlen];
	
		freeString(val);
		[dict release];

		return test;
	}

	switch (procno)
	{
		case PROC_SETPWENT:
		case PROC_ALIAS_SETENT:
			*outlen = 0;
			*outdata = NULL;
			system_log(LOG_DEBUG, logString);
			return YES;

		case PROC_GETSERVBYNAME: /* NONSTANDARD */
			stuff = [xdr twoStringsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			proto = stuff[1];
			system_log(LOG_DEBUG, "%s %s %s", logString, stuff[0], proto);
			if (proto[0] == '\0') proto = NULL;
			dict = [server serviceWithName:stuff[0] protocol:proto];
			if (proto != NULL) [dict setValue:proto forKey:"_lookup_service_protocol"];
			freeList(stuff);
			stuff = NULL;
			test = [self xdrItem:dict method:@selector(encodeService:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_GETSERVBYPORT: /* NONSTANDARD */
			stuff = [xdr intAndStringFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
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
			test = [self xdrItem:dict method:@selector(encodeService:intoXdr:) buffer:outdata length:outlen];
			[dict release];
			return test;

		case PROC_FIND: /* NONSTANDARD */
			stuff = [xdr threeStringsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s category %s key %s value %s", logString, stuff[0], stuff[1], stuff[2]);
			i = [LUAgent categoryWithName:stuff[0]];
			if (i == -1) dict = nil;
			else dict = [server itemWithKey:stuff[1] value:stuff[2] category:i];
			freeList(stuff);
			stuff = NULL;
			if (dict == nil)
			{
				test = NO;
			}
			else
			{
				test = [self xdrItem:dict method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
				[dict release];
			}
			return test;

		case PROC_LIST: /* NONSTANDARD */
			name = [xdr decodeString:indata length:inlen];
			if (name == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
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
			test = [self xdrList:list method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen server:server];
			[list release];
			return test;

		case PROC_QUERY: /* NONSTANDARD */
			dict = [xdr dictionaryFromBuffer:indata length:inlen];
			if (dict == nil)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, logString);
			name = [dict description];
			system_log(LOG_DEBUG, name);
			free(name);

			list = [server query:dict];
			[dict release];
			if (list == nil) return NO;
			test = [self xdrList:list method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen server:server];
			[list release];
			return test;

		case PROC_INITGROUPS: /* NONSTANDARD */
			name = [xdr decodeString:indata length:inlen];
			if (name == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			list = [server allGroupsWithUser:name];
			freeString(name);
			name = NULL;
			test = [self xdrInitgroups:list buffer:outdata length:outlen];
			[list release];
			return test;

		case PROC_INNETGR: /* NONSTANDARD */
			stuff = [xdr inNetgroupArgsFromBuffer:indata length:inlen];
			if (stuff == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
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
			name = [xdr decodeString:indata length:inlen];
			if (name == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			dict = [server itemWithKey:"name" value:name category:LUCategoryNetgroup];
			freeString(name);
			name = NULL;
			test = [self xdrNetgroup:dict buffer:outdata length:outlen server:server];
			[dict release];
			return test;

		case PROC_CHECKSECURITYOPT: /* NONSTANDARD */
			name = [xdr decodeString:indata length:inlen];
			if (name == NULL)
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			system_log(LOG_DEBUG, "%s %s", logString, name);
			test = [server isSecurityEnabledForOption:name];
			freeString(name);
			name = NULL;
			[self xdrInt:(test ? 1 : 0) buffer:outdata length:outlen];
			return YES;
	
		case PROC_CHECKNETWAREENBL: /* NONSTANDARD */
			system_log(LOG_DEBUG, logString);
			test = [server isNetwareEnabled];
			[self xdrInt:(test ? 1 : 0) buffer:outdata length:outlen];
			return YES;

		case PROC_SETLOGINUSER: /* NONSTANDARD */
			if ((inlen == 0) || (indata == NULL))
			{
				system_log(LOG_ERR, "%s - can't decode lookup value", logString);
				return NO;
			}

			i = [xdr intFromBuffer:indata length:inlen];
			system_log(LOG_DEBUG, "%s %d", logString, i);
			if (!shutting_down) [controller setLoginUser:i];
			[self xdrInt:1 buffer:outdata length:outlen];
			return YES;

		case PROC__GETSTATISTICS: /* NONSTANDARD */
			system_log(LOG_DEBUG, logString);
			test = [self xdrItem:statistics method:@selector(encodeDictionary:intoXdr:) buffer:outdata length:outlen];
			return test;

		case PROC__INVALIDATECACHE: /* NONSTANDARD */
			system_log(LOG_DEBUG, logString);
			if (!shutting_down) [controller flushCache];
			[self xdrInt:1 buffer:outdata length:outlen];
			[self xdrInt:0 buffer:outdata length:outlen];
			return YES;

		case PROC__SUSPEND: /* NONSTANDARD */
			system_log(LOG_DEBUG, logString);
			if (!shutting_down) [controller suspend];
			[self xdrInt:1 buffer:outdata length:outlen];
			return YES;

		default: 
			system_log(LOG_ERR, "%s: unknown proc %d", [t name], procno);
			return NO;
	}

	return NO;
}

- (void)encodeUser_A:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"passwd"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"uid"		from:item intoXdr:xdrs default:-2];
	[xdr encodeInt:	"gid"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"change"		from:item intoXdr:xdrs];
	[xdr encodeString:	"class"		from:item intoXdr:xdrs];
	[xdr encodeString:	"realname"	from:item intoXdr:xdrs];
	[xdr encodeString:	"home"		from:item intoXdr:xdrs];
	[xdr encodeString:	"shell"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"expire"		from:item intoXdr:xdrs];
}

- (void)encodeUser:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"passwd"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"uid"		from:item intoXdr:xdrs default:-2];
	[xdr encodeInt:	"gid"		from:item intoXdr:xdrs];
	[xdr encodeString:	"realname"	from:item intoXdr:xdrs];
	[xdr encodeString:	"home"		from:item intoXdr:xdrs];
	[xdr encodeString:	"shell"		from:item intoXdr:xdrs];
}

- (void)encodeShadowedUser_A:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"*"			intoXdr:xdrs];
	[xdr encodeInt:	"uid"		from:item intoXdr:xdrs default:-2];
	[xdr encodeInt:	"gid"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"change"		from:item intoXdr:xdrs];
	[xdr encodeString:	"class"		from:item intoXdr:xdrs];
	[xdr encodeString:	"realname"	from:item intoXdr:xdrs];
	[xdr encodeString:	"home"		from:item intoXdr:xdrs];
	[xdr encodeString:	"shell"		from:item intoXdr:xdrs];
	[xdr encodeInt:	"expire"		from:item intoXdr:xdrs];
}

- (void)encodeShadowedUser:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"*"			intoXdr:xdrs];
	[xdr encodeInt:	"uid"		from:item intoXdr:xdrs default:-2];
	[xdr encodeInt:	"gid"		from:item intoXdr:xdrs];
	[xdr encodeString:	"realname"	from:item intoXdr:xdrs];
	[xdr encodeString:	"home"		from:item intoXdr:xdrs];
	[xdr encodeString:	"shell"		from:item intoXdr:xdrs];
}

- (void)encodeGroup:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:		"name"	from:item intoXdr:xdrs];
	[xdr encodeString:		"passwd"	from:item intoXdr:xdrs];
	[xdr encodeInt:		"gid"	from:item intoXdr:xdrs];
	[xdr encodeStrings:	"users"	from:item intoXdr:xdrs max:_LU_MAXGRP];
}

- (void)encodeHost:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeStrings:	"name"		from:item intoXdr:xdrs max:_LU_MAXHNAMES];
	[xdr encodeIPAddrs:	"ip_address"	from:item intoXdr:xdrs max:_LU_MAXADDRS];
}

- (void)encodeNetwork:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeStrings:	"name"		from:item intoXdr:xdrs max:_LU_MAXNNAMES];
	[xdr encodeNetAddr:	"address"		from:item intoXdr:xdrs];
}

- (void)encodeService:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	char **portList;
	int portcount;
	unsigned long p;
	char *proto;

	proto = [item valueForKey:"_lookup_service_protocol"];
	if (proto != NULL)
	{
		if (proto[0] == '\0') proto = NULL;
	}

	[xdr encodeStrings:"name" from:item intoXdr:xdrs max:_LU_MAXSNAMES];

	portList = [item valuesForKey:"port"];
	portcount = [item countForKey:"port"];
	if (portcount <= 0) p = -1;
	else p = htons(atoi(portList[0]));

	[xdr encodeInt:p intoXdr:xdrs];

	if (proto == NULL)
		[xdr encodeString:"protocol" from:item intoXdr:xdrs];
	else
		[xdr encodeString:proto intoXdr:xdrs];
}

- (void)encodeProtocol:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeStrings:	"name"	from:item intoXdr:xdrs max:_LU_MAXPNAMES];
	[xdr encodeInt:		"number"	from:item intoXdr:xdrs];
}

- (void)encodeRpc:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeStrings:	"name"	from:item intoXdr:xdrs max:_LU_MAXRNAMES];
	[xdr encodeInt:		"number"	from:item intoXdr:xdrs];
}

- (void)encodeFS:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	char *opts;
	char type[8];
	char **optsList;
	int i, count, len;

	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"dir"		from:item intoXdr:xdrs];
	[xdr encodeString:	"vfstype"		from:item intoXdr:xdrs];

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

	[xdr encodeString:	opts 	intoXdr:xdrs];
	[xdr encodeString:	type 	intoXdr:xdrs];
	[xdr encodeInt:	"freq"	from:item intoXdr:xdrs];
	[xdr encodeInt:	"passno"	from:item intoXdr:xdrs];
	free(opts);
}

- (void)encodeMNT:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	char *opts;
	char *type;
	char **optsList;
	int i, count, len;

	[xdr encodeString:	"name"	from:item intoXdr:xdrs];
	[xdr encodeString:	"dir"	from:item intoXdr:xdrs];

	/* HORRIBLE HACK - we should keep the type in NetInfo! */
	type = [item valueForKey:"type"];
	if (type == NULL) [xdr encodeString:"nfs" intoXdr:xdrs];
	else [xdr encodeString:"type" from:item intoXdr:xdrs];

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

	opts[0] = '\0';
	for (i = 0; i < count; i++)
	{
		strcat(opts, optsList[i]);
		if (i < (count - 1)) strcat(opts, ",");
	}

	[xdr encodeString:	opts			intoXdr:xdrs];
	[xdr encodeInt:	"dump_freq"	from:item intoXdr:xdrs];
	[xdr encodeInt:	"passno"		from:item intoXdr:xdrs];
	free(opts);
}

- (void)encodePrinter:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	char *key;
	int i, count;
	char **l = NULL;

	[xdr encodeStrings:"name" from:item intoXdr:xdrs max:_LU_MAXPRNAMES];

	count = 0;
	for (i = 0; NULL != (key = [item keyAtIndex:i]); i++)
	{
		if (!strncmp(key, "_lookup_", 8)) continue;
		if (streq(key, "name")) continue;
		count++;
		l = appendString(key, l);
	}

	if (count > _LU_MAXPRPROPS)
	{
		system_log(LOG_ERR, "truncating at %d values", _LU_MAXPRPROPS);
		count = _LU_MAXPRPROPS;
	}

	[xdr encodeInt:count intoXdr:xdrs];
	for (i = 0; i < count; i++)
	{
		[xdr encodeString:l[i] intoXdr:xdrs];
		[xdr encodeString:l[i] from:item intoXdr:xdrs];
	}
	freeList(l);
	l = NULL;
}

- (void)encodeBootparams:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:		"name"		from:item intoXdr:xdrs];
	[xdr encodeStrings:	"bootparams"	from:item intoXdr:xdrs max:_LU_MAX_BOOTPARAMS_KV];
}

- (void)encodeBootp:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:	"name"		from:item intoXdr:xdrs];
	[xdr encodeString:	"bootfile"	from:item intoXdr:xdrs];
	[xdr encodeIPAddr:	"ip_address"	from:item intoXdr:xdrs];
	[xdr encodeENAddr:	"en_address"	from:item intoXdr:xdrs];
}

- (void)encodeAlias:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	[xdr encodeString:		"name"		from:item intoXdr:xdrs];
	[xdr encodeStrings:	"members"		from:item intoXdr:xdrs max:_LU_MAXALIASMEMBERS];
	[xdr encodeInt:		"alias_local"	from:item intoXdr:xdrs];
}

- (void)encodeDictionary:(LUDictionary *)item intoXdr:(XDR *)xdrs
{
	char *key;
	int i, count, n;

	count = [item count];
	[xdr encodeInt:count intoXdr:xdrs];

	for (i = 0; i < count; i++)
	{
		key = [item keyAtIndex:i];
		[xdr encodeString:key intoXdr:xdrs];
		n = [item countAtIndex:i];
		[xdr encodeStrings:key from:item intoXdr:xdrs maxCount:n maxLength:(unsigned int)-1];
	}
}

- (BOOL)xdrNetgroup:(LUDictionary *)item buffer:(char **)data length:(int *)len server:(LUServer *)server
{
	unsigned long i, count, size;
	char **names;
	char *xdrBuffer;
	XDR outxdr;

	if (item == nil) return NO;

	xdrBuffer = malloc(XDRSIZE);

	count = 0;
	i = [item countForKey:"hosts"];
	if (i != IndexNull) count += i;
	i = [item countForKey:"users"];
	if (i != IndexNull) count += i;
	i = [item countForKey:"domains"];
	if (i != IndexNull) count += i;

	*len = 0;
	xdrmem_create(&outxdr, xdrBuffer, XDRSIZE, XDR_ENCODE);

	if (!xdr_u_long(&outxdr, &count))
	{
		xdr_destroy(&outxdr);
		free(xdrBuffer);
		return NO;
	}

	size = xdr_getpos(&outxdr);
	[server copyToOOBuffer:xdrBuffer size:size];

	/* XXX Netgroups as members of other netgroups not supported! */

	names = [item valuesForKey:"hosts"];
	count = [item countForKey:"hosts"];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		xdr_setpos(&outxdr, 0);
		[xdr encodeString:names[i] intoXdr:&outxdr];
		[xdr encodeString:"-" intoXdr:&outxdr];
		[xdr encodeString:"-" intoXdr:&outxdr];
		size = xdr_getpos(&outxdr);
		[server copyToOOBuffer:xdrBuffer size:size];
	}

	names = [item valuesForKey:"users"];
	count = [item countForKey:"users"];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		xdr_setpos(&outxdr, 0);
		[xdr encodeString:"-" intoXdr:&outxdr];
		[xdr encodeString:names[i] intoXdr:&outxdr];
		[xdr encodeString:"-" intoXdr:&outxdr];
		size = xdr_getpos(&outxdr);
		[server copyToOOBuffer:xdrBuffer size:size];
	}

	names = [item valuesForKey:"domains"];
	count = [item countForKey:"domains"];
	if (count == IndexNull) count = 0;
	for (i = 0; i < count; i++)
	{
		xdr_setpos(&outxdr, 0);
		[xdr encodeString:"-" intoXdr:&outxdr];
		[xdr encodeString:"-" intoXdr:&outxdr];
		[xdr encodeString:names[i] intoXdr:&outxdr];
		size = xdr_getpos(&outxdr);
		[server copyToOOBuffer:xdrBuffer size:size];
	}

	*data = [server ooBuffer];
	*len = [server ooBufferLength];

	xdr_destroy(&outxdr);
	free(xdrBuffer);

	return YES;
}

- (BOOL)xdrInt:(int)i buffer:(char **)data length:(int *)len
{
	XDR outxdr;
	BOOL status;

	xdrmem_create(&outxdr, *data, MAX_INLINE_DATA, XDR_ENCODE);

	status = xdr_int(&outxdr, &i);
	if (!status)
	{
		system_log(LOG_ERR, "xdr_int failed");
		xdr_destroy(&outxdr);
		return NO;
	}

	*len = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);
	return YES;
}

- (BOOL)xdrList:(LUArray *)list
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len
	server:(LUServer *)server
{
	unsigned long i, count, size;
	static LUDictionary *item;
	char *xdrBuffer;
	XDR outxdr;

	if (list == nil) return NO;

	xdrBuffer = malloc(XDRSIZE);
	
	xdrmem_create(&outxdr, xdrBuffer, XDRSIZE, XDR_ENCODE);
	count = [list count];

	*len = 0;
	if (!xdr_u_long(&outxdr, &count))
	{
		xdr_destroy(&outxdr);
		free(xdrBuffer);
		return NO;
	}

	size = xdr_getpos(&outxdr);
	[server copyToOOBuffer:xdrBuffer size:size];

	for (i = 0; i < count; i++)
	{
		item = [list objectAtIndex:i];
		xdr_setpos(&outxdr, 0);

		[self perform:method with:item with:(id)&outxdr];

		size = xdr_getpos(&outxdr);
		[server copyToOOBuffer:xdrBuffer size:size];
	}

	*data = [server ooBuffer];
	*len = [server ooBufferLength];

	xdr_destroy(&outxdr);
	free(xdrBuffer);

	return YES;
}

- (BOOL)xdrItem:(LUDictionary *)item
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len
{
	XDR outxdr;
	BOOL realData;
	int h_errno;
	BOOL status;

	xdrmem_create(&outxdr, *data, MAX_INLINE_DATA, XDR_ENCODE);

	realData = (item != nil);
	[xdr encodeBool:realData intoXdr:&outxdr];

	if (!realData)
	{
		if (method == @selector(encodeHost:intoXdr:))
		{
			h_errno = HOST_NOT_FOUND;
			status = xdr_int(&outxdr, &h_errno);
			if (!status)
			{
				system_log(LOG_ERR, "xdr_int failed");
				xdr_destroy(&outxdr);
				return NO;
			}
		}
		*len = xdr_getpos(&outxdr);
		xdr_destroy(&outxdr);
		return YES;
	}

	[self perform:method with:item with:(id)&outxdr];

	if (method == @selector(encodeHost:intoXdr:))
	{
		h_errno = 0;
		status = xdr_int(&outxdr, &h_errno);
		if (!status)
		{
			system_log(LOG_ERR, "xdr_int failed");
			xdr_destroy(&outxdr);
			return NO;
		}
	}

	*len = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);
	return YES;
}

- (BOOL)xdrInitgroups:(LUArray *)list buffer:(char **)data length:(int *)len
{
	XDR outxdr;
	char **gidsSent = NULL;
	char **gids;
	LUDictionary *group;
	int j, ngids;
	int i, count;
	int n;

	if (list == nil) return NO;

	count = [list count];
	if (count == 0) return NO;

	xdrmem_create(&outxdr, *data, MAX_INLINE_DATA, XDR_ENCODE);

	for (i = 0; i < count; i++)
	{
		group = [list objectAtIndex:i];
		gids = [group valuesForKey:"gid"];
		if (gids == NULL) continue;
		ngids = [group countForKey:"gid"];
		if (ngids < 0) ngids = 0;
		for (j = 0; j < ngids; j++)
		{
			if (listIndex(gids[j], gidsSent) != IndexNull) continue;
			gidsSent = appendString(gids[j], gidsSent);
			n = atoi(gids[j]);
			[xdr encodeInt:n intoXdr:&outxdr];
		}
	}

	n = -99; /* XXX STUPID ENCODING ALERT - fix in libc someday */
	[xdr encodeInt:n intoXdr:&outxdr];

	*len = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);

	freeList(gidsSent);

	return YES;
}

@end
