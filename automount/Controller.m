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

#import "Controller.h"
#import "automount.h"
#import "Server.h"
#import "AMString.h"
#import "AMVnode.h"
#import "FstabMap.h"
#import "StaticMap.h"
#import "FileMap.h"
#import "HostMap.h"
#import "UserMap.h"
#import "NSLMap.h"
#import "NSLVnode.h"
#import "log.h"
#import "vfs_sysctl.h"
#import <unistd.h>
#import <stdio.h>
#import <signal.h>
#import <syslog.h>
#import <stdlib.h>
#import <string.h>
#import <errno.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#import <sys/socket.h>
#import <sys/param.h>
#import <sys/queue.h>
#import <sys/types.h>
#import <sys/wait.h>
#import <grp.h>

#define BIND_8_COMPAT

#import <sys/stat.h>
#import <nfs_prot.h>
#import <arpa/nameser.h>
#import <netinfo/ni.h>
#import <resolv.h>
#import "systhread.h"
#ifdef __APPLE__
extern int bindresvport(int sd, struct sockaddr *sa);
extern int mount(const char *, const char *, int, void *);
extern int unmount(const char *, int);
#else
#import <libc.h>
#import <sys/file.h>
extern int getpid(void);
extern int mkdir(const char *, int);
extern int chdir(const char *);
#endif
#import <URLMount/URLMount.h>

#define MOUNT_COMMAND "/sbin/mount_autofs"

#define HOSTINFO "/usr/bin/hostinfo"
#define OS_NEXTSTEP 0
#define OS_OPENSTEP 1
#define OS_MACOSX 2
#define OS_DARWIN 3

#define UNMOUNTALL_USING_NODETABLE 0

static char gConsoleDevicePath[] = "/dev/console";

static Boolean VnodeKeyEqual(const void *value1, const void *value2);
static CFHashCode VnodeKeyHash(const void *value);
static CFDictionaryKeyCallBacks VnodeDictionaryKeyCallBacks = {
	0,								/* version */
	NULL,							/* retain */
	NULL,							/* release */
	NULL,							/* copyDescription */
	VnodeKeyEqual,					/* equal */
	VnodeKeyHash					/* hash */
};

static Boolean VnodeEqual(const void *value1, const void *value2);
static CFDictionaryValueCallBacks VnodeDictionaryValueCallBacks = {
	0,								/* version */
	NULL,							/* retain */
	NULL,							/* release */
	NULL,							/* copyDescription */
	VnodeEqual						/* equal */
};

extern void nfs_program_2();

extern void select_loop(void *);
extern int run_select_loop;
extern int running_select_loop;

extern int protocol_1;  
extern int protocol_2;

#warning relying on internally derived copy of private and transport-specific xid...
extern u_long rpc_xid;

extern NSLMap *GlobalTargetNSLMap;

extern BOOL doServerMounts;

static void completeMount(Vnode *v, unsigned int status);

static gid_t
gidForGroup(char *name)
{
	struct group *g;

	g = getgrnam(name);
	if (g != NULL) return g->gr_gid;

	sys_msg(debug, LOG_WARNING, "Can't get gid for group %s", name);
	return 0;
}

@implementation Controller

- (Controller *)init:(char *)dir
{
	Vnode *root;
	char str[1024], *p;
	FILE *pf;
	float vers;
	int sock;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

	[super init];

	node_table_count = 0;
	server_table_count = 0;
	map_table_count = 0;

	node_id = 2;

	controller = self;

	mountDirectory = [String uniqueString:dir];
	rootMap = [[Map alloc] initWithParent:nil directory:mountDirectory];

	root = [rootMap root];

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		sys_msg(debug, LOG_ERR, "Can't create UDP socket");
		[self release];
		return nil;
	}
	
	bzero((char *)&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bindresvport(sock, (struct sockaddr *)&addr)) {
		addr.sin_port = 0;
		if (bind(sock, (struct sockaddr *)&addr, len) == -1) {
			sys_msg(debug, LOG_ERR, "Can't bind UDP socket to INADDR_LOOPBACK");
			[self release];
			return nil;
		}
	}

	transp = svcudp_create(sock);
	if (transp == NULL)
	{
		sys_msg(debug, LOG_ERR, "Can't create UDP service");
		[self release];
		return nil;
	}

	if (!svc_register(transp, NFS_PROGRAM, NFS_VERSION, nfs_program_2, 0))
	{
		sys_msg(debug, LOG_ERR, "svc_register failed");
		[self release];
		return nil;
	}

	gethostname(str, 1024);
	p = strchr(str, '.');
	if (p != NULL) *p = '\0';
	hostName = [String uniqueString:str];

	hostDNSDomain = nil;
	res_init();
	if (_res.options & RES_INIT)
	{
		hostDNSDomain = [String uniqueString:_res.defdname];
	}

#if defined (__ARCHITECTURE__)
	hostArchitecture = [String uniqueString:__ARCHITECTURE__];
#elif defined(__ppc__)
	hostArchitecture = [String uniqueString:"ppc"];
#elif defined(__i386__)
	hostArchitecture = [String uniqueString:"i386"];
#else
#error Unknown architecture
#endif

	hostByteOrder = nil;

#ifdef __BIG_ENDIAN__
	hostByteOrder = [String uniqueString:"big"];
#else
	hostByteOrder = [String uniqueString:"little"];
#endif

	pf = popen(HOSTINFO, "r");
	fscanf(pf, "%*[^\n]%*c");
	fscanf(pf, "%[^\n]%*c", str);
	pclose(pf);

	vers = 0.0;
	hostOS = NULL;

	p = strchr(str, ':');
	if (p != NULL)
	{
		p = strrchr(str, ' ');
		if (p != NULL)
		{
			sscanf(p+1, "%f", &vers);
		}
	}

	p = strchr(str, ' ');
	if (p != NULL)
	{
		p++;
		if (!strncmp(p, "NeXT", 4))
		{
			if (vers > 3.3)
			{
				hostOS = [String uniqueString:"openstep"];
				osType = OS_OPENSTEP;
			}
			else
			{
				hostOS = [String uniqueString:"nextstep"];
				osType = OS_NEXTSTEP;
			}
		}
		else if (!strncmp(p, "Darwin", 6))
		{
			hostOS = [String uniqueString:"darwin"];
			osType = OS_DARWIN;
		}
		else if (!strncmp(p, "Kernel", 6)) 
		{
			hostOS = [String uniqueString:"macosx"];
			osType = OS_MACOSX;
		}
	}

	if (hostOS == NULL) hostOS = [String uniqueString:"macosx"];

	sprintf(str, "%g", vers);
	hostOSVersion = [String uniqueString:str];
	hostOSVersionMajor = vers;
	p = strchr(str, '.');
	if (p == NULL) hostOSVersionMinor = 0;
	else hostOSVersionMinor = atoi(p+1);


	return self;
}

- (BOOL)createPath:(String *)path withUid:(int)uid allowAnyExisting:(BOOL)allowAnyExisting
{
	int i, p;
	char *s, t[1024];
	int status;
	struct stat dirinfo;
	BOOL successful = YES; 

	if (path == nil) return YES;
	if ([path length] == 0) return YES;

	p = 0;
	s = [path value];

	chdir("/");

	while (s != NULL)
	{
		/* Strip off leading slashes: */
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

		sys_msg(debug, LOG_DEBUG, "Creating intermediate directory %s...", t);
		
		status = mkdir(t, 0755);
		if (status == -1)
		{
			if (errno == EISDIR) {
				status = 0;
			} else if (errno != EEXIST) {
				goto Fail;
			} else if (allowAnyExisting) {
				status = 0;
			} else {
				status = lstat(t, &dirinfo);
				if (status == -1) goto Fail;
				if (! S_ISDIR(dirinfo.st_mode)) goto Fail;
			}
		}

		if (status != 0) goto Fail;

		chdir(t);
		s = [path scan:'/' pos:&p];
	}
	goto Done;
	
Fail:
	successful = NO;
	
Done:
	chdir("/");
	return successful;
}

