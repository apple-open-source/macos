/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef __SERVER_H__
#define __SERVER_H__

#import "RRObject.h"
#import <sys/time.h>
@class String;

@interface Server : RRObject
{
	String *myname;
	unsigned int timeout;
	unsigned int pings;
	unsigned long address;
	BOOL isLocalHost;
	BOOL isDead[4][2];
	unsigned short port[4];
	void *mountClient[4];
	unsigned int lastTime[4];
}

+ (BOOL)isMyAddress:(unsigned int)a mask:(unsigned int)m;
+ (BOOL)isMyAddress:(String *)addr;
+ (BOOL)isMyNetwork:(String *)net;

- (void)reset;
- (String *)name;
- (void)setTimeout:(unsigned int)t;
- (Server *)initWithName:(String *)servername;
- (BOOL)isLocalHost;
- (unsigned int)getHandle:(void *)fh
	size:(int *)s
	port:(unsigned short *)p
	forFile:(String *)filename
	version:(unsigned int)v
	protocol:(unsigned int)proto;
- (unsigned int)getNFSPort:(unsigned short *)p
	version:(unsigned int)v
	protocol:(unsigned int)proto;
- (unsigned long)address;

@end

#endif __SERVER_H__
