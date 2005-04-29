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
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <errno.h>
#import <string.h>
#import <sys/stat.h>

@implementation StaticMap

int setupLink(const char *target, const char *source)
{
	struct stat sb;
	int status;
	char linkContents[PATH_MAX];
	int link_length;
	BOOL targetDirPotential = YES;

	sys_msg(debug, LOG_DEBUG, "setupLink('%s', '%s'):", target, source);
	
	/* Calling symlink(2) on an NFS filesystem will inevitably generate a lookup RPC,
	   triggering an NSL populate if inside /Network.  The best to be hoped for here
	   is that the proper symlink (right target string) already exists (e.g. on a resync),
	   in which case this routine is entirely redundant:
	 */
	status = lstat(source, &sb);
	if (status || !S_ISLNK(sb.st_mode)) {
		sys_msg(debug, LOG_DEBUG, "setupLink: lstat('%s') failed: errno = %d (%s)", source, errno, strerror(errno));
		goto Try_link;
	}
	
	link_length = readlink(source, linkContents, sizeof(linkContents) - 1);
	if (link_length == 0) {
		sys_msg(debug, LOG_DEBUG, "setupLink: readlink('%s', ...) failed: errno = %d (%s)", source, errno, strerror(errno));
		goto Try_link;
	}
	linkContents[link_length] = (char)0;
	sys_msg(debug, LOG_DEBUG, "setupLink: comparing existing link contents '%s' to target '%s'...", linkContents, target);
	if (strcmp(linkContents, target) == 0) {
		sys_msg(debug, LOG_DEBUG, "setupLink: existing link '%s' -> '%s' needs no alteration.", source, linkContents);
		status = 0;
		goto Std_Exit;
	};
	
Try_link:
	sys_msg(debug, LOG_DEBUG, "setupLink: symlink(%s', '%s')...", target, source);
	status = symlink(target, source);
	if (status != 0)
	{
		if (errno != EEXIST) sys_msg(debug, LOG_NOTICE, "Error symlinking %s to %s: %s", source, target, strerror(errno));
		if (targetDirPotential) {
			targetDirPotential = NO;		/* This will be our one and only shot at successfully retrying! */
			
			if (errno != EEXIST) sys_msg(debug, LOG_NOTICE, "Attempting to unlink %s...", source);
			status = lstat(source, &sb);
			if (status == 0)
			{
				if (S_ISDIR(sb.st_mode)) status = rmdir(source);
				else status = unlink(source);
				
				if (status == 0) goto Try_link;

				sys_msg(debug, LOG_ERR, "Cannot unlink existing %s: %s", source, strerror(errno));
				goto Error_Exit;
			}
		}
		
		sys_msg(debug, LOG_ERR, "Cannot symlink %s to %s: %s", source, target, strerror(errno));
		goto Error_Exit;
	}

	if (targetDirPotential) {
		sys_msg(debug, LOG_DEBUG, "Symlinked %s to %s", source, target);
	} else {
		sys_msg(debug, LOG_NOTICE, "Replaced existing %s with %s...", source, target);
	}
	
Error_Exit:
	/* No cleanup necessary */ ;
	
Std_Exit:
	return status;
}