- (void)hashVnode:(Vnode *)v
{
	if (vnodeHashTable == nil) {
		vnodeHashTable = CFDictionaryCreateMutable(NULL, 0, &VnodeDictionaryKeyCallBacks, &VnodeDictionaryValueCallBacks);
	}
	CFDictionaryAddValue(vnodeHashTable, [v hashKey], v);
}

- (void)unhashVnode:(Vnode *)v
{
	if (vnodeHashTable) CFDictionaryRemoveValue(vnodeHashTable, [v hashKey]);
}

- (Vnode *)vnodeWithKey:(void *)vnodeKey {
	return (vnodeHashTable == nil) ? nil : (Vnode *)CFDictionaryGetValue(vnodeHashTable, vnodeKey);
}

- (void)registerVnode:(Vnode *)v
{
	[v setNodeID:node_id];

	if (node_table_count == 0)
		node_table = (node_table_entry *)malloc(sizeof(node_table_entry));
	else
		node_table = (node_table_entry *)realloc(node_table,
			(node_table_count + 1) * sizeof(node_table_entry));

	node_table[node_table_count].node_id = node_id;
	node_table[node_table_count].node = v;

	node_table_count++;
	node_id++;
}

- (BOOL)vnodeIsRegistered:(Vnode *)v
{
	int i;

	for (i = 0; i < node_table_count; i++)
	{
		if (node_table[i].node == v) return YES;
	}

	return NO;
}

- (Vnode *)vnodeWithID:(unsigned int)n
{
	int i;
	Vnode *v;

	for (i = 0; i < node_table_count; i++)
	{
		if (node_table[i].node_id == n)
		{
			v = node_table[i].node;
			[v markAccessTime];
			return v;
		}
	}

	return nil;
}

- (void)compactVnodeTableFrom:(int)startIndex
{
	int i, empty;

	/*
	    Track two pointer in a single pass across the vnode table:
			'empty' is a trailing pointer to the leftmost empty slot,
			'i' is the slot being checked
	 */
	for (i=0, empty=0; i < node_table_count; ++i)
	{
		if (node_table[i].node != nil)
		{
			/* This node is still in use, so shift it down */
			if (i != empty) {
				node_table[empty] = node_table[i];
				node_table[i].node_id = 0;
				node_table[i].node = nil;
			};
			++empty;
		}
	}
	
	if (empty != node_table_count)
	{
		node_table_count = empty;
		if (node_table_count == 0)
		{
			free(node_table);
			node_table = nil;
		} else {
			node_table = (node_table_entry *)realloc(node_table,
			node_table_count * sizeof(node_table_entry));
		};
	};
}

- (void)freeVnode:(Vnode *)v
{
	int i, nodeIndex = -1;
	Vnode *p;

	for (i = 0; i < node_table_count; i++)
	{
		if (node_table[i].node == v) {
			nodeIndex = i;
			break;
		};
	}

	if (nodeIndex == -1)
	{
		sys_msg(debug, LOG_ERR, "freeVnode for unregistered Vnode %u (%s)", [v nodeID], [[v path] value]);
	} else {
		node_table[nodeIndex].node = nil;
		node_table[nodeIndex].node_id = 0;
	};
	
	p = [v parent];
	if (p != nil) [p removeChild:v];
	[v release];
}

- (void)removeVnode:(Vnode *)v
{
	int i, err;
	unsigned int count;
	Array *kids;

	kids = [v children];
	if (kids == nil) count = 0;
	else count = [kids count];

	for (i = count - 1; i >= 0; i--)
		[self removeVnode:[kids objectAtIndex:i]];

	if ([[v map] mountStyle] == kMountStyleAutoFS) {
		err = rmdir([[v path] value]);
		if (err)
			sys_msg(debug, LOG_ERR, "Cannot remove %s: %s", [[v path] value], strerror(errno));
	}
	
	[self freeVnode:v];
}

- (void)destroyVnode:(Vnode *)v
{

	[self removeVnode:v];
	
	/*
	   Note that, in the process of deleting child nodes, nodes in unknown indices
	   may have been removed from the table; the only safe way to catch up on all
	   the table compaction now necessary is to start from 0.
	 */
	[self compactVnodeTableFrom:0];
}

