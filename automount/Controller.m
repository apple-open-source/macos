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

#define kUseExistingMount         0x00000001
#define kNoNewMount               0x00000002
#define kMountAtMountdir          0x00000004
#define kCreateNewSession         0x00000008
#define kMarkAutomounted		  0x00000010
#define kMountWithoutNotification 0x00000020

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

#define SHUNTAFPMOUNTS 0
#define AFPSCHEMEPREFIX "afp:/"
#define AFPGUESTUAM "AUTH=No%20User%20Authent"

#if SHUNTAFPMOUNTS
#import <AppleShareClient/afpHLMount.h>
#endif

#define HOSTINFO "/usr/bin/hostinfo"
#define OS_NEXTSTEP 0
#define OS_OPENSTEP 1
#define OS_MACOSX 2
#define OS_DARWIN 3

static char gConsoleDevicePath[] = "/dev/console";

extern void nfs_program_2();

extern void select_loop(void *);
extern int run_select_loop;
extern int running_select_loop;

extern int protocol_1;  
extern int protocol_2;

#warning relying on internally derived copy of private and transport-specific xid...
extern u_long rpc_xid;

extern NSLMap *GlobalTargetNSLMap;

void completeMount(Vnode *v, unsigned int status);

static gid_t
gidForGroup(char *name)
{
	struct group *g;

	g = getgrnam(name);
	if (g != NULL) return g->gr_gid;

	sys_msg(debug, LOG_WARNING, "Can't get gid for group %s", name);
	return 0;
}

int
sysctl_fsid(op, fsid, oldp, oldlenp, newp, newlen)
	int op;
	fsid_t *fsid;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	int ctlname[CTL_MAXNAME+2];
	size_t ctllen;
	const char *sysstr = "vfs.generic.ctlbyfsid";
	struct vfsidctl vc;

	ctllen = CTL_MAXNAME+2;
	if (sysctlnametomib(sysstr, ctlname, &ctllen) == -1) return -1;
	ctlname[ctllen] = op;

	bzero(&vc, sizeof(vc));
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = *fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return (sysctl(ctlname, ctllen + 1, oldp, oldlenp, &vc, sizeof(vc)));
}

