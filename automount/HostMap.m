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
#import "HostMap.h"
#import "Controller.h"
#import "HostVnode.h"
#import "AMString.h"
#import "automount.h"
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <syslog.h>


@implementation HostMap

- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v withType:(String *)type
{
	int i, p;
	Vnode *n, *x;
	char *s, t[1024];
	String *part;

	if (path == nil) return v;
	if ([path length] == 0) return v;

	p = 0;
	s = [path value];

	n = v;
	while (s != NULL)
	{
		while (s[0] == '/')
		{
			p++;
			s++;
		}
		for (i = 0; (s[i] != '/') && (s[i] != '\0'); i++) t[i] = s[i];
		t[i] = '\0';
		if (i == 0)
		{
			s = [path scan:'/' pos:&p];
			continue;
		}

		part = [String uniqueString:t];

		x = [n lookup:part];
		if (x == nil)
		{
			x = [[Vnode alloc] init];
			[x setMap:self];
			[controller registerVnode:x];
			[x setName:part];
			if (type != nil) [x setVfsType:type];
			[n addChild:x];
		}
		n = x;

		[part release];
		s = [path scan:'/' pos:&p];
	}

	return n;
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds
{
	String *x;
	char hn[1026], *dot;
	HostVnode *v;

	[super init];

	mountPoint = [controller mountDirectory];
	if (mountPoint != nil) [mountPoint retain];

	[self setName:ds];

	root = [[HostVnode alloc] init];
	[root setMap:self];
	[root setName:dir];
	[root setMounted:NO];
	[root setServer:nil];

	if (p != nil) [p addChild:root];
	[controller registerVnode:root];

	gethostname(hn, 1024);
	dot = strchr(hn, '.');
	if (dot != NULL) *dot = '\0';

	v = (HostVnode *)root;
	x = [String uniqueString:hn];
	[v vnodeForHost:x];
	[x release];

	return self;
}

@end
