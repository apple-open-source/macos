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
#import "FileMap.h"
#import "Controller.h"
#import "AMVnode.h"
#import "Server.h"
#import "AMString.h"
#import "automount.h"
#import "log.h"
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <errno.h>
#import <string.h>
#import <syslog.h>
#import <sys/mount.h>

@implementation FileMap

- (void)newMount:(String *)src dir:(String *)dst opts:(Array *)opts vfsType:(String *)type
{
	String *servername, *serversrc;
	Vnode *v;
	Server *server;

	serversrc = [src postfix:':'];
	if (serversrc == nil) return;

	servername = [src prefix:':'];
	if (servername == nil)
	{
		[serversrc release];
		return;
	}

	server = [controller serverWithName:servername];
	if (server == nil)
	{
		[servername release];
		return;
	}

	if (![self acceptOptions:opts])
	{
		sys_msg(debug, LOG_DEBUG, "Rejected options for %s on %s (FileMap)",
			[src value], [dst value]);
		[servername release];
		[serversrc release];
		return;
	}

	v = [self createVnodePath:dst from:root];
	if ([v type] == NFLNK)
	{
		/* mount already exists - do not override! */
		[servername release];
		[serversrc release];
		return;
	}

	[v setType:NFLNK];
	[v setServer:server];
	[v setSource:serversrc];
	[v setVfsType:type];
	[v setupOptions:opts];
	[v setServerDepth:0];
	[v addMntArg:MNT_DONTBROWSE];
	[servername release];
	[serversrc release];
	[self setupLink:v];
}

- (void)loadMounts
{
	FILE *fp;
	char line[1024], cloc[1024], copts[1024], csrc[1024];
	String *src, *loc, *opts;
	Array *options;
	int n;

	if (dataStore == nil) return;

	fp = fopen([dataStore value], "r");
	if (fp == NULL)
	{
		sys_msg(debug, LOG_ERR, "%s: %s", [dataStore value], strerror(errno));
		return;
	}

	sys_msg(debug_proc, LOG_DEBUG, "  FileMap/loadMounts: reading %s", [dataStore value]);

	while (fgets(line, 1024, fp) != NULL)
	{
		n = sscanf(line, "%s %s %s", cloc, copts, csrc);
		if ((n < 2) || (n > 3))
		{
			sys_msg(debug, LOG_ERR, "Bad input line in map %s: %s",
				[dataStore value], line);
			continue;
		}

		loc = [String uniqueString:cloc];
		if (n == 3)
		{
			opts = [String uniqueString:copts];
			src = [String uniqueString:csrc];
		}
		else
		{
			/* n == 2 */
			opts = [String uniqueString:""];
			src = [String uniqueString:copts];
		}

		options = [opts explode:','];

		[self newMount:src dir:loc opts:options vfsType:nil];

		[src release];
		[loc release];
		[opts release];
		[options release];
	}

	fclose(fp);
}

@end
