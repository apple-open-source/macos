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
#import <sys/types.h>
#import <sys/time.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/stat.h>
#import <errno.h>
#import "AMVnode.h"
#import "AMMap.h"
#import "Server.h"
#import "AMString.h"
#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import "automount.h"
#import "log.h"
#import <syslog.h>
#include "Controller.h"

@implementation Vnode

- (Vnode *)init
{
	[super init];

	relpath = nil;
	fullpath = nil;
	name = nil;
	link = nil;
	src = nil;
	server = nil;
	map = nil;

	mountInProgress = NO;
	mounted = NO;
	fake = NO;
	mountPathCreated = NO;
	marked = NO;

	supernode = nil;
	subnodes = [[Array alloc] init];

	attributes.type = NFDIR;
	attributes.mode = 0555 | NFSMODE_DIR;
	attributes.nlink = 1;
	attributes.uid = 0;
	attributes.gid = 0;
	attributes.size = 512;
	attributes.blocksize = 512;
	attributes.rdev = 0;
	attributes.blocks = 1;
	attributes.fsid = 0;
	attributes.fileid = 0;

	gettimeofday((struct timeval *)&attributes.atime, (struct timezone *)0);
	attributes.mtime = attributes.atime;
	attributes.ctime = attributes.atime;

	mntArgs = 0;
	mntTimeout = GlobalMountTimeout;

	mountTime = 0;
	timeToLive = GlobalTimeToLive;

	nfsStatus = NFS_OK;
	forcedNFSVersion = 0;
	forcedProtocol = 0;

	urlString = nil;
	authenticated_urlString = nil;

	bzero(&nfsArgs, sizeof(struct nfs_args));
#ifdef __APPLE__
	nfsArgs.version = NFS_ARGSVERSION;
	nfsArgs.addr = (struct sockaddr *)NULL;
	nfsArgs.addrlen = sizeof(struct sockaddr_in);
	nfsArgs.sotype = SOCK_DGRAM;
	nfsArgs.proto = IPPROTO_UDP;
	nfsArgs.fh = (u_char *)NULL; 
	nfsArgs.fhsize = sizeof(nfs_fh); 
	nfsArgs.flags = NFSMNT_TIMEO | NFSMNT_RETRANS;
	nfsArgs.wsize = NFS_WSIZE;
	nfsArgs.rsize = NFS_RSIZE;
	nfsArgs.readdirsize = NFS_READDIRSIZE;
	nfsArgs.timeo = GlobalMountTimeout * 10;
	nfsArgs.retrans = NFS_RETRANS;
	nfsArgs.maxgrouplist = NFS_MAXGRPS;
	nfsArgs.readahead = NFS_DEFRAHEAD;
	nfsArgs.hostname = NULL;
#else
	nfsArgs.addr = (struct sockaddr_in *)NULL;
	nfsArgs.flags = NFSMNT_TIMEO | NFSMNT_RETRANS;
	nfsArgs.wsize = NFS_WSIZE;
	nfsArgs.rsize = NFS_RSIZE;
	nfsArgs.timeo = GlobalMountTimeout * 10;
	nfsArgs.retrans = 5;
#endif
	return self;
}

- (void)dealloc
{
	if ([controller vnodeIsRegistered:self]) {
		sys_msg(debug, LOG_ERR, "Hey?! vnode being deallocated is still registered?!");
	};
	if (mountInProgress) {
		sys_msg(debug, LOG_DEBUG, "Hey?! vnode being deallocated has mount in progress?!");
	};
	if (relpath != nil) [relpath release];
	if (fullpath != nil) [fullpath release];
	if (name != nil) [name release];
	if (src != nil) [src release];
	if (link != nil) [link release];
	if (server != nil) [server release];
	if (vfsType != nil) [vfsType release];
	if (urlString != nil) [urlString release];
	if (authenticated_urlString != nil) [authenticated_urlString release];
	if (supernode != nil) [supernode release];
	if (subnodes != nil) [subnodes release];

	[super dealloc];
}

- (String *)name
{
	return name;
}