- (int)autofsmount:(Vnode *)v directory:(String *)dir args:(int)mntargs
{
	char str[MAXPATHLEN + 64];
	String *src;
    pid_t pid, terminated_pid;
    int result=0;
    union wait status;

	[self createPath:dir withUid:0 allowAnyExisting:YES];

	src = [v source];
	
	sprintf(str, "automount %s [%d]", [src value], getpid());
	[[v map] setHostname:[String uniqueString:str]];
	sys_msg(debug, LOG_DEBUG, "Mounting map %s on %s", [src value], [dir value]);

    pid = fork();
    if (pid == 0) {
		result = execl(MOUNT_COMMAND,
			MOUNT_COMMAND,
//			"-o", (mntargs & MNT_AUTOMOUNTED) ? "automounted" : "noautomounted",
//			"-o", (mntargs & MNT_DONTBROWSE) ? "nobrowse" : "browse",
//			str,
			"-f",
			str,
			[dir value],
			NULL);
		/* IF WE ARE HERE, WE WERE UNSUCCESSFUL */
        exit(result ? result : ECHILD);
    }

    if (pid == -1) {
        result = -1;
        goto mount_complete;
    }

    /* Success! */
    while ( (terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0 ) {
		/* retry if EINTR, else break out with error */
		if ( errno != EINTR ) {
			break;
		}
    }
    
    if (terminated_pid == pid) {
		if (WIFEXITED(status)) {
			result = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			result = EFAULT;
		} else {
			result = -1;
		}
	}

mount_complete:
	if (result != 0)
	{
		sys_msg(debug, LOG_ERR, "Can't autofs-mount map %s on %s: %s",
			[src value], [dir value], strerror(errno));
		return 1;
	}
	
	sys_msg(debug, LOG_DEBUG, "Mounted autofs %s on %s", [src value], [dir value]);

	[v setMounted:YES];
	
#ifndef __APPLE__
	[self mtabUpdate:v];
#endif

	return 0;

}

- (int)automount:(Vnode *)v directory:(String *)dir args:(int)mntargs nfsmountoptions:(int)mntoptionflags
{
	struct nfs_args args;
	struct sockaddr_in sin;
	struct file_handle fh;
	char str[MAXPATHLEN + 64];
	String *src;
	int status;

	[self createPath:dir withUid:0 allowAnyExisting:YES];

	src = [v source];

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(transp->xp_port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	bzero(&args, sizeof(args));

#ifdef __APPLE__
	args.addr = (struct sockaddr *)&sin;
	args.version = NFS_ARGSVERSION;
	args.addrlen = sizeof(struct sockaddr_in);
	args.sotype = SOCK_DGRAM;
	args.proto = IPPROTO_UDP;
	args.readdirsize = NFS_READDIRSIZE;
	args.maxgrouplist = NFS_MAXGRPS;
	args.readahead = NFS_DEFRAHEAD;
	args.fhsize = sizeof(nfs_fh);
	args.flags = mntoptionflags | NFSMNT_INT | NFSMNT_TIMEO | NFSMNT_RETRANS | NFSMNT_NOLOCKS;
	if (mntoptionflags & NFSMNT_ACREGMIN) args.acregmin = 0;
	if (mntoptionflags & NFSMNT_ACREGMAX) args.acregmax = 0;
	if (mntoptionflags & NFSMNT_ACDIRMIN) args.acdirmin = 0;
	if (mntoptionflags & NFSMNT_ACDIRMAX) args.acdirmax = 0;
	args.wsize = NFS_WSIZE;
	args.rsize = NFS_RSIZE;
#else
	args.addr = (struct sockaddr_in *)&sin;
	args.flags = NFSMNT_INT | NFSMNT_TIMEO | NFSMNT_RETRANS;
	args.wsize = NFS_WSIZE;
	args.rsize = NFS_RSIZE;
#endif
	if (debug == DEBUG_STDERR) {
		/* Don't hang system on internal errors */
		args.flags &= ~NFSMNT_INT;
		args.flags |= NFSMNT_SOFT;
	}

	args.timeo = 1;
	args.retrans = 5;

	bzero(&fh, sizeof(nfs_fh));
	fh.node_id = [v nodeID];

	args.fh = (u_char *)&fh; 
	sprintf(str, "automount %s [%d]", [src value], getpid());
	[[v map] setHostname:[String uniqueString:str]];
	args.hostname = [[[v map] hostname] value];

	sys_msg(debug, LOG_DEBUG, "Mounting map %s on %s", [src value], [dir value]);

#ifdef __APPLE__
	status = mount("nfs", [dir value], mntargs | MNT_AUTOMOUNTED, &args);
#else
	status = mount(MOUNT_NFS, [dir value], mntargs | MNT_AUTOMOUNTED), (caddr_t)&args);
#endif
	if (status != 0)
	{
		sys_msg(debug, LOG_ERR, "Can't mount map %s on %s: %s",
			[src value], [dir value], strerror(errno));
		return 1;
	}

	sys_msg(debug, LOG_DEBUG, "Mounted %s on %s", [src value], [dir value]);

	[v setMounted:YES];
#ifndef __APPLE__
	[self mtabUpdate:v];
#endif

	return 0;
}

- (BOOL)isFile:(String *)name
{
	struct stat sb;
	int status;

	if (name == nil) return NO;

	status = stat([name value], &sb);
	if (status != 0)
	{
		sys_msg(debug, LOG_ERR, "%s: %s", [name value], strerror(errno));
		return NO;
	}

	if (!(sb.st_mode & S_IFREG))
	{
		sys_msg(debug, LOG_ERR, "%s: Not a file", [name value]);
		return NO;
	}

	return YES;
}

- (int)autoMap:(Map *)map name:(String *)name directory:(String *)dir mountdirectory:(String *)mnt
{
	Vnode *maproot;
	int status;

	maproot = [map root];
	[maproot setSource:name];
	[maproot setLink:dir];

	if (map_table_count == 0)
		map_table = (map_table_entry *)malloc(sizeof(map_table_entry));
	else
		map_table = (map_table_entry *)realloc(map_table,
			(map_table_count + 1) * sizeof(map_table_entry));

	map_table[map_table_count].name = [name retain];
	map_table[map_table_count].dir = [dir retain];
	map_table[map_table_count].mountdir = [mnt retain];
	map_table[map_table_count].map = map;

	if ([map mountStyle] == kMountStyleAutoFS) {
		status = [self autofsmount:maproot directory:dir args:[map mountArgs]];
	} else {
		status = [self automount:maproot directory:dir args:[map mountArgs] nfsmountoptions:[map NFSMountOptions]];
	};
	if (status != 0) return status;

	map_table_count++;
	
	status = [map didAutoMount];
	return status;
}

/* Find autmount trigger path for given findmnt */
- (String *)findDirByMountDir:(String *)findmnt
{
	String *triggerPath = nil;
	int i, mountdir_len;
	int max_mountdir_len = -1, offset = -1;

	sys_msg(debug, LOG_DEBUG, "findDirByMountDir: Finding trigger path for %s.", [findmnt value]);
	
	/* traverse through map_table to find matching mountdir */
	for (i = 0; i < map_table_count; i++) {
		sys_msg(debug, LOG_DEBUG, "map_table[%d] dir=%s mountdir=%s.", i, [map_table[i].dir value], [map_table[i].mountdir value]); 
		/* Check if mountdir is the substring of findmnt */
		mountdir_len = [map_table[i].mountdir length];
		if (strncmp([map_table[i].mountdir value], [findmnt value], mountdir_len) == 0) {
			sys_msg(debug, LOG_DEBUG, "Substring %s found in %s.", [map_table[i].mountdir value], [findmnt value]);
			/* Check if this substring is the largest occuring substring.  Store the length and offset if it is */
			if (mountdir_len > max_mountdir_len) {
				max_mountdir_len = mountdir_len;
				offset = i;
			}
		}
	}

	if (max_mountdir_len > 0) {
		char findmntpath[PATH_MAX];
		int total_len;

		/* found the offset of the map table that we want to access */
		sys_msg(debug, LOG_DEBUG, "Best candidate for %s is %s.", [findmnt value], [map_table[offset].mountdir value]);
		
		/* replace /private/var/automount with /automount/static */
		/* Replace the prefix string from findmnt similar with 
		 * map_table.mountdir with map_table.dir.  We pass this
		 * string to appropriate Map to check if path exists.
		 * newstring = map_table.dir + (findmnt - map_table.mountdir)
		 * /private/var/automount/riemann => /automount/static/riemann
		 * /private/var/automount/Network => /automount/static/Network
		 */

		/* check if total length is less than PATH_MAX */
		total_len = [map_table[offset].dir length] + strlen(&([findmnt value][max_mountdir_len])) + 1;
		if (total_len > (PATH_MAX-1)) {
			sys_msg (debug, LOG_ERR, "findDirByMountDir: Path length of new string > PATH_MAX");
			goto out;
		}
		strncpy(findmntpath, [map_table[offset].dir value], [map_table[offset].dir length]+1); 
		/* findmnt is NULL terminated from parent function */
		strcat(findmntpath, &([findmnt value][max_mountdir_len]));
		
		/* send message to the indivdual map to get the trigger path */
		triggerPath = [map_table[offset].map findTriggerPath:[map_table[offset].map root] findPath:[String uniqueString:findmntpath]];
	} else {
		/* no matching string was found */
		sys_msg(debug, LOG_DEBUG, "findDirByMountDir: No matching string found.");
	}

out:
	return triggerPath;
}

- (int)mountmap:(String *)mapname directory:(String *)dir mountdirectory:(String *)mnt
{
	Vnode *root, *p;
	Map *map;
	char *s, *t;
	String *parent, *mountpt;
	id mapclass;
    int mountstatus;

	mapclass = [Map class];

	if (strcmp([mapname value], "-fstab") == 0)
	{
		mapclass = [FstabMap class];
	}
	else if (strcmp([mapname value], "-static") == 0)
	{
		mapclass = [StaticMap class];
	}
	else if (strcmp([mapname value], "-host") == 0)
	{
		mapclass = [HostMap class];
	}
	else if (strcmp([mapname value], "-user") == 0)
	{
		mapclass = [UserMap class];
	}
	else if (strcmp([mapname value], "-nsl") == 0)
	{
		mapclass = [NSLMap class];
	}
	else if ([self isFile:mapname])
	{
		mapclass = [FileMap class];
	}
	else if (strcmp([mapname value], "-null") == 0)
	{
		mapclass = [Map class];
	}
	else
	{
		sys_msg(debug, LOG_ERR, "Unknown map \"%s\"", [mapname value]);
		return 1;
	}

	root = [rootMap root];
	s = malloc([dir length] + 1);
	sprintf(s, "%s", [dir value]);
	t = strrchr(s, '/');
	if (t == NULL) 
	{
		sys_msg(debug, LOG_ERR, "Invalid directory \"%s\"", [dir value]);
		free(s);
		return 1;
	}

	*t++ = '\0';
	parent = [String uniqueString:s];
	mountpt = [String uniqueString:t];
	free(s);
	p = [rootMap createVnodePath:parent from:root];

	sys_msg(debug, LOG_DEBUG, "Initializing map \"%s\" parent \"%s\" mountpt \"%s\"", [mapname value], [parent value], [mountpt value]);
	[parent release];

	map = [[mapclass alloc] initWithParent:p directory:mountpt from:mapname mountdirectory:mnt];
	if (mapclass == [NSLMap class]) GlobalTargetNSLMap = (NSLMap *)map;

	[mountpt release];

	if (map == nil)
	{
		sys_msg(debug, LOG_ERR, "Map \"%s\" failed to initialize", [mapname value]);
		return 1;
	}

	mountstatus = [self autoMap:map name:mapname directory:dir mountdirectory:mnt];
    if (mountstatus) return mountstatus;
    
    return 0;
}

- (Map *)rootMap
{
	return rootMap;
}

- (int)nfsmount:(Vnode *)v withUid:(int)uid
{
	struct sockaddr_in sin;
	struct nfs_args args;
	char str[1024];
	struct file_handle fh;
	Server *s;
	int status;
	unsigned int vers, proto;
	unsigned short port;
    unsigned long urlMountFlags = kMarkAutomounted | kUseUIProxy;
	char *url;
	char mountDir[PATH_MAX];
	sigset_t curr_set;
	sigset_t blocked_set;
	pid_t mountPID;
	char retString[1024];
	uid_t effuid;
	gid_t storedgid, storedegid;
	struct stat sb;

#ifndef __APPLE__
	unsigned int fhsize;
#endif

	/*
	   It's possible this node was mounted without the automounter noticing:
	   make no assumptions here and double-check to avoid double-mounting.
	 */
	invalidate_fsstat_array();

	if (([[v map] mountStyle] != kMountStyleAutoFS) && [v mounted])
	{
		sys_msg(debug_mount, LOG_DEBUG, "%s is already mounted",
			[[v link] value]);
		[v setNfsStatus:NFS_OK];
		return 0;
	}
	
	if ([v source] == nil) {
		/* This is just an intermediate directory */
		[v setMounted:YES];
		return 0;
	} else {
		if ([[v map] mountStyle] == kMountStyleAutoFS) urlMountFlags |= kCreateNewSession;
		if ([v mntArgs] & MNT_DONTBROWSE) urlMountFlags |= kMarkDontBrowse;
		if ([v mntArgs] & MNT_NOSUID) urlMountFlags |= kNoSetUID;
		if ([v mntArgs] & MNT_NODEV) urlMountFlags |= kNoDevices;
		if ([v source] && (strcmp([[v source] value], "*") == 0)) {
			if ([v needsAuthentication]) {
				/* Mounting this URL may involve UI: */
				if (gUIAllowed) {
					urlMountFlags |= kMountAll;
				} else {
					sys_msg(debug, LOG_ERR, "Cannot mount URL '%s' for %s (UI not allowed).", [[v urlString] value], [[v path] value]);
					[v setNfsStatus:NFSERR_NXIO];
					return 1;
				}
			} else {
				/* A URL for a non-authenticated service (NFS): */
				urlMountFlags |= kMountAtMountdir;
			}
		} else {
			/* Not a mount-all server URL: */
			urlMountFlags |= kMountAtMountdir;
		};
	};

	s = [v server];
	if (s == nil)
	{
		sys_msg(debug, LOG_ERR, "No file server for %s", [[v link] value]);
		[v setNfsStatus:NFSERR_NXIO];
		return 1;
	}

	if (![v mountPathCreated])
	{
		if (![self createPath:[v link] withUid:uid allowAnyExisting:NO])
		{
			sys_msg(debug, LOG_ERR, "Can't create mount point %s",
				[[v link] value]);
			[v setNfsStatus:NFSERR_IO];
			return 1;
		}

		[v setMountPathCreated:YES];
	}

	sprintf(str, "%s:%s", [[s name] value], [[v source] value]);

	[s setTimeout:[v mntTimeout]];

	args = [v nfsArgs];
	args.hostname = str;

	sys_msg(debug, LOG_DEBUG, "Mounting %s on %s", str, [[v link] value]);

#ifdef __APPLE__
	vers = [v forcedNFSVersion];
	proto = [v forcedProtocol];

	if ([v vfsType] && (strcmp([[v vfsType] value], "url") == 0))
	{ 
#ifdef Darwin
		/* Darwin doesn't have AFP support.  */
		status = 1;  // fail
#else /* Darwin */
		/* Use URLMount to mount the specified URL: */
		effuid = geteuid();
		storedgid = getgid();
		storedegid = getegid();

		if ([v urlString] == nil) {
			sys_msg(debug, LOG_ERR, "Controller.nfsmount:withUid: nil URL string for %s?!", [[v name] value]);
			status = NFSERR_IO;
			goto URLMount_Failure;
		};
		
		url = [[v urlString] value];
		if (realpath([[v link] value], mountDir) == NULL)
		{
			sys_msg(debug, LOG_ERR, "Couldn't get real path for %s (%s: %s)", [[v link] value],
				mountDir, strerror(errno));
			status = NFSERR_IO;
			goto URLMount_Failure;
		}

		/* chown the path to the passed in UID */
		chown(mountDir, uid, gidForGroup("nobody"));

		status = 0;
		
		sigemptyset(&blocked_set);
		sigaddset(&blocked_set, SIGCHLD);
		sigprocmask(SIG_BLOCK, &blocked_set, &curr_set);
		
		if ([self mountInProgressForVnode:v forUID:uid]) {
			/* Don't bother forking for another mount request when one is already in progress;
			   delaying this response could result in a deadlock if it's coming (even indirectly)
			   from the UI mount proxy of the mount in progress
			   
			   Even though it opens up a potential race condition, skip the actual work of mounting now: */
			sys_msg(debug, LOG_DEBUG, "Blocked on mount transaction id 0x%08lx", [v transactionID]);
			gBlockedMountDependency = YES;
			gBlockingMountTransactionID = [v transactionID];
		} else {
			[v incrementMountInProgressCount];
			[v setTransactionID:rpc_xid];
			mountPID = fork();
			if (mountPID == -1) {
				status = (errno != 0) ? errno : -1;
			} else {
				status = 0;
				if (mountPID) {
					/* We are the parent process; abandon this call and let child process generate reply */
					gForkedMountInProgress = YES;
					gForkedMountPID = mountPID;
					
					/* The child process will eventually signal (SIGCHLD) when the mount is complete; mark the vnode as
					   'mount in progress' to prevent starting more than one mount while this attempt is in progress. */
					(void)[self recordMountInProgressFor:v uid:uid mountPID:mountPID transactionID:rpc_xid];
				} else {
					/* We are the child process; continue with this call but don't fall back into the main service loop */
					gForkedMount = YES;
				};
			};
		};
		
		sigprocmask(SIG_SETMASK, &curr_set, NULL);
		
		/* If there's a forked mount in progress we're not the ones to do the mount;
		   if we're a blocked dependency, we're not the ones to do the mount: */
		if ((status == 0) && !gForkedMountInProgress && !gBlockedMountDependency) {
			sys_msg(debug_mount, LOG_DEBUG, "Changing real and effective uid to %d...", uid);
			setreuid(getuid(), uid);
			setgid(gidForGroup("unknown"));  // unknown
			setegid(gidForGroup("unknown"));  // unknown

			retString[0] = (char)0;
			
			/* Look at the system console to figure out the uid of the logged-in user, if any: */
			status = stat(gConsoleDevicePath, &sb);
			if ((status != 0) || (sb.st_uid != uid) || ![v needsAuthentication]) {
				/* As a user different than the logged-in user, the UI proxy won't even TRY to mount a volume;
				if the URL is complete, though, this will successfully mount it without UI
				*/
				sys_msg(debug_mount, LOG_DEBUG, "Attempting to quietly automount URL '%s':", url);
				sys_msg(debug_mount, LOG_DEBUG, "\tserver = %s", [[[v server] name] value]);
				sys_msg(debug_mount, LOG_DEBUG, "\tmountdir = %s", mountDir);
				sys_msg(debug_mount, LOG_DEBUG, "\toptions = 0x%08lx", urlMountFlags & ~kUseUIProxy);
				sys_msg(debug_mount, LOG_DEBUG, "\tuid = %d", uid);
				status = MountCompleteURL(url, mountDir, sizeof(retString), retString, urlMountFlags & ~kUseUIProxy);
			} else {
				sys_msg(debug_mount, LOG_DEBUG, "Attempting to automount URL '%s':", url);
				sys_msg(debug_mount, LOG_DEBUG, "\tserver = %s", [[[v server] name] value]);
				sys_msg(debug_mount, LOG_DEBUG, "\tmountdir = %s", mountDir);
				sys_msg(debug_mount, LOG_DEBUG, "\toptions = 0x%08lx", urlMountFlags);
				sys_msg(debug_mount, LOG_DEBUG, "\tuid = %d", uid);
				if ((urlMountFlags & kUseUIProxy) && ([[v map] mountStyle] == kMountStyleAutoFS)) {
					status = AutomountServerURL(url, mountDir, &[v hashKey]->fsid, [v nodeID], sizeof(retString), retString, urlMountFlags | kAutoFSMount);
				} else {
					status = MountServerURL(url, mountDir, sizeof(retString), retString, urlMountFlags);
				}
			};
			if ((status == 0) && (urlMountFlags & kMountAll)) {
				strncpy(retString, mountDir, sizeof(retString));
				retString[sizeof(retString)-1] = (char)0;			/* Make sure it's terminated */
			};
		
			sys_msg(debug_mount, LOG_DEBUG, "Reverting real and effective uid to %d...", effuid);
			setreuid(getuid(), effuid);
			setgid(storedgid);
			setegid(storedegid);
	
			if (status)
			{
				sys_msg(debug_mount, LOG_DEBUG, "Received status = %d from forked MountServerURL", status);
			}
URLMount_Failure: ;
		};
#endif /* Darwin */
	}
	else
	{
		/* nfs */

		status = 1;

		if ((vers == 3) || (vers == 0))
		{
			/* Try NFS Version 3 */
			args.flags |= NFSMNT_NFSV3;

			if ((proto == protocol_1) || (proto == 0))
			{
				/* Try preferred protocol */
				args.proto = protocol_1;

				sys_msg(debug_mount, LOG_DEBUG, "Fetching NFS_V3/%s filehandle for %s", (args.proto == IPPROTO_UDP) ? "UDP" : "TCP", str);

				status = [s getHandle:(nfs_fh *)&fh size:&args.fhsize port:&port forFile:[v source] version:3 protocol:args.proto];
				if ((status != 0) && (vers == 3) && (proto != 0))
				{
					[v setNfsStatus:status];
					return 1;
				}
			}

			if ((status != 0) && ((proto == protocol_2) || (proto == 0)))
			{
				/* Try secondary protocol */
				args.proto = protocol_2;

				sys_msg(debug_mount, LOG_DEBUG, "Fetching NFS_V3/%s filehandle for %s", (args.proto == IPPROTO_UDP) ? "UDP" : "TCP", str);

				status = [s getHandle:(nfs_fh *)&fh size:&args.fhsize port:&port forFile:[v source] version:3 protocol:args.proto];
				if ((status != 0) && (vers == 3))
				{
					[v setNfsStatus:status];
					return 1;
				}
			}
		}

		if (status != 0)
		{
			/* Try NFS Version 2 */
			args.flags &= (~NFSMNT_NFSV3);

			if ((proto == protocol_1) || (proto == 0))
			{
				/* Try preferred protocol */
				args.proto = protocol_1;

				sys_msg(debug_mount, LOG_DEBUG, "Fetching NFS_V2/%s filehandle for %s", (args.proto == IPPROTO_UDP) ? "UDP" : "TCP", str);

				status = [s getHandle:(nfs_fh *)&fh size:&args.fhsize port:&port forFile:[v source] version:2 protocol:args.proto];
				if ((status != 0) && (proto != 0))
				{
					[v setNfsStatus:status];
					return 1;
				}
			}

			if ((status != 0) && ((proto == protocol_2) || (proto == 0)))
			{
				/* Try secondary protocol */
				args.proto = protocol_2;

				sys_msg(debug_mount, LOG_DEBUG, "Fetching NFS_V2/%s filehandle for %s", (args.proto == IPPROTO_UDP) ? "UDP" : "TCP", str);

				status = [s getHandle:(nfs_fh *)&fh size:&args.fhsize port:&port forFile:[v source] version:2 protocol:args.proto];
			}
		}

		if (status != 0)
		{
			[v setNfsStatus:status];
			return 1;
		}

		args.fh = (u_char *)&fh;
		if (args.proto == IPPROTO_UDP) args.sotype = SOCK_DGRAM;
		else args.sotype = SOCK_STREAM;
		proto = args.proto;

		vers = 2;
		if (args.flags & NFSMNT_NFSV3) vers = 3;

		bzero(&sin, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = [s address];
		args.addr = (struct sockaddr *)&sin;

		if (realpath([[v link] value], mountDir) == NULL)
		{
			sys_msg(debug, LOG_ERR, "Couldn't get real path for %s (%s: %s)", [[v link] value],
				mountDir, strerror(errno));
			status = NFSERR_IO;
		}
		else
		{
			status = mount("nfs", mountDir, [v mntArgs] | MNT_AUTOMOUNTED, &args);
			if (status == -1) status = (errno != 0) ? errno : EINVAL;
		}
	}
#else /* __APPLE__ */
	if ([v vfsType] && (strcmp([[v vfsType] value], "url") == 0))
	{
		status = 1;  // fail
	}
	else
	{
		vers = 2;
		proto = IPPROTO_UDP;
		args.flags &= (~NFSMNT_NFSV3);

		sys_msg(debug_mount, LOG_DEBUG, "Fetching filehandle for %s", str);

		status = [s getHandle:(nfs_fh *)&fh size:&fhsize port:&port forFile:[v source] version:vers protocol:proto];

		if (status != 0)
		{
			[v setNfsStatus:status];
			return 1;
		}

		args.fh = (u_char *)&fh;

		bzero(&sin, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_port = port;
		sin.sin_addr.s_addr = [s address];
		args.addr = (struct sockaddr_in *)&sin;
		status = mount(MOUNT_NFS, [[v link] value], [v mntArgs] | MNT_AUTOMOUNTED, (caddr_t)&args);
		if (status == -1) status = (errno != 0) ? errno : EINVAL;
	}
#endif /* __APPLE__ */
	
	if (gForkedMount || gSubMounter) gMountResult = status;

	if (!gForkedMountInProgress && !gBlockedMountDependency) {
		completeMount(v, status);
		return status;
	};

	return 0;
}

static void AddMountsInProgressListEntry(struct MountProgressRecord *pr)
{
	sigset_t curr_set;
	sigset_t block_set;

	/* Update the global mounts-in-progress list with delivery of SIGCHLD blocked
	   to avoid a race wrt. the 'gMountsInProgress' list: */
	sigemptyset(&block_set);
	sigaddset(&block_set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &block_set, &curr_set);
	LIST_INSERT_HEAD(&gMountsInProgress, pr, mpr_link);
	sigprocmask(SIG_SETMASK, &curr_set, NULL);
}

- (BOOL)mountInProgressForVnode:(Vnode *)v forUID:(uid_t)uid
{
	sigset_t curr_set;
	sigset_t block_set;
	struct MountProgressRecord *pr;
	BOOL result = NO;
	
	/* Check the global mounts-in-progress list with delivery of SIGCHLD blocked
	   to avoid a race wrt. the 'gMountsInProgress' list: */
	sigemptyset(&block_set);
	sigaddset(&block_set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &block_set, &curr_set);
	LIST_FOREACH(pr, &gMountsInProgress, mpr_link) {
		if ((pr->mpr_vp == v) && (pr->mpr_uid == uid)) {
			result = YES;
			break;
		};
	};
	sigprocmask(SIG_SETMASK, &curr_set, NULL);
	
	return result;
}

- (void)recordMountInProgressFor:(Vnode *)v uid:(uid_t)uid mountPID:(pid_t)mountPID transactionID:(u_long)transactionID
{
	struct MountProgressRecord *pr;
	
	pr = malloc(sizeof(*pr));
	if (pr == NULL) {
		sys_msg(debug, LOG_ERR, "Couldn't allocate mount progress record?!");
		return;
	};
	
	pr->mpr_mountpid = mountPID;
	pr->mpr_vp = [v retain];
	pr->mpr_uid = uid;
	pr->mpr_xid = transactionID;
	
	AddMountsInProgressListEntry(pr);
}

- (BOOL)checkMountInProgressForTransaction:(u_long)transactionID
{
	sigset_t curr_set;
	sigset_t block_set;
	struct MountProgressRecord *pr;
	BOOL answer = NO;
	
	/* Update the global mounts-in-progress list with delivery of SIGCHLD blocked
	   to avoid a race wrt. the 'gMountsInProgress' list: */
	sigemptyset(&block_set);
	sigaddset(&block_set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &block_set, &curr_set);
	LIST_FOREACH(pr, &gMountsInProgress, mpr_link) {
		if (pr->mpr_xid == transactionID) {
			answer = YES;
			break;
		};
	};
	sigprocmask(SIG_SETMASK, &curr_set, NULL);
	
	return answer;
}

static void completeMount(Vnode *v, unsigned int status)
{
	Vnode *serverNode;
	
	sys_msg(debug_mount, LOG_DEBUG, "completeMount: v = 0x%08lx, status = 0x%08lx", v, status);
	
	if (v == NULL)
	{
		sys_msg(debug, LOG_ERR, "NULL vnode pointer in completeMount?!");
	}
	else if (status != 0)
	{
		sys_msg(debug, LOG_ERR, "Can't mount %s:%s on %s: %s (%d)",
										[[[v server] name] value],
										[[v source] value],
										[[v link] value],
										strerror(status), status);
		if (v && [v source]) [v setNfsStatus:status];
	} else {
		sys_msg(debug_mount, LOG_DEBUG, "Mounted %s:%s on %s", [[[v server] name] value],
											[[v source] value],
											[[v link] value]);
	}

	/* Update the 'mounted' status of the entire subtree
	   [ some of which may have been mounted by sub-processes ]
	   unless this is being completed in a sub-mount process: */
	if (v && !(gForkedMount || gSubMounter)) {
		serverNode = v;
		if (doServerMounts) {
		  while ([serverNode serverDepth] > 0) serverNode = [serverNode parent];
		};
		[serverNode updateMountStatus];
#ifndef __APPLE__
		[self mtabUpdate:v];
#endif
	}
}

- (void)completeMountInProgressBy:(pid_t)mountPID exitStatus:(int)exitStatus
{
	struct MountProgressRecord *pr;
	
	sys_msg(debug_mount, LOG_DEBUG, "completeMountInProgressBy:%d, status 0x%08lx", mountPID, exitStatus);

	LIST_FOREACH(pr, &gMountsInProgress, mpr_link) {
		if ((pr->mpr_mountpid == mountPID) || (pr->mpr_mountpid == 0)) {
			sys_msg(debug_mount, LOG_DEBUG, "completeMountInProgressBy:%d, transaction id = 0x%08x", mountPID, pr->mpr_xid);
			completeMount(pr->mpr_vp, (WIFEXITED(exitStatus)) ? (unsigned int)WEXITSTATUS(exitStatus) : EFAULT);
			[pr->mpr_vp decrementMountInProgressCount];
			[pr->mpr_vp release];
			LIST_REMOVE(pr, mpr_link);
			free(pr);
			break;
		};
	};
}

- (int)dispatch_autofsreq:(struct autofs_userreq *)req forFSID:(struct fsid *)target_fsid
{
	int i;
	fsid_t *fs_fsid;

	/* Dispatch the mount request to the specific map: */
	for (i = 0; i < map_table_count; i++) {
		fs_fsid = [map_table[i].map mountedFSID];
		if ((fs_fsid->val[0] == target_fsid->val[0]) &&
			(fs_fsid->val[1] == target_fsid->val[1])) {
			return [map_table[i].map handle_autofsreq:req];
		};
	};
	
	return -1;
}

- (Server *)serverWithName:(String *)name
{
	int i;
	Server *s;

	for (i = 0; i < server_table_count; i++)
	{
		if ([name equal:server_table[i].name])
			return server_table[i].server;
	}

	s = [[Server alloc] initWithName:name];
	if (s == nil)
	{
		sys_msg(debug, LOG_ERR, "Unknown server: %s", [name value]);
		return nil;
	}

	if (server_table_count == 0)
		server_table = (server_table_entry *)malloc(sizeof(server_table_entry));
	else
		server_table = (server_table_entry *)realloc(server_table,
			(server_table_count + 1) * sizeof(server_table_entry));

	server_table[server_table_count].name = [name retain];
	server_table[server_table_count].server = s;

	server_table_count++;

	return s;
}

- (void)timeout
{
	int i;
	
	/* Tell maps to try to unmount */
	for (i = 0; i < map_table_count; i++) [map_table[i].map timeout];
}

- (void)showNode:(Vnode *)v
{
	char msg[2048];

	if (v == nil) return;

	msg[0] = '\0';

	sprintf(msg, "%4d %1d %s %s ", [v attributes].fileid, [v type],
		[v mounted] ? "M" : " ",
		[v fakeMount] ? "F" : " ");
	
	strcat(msg, "name=");
	if ([v name] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v name] value]);

	strcat(msg, " vfsType=");
	if ([v vfsType] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v vfsType] value]);

	strcat(msg, " path=");
	if ([v path] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v path] value]);
	
	strcat(msg, " link=");
	if ([v link] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v link] value]);
	
	strcat(msg, " url=");
	if ([v debugURLString] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v debugURLString] value]);
	
	strcat(msg, " source=");
	if ([v source] == nil) strcat(msg, "-nil-");
	else strcat(msg, [[v source] value]);

	strcat(msg, " server=");
	if ([v server] == nil) strcat(msg, "-nil-");
	else if ([[v server] isLocalHost]) strcat(msg, "-local-");
	else strcat(msg, [[[v server] name] value]);

	strcat(msg, " parent=");
	if ([v parent] == nil) strcat(msg, "-nil-");
	else {
                char parentStr[20];
		sprintf(parentStr, "%d", [[v parent] attributes].fileid);
		strcat(msg, parentStr);
	}
	
	sys_msg(debug, LOG_DEBUG, "%s", msg);
	usleep(1000);
}

