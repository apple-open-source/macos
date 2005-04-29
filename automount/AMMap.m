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
#import "AMMap.h"
#import "Controller.h"
#import "AMVnode.h"
#import "Server.h"
#import "AMString.h"
#import "automount.h"
#import "log.h"
#import "vfs_sysctl.h"
#import <sys/stat.h>
#import <sys/mount.h>
#import <netdb.h>
#import <syslog.h>
#import <string.h>
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/message.h>
#import <servers/bootstrap.h>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreServices/CoreServices.h>
#import <URLMount/URLMount.h>
#import <URLMount/URLMountPrivate.h>

#define AUTOFSMOUNTTARGET "autofs"

extern CFRunLoopRef gMainRunLoop;

#ifndef innetgr
extern int innetgr(const char *, const char *, const char *, const char *);
#endif
extern int doing_timeout;
extern BOOL doServerMounts;
extern char *gLookupTarget;

@implementation Map

typedef struct {
	mach_msg_header_t header;
	AMInfo_req_msg_body_t body;
	mach_msg_security_trailer_t trailer;
} AMInfo_server_req_msg_t;

typedef struct {
	mach_msg_header_t header;
	AMInfo_rsp_msg_body_t body;
} AMInfo_server_rsp_msg_t;

#if 0
typedef union {
	AMInfo_server_req_msg_t rcv;
	AMInfo_server_rsp_msg_t send;
} AMInfo_server_msg_t;
#endif

static char * bootstrap_error_string(int errNum);

