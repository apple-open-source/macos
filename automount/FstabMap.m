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
#import "FstabMap.h"
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
#ifdef __APPLE__
#import <fstab.h>
#else
#import <mntent.h>
#include <libc.h>
#endif

extern BOOL doServerMounts;

@implementation FstabMap

- (void)setupLink:(Vnode *)v
{
	String *x;
	char *s;
	int len;

	if (v == nil) return;

	if ([[v server] isLocalHost])
	{
		x = [String uniqueString:"/"];
		[v setLink:x];
		[x release];
		[v setMode:00777 | NFSMODE_LNK];
		[v setMounted:YES];
		return;
	}

	len = [mountPoint length] + [[v path] length] + 1;
	s = malloc(len);
	sprintf(s, "%s%s", [mountPoint value], [[v path] value]);

	x = [String uniqueString:s];
	free(s);
	[v setLink:x];
	[x release];
}

- (void)newMount:(String *)src dir:(String *)dst opts:(Array *)opts vfsType:(String *)type
{
	String *servername, *serversrc, *x;
        String *authOpts;
	Vnode *v, *s;
	Server *server;
	BOOL pathOK;
        int i;

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

	x = [String uniqueString:"net"];

        if (![opts containsObject:x]) {

            printf("Object is not there %s...\n", [[opts objectAtIndex:0] value]);
        }
        
	if ((![opts containsObject:x]) || (![self acceptOptions:opts]))
	{
		sys_msg(debug, LOG_DEBUG, "Rejected options for %s on %s (FstabMap)",
   	             [src value], [dst value]);
		[x release];
		[servername release];
		[serversrc release];
		return;
	}
	[x release];

	pathOK = [self checkVnodePath:servername from:root];
	s = [self createVnodePath:servername from:root];

	if ((!pathOK) && doServerMounts) [s setServer:server];

	v = [self createVnodePath:serversrc from:s];
	if ([v type] == NFLNK)
	{
		/* mount already exists - do not override! */
		[servername release];
		[serversrc release];
		return;
	}

        authOpts = [String uniqueString:""];

        for (i=0;i<[opts count];i++) {
            String *opt = [opts objectAtIndex:i];
            if (!strncmp([opt value], "url==", 5)) {
                authOpts = [[opt postfix:'='] postfix:'='];
                sys_msg(debug, LOG_DEBUG, "*******Found url string %s\n", [authOpts value]);
            }
        }

	[v setType:NFLNK];
	[v setServer:server];
	[v setSource:serversrc];
	[v setupOptions:opts];
        [v setVfsType:type];
        [v setUrlString:authOpts];
	[servername release];
	[serversrc release];
	[self setupLink:v];
}

- (void)postProcess:(Vnode *)v
{
	unsigned int i, len;
	Array *kids;

	if ([v server] != nil)
	{
		/* Top level directory for this server */
		[v setType:NFLNK];
		[v setMode:01777 | NFSMODE_LNK];
		[self setupLink:v];
		return;
	}

	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];

	for (i = 0; i < len; i++)
		[self postProcess:[kids objectAtIndex:i]];
}

#ifdef __APPLE__
- (void)loadMounts
{
	struct fstab *f;
	String *spec, *file, *type, *opts, *vfstype;
	char hn[1026];
	Array *options;

	gethostname(hn, 1024);
	strcat(hn, ":/");
	spec = [String uniqueString:hn];
	file = [String uniqueString:""];
	type = [String uniqueString:"rw"];
	opts = [String uniqueString:"net"];
        vfstype = [String uniqueString:"nfs"];

	options = [[Array alloc] init];
	[options addObject:opts];
	[options addObject:type];

        [self newMount:spec dir:file opts:options vfsType:vfstype];

	[spec release];
	[file release];
	[type release];
	[opts release];
	[options release];
        [vfstype release];

	setfsent();
	while (NULL != (f = getfsent()))
	{
		spec = [String uniqueString:f->fs_spec];
		file = [String uniqueString:f->fs_file];
		type = [String uniqueString:f->fs_type];
		opts = [String uniqueString:f->fs_mntops];
                vfstype = [String uniqueString:f->fs_vfstype];

		options = [opts explode:','];
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
#else
- (void)loadMounts
{
	struct mntent *f;
	String *spec, *file, *opts, *vfstype;
	Array *options;
	FILE *x;
	char hn[1026];

	gethostname(hn, 1024);
	strcat(hn, ":/");
	spec = [String uniqueString:hn];
	file = [String uniqueString:""];
	opts = [String uniqueString:"net"];
        vfstype = [String uniqueString:"nfs"];

	options = [[Array alloc] init];
	[options addObject:opts];

	[self newMount:spec dir:file opts:options];

	[spec release];
	[file release];
	[opts release];
	[options release];
        [vfstype release];

	x = setmntent(NULL, "r");
	while (NULL != (f = getmntent(x)))
	{
		spec = [String uniqueString:f->mnt_fsname];
		file = [String uniqueString:f->mnt_dir];
		opts = [String uniqueString:f->mnt_opts];
                vfstype = [String uniqueString:f->mnt_vfstype];

		options = [opts explode:','];

                [self newMount:spec dir:file opts:options vfsType:vfstype];

		[spec release];
		[file release];
		[opts release];
                [vfstype release];
		[options release];
	}
	endmntent(x);
}
#endif

- (FstabMap *)initWithParent:(Vnode *)p
	directory:(String *)dir
	from:(String *)ds
{
	dataStore = nil;

	if (ds != nil)
	{
		dataStore = ds;
		if (dataStore != nil) [dataStore retain];
	}

	[super initWithParent:p directory:dir];

	[self setName:ds];
	[self loadMounts];
	[self postProcess:root];
	return self;
}

- (void)dealloc
{
	if (dataStore != nil) [dataStore release];
	[super dealloc];
}

@end