- (void)unmountAutomounts:(int)use_force
{
	int i;
	int status;
	Vnode *v;

	sys_msg(debug, LOG_DEBUG, "Unmounting automounts");
	
	invalidate_fsstat_array();

	chdir("/");

    if (UNMOUNTALL_USING_NODETABLE && use_force)
    {
        /* unmount normal NFS mounts */
        for (i = node_table_count - 1; i >= 0; i--)
        {
            v = node_table[i].node;
    
            if ([v fakeMount]) continue;
            if ([v server] == nil) continue;
            if ([v source] == nil) [v setMounted:NO];
            if ([[v server] isLocalHost]) [v setMounted:NO];
    
            if (![v mounted]) continue;
    
#ifdef __APPLE__
            if (use_force)
            {
                sys_msg(debug, LOG_WARNING, "Force-unmounting %s", [[v link] value]);
                status = unmount([[v link] value], MNT_FORCE);
            }
            else
            {
                sys_msg(debug, LOG_WARNING, "Unmounting %s", [[v link] value]);
                status = unmount([[v link] value], 0);
            }
#else
            status = unmount([[v link] value]);
#endif
    
            if (status == 0)	
            {
                [v setMounted:NO];
#ifndef __APPLE__
                [self mtabUpdate:v];
#endif
                sys_msg(debug, LOG_DEBUG, "Unmounted %s", [[v link] value]);
            }
            else
            {
                sys_msg(debug, LOG_DEBUG, "Unmount failed for %s: %s",
                    [[v link] value], strerror(errno));
            }
        }
    }
    else
    {
        /* Tell individual maps to try to unmount all nodes */
        for (i = 0; i < map_table_count; i++) {
            [map_table[i].map unmountAutomounts:use_force];
        };
    }
}