- (id)init
{
	[super init];
	
	NFSMountOptions = 0;
	
	AMInfoServicePort = MACH_PORT_NULL;
	AMInfoPortContext = NULL;
	AMInforeqRLS = NULL;

	[self setMountStyle: kMountStyleParallel];
	
	return self;
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir
{
	return [self initWithParent:p directory:dir from:nil mountdirectory:nil];
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds
{
	return [self initWithParent:p directory:dir from:ds mountdirectory:nil];
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt
{
	return [self initWithParent:p directory:dir from:ds mountdirectory:mnt withRootVnodeClass:[Vnode class]];
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt withRootVnodeClass:(Class)rootVnodeClass
{
	char mountdirpath[MAXPATHLEN];
	
	[self init];

	if (mnt == NULL) mnt = [controller mountDirectory];
	if (strcasecmp([mnt value], AUTOFSMOUNTTARGET) != 0) {
		[self setMountDirectory: mnt];
		[self setMountStyle: kMountStyleParallel];
	} else {
		snprintf(mountdirpath, sizeof(mountdirpath), "%s/%s", [[p path] value], [dir value]);
		[self setMountDirectory: [String uniqueString:mountdirpath]];
		[self setMountStyle: kMountStyleAutoFS];
	};

	if (ds) {
		[self setName: ds];
	} else {
		[self setName: [String uniqueString:"-null"]];
	};

	root = [[rootVnodeClass alloc] init];
	[root setMap:self];
	[root setName:dir];
	if (p != nil) [p addChild:root];
	[controller registerVnode:root];
    
	return self;
}

/*
 *	-cleanup exists for maps that need to do some form of clean up
 *	(release resources, etc.) before the global Vnode and Server
 *	tables have been released.  For example, StaticMap uses this
 *	to remove symlinks for static mounts.
 */
- (void)cleanup
{
}

- (void)setName:(String *)n
{
	[n retain];
	[name release];
	name = n;
}

- (String *)name
{
	return name;
}

- (void)setHostname:(String *)hn
{
	[hn retain];
	[hostname release];
	hostname = hn;
}

- (String *)hostname
{
	return hostname;
}

- (void)dealloc
{
	if (AMInfoServicePort) [self deregisterAMInfoService];

	if (mountPoint != nil) [mountPoint release];
	if (name != nil) [name release];
	if (root != nil)
	{
		[controller destroyVnode:root];
	}
	
	[super dealloc];
}

- (unsigned int)didAutoMount
{
	struct Vnode *rootVnode;
	int result;
	struct stat sb;
	struct statfs sfs;

	rootVnode = [self root];
	
	/* Look up and save off the fsid of the newly mounted map: */
	result = statfs([[rootVnode path] value], &sfs);
	if (result != 0) {
		sys_msg(debug, LOG_ERR, "Can't determine status of autofs map on %s (%s)", [[rootVnode path] value], strerror(errno));
		return errno == 0 ? 1 : errno;
	}
	[self setFSID:&sfs.f_fsid];

	if ([self mountStyle] == kMountStyleAutoFS) {
		/* Hash the root vnode: Pick up the root node id assigned by autofs: */
		result = stat([[rootVnode path] value], &sb);
		if (result) {
			sys_msg(debug, LOG_ERR, "Couldn't determine root node id for autofs node '%s' (%s)?!", [[rootVnode path] value], strerror(errno));
			return errno == 0 ? 1 : errno;
		}
		[rootVnode setNodeID:sb.st_ino];
		[rootVnode setHashKey:sfs.f_fsid nodeID:sb.st_ino];
	}
	
	return [self registerAMInfoService];
}

- (Vnode *)root
{
	return root;
}

- (void)setMountDirectory:(String *)mnt
{
	[mnt retain];
	[mountPoint release];
	mountPoint = mnt;
}

- (String *)mountPoint
{
	return mountPoint;
}

- (void)setNFSMountOptions:(int)mountOptions
{
	NFSMountOptions = mountOptions;
}

- (int)NFSMountOptions
{
	return NFSMountOptions;
}

- (int)mountArgs
{
	return 0;
}

- (void)setFSID:(fsid_t *)fsid
{
	mountedMapFSID = *fsid;
}

- (fsid_t *)mountedFSID
{
	return &mountedMapFSID;
}

BOOL AbandonMountSequence( void )
{
	/* If there's a mount in progress in a subnode and this process is NOT the submounter,
	   leave the remainder of the mount sequence to the submounter ancestor */
	return (gForkedMountInProgress || gBlockedMountDependency) && !gSubMounter;
}

- (int)mount:(Vnode *)v withUid:(int)uid
{
	unsigned int i, len = 0, status = 0, substatus, fail = 0;
	Array *kids;
	int result;
	pid_t child_pid;

	sys_msg(debug_mount, LOG_DEBUG, "Mount triggered at %s", [[v path] value]);
	if ([v server]) {
		/* Mount a server, possibly fork()-ing in the process and returning with gForkedMountInProgress or gBlockedMountDependency set: */
		sys_msg(debug_mount, LOG_DEBUG, "Mounting %s...", [[v path] value]);
		status = [controller nfsmount:v withUid:uid];
		if (status != 0)
		{
			sys_msg(debug_mount, LOG_ERR, "Attempt to mount %s returned %d (%s)", [[v path] value], status, strerror(status));
			goto Mount_Done;
		}
		sys_msg(debug_mount, LOG_DEBUG, "Mount of %s completed successfully.", [[v path] value]);
	};
	
	/* if there's a mount in progress in another process, leave subnodes for that child process to handle: */
	if (AbandonMountSequence()) {
		sys_msg(debug_mount, LOG_DEBUG, "Abandoning mount sequence from parent process %d [outside]...", getpid());
		sys_msg(debug_mount, LOG_DEBUG, "Mount at %s returning status = %d", [[v path] value], status);
		goto Mount_Done;
	};
	
	if (gForkedMount && !gSubMounter) {
		/* The previous mount left this process running as a forked-off mount child-process */
		sys_msg(debug_mount, LOG_DEBUG, "Marking process %d as gSubMounter...", getpid());
		gSubMounter = YES;		/* Note the fact that we're the top-most mounter child process */
		gForkedMount = NO;		/* Make way for recording subsequent offsprint processes */
	};
	
	if (gSubMounter) {
		if (gForkedMount) {
			/* We're offspring from a mouter child process and grandchild processes should not continue in the loop: */
			sys_msg(debug_mount, LOG_DEBUG, "Forcing exit from grand-child process %d...", getpid());
			exit((gMountResult < 128) ? gMountResult : ECANCELED);
		} else if (gForkedMountInProgress) {
			sys_msg(debug_mount, LOG_DEBUG, "Submounter %d: waiting for exit of grand-child process %d...", getpid(), gForkedMountPID);
			while ((((child_pid = wait4(gForkedMountPID, &result, 0, NULL)) != 0) && (child_pid != -1)) ||
				   ((child_pid == -1) && (errno == EINTR))) {
				sys_msg(debug_mount, LOG_DEBUG, "Submounter %d: wait4() returned %d (errno = %d, %s)...", getpid(), child_pid, errno, strerror(errno));
				if (child_pid == gForkedMountPID) {
					sys_msg(debug_mount, LOG_DEBUG, "Submounter %d: completing mount by grand-child process %d...", getpid(), gForkedMountPID);
					[controller completeMountInProgressBy:gForkedMountPID exitStatus:result];
					/* This process will find "gForkedMountInProgress && !gSubMounter" and will abandon further mount attempts... */
				};
			};
			sys_msg(debug_mount, LOG_DEBUG, "Submounter %d: wait4() loop terminated with %d (errno = %d, %s)...", getpid(), child_pid, errno, strerror(errno));
			gForkedMountInProgress = NO;
			gForkedMountPID = (pid_t)-1;
		};
	};
		
	/* Having mounted the top-level node, mount the lower nodes (being careful not to repeat the loop inside
	   subsequent further offspring processes */
	fail = 0;
	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];
	for (i = 0; i < len; i++)
	{
		Vnode *submountpoint = [kids objectAtIndex:i];
				
		/* Mount a subnode, possibly leaving two processes running here;
		   one of them will find 'gForkedMount' set in the next iteration and exit; the other will continue */
		sys_msg(debug_mount, LOG_DEBUG, "Mounting offspring at %s", [[submountpoint path] value]);
		substatus = [self mount:submountpoint withUid:uid];
		if (substatus != 0) fail++;

		/* if there's now, for the first time, a mount in progress in another process,
		   leave subnodes for that child process to handle and speed a return here from the parent
		   process (the top-level automount process): */
		if (AbandonMountSequence()) {
			sys_msg(debug_mount, LOG_DEBUG, "Abandoning mount sequence from parent process %d [inside]...", getpid());
			sys_msg(debug_mount, LOG_DEBUG, "Mount at %s returning status = %d", [[v path] value], status);
			goto Mount_Done;
		};
	}

Mount_Done:
	if ((len != 0) && (fail == len) && ([v source] == nil))
	{
		[v setMounted:NO];
		status = 0;
	}

	if ([v serverDepth] == 0) [[v server] reset];

	sys_msg(debug_mount, LOG_DEBUG, "Mount at %s returning final status = %d", [[v path] value], status);
	return status;
}

- (int)attemptUnmountChildren:(Vnode *)v usingForce:(int)use_force withRemountOnFailure:(BOOL)remountOnFailure
{
	int i, len;
	Array *kids;
	Vnode *k;
	int status;

	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];

	for (i = 0; i < len; i++)
	{
		k = [kids objectAtIndex:i];
		status = [self unmount:k withRemountOnFailure:(remountOnFailure || (doServerMounts && [k server]))];
		if ((status != 0) && remountOnFailure)
		{
			[self mount:v withUid:99];
			return 1;
		}
	}
	
	return 0;
}

- (int)unmount:(Vnode *)v withRemountOnFailure:(BOOL)remountOnFailure
{
	return [self unmount:v usingForce:0 withRemountOnFailure:remountOnFailure];
}

- (int)unmount:(Vnode *)v usingForce:(int)use_force withRemountOnFailure:(BOOL)remountOnFailure
{
	struct timeval tv;
	int status;

	if ((v == nil) || [v fakeMount]) return 0;

	if ([v mounted] &&
		([v server] != nil) &&
		(![[v server] isLocalHost]))
	{
		if (doing_timeout) {
			if ([v vfsType] && (strcasecmp([[v vfsType] value], "url") == 0)) return 1;
			if ([v mountTimeToLive] == 0) return 1;

			gettimeofday(&tv, NULL);
			if (tv.tv_sec < ([v mountTime] + [v mountTimeToLive])) return 1;

		};
		
		if ([[v map] mountStyle] == kMountStyleParallel) {
			sys_msg(debug, LOG_DEBUG, "Checking %s from server '%s' (on %s)", [[v path] value], [[[v server] name] value], [[v link] value]);
		} else {
			sys_msg(debug, LOG_DEBUG, "Checking %s from server '%s'", [[v path] value], [[[v server] name] value]);
		};

		if ([v children]) {
			/*
			   Try to unmount child nodes.  If 'doServerMounts' is set, it's
			   important to re-mount any nodes that get unmounted if any node
			   cannot be unmounted (e.g. is busy)
			 */
			status = [self attemptUnmountChildren:v usingForce:use_force withRemountOnFailure:doServerMounts];
			if (status != 0) return status;
		}
		
		status = [controller attemptUnmount:v usingForce:use_force];
		if (status != 0) 
		{
			[self mount:v withUid:99];
			return 1;
		}
		return 0;
	} else {
		if ([v children]) {
			status = [self attemptUnmountChildren:v usingForce:use_force withRemountOnFailure:(remountOnFailure || (doServerMounts && [v server]))];
			if (status != 0) return status;
		}
	}
	
	return 0;
}

- (int)unmountAutomounts:(int)use_force
{
	return [self unmount:root usingForce:use_force withRemountOnFailure:NO];
}

- (AMMountStyle)mountStyle
{
	return mountStyle;
}

- (void) setMountStyle:(AMMountStyle)style
{
	mountStyle = style;
}

- (int)handle_autofsreq:(struct autofs_userreq *)req
{
	VNodeHashKey hashKey;
    char pathbuffer[MAXPATHLEN + 1];
	String *path = nil;
	Vnode *v = nil;
	int result = 0;
	struct autofs_mounterreq mounter_req;
	size_t mounter_reqlen = sizeof(mounter_req);

	/* Locate the node to be mounted: try a hash-lookup first */
	hashKey.fsid = *[self mountedFSID];
	hashKey.nodeid = req->au_ino;	
	v = [controller vnodeWithKey:&hashKey];
	if (v == nil) {
		/* Try to locate the node using the path specified: */
		strncpy(pathbuffer, req->au_name, sizeof(pathbuffer) - 1);
		pathbuffer[sizeof(pathbuffer) - 1] = (char)0;		/* Make sure the string's terminated */
		path = [String uniqueString:pathbuffer];
		if (path == nil) {
			result = ENOMEM;
			goto Send_Response;
		}
		
		v = [self lookupVnodePath:path from:root];
	}
	if (v == nil) {
		result = ENOENT;
		goto Send_Response;
	}

	if ([v server]) {
		/* make sure this process is marked as the node's mounter: */
		mounter_req.amu_ino = req->au_ino;
		mounter_req.amu_pid = getpid();
		mounter_req.amu_uid = 0;
		mounter_req.amu_flags = ([v vfsType] && strcmp([[v vfsType] value], "nfs")) ? AUTOFS_MOUNTERREQ_UID : 0;
		sys_msg(debug, LOG_DEBUG, "Trying to become mounter for autofs node '%s' (fsid [%ld,%ld], node %ld, pid=%ld)...",
									[[v path] value],
									[self mountedFSID]->val[0], [self mountedFSID]->val[1],
									req->au_ino,
									getpid());
		result = sysctl_fsid(AUTOFS_CTL_MOUNTER, [self mountedFSID], NULL, 0, &mounter_req, mounter_reqlen);
		if (result != 0) {
			sys_msg(debug, LOG_ERR, "Couldn't become mounter for autofs node '%s' (%s)?!", [[v path] value], strerror(errno));
		}

		/* Now do the mount */
		sys_msg(debug, LOG_DEBUG, "handle_autofsreq: mounting '%s'...", [[v path] value]);
		result =[[v map] mount:v withUid:req->au_uid];
		[[v server] reset];
		if (gForkedMountInProgress || gBlockedMountDependency) {
			sys_msg(debug, LOG_DEBUG, "handle_autofsreq: forked mount in progress...");
		} else {
			sys_msg(debug, LOG_DEBUG, "\tresult = %d (%s)?!", result, strerror(result ? errno : 0));
		}
	} else {
		sys_msg(debug, LOG_DEBUG, "handle_autofsreq: populating '%s'...", [[v path] value]);
		gLookupTarget = "[ autofs trigger ]";
		[v generateDirectoryContents:NO];
		result = 0;
	}
	
Send_Response:
	return result;
}

void AMInfoMsgCallback(CFMachPortRef CFPort, void *msg, CFIndex size, void *info) {
	mach_msg_trailer_t *trailer; 
	mach_msg_trailer_size_t trailer_size = 0;

	if (0) {
		trailer = (mach_msg_trailer_t *)((char *)msg + ((mach_msg_header_t *)msg)->msgh_size);
	};
#if 0
	sys_msg(debug, LOG_DEBUG, "AMInfoMsgCallback: Received %ld bytes\n", size);
	sys_msg(debug, LOG_DEBUG, "\tCFPort = 0x%08x (Mach port 0x%08lx)...", (unsigned long)CFPort, (unsigned long)CFMachPortGetPort(CFPort));
	sys_msg(debug, LOG_DEBUG, "\tmsg = 0x%08x...", (unsigned long)msg);
	sys_msg(debug, LOG_DEBUG, "\tinfo = 0x%08x...", (unsigned long)info);
#endif
	(void)post_AMInfoServiceRequest(CFMachPortGetPort(CFPort), (Map *)info, (mach_msg_header_t *)msg, (size_t)size + trailer_size);
}

- (int)registerAMInfoService
{
	char servicename[sizeof(AMINFOSERVICENAMEPREFIX) + 1 + MNAMELEN + 1];	/* MNAMELEN bytes for target mountpoint name */
	mach_port_t bp;
	mach_port_t parent_bp;
	kern_return_t ret;
	Boolean freeAMInfoPortContext = false;
	unsigned int result = -1;
	
	sys_msg(debug, LOG_DEBUG, "registerAMInfoService: allocating and registering AMInfoServicePort for Map 0x%08lx...", self);
	
	AMInfoPortContext = calloc(1, sizeof(*AMInfoPortContext));
	if (AMInfoPortContext == NULL) {
		result = ENOMEM;
		goto Err_Exit;
	};
	
    AMInfoPortContext->version = 0;
    AMInfoPortContext->info = self;
    AMInfoPortContext->retain = NULL;
    AMInfoPortContext->release = NULL;
    AMInfoPortContext->copyDescription = NULL;
	
	AMInfoServicePort = CFMachPortCreate(NULL, &AMInfoMsgCallback, AMInfoPortContext, &freeAMInfoPortContext);
	if (AMInfoServicePort == NULL)
	{
		sys_msg(debug, LOG_ERR, "registerAMInfoService: could not create AMInfoServicePort");
		goto Err_Exit;
	}

    /* Create a run loop source for the service port with some minimal context passed on: */
	AMInforeqRLS = CFMachPortCreateRunLoopSource(NULL, AMInfoServicePort, 0);
	if (AMInforeqRLS == NULL)
	{
		sys_msg(debug, LOG_ERR, "registerAMInfoService: could not create CFRunLoopSource");
		goto Err_Exit;
	}

	/* Set up to read messages from the Mach port in the main event loop: */
	CFRunLoopAddSource(gMainRunLoop, AMInforeqRLS, kCFRunLoopDefaultMode);

	/*
	 * Declare a service name associated with this server.
	 */
	snprintf(servicename, sizeof(servicename), "%s.%s", AMINFOSERVICENAMEPREFIX, [hostname value]);
	
	/* Publish the port in the lowest-level [boot] bootstrap port: */
	ret = task_get_bootstrap_port(mach_task_self(), &bp);
	if (ret != KERN_SUCCESS) {
		sys_msg(debug_mount, LOG_ERR, 
                "registerAMInfoService: task_get_bootstrap_port(..., &bp): 0x%x: %s",
                    ret, mach_error_string(ret));
		goto Err_Exit;
	}

	/* Find the outermost bootstrap pointer for registration: */
	while (((ret = bootstrap_parent(bp, &parent_bp)) == BOOTSTRAP_SUCCESS) &&
		   (parent_bp != bp)) {
		bp = parent_bp;
	};
	
	ret = bootstrap_register(bp, servicename, CFMachPortGetPort(AMInfoServicePort));
	if (ret != BOOTSTRAP_SUCCESS) {
		sys_msg(debug_mount, LOG_ERR,
                "registerAMInfoService: bootstrap_register(%s): 0x%x: %s", 
                    servicename, ret, bootstrap_error_string(ret));
		goto Err_Exit;
	}
	
	result = 0;
	goto Std_Exit;

Err_Exit:
	/* No special cleanup necessary */;
	
Std_Exit:
	if (AMInfoPortContext && freeAMInfoPortContext) free(AMInfoPortContext);
	
    return result;
}

- (void)handleAMInfoRequest:(mach_msg_header_t *)msg ofSize:(size_t)size onPort:(mach_port_t)port
{
	AMInfo_req_msg_body_t *body;
	mach_msg_trailer_t *trailer;
    char pathbuffer[MAXPATHLEN + 1];
	AMInfo_server_rsp_msg_t rsp;
	char *str = NULL;
	size_t len;
	mach_msg_return_t mret;
	String *path = nil;
	Vnode *v = nil;
	
	body = (AMInfo_req_msg_body_t *)((char *)msg + sizeof(mach_msg_header_t));
	trailer = (mach_msg_trailer_t *)((char *)msg + msg->msgh_size);
    
#if 0
	sys_msg(debug, LOG_DEBUG, "handlePendingAMInfoRequests: info requested = 0x%08lx", body->request);
	sys_msg(debug, LOG_DEBUG, "handlePendingAMInfoRequests: path = '%s'", body->path);
#endif
	
/*  uid_t clientuid = trailer->msgh_sender.val[0];		*/

	bzero(&rsp, sizeof(rsp));
	rsp.header = *msg;
	rsp.body.result = -1;
	str = (char *)&rsp.body.returnBuffer;
	
	/* Check if any unknown or unsupported options are being requested: */
	if (body->request & ~(AMI_MOUNTOPTIONS | AMI_URL | AMI_MOUNTDIR | AMI_TRIGGERPATH)) {
		rsp.body.result = EINVAL;
		goto Send_Response;
	};

	/* Convert the string to Unique string */
	strncpy(pathbuffer, body->path, sizeof(pathbuffer) - 1);
	pathbuffer[sizeof(pathbuffer) - 1] = (char)0;		/* Make sure the string's terminated */
	path = [String uniqueString:pathbuffer];
	if (path == nil) {
		rsp.body.result = ENOMEM;
		goto Send_Response;
	};
		
	/* Generate the trigger path for given real path */
	if (body->request & AMI_TRIGGERPATH) {
		String *triggerPath = nil;
	
		triggerPath = [controller findDirByMountDir:path];
		if (triggerPath == nil) {
			rsp.body.result = EINVAL;
			goto Send_Response;
		}
	
		rsp.body.triggerPathOffset = str - (char *)&rsp.body.triggerPathOffset;
		len = [triggerPath length];
		if (len < (MAXPATHLEN-1)) {
			len = MAXPATHLEN-1;
		}
		strncpy(str, [triggerPath value], len);
		
		str[len] = 0;
		str += len + 1; 	/* include terminating byte in count */
		str += (( 4 - ((unsigned long)str & 3)) & 3);  /* Round up to nearest 4-byte multiple */

		[triggerPath release];
		
		/* currently AMI_TRIGGERPATH message is not combined with any other AMI messages.
		 * Send the successs message back */
		rsp.body.result = 0;
		goto Send_Response;
	} else {
		rsp.body.triggerPathOffset = 0;
	}

	/* Locate the node for the path specified: Not REQUIRED for AMI_TRIGGERPATH */
	v = [self lookupVnodePath:path from:[[controller rootMap] root]];
	if (v == nil) {
		rsp.body.result = ENOENT;
		goto Send_Response;
	};
	
	/* Generate the mount options field: */
	if (body->request & AMI_MOUNTOPTIONS) {
		rsp.body.mountOptions = 0;  /* kMarkAutomounted | kUseUIProxy | kMountAll */
		if ([v mntArgs] & MNT_DONTBROWSE) rsp.body.mountOptions |= kMarkDontBrowse;
	};

	/* Generate the server URL string: */
	if (body->request & AMI_URL) {
		rsp.body.URLOffset = str - (char *)&rsp.body.URLOffset;
		if ([v urlString]) {
			len = strlen([[v urlString] value]);
			if (len > (MAXURLLENGTH-1)) {
				len = MAXURLLENGTH-1;
			};
			strncpy(str, [[v urlString] value], len);
		} else {
			len = 0;
		};
		str[len] = (char)0;
		str += len + 1;			/* Include terminating null byte in count */
		str += ((4 - ((unsigned long)str & 3)) & 3);		/* Round up to nearest 4-byte multiple */
	} else {
		rsp.body.URLOffset = 0;
	};
	
	/* Generate the target mount path: */
	if (body->request & AMI_MOUNTDIR) {
		rsp.body.mountDirOffset = str - (char *)&rsp.body.mountDirOffset;
		if ([v link]) {
			len = strlen([[v link] value]);
			if (len > (MAXPATHLEN-1)) {
				len = MAXPATHLEN-1;
			};
			strncpy(str, [[v link] value], len);
		} else {
			len = strlen([[v path] value]);
			if (len > (MAXPATHLEN-1)) {
				len = MAXPATHLEN-1;
			};
			strncpy(str, [[v path] value], len);
		};
		str[len] = (char)0;
		str += len + 1;			/* Include terminating null byte in count */
		str += ((4 - ((unsigned long)str & 3)) & 3);		/* Round up to nearest 4-byte multiple */
	} else {
		rsp.body.mountDirOffset = 0;
	};
	
	rsp.body.result = 0;
	
Send_Response:
	/* Generate a response message: */
	rsp.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	rsp.header.msgh_size = str - (char *)&rsp.header;
	rsp.header.msgh_local_port = MACH_PORT_NULL;
	rsp.header.msgh_id = msg->msgh_id + 100;
	
#if 0
	sys_msg(debug, LOG_DEBUG, "handlePendingAMInfoRequests: sending %d-byte response to port 0x%lx.",
						str - (char *)&rsp.header,
						port);
#endif
	mret = mach_msg(&rsp.header,
					MACH_SEND_MSG,
					str - (char *)&rsp.header,
					0,
					MACH_PORT_NULL,
					MACH_MSG_TIMEOUT_NONE,
					MACH_PORT_NULL);
	if (mret != MACH_MSG_SUCCESS) {
		sys_msg(debug_mount, LOG_DAEMON | LOG_ERR, "mach_msg(response): 0x%x: %s\n", 
										mret, mach_error_string(mret));
		return;
	};
	
	if (path) [path release];
}

- (int)deregisterAMInfoService
{
	if (AMInforeqRLS) {
		sys_msg(debug, LOG_DEBUG, "deregisterAMInfoService: releasing AMInforeqRLS for Map 0x%08x.", self);
		CFRunLoopRemoveSource(gMainRunLoop, AMInforeqRLS, kCFRunLoopDefaultMode);
		CFRelease(AMInforeqRLS);
		AMInforeqRLS = NULL;
	}
	
	if (AMInfoServicePort) {
		sys_msg(debug, LOG_DEBUG, "deregisterAMInfoService: releasing AMInfoServicePort for Map 0x%08x.", self);
		CFMachPortInvalidate(AMInfoServicePort);
		CFRelease(AMInfoServicePort);
		AMInfoServicePort = MACH_PORT_NULL;
	};
	
	if (AMInfoPortContext) {
		sys_msg(debug, LOG_DEBUG, "deregisterAMInfoService: releasing AMInfoPortContext for Map 0x%08x.", self);
		free(AMInfoPortContext);
		AMInfoPortContext = NULL;
	};
	
    return 0;
}

- (void)reInit
{
	[root setMarked:NO];
}

- (void)timeout
{
	[self unmount:root withRemountOnFailure:NO];
}

- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v
{
	return [self createVnodePath:path from:v withType:nil];
}

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
	gLookupTarget = s;

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
		else
		{
			[x setMarked:NO];	/* This Vnode is still alive */
		}
		n = x;

		[part release];
		s = [path scan:'/' pos:&p];
	}

	return n;
}