- (void)setName:(String *)n
{
	if (n == name) return;

	[name release];
	name = [n retain];
	
	if (relpath != nil) [relpath release];
	relpath = nil;
	if (fullpath != nil) [fullpath release];
	fullpath = nil;
}

- (String *)source
{
	return src;
}

- (void)setSource:(String *)s
{
	if (s == src) return;

	[src release];
	src = [s retain];
}

- (String *)vfsType
{
	return vfsType;
}

- (void)setVfsType:(String *)s
{
	if (s == vfsType) return;

	[vfsType release];
	vfsType = [s retain];
}

- (Server *)server
{
	return server;
}

- (void)setServer:(Server *)s
{
	if (server == s) return;

	if (server != nil) [server release];
	if (s != nil) server = [s retain];
	else server = nil;
}

- (Map *)map
{
	return map;
}

- (void)setMap:(Map *)m
{
	map = m;
}

- (String *)link
{
	return link;
}

- (void)setLink:(String *)l
{
	if (l == link) return;

	[link release];
	link = [l retain];
}

- (void)setUrlString:(String *)n
{
	if (urlString != nil) [urlString release];
	urlString = n;
	[urlString retain];
}

- (void)setAuthenticatedUrlString:(String *)n
{
	if (authenticated_urlString != nil) [authenticated_urlString release];
	authenticated_urlString = n;
	[authenticated_urlString retain];
}

- (String *)urlString
{
	static String *defaultURLString = NULL;
	
	if (authenticated_urlString) return authenticated_urlString;
	if (urlString) return urlString;
	if (defaultURLString) return defaultURLString;
	defaultURLString = [String uniqueString:""];
	return defaultURLString;
}

- (String *)debugURLString
{
	return authenticated_urlString ? authenticated_urlString : urlString;
}

- (struct fattr)attributes
{
	struct fattr attrs;
	
	attrs = attributes;
	if (attrs.mode & S_ISVTX) {
		if ([self serverMounted])
			attrs.mode |= S_ISUID;
		if ([self needsAuthentication])
			attrs.mode |= S_ISGID;
	}
	
	return attrs;
}

- (void)setAttributes:(struct fattr)a
{
	attributes = a;
	gettimeofday((struct timeval *)&attributes.atime, (struct timezone *)0);
	attributes.mtime = attributes.atime;
}

- (ftype)type
{
	return attributes.type;
	gettimeofday((struct timeval *)&attributes.atime, (struct timezone *)0);
}

- (void)setType:(ftype)t
{
	attributes.type = t;
	gettimeofday((struct timeval *)&attributes.atime, (struct timezone *)0);
	attributes.mtime = attributes.atime;
	attributes.ctime = attributes.atime;
}

- (struct nfs_args)nfsArgs
{
	return nfsArgs;
}

- (unsigned int)forcedNFSVersion
{
	return forcedNFSVersion;
}

- (unsigned int)forcedProtocol
{
	return forcedProtocol;
}

- (void)resetTime
{
	gettimeofday((struct timeval *)&attributes.atime, (struct timezone *)0);
	attributes.mtime = attributes.atime;
}

- (void)markDirectoryChanged
{
	struct timeval now;

	do {
		gettimeofday(&now, NULL);
	} while ((now.tv_sec == attributes.mtime.seconds) && (now.tv_usec == attributes.mtime.useconds));
	
	attributes.mtime.seconds = now.tv_sec;
	attributes.mtime.useconds = now.tv_usec;
	attributes.ctime = attributes.mtime;
	attributes.atime = attributes.mtime;
	
	sys_msg(debug, LOG_DEBUG, "markDirectoryChanged: new mtime for '%s' is %d.%06d...",
									[[self path] value], attributes.mtime.seconds, attributes.mtime.useconds);
}

- (void)resetAllTimes
{
	struct timeval now;
	
	do {
		gettimeofday(&now, (struct timezone *)0);
	} while ((now.tv_sec == attributes.atime.seconds) && (now.tv_usec == attributes.atime.useconds));
	attributes.atime.seconds = now.tv_sec;
	attributes.atime.useconds = now.tv_usec;
	attributes.mtime = attributes.atime;
	attributes.ctime = attributes.atime;
}

