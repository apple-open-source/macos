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
#import "NIMap.h"
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
#import <netinfo/ni.h>
#ifndef __APPLE__
#import <libc.h>
#endif

@implementation NIMap

+ (unsigned int)loadNetInfoMaps
{
	void *d, *up;
	ni_id nid, kid;
	ni_idlist l;
	ni_proplist p;
	ni_namelist *n;
	int i, loaded, autoStatus;
	ni_status status;
	ni_index where;
	char line[1024];
	String *mountDir, *mountPath, *mapName;
	Vnode *rr;
	NIMap *map;

	loaded = 0;
	rr = [[controller rootMap] root];

	status = ni_open(NULL, ".", &d);
	if (status != NI_OK)
	{
		sys_msg(debug, LOG_ERR, "Can't connect to local NetInfo domain");
		return 1;
	}
	
	/* For each domain */
	for (;;)
	{
		status = ni_pathsearch(d, &nid, "/mountmaps");
		if (status == NI_OK)
		{
			status = ni_children(d, &nid, &l);
			if (status != NI_OK)
			{
				sys_msg(debug, LOG_ERR, "Can't list NetInfo /mountmaps: %s",
					ni_error(status));
				return 1;
			}

			/* For each subdirectory of /mountmaps */
			for (i = 0; i < l.ni_idlist_len; i++)
			{
				kid.nii_object = l.ni_idlist_val[i];
				status = ni_read(d, &kid, &p);
				if (status != NI_OK)
				{
					sys_msg(debug, LOG_ERR,
						"Error reading NetInfo dir %lu: %s",
						kid.nii_object, ni_error(status));
					return 1;
				}

				where = ni_proplist_match(p, "name", NULL);

				if (where == NI_INDEX_NULL)
				{
					sys_msg(debug, LOG_ERR, "No name for NetInfo map %lu",
						l.ni_idlist_val[i]);
					ni_proplist_free(&p);
					continue;
				}

				n = &(p.ni_proplist_val[where].nip_val);
				if (n->ni_namelist_len == 0)
				{
					sys_msg(debug, LOG_ERR,
						"No value for name for NetInfo map %lu",
						l.ni_idlist_val[i]);
					ni_proplist_free(&p);
					continue;
				}

				mapName = [String uniqueString:n->ni_namelist_val[0]];

				/* Locate the mount point */
				where = ni_proplist_match(p, "dir", NULL);
				if (where == NI_INDEX_NULL)
				{
					mountDir = [mapName retain];
				}
				else
				{
					n = &(p.ni_proplist_val[where].nip_val);
					if (n->ni_namelist_len == 0)
					{
						sys_msg(debug, LOG_ERR,
							"No value of dir property for NetInfo map %s",
							[mapName value]);
						ni_proplist_free(&p);
						[mapName release];
						continue;
					}
					
					mountDir = [String uniqueString:n->ni_namelist_val[0]];
				}

				sprintf(line, "/%s", [mountDir value]);
				mountPath = [String uniqueString:line];

				sys_msg(debug, LOG_DEBUG, "Loading NetInfo map \"%s\"",
					[mapName value]);

				map = (NIMap *)[[NIMap alloc] initWithParent:rr
					directory:mountDir from:nil];

				[map setName:mapName];
				[map loadNIMap:kid.nii_object domain:d directory:mountDir];

				autoStatus = [controller autoMap:map name:mapName
					directory:mountPath];

				if (autoStatus == 0) loaded++;
				
				ni_proplist_free(&p);
				[mountDir release];
				[mountPath release];
				[mapName release];
			}

			ni_idlist_free(&l);
		}

		status = ni_open(d, "..", &up);
		if (status != NI_OK)
		{
			ni_free(d);
			break;
		}

		ni_free(d);
		d = up;
	}

	sys_msg(debug, LOG_DEBUG, "loadNetInfoMaps loaded %d maps", loaded);
	if (loaded == 0) return 1;
	return 0;
}