- (void)newMount:(String *)src dir:(String *)dst opts:(Array *)opts vfsType:(String *)type
{
	String *servername = NULL;
	String *serversrc = NULL;
	String *link = NULL;
	Vnode *v;
	Server *server;
	int status = 0;

	serversrc = [src postfix:':'];
	if (serversrc == nil)
	{
		status = EINVAL;
		goto Error_Exit;
	}

	servername = [src prefix:':'];
	if (servername == nil)
	{
		status = EINVAL;
		goto Error_Exit;
	}

	server = [controller serverWithName:servername];
	if (server == nil)
	{
		status = EINVAL;
		goto Error_Exit;
	}

	if (![self acceptOptions:opts])
	{
		sys_msg(debug, LOG_DEBUG, "Rejected options for %s on %s (StaticMap)", [src value], [dst value]);
		status = EINVAL;
		goto Error_Exit;
	}

	link = [String concatStrings:[root path] :dst];
	
	status = setupLink([link value], [dst value]);
	if (status) goto Error_Exit;

	v = [self createVnodePath:dst from:root];
	if ([v type] == NFLNK)
	{
		/* mount already exists - do not override! */
		status = EEXIST;
		goto Error_Exit;
	}

	[v setType:NFLNK];
	[v setVfsType:type];	/* Must come before setServer:, which looks at vfsType */
	[v setServer:server];
	[v setSource:serversrc];
	[v setupOptions:opts];
	[v setServerDepth:0];
	[v addMntArg:MNT_DONTBROWSE];

Error_Exit:
	[link release];
	[servername release];
	[serversrc release];
	
	if ((status == 0) && ([self mountStyle] == kMountStyleParallel)) [self setupLink:v];
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

- (void)reInit
{
	sys_msg(debug, LOG_DEBUG, "[StaticMap reInit]: calling [super reInit]...");
	[super reInit];
	sys_msg(debug, LOG_DEBUG, "[StaticMap reInit]: removing symlinks for marked nodes...");
	[self removeLinksRecursivelyFrom:root fromMarkedNodesOnly:YES];
	sys_msg(debug, LOG_DEBUG, "[StaticMap reInit]: done.");
}

/*
 *	Before exiting, remove any symlinks we created at init time.
 */
- (void)cleanup
{
	[self removeLinksRecursivelyFrom:root fromMarkedNodesOnly:NO];
}


/*
 * As part of shutting down or reinitializing, remove any ordinary symlinks
 * that were created.  The path to the symlink is the Vnode's path minus the
 * leading part that is the path of the root node.
 *
 * For example, if the command link options were:
 *	-static /automount/static
 * and there is a static mount on /Network/Applications, then
 * /Network/Applications is a symlink to /automount/static/Network/Applications
 * and the root Vnode's path is /automount/static and the automount Vnode's
 * path is /automount/static/Network/Applications.  We need to remove the
 * /automount/static from the beginning and end up with /Network/Applications.
 */
- (void)removeLinksRecursivelyFrom:(Vnode*)v fromMarkedNodesOnly:(BOOL)markedNodesOnly
{
	int status, i, len;
	char *path;
	Array *kids;
	
	sys_msg(debug, LOG_DEBUG, "[StaticMap removeLinksRecursivelyFrom:markedNodesOnly:%d] at '%s'...",
									markedNodesOnly, (v && [v path]) ? [[v path] value] : "[void]");

	if (([v type] == NFLNK) && (!markedNodesOnly || [v marked]))
	{
		path = [[v path] value] + [[root path] length];
		sys_msg(debug, LOG_DEBUG, "unlinking %s", path);
		status = unlink(path);
		if (status != 0)
		{
			sys_msg(debug, LOG_ERR, "removeLinks cannot unlink %s: %s", path, strerror(errno));
		}
	}

	kids = [v children];
	len = 0;
	if (kids != nil)
		len = [kids count];
	for (i=0; i<len; ++i)
	{
		[self removeLinksRecursivelyFrom:[kids objectAtIndex:i] fromMarkedNodesOnly:markedNodesOnly];
	}
}

/* findTriggerPath takes root Vnode from map table and a possible 
   path created in Controller.m and checks if the path exists 
   in automount.  It returns triggerPath based on map type
 */
- (String *)findTriggerPath:(Vnode *)curRoot findPath:(String *)findPath
{
	int pathlen;
	char pathbuf[PATH_MAX];
	Vnode *newVnode = nil;

	pathlen = [[curRoot path] length];
	if (pathlen > [findPath length]) {
		sys_msg (debug, LOG_DEBUG, "findTriggerPath: Invalid pathlength");
		goto out;
	}
	
	/* findPath is already NULL-terminated from parent function */
	strcpy(pathbuf, &([findPath value][pathlen]));
	/* if string is NULL or just "/" */
	if (!strlen(pathbuf) || !strcmp(pathbuf,"/")) {
		goto out;
	}
	sys_msg (debug, LOG_DEBUG, "findTriggerPath: Finding %s for %s.", pathbuf, [findPath value]);
	newVnode = [self lookupVnodePath:[String uniqueString:pathbuf] from:curRoot];

out:
	if (newVnode) {
		sys_msg (debug, LOG_DEBUG, "findTriggerPath: Returning %s.", [[newVnode relativepath] value]);
		return [newVnode relativepath];
	} else {
		sys_msg (debug, LOG_DEBUG, "findTriggerPath: String not found.  Returning nil."); 
		return nil;
	}
}

@end