- (int)mntArgs
{
	return mntArgs;
}

- (void)addMntArg:(int)arg
{
	mntArgs |= arg;
}

- (int)mntTimeout
{
	return mntTimeout;
}

- (unsigned int)nfsStatus
{
	return nfsStatus;
}

- (void)setNfsStatus:(unsigned int)s
{
	nfsStatus = s;
}

- (void)setupOptions:(Array *)o
{
	int i, x, len;
	char *s;

	len = [o count];
	for (i = 0; i < len; i++)
	{
		s = [[o objectAtIndex:i] value];

		if (!strcmp(s, "")) continue;
		else if (!strcmp(s, "rq")) continue; /* XXX */
		else if (!strcmp(s, "sw")) continue; /* XXX */
		else if (!strcmp(s, "bg")) continue; /* XXX */
		else if (!strcmp(s, "-b")) continue; /* XXX */
		else if (!strcmp(s, "net")) continue; /* XXX */
		else if (!strcmp(s, "rw")) mntArgs &= ~MNT_RDONLY;
		else if (!strcmp(s, "hard")) mntArgs &= ~NFSMNT_SOFT;

		else if (!strcmp(s, "ro")) mntArgs |= MNT_RDONLY;
		else if (!strcmp(s, "rdonly")) mntArgs |= MNT_RDONLY;
		else if (!strcmp(s, "suid")) mntArgs &= (~MNT_NOSUID);
		else if (!strcmp(s, "nosuid")) mntArgs |= MNT_NOSUID;
		else if (!strcmp(s, "exec")) mntArgs &= (~MNT_NOEXEC);
		else if (!strcmp(s, "noexec")) mntArgs |= MNT_NOEXEC;
		else if (!strcmp(s, "dev")) mntArgs &= (~MNT_NODEV);
		else if (!strcmp(s, "nodev")) mntArgs |= MNT_NODEV;
		else if (!strcmp(s, "union")) mntArgs |= MNT_UNION;
		else if (!strcmp(s, "sync")) mntArgs |= MNT_SYNCHRONOUS;

		else if (!strcmp(s, "-s")) nfsArgs.flags |= NFSMNT_SOFT;
		else if (!strcmp(s, "soft")) nfsArgs.flags |= NFSMNT_SOFT;
		else if (!strcmp(s, "-i")) nfsArgs.flags |= NFSMNT_INT;
		else if (!strcmp(s, "intr")) nfsArgs.flags |= NFSMNT_INT;
		else if (!strcmp(s, "conn")) nfsArgs.flags &= (~NFSMNT_NOCONN);
		else if (!strcmp(s, "-c")) nfsArgs.flags |= NFSMNT_NOCONN;
		else if (!strcmp(s, "noconn")) nfsArgs.flags |= NFSMNT_NOCONN;
		else if (!strcmp(s, "locks")) nfsArgs.flags &= (~NFSMNT_NOLOCKS);
		else if (!strcmp(s, "lockd")) nfsArgs.flags &= (~NFSMNT_NOLOCKS);
		else if (!strcmp(s, "-L")) nfsArgs.flags |= NFSMNT_NOLOCKS;
		else if (!strcmp(s, "nolocks")) nfsArgs.flags |= NFSMNT_NOLOCKS;
		else if (!strcmp(s, "nolockd")) nfsArgs.flags |= NFSMNT_NOLOCKS;
		else if (!strcmp(s, "-q")) nfsArgs.flags |= NFSMNT_NQNFS;
		else if (!strcmp(s, "nqnfs")) nfsArgs.flags |= NFSMNT_NQNFS;
		else if (!strcmp(s, "-2"))
		{
			nfsArgs.flags &= (~NFSMNT_NFSV3);
			forcedNFSVersion = 2;
		}
		else if (!strcmp(s, "nfsv2"))
		{
			nfsArgs.flags &= (~NFSMNT_NFSV3);
			forcedNFSVersion = 2;
		}
		else if (!strcmp(s, "-3"))
		{
			nfsArgs.flags |= NFSMNT_NFSV3;
			forcedNFSVersion = 3;
		}
		else if (!strcmp(s, "nfsv3"))
		{
			nfsArgs.flags |= NFSMNT_NFSV3;
			forcedNFSVersion = 3;
		}
		else if (!strcmp(s, "-K")) nfsArgs.flags |= NFSMNT_KERB;
		else if (!strcmp(s, "kerb")) nfsArgs.flags |= NFSMNT_KERB;
		else if (!strcmp(s, "-d")) nfsArgs.flags |= NFSMNT_DUMBTIMR;
		else if (!strcmp(s, "dumbtimer")) nfsArgs.flags |= NFSMNT_DUMBTIMR;
		else if (!strcmp(s, "-P")) nfsArgs.flags |= NFSMNT_RESVPORT;
		else if (!strcmp(s, "resvport")) nfsArgs.flags |= NFSMNT_RESVPORT;
		else if (!strcmp(s, "-l")) nfsArgs.flags |= NFSMNT_RDIRPLUS;
		else if (!strcmp(s, "rdirplus")) nfsArgs.flags |= NFSMNT_RDIRPLUS;

#ifdef __APPLE__
		else if (!strcmp(s, "-U"))
		{
			forcedProtocol = IPPROTO_UDP;
			nfsArgs.proto = IPPROTO_UDP;
		}
		else if (!strcmp(s, "UDP"))
		{
			forcedProtocol = IPPROTO_UDP;
			nfsArgs.proto = IPPROTO_UDP;
		}
		else if (!strcmp(s, "udp"))
		{
			forcedProtocol = IPPROTO_UDP;
			nfsArgs.proto = IPPROTO_UDP;
		}

		else if (!strcmp(s, "-T")) 
		{
			forcedProtocol = IPPROTO_TCP;
			nfsArgs.proto = IPPROTO_TCP;
		}
		else if (!strcmp(s, "TCP")) 
		{
			forcedProtocol = IPPROTO_TCP;
			nfsArgs.proto = IPPROTO_TCP;
		}
		else if (!strcmp(s, "tcp")) 
		{
			forcedProtocol = IPPROTO_TCP;
			nfsArgs.proto = IPPROTO_TCP;
		}

		else if (!strncmp(s, "-I=", 3))
		{
			x = atoi(s+3);
			if (x <= 0) x = NFS_READDIRSIZE;
			nfsArgs.readdirsize = x;
		}
		else if (!strncmp(s, "-a=", 3))
		{
			x = atoi(s+3);
			if (x <= 0) x = NFS_DEFRAHEAD;
			nfsArgs.readahead = x;
		}
		else if (!strncmp(s, "-g=", 3))
		{
			x = atoi(s+3);
			if (x <= 0) x = NFS_MAXGRPS;
			nfsArgs.maxgrouplist = x;
		}
#endif
		else if (!strncmp(s, "-w=", 3))
		{
			nfsArgs.flags |= NFSMNT_WSIZE;
			x = atoi(s+3);
			if (x <= 0) x = NFS_WSIZE;
			nfsArgs.wsize = x;
		}
		else if (!strncmp(s, "wsize=", 6))
		{
			nfsArgs.flags |= NFSMNT_WSIZE;
			x = atoi(s+6);
			if (x <= 0) x = NFS_WSIZE;
			nfsArgs.wsize = x;
		}
		else if (!strncmp(s, "-r=", 3))
		{
			nfsArgs.flags |= NFSMNT_RSIZE;
			x = atoi(s+3);
			if (x <= 0) x = NFS_RSIZE;
			nfsArgs.rsize = x;
		}
		else if (!strncmp(s, "rsize=", 6))
		{
			nfsArgs.flags |= NFSMNT_RSIZE;
			x = atoi(s+6);
			if (x <= 0) x = NFS_RSIZE;
			nfsArgs.rsize = x;
		}
		else if (!strncmp(s, "-t=", 3))
		{
			nfsArgs.flags |= NFSMNT_TIMEO;
			x = atoi(s+3);
			if (x <= 0) x = 7;
			nfsArgs.timeo = x;
		}
		else if (!strncmp(s, "timeo=", 6))
		{
			nfsArgs.flags |= NFSMNT_TIMEO;
			x = atoi(s+6);
			if (x <= 0) x = 7;
			nfsArgs.timeo = x;
		}
		else if (!strncmp(s, "-x=", 3))
		{
			nfsArgs.flags |= NFSMNT_TIMEO;
			x = atoi(s+3);
			if (x <= 0) x = 3;
			nfsArgs.retrans = x;
		}
		else if (!strncmp(s, "retrans=", 8))
		{
			nfsArgs.flags |= NFSMNT_TIMEO;
			x = atoi(s+8);
			if (x <= 0) x = 3;
			nfsArgs.retrans = x;
		}
		else if (!strncmp(s, "mnttimeo=", 9))
		{
			x = atoi(s+9);
			if (x <= 0) x = GlobalMountTimeout;
			mntTimeout = x;
		}
		else if (!strncmp(s, "ttl=", 4))
		{
			x = atoi(s+4);
			if (x < 0) x = GlobalTimeToLive;
			timeToLive = x;
		}
		else if (!strncmp(s, URL_KEY_STRING, sizeof(URL_KEY_STRING)-1))
		{
			String *url = [String uniqueString:(s+sizeof(URL_KEY_STRING)-1)];;
			sys_msg(debug, LOG_DEBUG, "***** Found url string %s", [url value]);
			[self setUrlString:url];
			[url release];
		}
		else if (!strncmp(s, AUTH_URL_KEY_STRING, sizeof(AUTH_URL_KEY_STRING)-1))
		{
			String *url = [String uniqueString:(s+sizeof(AUTH_URL_KEY_STRING)-1)];
			sys_msg(debug, LOG_DEBUG, "***** Found authenticated url string %s", [urlString value]);
			[self setAuthenticatedUrlString:url];
			[url release];
		}
		else if (!strncmp(s, "arch==", 6)) {}
		else if (!strncmp(s, "arch!=", 6)) {}
		else if (!strncmp(s, "endian==", 8)) {}
		else if (!strncmp(s, "endian!=", 8)) {}
		else if (!strncmp(s, "domain==", 8)) {}
		else if (!strncmp(s, "domain!=", 8)) {}
		else if (!strncmp(s, "host==", 6)) {}
		else if (!strncmp(s, "host!=", 6)) {}
		else if (!strncmp(s, "network==", 9)) {}
		else if (!strncmp(s, "network!=", 9)) {}
		else if (!strncmp(s, "netgroup==", 10)) {}
		else if (!strncmp(s, "netgroup!=", 10)) {}
		else if (!strncmp(s, "os==", 4)) {}
		else if (!strncmp(s, "os!=", 4)) {}
		else if (!strncmp(s, "osvers==", 8)) {}
		else if (!strncmp(s, "osvers!=", 8)) {}
		else if (!strncmp(s, "osvers<", 7)) {}
		else if (!strncmp(s, "osvers<=", 8)) {}
		else if (!strncmp(s, "osvers>", 7)) {}
		else if (!strncmp(s, "osvers>=", 8)) {}
		else if (!strcmp(s, "noquota")) {}
		else if (!strcmp(s, "grpid")) {}

		else
		{
			sys_msg(debug, LOG_DEBUG, "%s: option %s ignored",
				[[self path] value], s);
		}
	}
}