- (void)newMount:(String *)src dir:(String *)dst opts:(Array *)opts vfsType:(String *)type
{
	String *servername, *serversrc;
	Vnode *v;
	Server *server;

	serversrc = [src postfix:':'];
	if (serversrc == nil)
	{
		sys_msg(debug, LOG_DEBUG, "%s has no source directory", [src value]);
		return;
	}

	servername = [src prefix:':'];
	if (servername == nil)
	{
		sys_msg(debug, LOG_DEBUG, "%s has no server name", [src value]);
		[serversrc release];
		return;
	}

	server = [controller serverWithName:servername];
	if (server == nil)
	{
		sys_msg(debug, LOG_DEBUG, "No server named %s", [servername value]);
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
		sys_msg(debug, LOG_DEBUG, "Rejected options for %s on %s",
			[src value], [dst value]);
		[servername release];
		[serversrc release];
		return;
	}

	v = [self createVnodePath:dst from:root];

	if ([v type] == NFLNK)
	{
		sys_msg(debug, LOG_DEBUG, "Duplicate mount %s on %s",
			[src value], [dst value]);
		/* mount already exists - do not override! */
		[servername release];
		[serversrc release];
		return;
	}

	[v setServer:server];
	[v setSource:serversrc];
        [v setVfsType:type];
	[v setType:NFLNK];

	[v setupOptions:opts];
	[servername release];
	[serversrc release];

	[self setupLink:v];
}

- (void)loadMapFromDomain:(void *)d
	directory:(ni_id)dir
	base:(String *)base
	baseopts:(Array *)baseopts
{
	ni_idlist l;
	ni_proplist p;
	ni_status status;
	ni_id c;
	int i, o, len, islocalhost;
	ni_index where;
	String *src, *loc, *opt, *path, *x;
	Array *opts;
	ni_namelist *n;
	char *t, hname[1026];
	Vnode *v;
	Server *s;

	status = ni_children(d, &dir, &l);
 	if (status != NI_OK)
	{
		sys_msg(debug, LOG_ERR, "Error accessing NetInfo dir %lu: %s",
			dir.nii_object, ni_error(status));
		return;
	}

	for (i = 0; i <  l.ni_idlist_len; i++)
	{
		islocalhost = 0;
		src = nil;

		c.nii_object = l.ni_idlist_val[i];

		status = ni_read(d, &c, &p);
		if (status != NI_OK)
		{
			sys_msg(debug, LOG_ERR, "Error reading NetInfo dir %lu: %s",
				c.nii_object, ni_error(status));
			return;
		}

		where = ni_proplist_match(p, "dir", NULL);
		if (where == NI_INDEX_NULL)
			where = ni_proplist_match(p, "name", NULL);

		if (where == NI_INDEX_NULL)
		{
			ni_proplist_free(&p);
			continue;
		}

		n = &(p.ni_proplist_val[where].nip_val);
		if (n->ni_namelist_len == 0)
		{
			ni_proplist_free(&p);
			continue;
		}

		if (!(strcmp(n->ni_namelist_val[0], "localhost")))
		{
			gethostname(hname, 1024);
			loc = [String uniqueString:hname];
			strcat(hname, ":/");
			src = [String uniqueString:hname];
			islocalhost = 1;
		}
		else
		{
			loc = [String uniqueString:n->ni_namelist_val[0]];
		}

		where = ni_proplist_match(p, "link", NULL);
		if (where != NI_INDEX_NULL)
		{
			n = &(p.ni_proplist_val[where].nip_val);
			if (n->ni_namelist_len == 0)
			{
				ni_proplist_free(&p);
				continue;
			}

			x = [String uniqueString:n->ni_namelist_val[0]];
			v = [self createVnodePath:base from:root];
			[self symlink:x name:loc atVnode:v];
			ni_proplist_free(&p);
			[loc release];
			[x release];
			continue;
		}

		opts = [[Array alloc] init];
		for (o = 0; o < [baseopts count]; o++)
			[opts addObject:[baseopts objectAtIndex:o]];

		where = ni_proplist_match(p, "opts", NULL);
		if (where != NI_INDEX_NULL)
		{
			n = &(p.ni_proplist_val[where].nip_val);
			for (o = 0; o < n->ni_namelist_len; o++)
			{
				opt = [String uniqueString:n->ni_namelist_val[o]];
				[opts addObject:opt];
				[opt release];
			}
		}

		len = [base length];
		len += [loc length];
		len += 2;
		t = malloc(len);
		sprintf(t, "%s/%s", [base value], [loc value]);
		path = [String uniqueString:t];
		free(t);

		[self loadMapFromDomain:d directory:c base:path baseopts:opts];

		if (islocalhost == 0)
		{
			where = ni_proplist_match(p, "fspec", NULL);

			if (where == NI_INDEX_NULL)
			{
				ni_proplist_free(&p);
				[loc release];
				[opts release];
				[path release];
				continue;
			}

			n = &(p.ni_proplist_val[where].nip_val);
			if (n->ni_namelist_len == 0)
			{
				ni_proplist_free(&p);
				[loc release];
				[opts release];
				[path release];
				continue;
			}

			t = strchr(n->ni_namelist_val[0], ':');
			if (t == NULL)
			{
				x = [String uniqueString:n->ni_namelist_val[0]];
				s = [controller serverWithName:x];
				[x release];

				if (s == nil)
				{
					sys_msg(debug, LOG_ERR, "Can't get %s server %s",
						[path value], n->ni_namelist_val[0]);
					ni_proplist_free(&p);
					[loc release];
					[opts release];
					[path release];
					continue;
				}

				v = [self createVnodePath:path from:root];
				[v setServer:s];
				ni_proplist_free(&p);
				[loc release];
				[opts release];
				[path release];
				continue;
			}

			src = [String uniqueString:n->ni_namelist_val[0]];
		}

                [self newMount:src dir:path opts:opts vfsType:nil];

		[src release];
		[loc release];
		[opts release];
		[path release];

		ni_proplist_free(&p);

	}

	ni_idlist_free(&l);
}

