/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2001 Apple Computer, Inc.  All Rights
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
#import "StaticMap.h"
#import "Controller.h"
#import "AMString.h"
#import "AMVnode.h"
#import "automount.h"
#import "log.h"
#import <fstab.h>
#import <stdlib.h>
#import <unistd.h>
#import <errno.h>
#import <string.h>
#import <sys/stat.h>

@implementation StaticMap

- (void)newMount:(String *)src dir:(String *)dst opts:(Array *)opts vfsType:(String *)type
{
	String *servername, *serversrc, *link;
	String *authOpts, *opt;
	int i;
	Vnode *v;
	Server *server;
	int status;
	struct stat sb;

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

	if ([server isLocalHost])
	{
		[serversrc release];
		serversrc = [String uniqueString:"/"];
	}

	if (![self acceptOptions:opts])
	{
		sys_msg(debug, LOG_DEBUG, "Rejected options for %s on %s (StaticMap)",
			[src value], [dst value]);
		[servername release];
		[serversrc release];
		return;
	}

	authOpts = [String uniqueString:""];
	
	for (i = 0; i < [opts count]; i++)
	{
		opt = [opts objectAtIndex:i];
		if (!strncmp([opt value], "url==", 5))
		{
			authOpts = [[opt postfix:'='] postfix:'='];
			sys_msg(debug, LOG_DEBUG, "***** Found url string %s", [authOpts value]);
		}
	}

	status = lstat([dst value], &sb);
	if (status == 0)
	{
		if (sb.st_mode & S_IFDIR) status = rmdir([dst value]);
		else status = unlink([dst value]);

		if (status < 0)
		{
			sys_msg(debug, LOG_WARNING, "Cannot unlink %s: %s", [dst value], strerror(errno));
			[servername release];
			[serversrc release];
			return;
		}
	}

	link = [String concatStrings:[root path] :dst];
	status = symlink([link value], [dst value]);
	if (status < 0)
	{
		sys_msg(debug, LOG_WARNING, "Cannot symlink %s to %s: %s", [dst value], [link value], strerror(errno));
		[servername release];
		[serversrc release];
		[link release];
		return;
	}
	[link release];

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
	[v setUrlString:authOpts];
	[servername release];
	[serversrc release];
	[self setupLink:v];
}

- (void)loadMounts
{
	struct fstab *f;
	String *spec, *file, *type, *opts, *vfstype;
	String *netopt;
	Array *options;
	BOOL hasnet;

	netopt = [String uniqueString:"net"];

	setfsent();
	while (NULL != (f = getfsent()))
	{
		opts = [String uniqueString:f->fs_mntops];
		options = [opts explode:','];
		hasnet = [options containsObject:netopt];
		if (hasnet)
		{
			[opts release];
			[options release];
			continue;
		}

		spec = [String uniqueString:f->fs_spec];
		file = [String uniqueString:f->fs_file];
		type = [String uniqueString:f->fs_type];
		vfstype = [String uniqueString:f->fs_vfstype];

		if (type != nil) [options addObject:type];

		[self newMount:spec dir:file opts:options vfsType:vfstype];

		[spec release];
		[file release];
		[type release];
		[opts release];
		[options release];
		[vfstype release];
	}
	endfsent();
}

@end