- (unsigned int)mode
{
	return attributes.mode;
}

- (void)setMode:(unsigned int)m
{
	attributes.mode = m;
}

- (unsigned int)nodeID
{
	return attributes.fileid;
}

- (void)setNodeID:(unsigned int)n
{
	attributes.fileid = n;
}

#define STATFSARRAY_SIZE_MARGIN 25
#define MAX_STATFSARRAY_SIZE 10000
static int statfs_array_size = 0;
static struct statfs *statfs_array = NULL;

int resize_statfs_array(int newentrycount)
{
	if (statfs_array) {
		statfs_array = realloc(statfs_array, sizeof(struct statfs) * newentrycount);
	} else {
		statfs_array = malloc(sizeof(struct statfs) * newentrycount);
	};
	if (statfs_array == NULL) return ENOMEM;
	statfs_array_size = newentrycount;
	
	return 0;
}

- (BOOL)checkPathIsMount:(char *)apath
{
	int fs_count, i;
	
	if (apath == NULL) return NO;
	
	sys_msg(debug_mount, LOG_DEBUG, "Checking path %s", apath);

fsstat_loop:
	if (statfs_array_size == 0) {
		fs_count = -1;				/* Fall into error handling loop, below */
	} else {
		fs_count = getfsstat(statfs_array, statfs_array_size * sizeof(struct statfs), MNT_NOWAIT);
	};
	if ((fs_count == -1) || (fs_count >= statfs_array_size)) {
		/* The only way to be sure ALL mounted filesystems have been included is
		 * to see the system return a value less than the available buffer space:
		 */
		fs_count = getfsstat(NULL, 0, MNT_NOWAIT);
		if (fs_count > MAX_STATFSARRAY_SIZE)  return NO;
		if (resize_statfs_array(fs_count + STATFSARRAY_SIZE_MARGIN) != 0) return NO;
		
		/* statfs_array_size SHOULD always be >0 now, but would be infinite loop otherwise... */
		if (statfs_array_size == 0) return NO;
		
		goto fsstat_loop;
	};
	
	for (i = 0; i < fs_count; ++i) {
		/* Test to see whether f_mntonname starts with 'apath': */
		if (strncmp(statfs_array[i].f_mntonname, apath, strlen(apath)) == 0) return YES;
	};
	
	return NO;
}