- (Vnode *)lookupVnodePath:(String *)path from:(Vnode *)v
{
	int i, p;
	Vnode *n, *x;
	char *s, t[1024];
	String *part;

	if (path == nil) return nil;
	if ([path length] == 0) return v;

	p = 0;
	s = [path value];
	gLookupTarget = s;

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
		[part release];
		if (x == nil) return nil;

		n = x;
		s = [path scan:'/' pos:&p];
	}

	return n;
}

- (BOOL)checkVnodePath:(String *)path from:(Vnode *)v
{
	return [self lookupVnodePath:path from:v] != nil;
}

- (Vnode *)mkdir:(String *)s attributes:(void *)x atVnode:(Vnode *)v
{
	Vnode *n;
	struct fattr f;
	sattr *a;

	gLookupTarget = [s value];
	
	n = [v lookup:s];
	if (n != nil) return n;

	n = [[v map] createVnodePath:s from:v];
	if (n == nil) return nil;

	a = (sattr *)x;
	f = [n attributes];
	f.mode = a->mode | NFSMODE_DIR;
	f.uid = a->uid;
	f.gid = a->gid;
	f.size = a->size;
	f.atime = a->atime;
	f.mtime = a->mtime;

	[n setAttributes:f];
	return n;
}

- (Vnode *)symlink:(String *)l name:(String *)s atVnode:(Vnode *)v
{
	Vnode *n;

	gLookupTarget = [s value];

	n = [v lookup:s];
	if (n != nil) return n;

	n = [[v map] createVnodePath:s from:v];
	if (n == nil) return nil;

	[n setLink:l];
	[n setType:NFLNK];
	[n setMode:00755 | NFSMODE_LNK];
	[n setMounted:YES];
	[n setFakeMount:YES];

	return n;
}