- (void)unmountMaps:(int)use_force
{
	int i, status;
	Vnode *v;

	sys_msg(debug, LOG_DEBUG, "Unmounting maps");
	
	invalidate_fsstat_array();
	
	/* unmount automounter */
	for (i = node_table_count - 1; i >= 0; i--)
	{
		v = node_table[i].node;

		if ([v fakeMount]) continue;
		if ([v server] != nil) continue;
		if (![v mounted]) continue;
		if ([v link] == nil) continue;

#ifdef __APPLE__
		if ([[v map] mountStyle] == kMountStyleAutoFS) {
			status = [self attemptUnmount:v usingForce:use_force];
		} else {
			if (use_force)
			{
				sys_msg(debug, LOG_WARNING, "Force-unmounting %s", [[v link] value]);
				if ((status = sysctl_unmount([[v map] mountedFSID], MNT_FORCE)) != 0) {
					status = unmount([[v link] value], MNT_FORCE);
				};
			}
			else
			{
				sys_msg(debug, LOG_DEBUG, "Unmounting %s", [[v link] value]);
				if ((status = sysctl_unmount([[v map] mountedFSID], 0)) != 0) {
					status = unmount([[v link] value], 0);
				};
			};
		};
#else
		status = unmount([[v link] value]);
#endif
		if (status == 0)	
		{
			[v setMounted:NO];
#ifndef __APPLE__
			[self mtabUpdate:v];
#endif
          [[v map] deregisterAMInfoService];
            
			sys_msg(debug, LOG_DEBUG, "Unmounted %s", [[v link] value]);
		}
		else
		{
			sys_msg(debug, LOG_DEBUG, "Unmount failed for %s: %s",
				[[v link] value], strerror(errno));
		}
	}
}