- (BOOL)mounted
{
	/* This code exists for two reason:
	 *
	 * 1. In support of pre-mounted AFP home directories. At login time, LoginWindow
	 * logs on to the AFP server using the user's name and password, and mounts the
	 * volume with the home directory on the same private directory that automount would
	 * use.  This code detects when such a volume has magically appeared, and proceeds
	 * as if automount had triggered the mount itself.  This way, automount does not
	 * try to mount the AFP server using guest access.
	 *
	 * When automount starts responding to volume mount/unmount notifications, this
	 * code will probably not be needed (since the notification should be able to
	 * update the mounted status of all automount points).
	 *
	 * 2. In support of changing network/directory configurations.  It's possible that
	 * the network or directory configuration gets changed to remove a server from view.
	 * If that server is currently mounted, however, it will be left mounted without a
	 * Vnode that references it.  If the directory or network configuration is changed
	 * to return the server to view, this code will detect the mount that endured and
	 * return the system to its state prior to the server's disappearance.
	 *
	 * You can use -descendantMounted to recursively check for the mounted flag being set.
	 */

	if (mounted) return YES;
	if (([self link] == NULL) || ([[self link] value] == NULL)) return NO;
	
	sys_msg(debug_mount, LOG_DEBUG, "Checking for mounts on %s...", [[self link] value]);
	mounted = [self checkPathIsMount:[[self link] value]];
	if (!mounted) [self setMountPathCreated:NO]; 

	return mounted;
}