int
sysctl_unmount(fsid_t *fsid, int flag)
{

	return (sysctl_fsid(VFS_CTL_UMOUNT, fsid, NULL, 0, &flag,
		sizeof(flag)));
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
	if (bindresvport(sock, &addr)) {
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

- (BOOL)createPath:(String *)path
{
	return [self createPath:path withUid:0];
}

- (BOOL)createPath:(String *)path withUid:(int)uid
{
	int i, p;
	char *s, t[1024];
	int status;

	if (path == nil) return YES;
	if ([path length] == 0) return YES;

	p = 0;
	s = [path value];

	chdir("/");

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

		sys_msg(debug, LOG_DEBUG, "Creating intermediate directory %s...", t);
		
		status = mkdir(t, 0755);
		if (status == -1)
		{
			 if (errno == EEXIST) status = 0;
			 if (errno == EISDIR) status = 0;
		}

		if (status != 0) return NO;

		chdir(t);
		s = [path scan:'/' pos:&p];
	}

	chdir("/");
	return YES;
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
			[v resetTime];
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
		sys_msg(debug, LOG_ERR, "Unregistered Vnode %u (%s)", [v nodeID], [[v path] value]);
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
	int i;
	unsigned int count;
	Array *kids;

	kids = [v children];
	if (kids == nil) count = 0;
	else count = [kids count];

	for (i = count - 1; i >= 0; i--)
		[self removeVnode:[kids objectAtIndex:i]];

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

- (unsigned int)automount:(Vnode *)v directory:(String *)dir args:(int)mntargs
{
	struct nfs_args args;
	struct sockaddr_in sin;
	struct file_handle fh;
	char str[MAXPATHLEN + 64];
	String *src;
	int status;

	[self createPath:dir];

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
	args.flags = NFSMNT_INT | NFSMNT_TIMEO | NFSMNT_RETRANS | NFSMNT_NOLOCKS;
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

- (unsigned int)autoMap:(Map *)map name:(String *)name directory:(String *)dir mountdirectory:(String *)mnt
{
	Vnode *maproot;
	unsigned int status;
	struct statfs sfs;

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

	status = [self automount:maproot directory:dir args:[map mountArgs]];
	if (status != 0) return status;

	/* Look up and save off the fsid of the newly mounted map: */
	status = statfs([dir value], &sfs);
	if (status != 0) return status;
	[map setFSID:&sfs.f_fsid];

	map_table_count++;
	
	status = [map didAutoMount];
	return status;
}

- (unsigned int)mountmap:(String *)mapname directory:(String *)dir mountdirectory:(String *)mnt
{
	Vnode *root, *p;
	Map *map;
	char *s, *t;
	String *parent, *mountpt;
	id mapclass;
    unsigned int mountstatus;

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
    
    return [map registerAMInfoService];
}

- (Map *)rootMap
{
	return rootMap;
}

BOOL URLFieldSeparator(char c)
{
	switch (c) {
		case '@':
		case '/':
			return YES;
		
		default:
			return NO;
	};
}

BOOL URLIsComplete(const char *url)
{
	const char *urlcontent;
	const char *auth_field = NULL;
	const char *p;
		
	if (strncasecmp(url, AFPSCHEMEPREFIX, sizeof(AFPSCHEMEPREFIX)-1) != 0) return NO;
	
	urlcontent = strchr(url + sizeof(AFPSCHEMEPREFIX) - 1, '/');
	if (urlcontent == NULL) return NO;
	
	for (p = urlcontent + 1; *p; ++p) {
		if (*p == ';') auth_field = p + 1;
		if (URLFieldSeparator(*p)) break;
	};
	
	if (auth_field == NULL) return NO;
	
	return strncasecmp(auth_field, AFPGUESTUAM, sizeof(AFPGUESTUAM) - 1) ? NO : YES;	
}

- (unsigned int)nfsmount:(Vnode *)v withUid:(int)uid
{
	struct sockaddr_in sin;
	struct nfs_args args;
	char str[1024];
	struct file_handle fh;
	Server *s;
	unsigned int status;
	unsigned int vers, proto;
	unsigned short port;
	String *urlMountType;
    unsigned long urlMountFlags = kMarkAutomounted | kUseUIProxy;
	char *url;
	char mountDir[PATH_MAX];
	sigset_t curr_set;
	sigset_t blocked_set;
	pid_t mountPID;
	char retString[1024];
	uid_t effuid;
	gid_t storedgid, storedegid;

	urlMountType = [String uniqueString:"url"];

#ifndef __APPLE__
	unsigned int fhsize;
#endif

	if ([v mounted])
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
		String *nslEntrySource = [String uniqueString:"*"];
		if ([v mntArgs] & MNT_DONTBROWSE) urlMountFlags |= kMarkDontBrowse;
		if ([[v source] equal:nslEntrySource]) {
			sys_msg(debug, LOG_ERR, "No source specified for %s (URL='%s')\n", [[v link] value], [[v urlString] value]);
			[v setNfsStatus:NFSERR_NXIO];
			return 1;
		} else {
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
		if (![self createPath:[v link] withUid:uid])
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

	sys_msg(debug, LOG_DEBUG, "Mounting %s on %s",
		str, [[v link] value]);

#ifdef __APPLE__
	vers = [v forcedNFSVersion];
	proto = [v forcedProtocol];

	if ([[v vfsType] equal:urlMountType])
	{ 
#ifdef Darwin
		/* Darwin doesn't have AFP support.  */
		status = 1;  // fail
#else /* Darwin */
#if 1
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
		
		if ([v mountInProgress]) {
			/* Don't bother forking for another mount request when one is already in progress;
			   delaying this response could result in a deadlock if it's coming (even indirectly)
			   from the UI mount proxy of the mount in progress
			   
			   Even though it opens up a potential race condition, skip the actual work of mounting now: */
			sys_msg(debug, LOG_DEBUG, "Blocked on mount transaction id 0x%08lx", [v transactionID]);
			gBlockedMountDependency = YES;
			gBlockingMountTransactionID = [v transactionID];
		} else {
			[v setMountInProgress:YES];
			[v setTransactionID:rpc_xid];
			mountPID = fork();
			if (mountPID == -1) {
				status = (errno != 0) ? errno : -1;
			} else {
				status = 0;
				if (mountPID) {
					/* We are the parent process; abandon this call and let child process generate reply */
					gForkedMountInProgress = YES;
					
					/* The child process will eventually signal (SIGCHLD) when the mount is complete; mark the vnode as
					   'mount in progress' to prevent starting more than one mount while this attempt is in progress. */
					(void)[self recordMountInProgressFor:v mountPID:mountPID];
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
			setreuid(getuid(), uid);
			setgid(gidForGroup("unknown"));  // unknown
			setegid(gidForGroup("unknown"));  // unknown
	
#if SHUNTAFPMOUNTS
			if (strncasecmp(url, AFPSCHEMEPREFIX, sizeof(AFPSCHEMEPREFIX)-1) == 0)
			{
				/* Shunt AFP URLs directly to the [low-level] AFP client: */
				if (!afpLoaded)
				{
					system("/System/Library/Filesystems/AppleShare/afpLoad");
					afpLoaded++;
				}

				sys_msg(debug_mount, LOG_DEBUG,
					"Attempting to automount AFP url: \n\tURL = %s, \n\tserver = %s, \n\tmountdir = %s\n\toptions = 0x%08lx\n\tuid = %d",
					url, [[[v server] name] value], mountDir, urlMountFlags, uid);
	
				status = afp_LLMount(url, mountDir, sizeof(retString), retString, urlMountFlags);
			}
			else
#else /* SHUNTAFPMOUNTS */
        if (1)
#endif /* SHUNTAFPMOUNTS */
			{
				struct stat sb;
	
				retString[0] = (char)0;
				
				/* Look at the system console to figure out the uid of the logged-in user, if any: */
				status = stat(gConsoleDevicePath, &sb);
				if (URLIsComplete(url) || (status != 0) || (sb.st_uid != uid)) {
					/* As a user different than the logged-in user, the UI proxy won't even TRY to mount a volume;
					if the URL is complete, though, this will successfully mount it without UI
					*/
					sys_msg(debug_mount, LOG_DEBUG,
						"Attempting to quietly automount url:\n\tURL = %s,\n\tserver = %s,\n\tmountdir = %s\n\toptions = 0x%08lx\n\tuid = %d",
						url, [[[v server] name] value], mountDir, urlMountFlags & ~kUseUIProxy, uid);
					status = MountCompleteURL(url, mountDir, sizeof(retString), retString, urlMountFlags & ~kUseUIProxy);
				} else {
					sys_msg(debug_mount, LOG_DEBUG,
						"Attempting to automount url: \n\tURL = %s,\n\tserver = %s,\n\tmountdir = %s\n\toptions = 0x%08lx\n\tuid = %d",
						url, [[[v server] name] value], mountDir, urlMountFlags, uid);
					status = MountServerURL(url, mountDir, sizeof(retString), retString, urlMountFlags);
				};
				if ((status == 0) && (urlMountFlags & kMountAll)) {
					strncpy(retString, mountDir, sizeof(retString));
					retString[sizeof(retString)-1] = (char)0;			/* Make sure it's terminated */
				};
			}
		
			setreuid(getuid(), effuid);
			setgid(storedgid);
			setegid(storedegid);
	
			if (status)
			{
				sys_msg(debug_mount, LOG_DEBUG, "Recieved status = %d from forked MountServerURL", status);
			}
URLMount_Failure: ;
		};
#else /* 1 */
		/* afp or http */
		effuid = geteuid();
		storedgid = getgid();
		storedegid = getegid();
#endif /* 1 */
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
		}
	}
#else /* __APPLE__ */
	if ([[v vfsType] equal:urlMountType])
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
	}
#endif /* __APPLE__ */
	
	[urlMountType release];

	if (gForkedMount) gMountResult = status;

	if (!gForkedMountInProgress && !gBlockedMountDependency) {
		completeMount(v, status);
		return status;
	};

	return 0;
}

void AddMountsInProgressListEntry(struct MountProgressRecord *pr)
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

- (void)recordMountInProgressFor:(Vnode *)v mountPID:(pid_t)mountPID
{
	struct MountProgressRecord *pr = [v mountInfo];
	
	pr->mpr_mountpid = mountPID;
	pr->mpr_vp = [v retain];
	
	AddMountsInProgressListEntry(pr);
}

void completeMount(Vnode *v, unsigned int status)
{
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
	
		/* Tell the node that it was mounted */
		if (v) {
			[v setMounted:YES];
			[v markDirectoryChanged];
			if ([v parent]) {
				[[v parent] markDirectoryChanged];
			};
		};
	
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
		sys_msg(debug_mount, LOG_DEBUG, "completeMountInProgressBy:%d, status 0x%08lx pr = 0x%08lx", pr);
		if ((pr->mpr_mountpid == mountPID) || (pr->mpr_mountpid == 0)) {
			completeMount(pr->mpr_vp, (unsigned int)WEXITSTATUS(exitStatus));
			[pr->mpr_vp setMountInProgress:NO];
			[pr->mpr_vp release];
			LIST_REMOVE(pr, mpr_link);
			break;
		};
	};
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

	chdir("/");

    if (use_force)
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
        /* Tell maps to try to unmount all nodes */
        for (i = 0; i < map_table_count; i++) {
            [map_table[i].map unmount:[map_table[i].map root] withRemountOnFailure:NO];
        };
    }
}

- (void)unmountMaps:(int)use_force
{
	int i, status;
	Vnode *v;

	sys_msg(debug, LOG_DEBUG, "Unmounting maps");
	
	/* unmount automounter */
	for (i = node_table_count - 1; i >= 0; i--)
	{
		v = node_table[i].node;

		if ([v fakeMount]) continue;
		if ([v server] != nil) continue;
		if (![v mounted]) continue;
		if ([v link] == nil) continue;

#ifdef __APPLE__
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
				sys_msg(debug, LOG_ERR, "validate: abandoning mountpoint %s.", mountpoint);
					
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

	[self unmountAutomounts:1];
	[self unmountMaps:1];

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

- (unsigned int)attemptUnmount:(Vnode *)v
{
	int status;

	if (v == nil) return EINVAL;

	if (![v mounted]) return 0;

	if ([v type] != NFLNK) return EINVAL;

	if ([v source] == nil)
	{
		[v setMounted:NO];
		return 0;
	}

	sys_msg(debug_mount, LOG_DEBUG, "Attempting to unmount %s",
		[[v link] value]);
#ifdef __APPLE__
	status = unmount([[v link] value], 0);
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
		return 0;
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

@end