/*
 * Validate updates the Vnode hierarchy and Servers to match new network
 * settings (including NetInfo or Directory Services changes).
 *
 * This is implemented as a combination of "mark and sweep" cleanup
 * (like in a garbage collector) and first-time initialization.  It starts
 * with a "mark" phase where all Vnodes are marked.  Then the maps are
 * asked to re-initialize themselves.  As part of this process, existing
 * Vnodes that would have been created during initialization are unmarked.
 * Newly created Vnodes are created unmarked.  After the re-initialization,
 * all marked Vnodes are removed (and unmounted if mounted), and empty
 * directories are removed.
 */
- (void)validate
{
	int i,j;
	Vnode *v;
	char *mountpoint;
	BOOL needNotify = NO;
	unsigned int stored_node_id = node_id;
       	
	/* Mark all Vnodes */
	sys_msg(debug, LOG_DEBUG, "validate: Marking Vnodes");
	for (i=0; i<node_table_count; ++i)
	{
		[node_table[i].node setMarked:YES];
	}
	
	/* (Re-)Initialize maps, unmarking nodes that still exist */
	sys_msg(debug, LOG_DEBUG, "validate: Re-initializing maps");
	for (i=0; i<map_table_count; ++i)
	{
		[map_table[i].map reInit];
	}

	/*
	 * Unmount and release marked Vnodes
	 *
	 * We traverse the node table in reverse order so that we will visit
	 * children before their parents.  That way, if a server cannot be
	 * unmounted, we can avoid removing all ancestors.  The server,
	 * and any otherwise unreferenced ancestors, remain marked so that
	 * we can remove them when the server is eventually unmounted (once
	 * we add code to detect unmounts).
	 */
	sys_msg(debug, LOG_DEBUG, "validate: Releasing marked nodes");
	for (i=node_table_count-1; i>=0; --i)
	{
		v = node_table[i].node;
		
		if ([v marked] && ![v hasChildren])
		{
			/* Find out where the node would be mounted */
			mountpoint = [[v link] value];
			
#if 0
			if ([v source] != nil && mountpoint != nil && [v mounted] && ![[v server] isLocalHost])
			{
				/*
				   This server is currently mounted.  It may not be safe to unmount it now
				   (a disconnected NFS mount will hang forever in unmount()), but leave the
				   mount undisturbed.
				 */
				sys_msg(debug, LOG_INFO, "validate: abandoning mountpoint %s.", mountpoint);
					
				/* Pretend this node was unmarked to begin with to leave its accessor path */
				continue;
			}
#endif
			/*
			 * Release "v".  Note that we don't call -[Controller destroyVnode:] here.  The node
			 * table will be compacted after all Vnodes have been invalidated.
			 */
			sys_msg(debug, LOG_DEBUG, "validate: releasing %s", mountpoint);
			[self removeVnode:v];
			
			/* We need to notify the Finder so it can update its views */
			needNotify = YES;
		}
	}
	
	/* Compact the node table ("squeeze out" released nodes) */
	[self compactVnodeTableFrom:0];
	
	/* Release any unreferenced Servers */
	for (i=0,j=0; i<server_table_count; ++i)
	{
		if ([server_table[i].server retainCount] == 1)
		{
			/* This server no longer in use, so release it */
			sys_msg(debug, LOG_DEBUG, "validate: releasing server %s",
				[server_table[i].name value], (unsigned long) server_table[i].server);
			[server_table[i].name release];
			[server_table[i].server release];
		}
		else
		{
			/* This server is still in use, so shift it down */
			if (i != j)
				server_table[j] = server_table[i];
			++j;
		}
	}
	sys_msg(debug, LOG_DEBUG, "validate: %d servers", j);
	server_table = realloc(server_table, j*sizeof(server_table_entry));
	server_table_count = j;

	/* Cause Finder to update its views. */
	if (needNotify || node_id > stored_node_id)
	{
		sys_msg(debug, LOG_DEBUG, "validate: FNNotifyAll");
		FNNotifyAll(kFNDirectoryModifiedMessage, kNilOptions);
	}
	else
	{
		sys_msg(debug, LOG_DEBUG, "validate: no FNNotifyAll necessary");
	}
	
	sys_msg(debug, LOG_DEBUG, "validate: done");
}