- (BOOL)descendantMounted
{
	int i, len;
	Array *kids;
	Vnode *child;
	BOOL answer;

	answer = NO;
	kids = [self children];
	len = 0;
	if (kids != nil)
		len = [kids count];
	for (i=0; i<len; ++i)
	{
		child = [kids objectAtIndex:i];
		answer = child->mounted;
		if (answer)
			break;
		answer = [child descendantMounted];
		if (answer)
			break;
	}
	
	return answer;
}

- (BOOL)serverMounted
{
	BOOL answer;
	String *urlMountType = [String uniqueString:"url"];
	String *nslEntrySource = [String uniqueString:"*"];

	answer = [self mounted];
	
	if ([[self vfsType] equal:urlMountType] && !([[self source] equal:nslEntrySource]))	/* Don't try this for mount-all vnodes */
	{
		/*
		 *	For non-NSL mounts using a URL (such as an AFP server in fstab/NetInfo),
		 *	the mounted flag seems to always be false (since the server's sticky symlink
		 *	points to a local directory which is not a mount point).
		 *
		 *	I think that is a bug.  It may be a workaround for URL mounts apparently failing
		 *	the first time they are triggered.  In any case, we have to recursively walk the
		 *	server's hierarchy to see if anything is mounted.
		 */
		if ([self descendantMounted]) answer = YES;
	}

	[nslEntrySource release];
	[urlMountType release];

	return answer;
}