- (void)loadNIMap:(unsigned int)n domain:(void *)d directory:(String *)dir
{
	ni_status status;
	ni_id nid;
	String *path;
	Array *opts;
	
	nid.nii_object = n;
	status = ni_self(d, &nid);
	if (status != NI_OK)
	{
		sys_msg(debug, LOG_ERR, "Can't read NetInfo directory %lu: %s",
			n, ni_error(status));
		return;
	}

	opts = [[Array alloc] init];

	path = [String uniqueString:""];
	[self loadMapFromDomain:d directory:nid base:path baseopts:opts];
	[path release];
	[opts release];
	[self postProcess:root];
}

- (void)loadMounts
{
	void *d, *up;
	ni_id nid;
	ni_status status;
	char line[1024];

	if (dataStore == nil) return;

	/* Search for /mountmaps/<name> */
	sprintf(line, "/mountmaps/%s", [dataStore value]);

	status = ni_open(NULL, ".", &d);
	if (status != NI_OK)
	{
		sys_msg(debug, LOG_ERR, "Can't connect to local NetInfo domain");
		return;
	}

	for (;;)
	{
		status = ni_pathsearch(d, &nid, line);
		if (status == NI_OK) break;

		status = ni_open(d, "..", &up);
		if (status != NI_OK)
		{
			sys_msg(debug, LOG_ERR, "%s: no such NetInfo directory", line);
			ni_free(d);
			return;
		}

		ni_free(d);
		d = up;
	}

	[self loadNIMap:nid.nii_object domain:d directory:mountPoint];

	ni_free(d);
}

- (void)setName:(String *)n
{
	[super setName:n];

	if (dataStore != nil) [dataStore release];
	dataStore = n;
	[dataStore retain];
}

@end