- (BOOL)testOptEqualKey:(char *)k val:(char *)v
{
	char str[1024], *p;
	int len, off;
	String *x;
	BOOL yn;

	if (!strcmp(k, "host"))
	{
		if (!strcmp(v, [[controller hostName] value])) return YES;
		if ([controller hostDNSDomain] == nil) return NO;
		sprintf(str, "%s.%s", [[controller hostName] value],
			[[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return YES;
		return NO;
	}
	else if (!strcmp(k, "netgroup"))
	{
		if (innetgr(v, [[controller hostName] value], NULL, NULL)) return YES;
		return NO;
	}
	else if (!strcmp(k, "network"))
	{
		/* value is <address>[/<bits>] */
		x = [String uniqueString:v];

		p = strchr(v, '/');
		if (p == NULL) yn = [Server isMyAddress:x];
		else yn = [Server isMyNetwork:x];
		[x release];
		return yn;
	}
	else if (!strcmp(k, "domain"))
	{
		if ([controller hostDNSDomain] == nil) return NO;

		strcpy(str, [[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return YES;

		len = strlen(v);
		if (len >= [[controller hostDNSDomain] length]) return NO;
		off = [[controller hostDNSDomain] length] - len;
		while (v[0] == '.' && str[off] == '.')
		{
			v++;
			off++;
		}
		if (str[off - 1] != '.') return NO;
		if (!strcmp(v, str + off)) return YES;

		return NO;
	}
	else if (!strcmp(k, "os"))
	{
		if (!strcmp(v, [[controller hostOS] value])) return YES;
		return NO;
	}
	else if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		if (!strcmp(str, [[controller hostOSVersion] value])) return YES;

		p = strchr(str, '.');
		if (p == NULL)
		{
			if (atoi(str) == [controller hostOSVersionMajor]) return YES;
		}
		else
		{
			*p = '\0';
			p++;
			if (atoi(str) != [controller hostOSVersionMajor]) return NO;
			if (atoi(p) == [controller hostOSVersionMinor]) return YES;
		}

		return NO;
	}
	else if (!strcmp(k, "arch"))
	{
		if (!strcmp(v, [[controller hostArchitecture] value])) return YES;
		return NO;
	}
	else if (!strcmp(k, "byte"))
	{
		if (!strcmp(v, [[controller hostByteOrder] value])) return YES;
		return NO;
	}
	else if (!strcmp(k, "url"))
	{
		return YES;
	}
	else if (!strcmp(k, "authenticated_url"))
	{
		return YES;
	}

	/* unknown key - refuse it */
	return NO;
}

- (BOOL)testOptNotEqualKey:(char *)k val:(char *)v
{
	char str[1024], *p;
	int len, off;
	String *x;
	BOOL yn;
	if (!strcmp(k, "host"))
	{
		if (!strcmp(v, [[controller hostName] value])) return NO;
		if ([controller hostDNSDomain] == nil) return YES;
		sprintf(str, "%s.%s", [[controller hostName] value],
			[[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return NO;
		return YES;
	}
	else if (!strcmp(k, "netgroup"))
	{
		if (innetgr(v, [[controller hostName] value], NULL, NULL)) return NO;
		return YES;
	}
	else if (!strcmp(k, "network"))
	{
		/* value is <address>[/<bits>] */
		x = [String uniqueString:v];

		p = strchr(v, '/');
		if (p == NULL) yn = [Server isMyAddress:x];
		else yn = [Server isMyNetwork:x];
		[x release];
		return (!yn);
	}
	else if (!strcmp(k, "domain"))
	{
		if ([controller hostDNSDomain] == nil) return YES;

		strcpy(str, [[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return NO;

		len = strlen(v);
		if (len >= [[controller hostDNSDomain] length]) return YES;
		off = [[controller hostDNSDomain] length] - len;
		while (v[0] == '.' && str[off] == '.')
		{
			v++;
			off++;
		}
		if (str[off - 1] != '.') return YES;
		if (!strcmp(v, str + off)) return NO;

		return YES;
	}
	else if (!strcmp(k, "os"))
	{
		if (!strcmp(v, [[controller hostOS] value])) return NO;
		return YES;
	}
	else if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		if (!strcmp(str, [[controller hostOSVersion] value])) return NO;

		p = strchr(str, '.');
		if (p == NULL)
		{
			if (atoi(str) != [controller hostOSVersionMajor]) return YES;
		}
		else
		{
			*p = '\0';
			p++;
			if (atoi(str) != [controller hostOSVersionMajor]) return YES;
			if (atoi(p) == [controller hostOSVersionMinor]) return NO;
		}

		return YES;
	}
	else if (!strcmp(k, "arch"))
	{
		if (!strcmp(v, [[controller hostArchitecture] value])) return NO;
		return YES;
	}
	else if (!strcmp(k, "byte"))
	{
		if (!strcmp(v, [[controller hostByteOrder] value])) return NO;
		return YES;
	}

	/* unknown key - refuse it */
	return NO;
}

- (BOOL)testOptGreaterKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		vminor = 0;
		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] > vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] > vmajor) return YES;
		if ([controller hostOSVersionMajor] < vmajor) return NO;

		if ([controller hostOSVersionMinor] > vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptGreaterEqualKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] >= vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] > vmajor) return YES;
		if ([controller hostOSVersionMajor] < vmajor) return NO;

		if ([controller hostOSVersionMinor] >= vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptLessKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] < vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] < vmajor) return YES;
		if ([controller hostOSVersionMajor] > vmajor) return NO;

		if ([controller hostOSVersionMinor] < vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptLessEqualKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] <= vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] < vmajor) return YES;
		if ([controller hostOSVersionMajor] > vmajor) return NO;

		if ([controller hostOSVersionMinor] <= vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)acceptOptions:(Array *)opts
{
	String *o;
	unsigned int i, len, n;
	char tkey[256], tval[256];
	BOOL status;

	if (opts == nil) return YES;

	len = [opts count];
	if (len == 0) return YES;

	for (i = 0; i < len; i++)
	{
		o = [opts objectAtIndex:i];

		n = sscanf([o value], "%[^=]==%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s == %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^!]!=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptNotEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s != %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^>]>=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptGreaterEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s >= %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^>]>%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptGreaterKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s > %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^<]<=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptLessEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s <= %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^<]<%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptLessKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s < %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}
	}

	return YES;
}

static char * bootstrap_error_string(int errNum) {
	switch (errNum) {
	  case BOOTSTRAP_NOT_PRIVILEGED:
		return "Bootstrap not privileged";
		break;
	  case BOOTSTRAP_NAME_IN_USE:
		return "Bootstrap name in use";
		break;
	  case BOOTSTRAP_UNKNOWN_SERVICE:  
		return "Bootstrap unknown service";
		break;
	  case BOOTSTRAP_SERVICE_ACTIVE:
		return "Bootstrap service active";
		break; 
	  case BOOTSTRAP_BAD_COUNT:
		return "Bootstrap bad count";
		break;
	  case BOOTSTRAP_NO_MEMORY:
		return "Bootstrap non memory";
		break;      
	  case BOOTSTRAP_SUCCESS:
		return "No error";
		break;
	};
	
	return mach_error_string(errNum);
} 

/* Currently only defined for -static and -fstab mounts */
-(String *)findTriggerPath:(Vnode *)curRoot findPath:(String *)findPath
{
	return nil;
}

@end