- (void)resetMountTime
{
	struct timeval tv;
	
	if (mounted)
	{
		gettimeofday(&tv, NULL);
		mountTime = tv.tv_sec;
	}
	else mountTime = 0;
}

- (struct MountProgressRecord *)mountInfo
{
	return &mountInfo;
}

- (void)setMounted:(BOOL)m
{
	mounted = m;
	[self resetMountTime];
	[self resetTime];	/* Reset time so NFS client will refetch symlink */
}

- (BOOL)mountInProgress
{
	return mountInProgress;
}

- (void)setMountInProgress:(BOOL)newMountInProgressState
{
	mountInProgress = newMountInProgressState;
}

- (unsigned long)transactionID
{
	return transactionID;
}

- (void)setTransactionID:(unsigned long)xid
{
	transactionID = xid;
}

- (BOOL)fakeMount
{
	return fake;
}

- (void)setFakeMount:(BOOL)m
{
	fake = m;
}

- (unsigned int)mountTime
{
	return mountTime;
}

- (unsigned int)mountTimeToLive
{
	return timeToLive;
}

- (BOOL)mountPathCreated
{
	return mountPathCreated;
}

- (void)setMountPathCreated:(BOOL)m
{
	mountPathCreated = m;
}

- (void)getFileHandle:(nfs_fh *)fh
{
	bzero(fh, sizeof(nfs_fh));
	bcopy(&attributes.fileid, fh, sizeof(unsigned int));
}

- (Vnode *)lookup:(String *)n
{
	int i, count;
	Vnode *sub;

	if (strcmp([n value], ".") == 0) return self;

	if (strcmp([n value], "..") == 0)
	{
		if (supernode == nil) return self;
		return supernode;
	}

	count = [subnodes count];
	for (i = 0; i < count; i++)
	{
		sub = [subnodes objectAtIndex:i];
		if ([n equal:[sub name]]) return sub;
	}

	return nil;
}

- (int)symlinkWithName:(char *)from to:(char *)to attributes:(struct nfsv2_sattr *)attributes
{
	return NFSERR_ROFS;
}

- (int)remove:(String *)name
{
	return NFSERR_ROFS;
}

- (Vnode *)parent
{
	return supernode;
}

- (void)setParent:(Vnode *)p
{
	if (supernode == p) return;

	if (supernode != nil) [supernode release];
	supernode = [p retain];
	
	if (relpath != nil) [relpath release];
	relpath = nil;
	if (fullpath != nil) [fullpath release];
	fullpath = nil;
}

- (Array *)children
{
	return (Array *)subnodes;
}

- (void)addChild:(Vnode *)child
{
	if ([subnodes containsObject:child]) return;
	[subnodes addObject:child];
	[child setParent:self];
}

- (void)removeChild:(Vnode *)child
{
	if (child == nil) return;

	if (![subnodes containsObject:child])
	{
		sys_msg(debug, LOG_ERR,
			"Attempt to remove node %d (%s) from non-parent node %d (%s)",
			[child nodeID], [[child name] value],
			[self nodeID], [[self name] value]);
		return;
	}

	[subnodes removeObject:child];
}

/* A convenience function to detect whether a node has any children */
- (BOOL)hasChildren
{
	if (subnodes == NULL)
		return NO;
	
	return ([subnodes count] != 0);
}

- (Array *)dirlist
{
	int i, count;
	Array *list;

	count = [subnodes count];
	list = [[Array alloc] init];

	[list addObject:self];
	if (supernode == nil) [list addObject:self];
	else [list addObject:supernode];

	for (i = 0; i < count; i++)
	{
		[list addObject:[subnodes objectAtIndex:i]];
	}

	return list;
}

