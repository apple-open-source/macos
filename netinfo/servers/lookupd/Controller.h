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
 * Controller.h
 *
 * Controller for lookupd
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#import "sys.h"
#import <netinfo/lookup_types.h>
#import "LUDictionary.h"
#import "LUArray.h"
#import "LUGlobal.h"
#import "LUServer.h"
#import <NetInfo/syslock.h>

#ifdef _IPC_UNTYPED_
typedef struct {
	NDR_record_t NDR;
	lookup_name name;
	mach_msg_trailer_t trailer;
} lookup_link_Request;

typedef struct {  
	NDR_record_t NDR;
	int proc;
	mach_msg_type_number_t indataCnt;
	unit indata[4096];
	mach_msg_trailer_t trailer;
} lookup_all_Request;

typedef struct {
	NDR_record_t NDR;
	int proc;
	mach_msg_type_number_t indataCnt;
	unit indata[4096];
	mach_msg_trailer_t trailer;
} lookup_one_Request;

typedef struct {
	/* start of the kernel processed data */
	mach_msg_body_t msgh_body;
	mach_msg_ool_descriptor_t indata;
	/* end of the kernel processed data */
	NDR_record_t NDR;
	int proc;
	mach_msg_type_number_t indataCnt;
	mach_msg_trailer_t trailer;
} lookup_ooall_Request;

typedef struct {
	mach_msg_header_t head;
	union {
		lookup_link_Request	link;
		lookup_all_Request	all;
		lookup_one_Request	one;
		lookup_ooall_Request	ooall;
	} requests;
} lookup_request_msg;

typedef struct {
	NDR_record_t NDR;
	kern_return_t RetCode;
	int procno;
} __Reply___lookup_link_t;

typedef struct {
	/* start of the kernel processed data */
	mach_msg_body_t msgh_body;
	mach_msg_ool_descriptor_t outdata;
	/* end of the kernel processed data */
	NDR_record_t NDR;
	mach_msg_type_number_t outdataCnt;
} __Reply___lookup_all_t;

typedef struct {
	NDR_record_t NDR;
	kern_return_t RetCode;
	mach_msg_type_number_t outdataCnt;
	unit outdata[4096];
} __Reply___lookup_one_t;

typedef struct {
	/* start of the kernel processed data */
	mach_msg_body_t msgh_body;
	mach_msg_ool_descriptor_t outdata;
	/* end of the kernel processed data */
	NDR_record_t NDR;
	mach_msg_type_number_t outdataCnt;
} __Reply___lookup_ooall_t;

typedef struct {
	mach_msg_header_t head;
	union {
		__Reply___lookup_link_t		lookup;
		__Reply___lookup_all_t		all;
		__Reply___lookup_one_t	 	one;
		__Reply___lookup_ooall_t	ooall;
		mig_reply_error_t		error;
	} replies;
} lookup_reply_msg;

#else

typedef struct lookup_request_msg {
	msg_header_t head;
	msg_type_t itype;
	int i;
	msg_type_t dtype;
	inline_data data;
} lookup_request_msg;

#define lookup_reply_msg lookup_request_msg
#endif

@interface Controller : Root
{
	syslock *serverLock;
	syslock *threadCountLock;
	LUDictionary *globalDict;
	LUDictionary *configDict[NCATEGORIES];
	LUArray *serverList;
	int threadCount;
	int idleThreads;
	int maxThreads;
	int maxIdleThreads;
	int maxIdleServers;
	LUDictionary *loginUser;
	char **agentNames;
	char **dnsSearchList;
	char **netAddrList;
	char *portName;
	id *agents;
	int agentCount;
	BOOL shutdownServerThreads;
}

- (Controller *)initWithName:(char *)name;
- (BOOL)registerPort:(char *)name;
- (void)startServerThread;
- (LUServer *)checkOutServer;
- (void)checkInServer:(LUServer *)server;

- (void)setLoginUser:(int)uid;
- (void)flushCache;
- (void)suspend;

- (char *)portName;
- (char **)dnsSearchList;
- (char **)netAddrList;

- (void)serverLoop;
- (void)lookupdMessage;

- (id)agentClassNamed:(char *)name;

@end
