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
 * MachRPC.h
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"

#define PROC_GETPWENT 0
#define PROC_GETPWENT_A 1
#define PROC_GETPWUID 2
#define PROC_GETPWUID_A 3
#define PROC_GETPWNAM 4
#define PROC_GETPWNAM_A 5
#define PROC_SETPWENT 6
#define PROC_GETGRENT 7
#define PROC_GETGRGID 8
#define PROC_GETGRNAM 9
#define PROC_INITGROUPS 10
#define PROC_GETHOSTENT 11
#define PROC_GETHOSTBYNAME 12
#define PROC_GETHOSTBYADDR 13
#define PROC_GETIPV6NODEBYNAME 14
#define PROC_GETIPV6NODEBYADDR 15
#define PROC_GETNETENT 16
#define PROC_GETNETBYNAME 17
#define PROC_GETNETBYADDR 18
#define PROC_GETSERVENT 19
#define PROC_GETSERVBYNAME 20
#define PROC_GETSERVBYPORT 21
#define PROC_GETPROTOENT 22
#define PROC_GETPROTOBYNAME 23
#define PROC_GETPROTOBYNUMBER 24
#define PROC_GETRPCENT 25
#define PROC_GETRPCBYNAME 26
#define PROC_GETRPCBYNUMBER 27
#define PROC_GETFSENT 28
#define PROC_GETFSBYNAME 29
#define PROC_PRDB_GET 30
#define PROC_PRDB_GETBYNAME 31
#define PROC_BOOTPARAMS_GETENT 32
#define PROC_BOOTPARAMS_GETBYNAME 33
#define PROC_BOOTP_GETBYIP 34
#define PROC_BOOTP_GETBYETHER 35
#define PROC_ALIAS_GETBYNAME 36
#define PROC_ALIAS_GETENT 37
#define PROC_ALIAS_SETENT 38
#define PROC_INNETGR 39
#define PROC_GETNETGRENT 40
#define PROC_FIND 41
#define PROC_LIST 42
#define PROC_QUERY 43
#define PROC_CHECKSECURITYOPT 44
#define PROC_CHECKNETWAREENBL 45
#define PROC_SETLOGINUSER 46
#define PROC__GETSTATISTICS 47
#define PROC__INVALIDATECACHE 48
#define PROC__SUSPEND 49
#define PROC_DNS_PROXY 50
#define PROC_GETADDRINFO 51
#define PROC_GETNAMEINFO 52
#define NPROCS 53

#import "LUDictionary.h"
#import "LUArray.h"
#import "Controller.h"
#import "LUGlobal.h"
#import "lu_xdr.h"

#define nonStandardProc  0
#define standardDictionaryProc 1
#define standardListProc 2

typedef struct
{
	int type;
	SEL encoder;
	int decoder;
	char *key;
	LUCategory cat;
} proc_helper_t;

@interface MachRPC : Root
{
	proc_helper_t proc_helper[NPROCS];
}

- (MachRPC *)init:(id)sender;

- (void)process;

- (BOOL)process:(int)procno
	inData:(char *)indata
	inLength:(unsigned int)inlen
	outData:(char **)outdata
	outLength:(unsigned int *)outlen;

- (BOOL)xdrInt:(int)i buffer:(char **)data length:(int *)len;

- (BOOL)xdrList:(LUArray *)list
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len
	server:(LUServer *)server;

- (BOOL)xdrItem:(LUDictionary *)item
	method:(SEL)method
	buffer:(char **)data
	length:(int *)len;

- (BOOL)xdrInitgroups:(LUDictionary *)item buffer:(char **)data length:(int *)len;
- (BOOL)xdrNetgroup:(LUDictionary *)item buffer:(char **)data length:(int *)len server:(LUServer *)server;

- (void)encodeAttribute:(char *)key 
	from:(LUDictionary *)item
	intoXdr:(lu_xdr_t *)xdrs
	count:(unsigned long)n;

- (char *)decodeString:(char *)buf length:(int)len;
- (char *)decodeInt:(char *)buf length:(int)len;
- (char *)decodeIPV6Addr:(char *)buf length:(int)len;
- (char *)decodeIPAddr:(char *)buf length:(int)len;
- (char *)decodeIPNet:(char *)buf length:(int)len;
- (char *)decodeENAddr:(char *)buf length:(int)len;

- (int)intFromBuffer:(char *)buf length:(int)len;
- (char **)twoStringsFromBuffer:(char *)buf length:(int)len;
- (char **)threeStringsFromBuffer:(char *)buf length:(int)len;
- (char **)intAndStringFromBuffer:(char *)buf length:(int)len;
- (char **)inNetgroupArgsFromBuffer:(char *)buf length:(int)len;

- (LUDictionary *)dictionaryFromBuffer:(char *)buf length:(int)len;

@end