- (void)reInit
{
	int i, j, current_map_count;
	map_table_entry *current_maps;
	systhread *rpcLoop;
	Vnode *root;

	[self unmountAutomounts:0];
	[self unmountMaps:0];

	current_maps = calloc(map_table_count, sizeof(map_table_entry));
	if (current_maps == NULL) return;
	
	for (i = 0, j = 0; i < map_table_count; i++)
	{
		if ([[map_table[i].map root] parent] == nil)
		{
			/* This is the root map, always created first */
			continue;
		} else {
			current_maps[j] = map_table[i];
			current_maps[j].map = nil;			/* Not actually saved */
			++j;
		};
		[map_table[i].map release];
	};
	[rootMap release];
	
	current_map_count = j;
	map_table_count = 0;
	free(map_table);
	map_table = NULL;
	
	if (node_table_count > 0) {
		for (i = 0; i < node_table_count; i++)
		{
			if (node_table[i].node) [node_table[i].node release];
		}
		node_table_count = 0;
		free(node_table);
		node_table = NULL;
	};
	
	if (server_table_count > 0)
	{
		for (i = 0; i < server_table_count; i++)
		{
			if (server_table[i].name) [server_table[i].name release];
			if (server_table[i].server) [server_table[i].server release];
		}
		server_table_count = 0;
		free(server_table);
		server_table = NULL;
	};
	
	run_select_loop = 1;
	rpcLoop = systhread_new();
	systhread_run(rpcLoop, select_loop, NULL);
	systhread_yield();

	rootMap = [[Map alloc] initWithParent:nil directory:mountDirectory];
	root = [rootMap root];

	for (i = 0; i < current_map_count; i++)
	{
		sys_msg(debug, LOG_DEBUG, "Reinitializing map \"%s\" (on \"%s\", in \"%s\")",
							[current_maps[i].name value],
							[current_maps[i].dir value],
							[current_maps[i].mountdir value]);
		
		[controller mountmap:current_maps[i].name directory:current_maps[i].dir mountdirectory:current_maps[i].mountdir];
	}

	free(current_maps);
	
	run_select_loop = 0;
	while (running_select_loop)
	{
		systhread_yield();
	}

	sys_msg(debug, LOG_DEBUG, "Reset complete");
}

- (void)checkForUnmounts
{
	BOOL foundUnmount = NO;
	int i;
	
	revalidate_fsstat_array(NULL);
	
	for (i=0; i<map_table_count; ++i)
	{
		sys_msg(debug, LOG_DEBUG, "Checking map %s for unmounts", [map_table[i].name value]);
		foundUnmount = foundUnmount || [[map_table[i].map root] checkForUnmount];
	}
	
	if (foundUnmount)
	{
		sys_msg(debug, LOG_DEBUG, "Found an unmount.  Notifying.");
		FNNotifyAll(kFNDirectoryModifiedMessage, kNilOptions);
	}
}