- (String *)relativepath
{
	String *n;
	char *s;

	if (relpath != nil) return relpath;
	if (self == [map root]) return [String uniqueString:""];
	if (supernode == nil) return [String uniqueString:"/"];

	n = [supernode relativepath];
	if (!strcmp([n value], "/"))
	{
		s = malloc(1 + [name length] + 1);
		sprintf(s, "/%s", [name value]);
	}
	else
	{
		s = malloc([n length] + 1 + [name length] + 1);
		sprintf(s, "%s/%s", [n value], [name value]);
	}

	relpath = [String uniqueString:s];
	free(s);

	return relpath;
}

- (String *)path
{
	String *n;
	char *s;

	if (fullpath != nil) return fullpath;

	if (supernode == nil)
	{
		fullpath = [String uniqueString:"/"];
		return fullpath;
	}

	n = [supernode path];
	if (!strcmp([n value], "/"))
	{
		s = malloc(1 + [name length] + 1);
		sprintf(s, "/%s", [name value]);
	}
	else
	{
		s = malloc([n length] + 1 + [name length] + 1);
		sprintf(s, "%s/%s", [n value], [name value]);
	}

	fullpath = [String uniqueString:s];
	free(s);

	return fullpath;
}

- (void)invalidateRecursively:(BOOL)invalidateDescendants
{
    return;
}

/*
 * Return YES if the node's server may require authentication.
 *
 * This should be lightweight because it is called as the result of
 * an NFS getattr call.  A simple parse of a URL or options would
 * be appropriate.  Searching the keychain for an entry corresponding
 * to the server would probably be too costly.
 */
- (BOOL)needsAuthentication
{
	/* Assumes normal automount points do not require authentication. */
	return NO;
}

- (BOOL)marked
{
	return marked;
}

- (void)setMarked:(BOOL)m
{
	marked = m;
}

/*
 * Returns YES if there was an unmount in the node's hierarchy.
 * Along the way, it updates the .mounted variable to reflect the
 * current mounted status -- if it was previously marked mounted,
 * but is now unmounted.  For any node whose mounted status is changed,
 * that node's time is updated, as is the time of its parent.
 */
- (BOOL)checkForUnmount
{
	int i, len;
	Array *kids;
	BOOL result = NO;
	
	kids = [self children];
	len = 0;
	if (kids != nil)
		len = [kids count];

	if (len == 0)
	{
		/*
		 * This is a potential mount point.  Has it been unmounted?
		 */
		if (mounted && ![self fakeMount] && [self link] != nil && [self server] != nil && ![self checkPathIsMount: [[self link] value]])
		{
			sys_msg(debug, LOG_DEBUG, "%s has been unmounted.", [[self link] value]);
			[self setMounted: NO];
			[self resetTime];
			if ([self parent] != nil)
				[[self parent] resetTime];
			result = YES;
		}
	}
	else
	{
		for (i=0; i<len; ++i)
		{
			if ([[kids objectAtIndex: i] checkForUnmount])
				result = YES;
		}

		/*
		 * We need to detect when the last of a server's volumes become
		 * unmounted so we can reset the server to the unmounted state
		 * so that it will properly trigger the next time the symlink
		 * is read.
		 *
		 * Note that there are some Vnodes that are mounted, but are
		 * not servers.  They will either be marked as fake mounts,
		 * or won't have an associated server (i.e. server == nil).
		 * The checks below match -[Controller unmountMaps].
		 */
		if (mounted && ![self fakeMount] && [self server] != nil && ![self descendantMounted])
		{
			/*
			 * The last descendant for this node is now unmounted.
			 * Mark this node unmounted, and return that we
			 * detected an unmount.
			 */
			sys_msg(debug, LOG_DEBUG, "Last unmount for server %s", [[self path] value]);
			[self setMounted: NO];
			[self resetTime];
			if ([self parent] != nil)
				[[self parent] resetTime];
			result = YES;
		}
	}
	
	return result;
}

@end