- (void)dealloc
{
	int i;

	[self unmountAutomounts:MNT_FORCE];
	[self unmountMaps:MNT_FORCE];

	/* Give maps a chance to clean up before nodes or servers are released. */
	for (i = 0; i < map_table_count; i++)
	{
		[map_table[i].map cleanup];

		[map_table[i].name release];
		[map_table[i].dir release];
		[map_table[i].mountdir release];
		[map_table[i].map release];
	}
	map_table_count = 0;
	free(map_table);
	map_table = NULL;

	for (i = 0; i < node_table_count; i++)
	{
		[node_table[i].node release];
	}
	node_table_count = 0;
	free(node_table);
	node_table = NULL;

	for (i = 0; i < server_table_count; i++)
	{
		[server_table[i].name release];
		[server_table[i].server release];
	}

	server_table_count = 0;
	free(server_table);
	server_table = NULL;

	if (hostName != nil) [hostName release];
	if (hostDNSDomain != nil) [hostDNSDomain release];
	if (hostArchitecture != nil) [hostArchitecture release];
	if (hostByteOrder != nil) [hostByteOrder release];
	if (hostOS != nil) [hostOS release];
	if (hostOSVersion != nil) [hostOSVersion release];

	[super dealloc];
}

- (int)attemptUnmount:(Vnode *)v usingForce:(int)use_force
{
	int status;

	if (v == nil) return EINVAL;

	if ([[v map] mountStyle] == kMountStyleParallel) {
		if (![v mounted]) return 0;
		if ([v type] != NFLNK) return EINVAL;

		if ([v source] == nil)
		{
			[v setMounted:NO];
			return 0;
		}
	};
	
	if ([[v map] mountStyle] == kMountStyleAutoFS) {
		int fs_count, i;
		struct statfs *fsinfo;
		uid_t effuid;

		sys_msg(debug_mount, LOG_DEBUG, "Attempting to %s unmount all instances of %s",
					(use_force ? "forcibly" : ""),
					[[v path] value]);

		fs_count = revalidate_fsstat_array(&fsinfo);
		if (fs_count <= 0) {
			sys_msg(debug, LOG_DEBUG, "attemptUnmount: get_fsstat_array failed returned fs count of %d: %s", fs_count, strerror(errno));
			return NO;
		};
		
		effuid = geteuid();
		for (i=0; i < fs_count; ++i)
		{
			if (strcmp(fsinfo[i].f_mntonname, [[v path] value]) == 0) {
				seteuid(fsinfo[i].f_owner);
#ifdef __APPLE__
				status = unmount(fsinfo[i].f_mntonname, use_force);
#else
				status = unmount(fsinfo[i].f_mntonname);
#endif
				if (status == 0)
				{
					sys_msg(debug, LOG_DEBUG, "Unmounted %s (uid = %d)", [[v path] value], fsinfo[i].f_owner);
#ifndef __APPLE__
					[self mtabUpdate:v];
#endif
				}
			};
		};
		seteuid(effuid);
		invalidate_fsstat_array();
		return 0;
	} else {
		sys_msg(debug_mount, LOG_DEBUG, "Attempting to %s unmount %s", (use_force ? "forcibly" : ""), [[v link] value]);
	
#ifdef __APPLE__
		status = unmount([[v link] value], use_force);
#else
		status = unmount([[v link] value]);
#endif
		if (status == 0)
		{
			sys_msg(debug, LOG_DEBUG, "Unmounted %s", [[v link] value]);
			[v setMounted:NO];
#ifndef __APPLE__
			[self mtabUpdate:v];
#endif
			invalidate_fsstat_array();
			return 0;
		}
	}

	sys_msg(debug_mount, LOG_DEBUG, "Unmount %s failed: %s",
		[[v link] value], strerror(errno));

	[v resetMountTime];
	return 1;
}

- (void)printTree
{
	int i;

	sys_msg(debug, LOG_DEBUG, "********** Map Table **********");
	for (i = 0; i < map_table_count; i++)
	{
		sys_msg(debug, LOG_DEBUG, "Map %s   Directory %s (%s+%s)   MountDir %s",
			[map_table[i].name value], [map_table[i].dir value],
			[[[[map_table[i].map root] parent] path] value],
			[[[map_table[i].map root] name] value],
			[map_table[i].mountdir value]);

		[self printNode:[map_table[i].map root] level:0];
	}

	sys_msg(debug, LOG_DEBUG, "********** Node Table **********");
	for (i = 0; i < node_table_count; i++)
	{
		[self showNode:node_table[i].node];
	}

	sys_msg(debug, LOG_DEBUG, "********** Server Table **********");
	for (i = 0; i < server_table_count; i++)
	{
		sys_msg(debug, LOG_DEBUG, "%d: %s", i, [server_table[i].name value]);
		usleep(1000);
	}
}

- (void)printNode:(Vnode *)v level:(unsigned int)l
{
	unsigned int i, len;
	Array *kids;
	char msg[1024];
	Server *s;

	if (v == nil) return;

	msg[0] = '\0';
	strcat(msg, "  ");

	len = l * 4;
	for (i = 0; i < len; i++) strcat(msg, " ");

	strcat(msg, [[v name] value]);
	if ([v type] == NFLNK)
	{
		if ([v mounted]) strcat(msg, " <-- ");
		else strcat(msg, " ... ");

		s = [v server];
		if (s == nil)	strcat(msg, "-unknown_server-");
		else strcat(msg, [[[v server] name] value]);
		if ([v source] != nil)
		{
			strcat(msg, ":");
			strcat(msg, [[v source] value]);
		}
	}

	sys_msg(debug, LOG_DEBUG, "%s", msg);
	usleep(1000);

	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];

	for (i = 0; i < len; i++)
	{
		[self printNode:[kids objectAtIndex:i] level:l+1];
	}
}

- (String *)mountDirectory
{
	return mountDirectory;
}

- (String *)hostName
{
	return hostName;
}

- (String *)hostDNSDomain
{
	return hostDNSDomain;
}

- (String *)hostArchitecture
{
	return hostArchitecture;
}

- (String *)hostByteOrder
{
	return hostByteOrder;
}

- (String *)hostOS
{
	return hostOS;
}

- (String *)hostOSVersion
{
	return hostOSVersion;
}

- (int)hostOSVersionMajor
{
	return hostOSVersionMajor;
}

- (int)hostOSVersionMinor
{
	return hostOSVersionMinor;
}

#ifndef __APPLE__
- (void)mtabUpdate:(Vnode *)v
{
	FILE *f, *g;
	char line[1024], target[1024];
	unsigned int vid, pid, len;

	vid = [v nodeID];
	pid = getpid();

	if ([v server] == nil)
	{
		sprintf(target, "<automount>:%s \"%s\" nfs auto %u %u",
			[[[v map] name] value], [[v link] value], vid, pid);
	}
	else
	{
		sprintf(target, "%s:%s \"%s\" nfs auto %u %u",
			[[[v server] name] value], [[v source] value],
			[[v link] value], vid, pid);
	}

	if ([v mounted])
	{
		f = fopen("/etc/mtab", "a");
		if (f == NULL)
		{
			sys_msg(debug, LOG_ERR, "Can't write /etc/mtab: %s",
				strerror(errno));
			return;
		}

		fprintf(f, "%s\n", target);
		fclose(f);
		return;
	}

	f = fopen("/etc/mtab", "r");
	if (f == NULL)
	{
		sys_msg(debug, LOG_ERR, "Can't read /etc/mtab: %s", strerror(errno));
		return;
	}

	g = fopen("/etc/auto_mtab", "w");
	if (f == NULL)
	{
		sys_msg(debug, LOG_ERR, "Can't create /etc/auto_mtab: %s",
			strerror(errno));
		return;
	}

	len = strlen(target);

	while (fgets(line, 1024, f))
	{
		if (strncmp(line, target, len)) fprintf(g, "%s", line);
	}

	fclose(f);
	fclose(g);
	rename("/etc/auto_mtab", "/etc/mtab");
}
#endif

static Boolean VnodeKeyEqual(const void *value1, const void *value2) {
	return ( (((VNodeHashKey *)value1)->nodeid == ((VNodeHashKey *)value2)->nodeid) &&
			 (((VNodeHashKey *)value1)->fsid.val[0] == ((VNodeHashKey *)value2)->fsid.val[0]) &&
			 (((VNodeHashKey *)value1)->fsid.val[1] == ((VNodeHashKey *)value2)->fsid.val[1]) );
}

static CFHashCode VnodeKeyHash(const void *value) {
	return (CFHashCode)(((VNodeHashKey *)value)->nodeid ^ ((VNodeHashKey *)value)->fsid.val[0]);
}

static Boolean VnodeEqual(const void *value1, const void *value2) {
	return (value1 == value2);
}

@end
