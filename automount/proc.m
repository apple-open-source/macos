/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <sys/types.h>
#include <sys/errno.h>
#import <sys/stat.h>
#import <syslog.h>
#import <unistd.h>
#import <stdlib.h>
#import <string.h>
#import "automount.h"
#import "NFSHeaders.h"
#import "Controller.h"
#import "AMMap.h"
#import "log.h"
#import "AMVnode.h"
#import "AMString.h"

/* Some essential cheats: */
struct svcudp_data {
	u_int   su_iosz;	/* byte size of send.recv buffer */
	u_long	su_xid;		/* transaction id */
	XDR	su_xdrs;	/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char * 	su_cache;	/* cached data, NULL if no cache */
};
#define	su_data(xprt)	((struct svcudp_data *)(xprt->xp_p2))

u_long rpc_xid;			/* copy of su_xid, derived from rm_xid */

#ifndef __APPLE__
#define nfsproc_create_2_svc nfsproc_create_2
#define nfsproc_getattr_2_svc nfsproc_getattr_2
#define nfsproc_link_2_svc nfsproc_link_2
#define nfsproc_lookup_2_svc nfsproc_lookup_2
#define nfsproc_mkdir_2_svc nfsproc_mkdir_2
#define nfsproc_null_2_svc nfsproc_null_2
#define nfsproc_read_2_svc nfsproc_read_2
#define nfsproc_readdir_2_svc nfsproc_readdir_2
#define nfsproc_readlink_2_svc nfsproc_readlink_2
#define nfsproc_remove_2_svc nfsproc_remove_2
#define nfsproc_rename_2_svc nfsproc_rename_2
#define nfsproc_rmdir_2_svc nfsproc_rmdir_2
#define nfsproc_root_2_svc nfsproc_root_2
#define nfsproc_setattr_2_svc nfsproc_setattr_2
#define nfsproc_statfs_2_svc nfsproc_statfs_2
#define nfsproc_symlink_2_svc nfsproc_symlink_2
#define nfsproc_write_2_svc nfsproc_write_2
#define nfsproc_writecache_2_svc nfsproc_writecache_2
#endif

int _rpcpmstart;	/* Started by a port monitor ? */
int _rpcfdtype;		/* Whether Stream or Datagram ? */
int _rpcsvcdirty;	/* Still serving ? */

extern void send_pid_to_parent(void);
extern int doing_timeout;

extern int debug;
extern int debug_proc;

Vnode *new_mount_dir;

#import <stdio.h>
struct debug_file_handle
{
	unsigned int i[8];
};

char *
fhtoc(nfs_fh *fh)
{
	static char str[32];
	struct debug_file_handle *dfh;

	dfh = (struct debug_file_handle *)fh;

	sprintf(str, "%u", dfh->i[0]);
	return str;
}

/*
 * add up sizeof (valid + fileid + name + cookie) - strlen(name)
 */
#define ENTRYSIZE (3 * BYTES_PER_XDR_UNIT + NFS_COOKIESIZE)

/*
 * sizeof(status + eof)
 */
#define JUNKSIZE (2 * BYTES_PER_XDR_UNIT)

attrstat *
nfsproc_getattr_2_svc(nfs_fh *fh, struct svc_req *req)
{
	static attrstat astat;
	struct file_handle *ifh;
	Vnode *n;

	ifh = (struct file_handle *)fh;

	sys_msg(debug_proc, LOG_DEBUG, "-> getattr");
	sys_msg(debug_proc, LOG_DEBUG, "    fh = %s", fhtoc(fh));

	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "getattr for non-existent file handle %s",
			fhtoc(fh));
		astat.status = NFSERR_NOENT;
	}
	else
	{
		/* Allow a getattr to succeed even if a previous mount attempt was canceled: */
		astat.status = [n nfsStatus];
		if (astat.status == ECANCELED) astat.status = NFS_OK;
	}

	if (astat.status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- getattr (error %d)", astat.status);
		return(&astat);
	}

	sys_msg(debug_proc, LOG_DEBUG, "    name = %s", [[n name] value]);
	astat.attrstat_u.attributes = [n attributes];
	if ((astat.attrstat_u.attributes.mode & S_IFMT) == S_IFLNK) {
		sys_msg(debug_proc, LOG_DEBUG, "    (Link flags: %s; %s,%s)",
			(astat.attrstat_u.attributes.mode & S_ISVTX) ? "trigger" : "not trigger",
			(astat.attrstat_u.attributes.mode & S_ISUID) ? "mounted" : "not mounted",
			(astat.attrstat_u.attributes.mode & S_ISGID) ? "needs auth." : "no auth. needed");
	}

	sys_msg(debug_proc, LOG_DEBUG, "<- getattr");
	return(&astat);
}

/* Does something */
diropres *
nfsproc_lookup_2_svc(diropargs *args, struct svc_req *req)
{
	static diropres res;
	struct file_handle *ifh;
	Vnode *n;
	String *s;

	ifh = (struct file_handle *)&(args->dir);

	sys_msg(debug_proc, LOG_DEBUG, "-> lookup");
	sys_msg(debug_proc, LOG_DEBUG, "    dir fh = %s", fhtoc(&(args->dir)));
	sys_msg(debug_proc, LOG_DEBUG, "    file = %s", args->name);

	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "lookup for non-existent file handle %s",
			fhtoc(&(args->dir)));
		res.status = NFSERR_NOENT;
	}
	else res.status = [n nfsStatus];
	if (res.status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- lookup (error %d)", res.status);
		return(&res);
	}

	s = [String uniqueString:args->name];
	n = [n lookup:s];
	[s release];

	if (n == nil) res.status = NFSERR_NOENT;
	else
	{
		/* Allow a lookup to succeed even if a previous mount attempt was canceled: */
		res.status = [n nfsStatus];
		if (res.status == ECANCELED) res.status = NFS_OK;
	}
	if (res.status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- lookup (res=%d)", res.status);
		return(&res);
	}

	[n getFileHandle:(nfs_fh *)&res.diropres_u.diropres.file];

	sys_msg(debug_proc, LOG_DEBUG, "    return fh = %s",
		fhtoc(&res.diropres_u.diropres.file));

	res.diropres_u.diropres.attributes = [n attributes];
	if ((res.diropres_u.diropres.attributes.mode & S_IFMT) == S_IFLNK) {
		sys_msg(debug_proc, LOG_DEBUG, "    (Link flags: %s; %s,%s)",
			(res.diropres_u.diropres.attributes.mode & S_ISVTX) ? "trigger" : "not trigger",
			(res.diropres_u.diropres.attributes.mode & S_ISUID) ? "mounted" : "not mounted",
			(res.diropres_u.diropres.attributes.mode & S_ISGID) ? "needs auth." : "no auth. needed");
	}

	sys_msg(debug_proc, LOG_DEBUG, "<- lookup");
	return(&res);
}

readlinkres *
nfsproc_readlink_2_svc(nfs_fh *fh, struct svc_req *req)
{
	static readlinkres res;
	struct file_handle *ifh;
	Vnode *n;
	unsigned int status;
	struct authunix_parms *aup;
	int uid;

	uid = -2;
	if (req->rq_cred.oa_flavor == AUTH_UNIX)
	{
		aup = (struct authunix_parms *)req->rq_clntcred;
		uid = aup->aup_uid;
		sys_msg(debug_proc, LOG_DEBUG, "\t uid %d requested a new link check", uid);
	}
	else
	{
		sys_msg(debug_proc, LOG_DEBUG, "\t uid unknown requested a new link check");
	}

	ifh = (struct file_handle *)fh;

	sys_msg(debug_proc, LOG_DEBUG, "-> readlink");
	sys_msg(debug_proc, LOG_DEBUG, "    fh = %s", fhtoc(fh));
	rpc_xid = su_data(req->rq_xprt)->su_xid;
	sys_msg(debug_proc, LOG_DEBUG, "	xid = 0x%08lx", rpc_xid);

	if (doing_timeout) return NULL;

	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "readlink for non-existent file handle %s",
			fhtoc(fh));
		res.status = NFSERR_NOENT;
	}
	else if ([n type] != NFLNK) res.status = NFSERR_ISDIR;
	else res.status = [n nfsStatus];

	if (res.status != NFS_OK)
	{
		return(&res);
		sys_msg(debug_proc, LOG_DEBUG, "<- readlink (1)");
	}

	status = 0;
#warning test here for a bad afp mount ... (i.e. disconnected)
	if ([n type] == NFLNK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "	NFLNK");
	}

	if (([n type] == NFLNK) && (![n mounted]))
	{
		sys_msg(debug_proc, LOG_DEBUG, "	not mounted");
#warning deriving internal copy of private and transport-specific xid...
		status = [[n map] mount:n withUid:uid];
		
		if (gBlockedMountDependency) {
			sys_msg(debug_proc, LOG_DEBUG, "	mount is in progress; transaction id = 0x%08lx", gBlockingMountTransactionID);
			if (gBlockingMountTransactionID == rpc_xid) {
				/* This is a retransmission of the original readlink request
				   that triggered the mount in the first place; best to wait
				   until the mount's complete. */
				sys_msg(debug_proc, LOG_DEBUG, "<- [readlink abandoned]");
				gBlockedMountDependency = NO;	/* This is a one-shot deal */
				return NULL;				/* Try again later... */
			};
			
			/* This is a mount in progress from another call - don't wait up and don't hold up THIS call! */
			gBlockedMountDependency = NO;	
		};

		if (gForkedMountInProgress) {
			sys_msg(debug_proc, LOG_DEBUG, "<- [readlink abandoned]");
			gForkedMountInProgress = NO;
			return NULL;
		};
		
		if (status != 0)
		{
			res.status = (status == ECANCELED) ? ECANCELED : NFSERR_NOENT;
			sys_msg(debug_proc, LOG_DEBUG, "<- readlink (2)");
			return(&res);
		}
	}

	sys_msg(debug_proc, LOG_DEBUG, "    name = %s", [[n name] value]);
	res.readlinkres_u.data = [[n link] value];
	sys_msg(debug_proc, LOG_DEBUG, "    link = %s", res.readlinkres_u.data);
	sys_msg(debug_proc, LOG_DEBUG, "<- readlink");
	return(&res);
}

/* Does something */
readdirres *
nfsproc_readdir_2_svc(readdirargs *args, struct svc_req *req)
{
	static readdirres res;
	Vnode *n, *v;
	struct entry *e, *nexte;
	struct entry **entp;
	unsigned int cookie, count, entrycount, i, nlist;
	struct file_handle *ifh;
	Array *list;
	String *s;

	ifh = (struct file_handle *)&(args->dir);
	cookie = *(unsigned int*)args->cookie;

	sys_msg(debug_proc, LOG_DEBUG, "-> readdir");
	sys_msg(debug_proc, LOG_DEBUG, "    dir fh = %s", fhtoc(&(args->dir)));
	sys_msg(debug_proc, LOG_DEBUG, "    cookie = %u", cookie);
	sys_msg(debug_proc, LOG_DEBUG, "    count = %u", args->count);

	/*
	 * Free up old stuff
	 */
	e = res.readdirres_u.reply.entries;
	while (e != NULL)
	{
		nexte = e->nextentry;
		free(e);
		e = nexte;
	}
	res.readdirres_u.reply.entries = NULL;

	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "readdir for non-existent file handle %s",
			fhtoc(&(args->dir)));
		res.status = NFSERR_NOENT;
	}
	else if ([n type] != NFDIR) res.status = NFSERR_NOTDIR;
	else res.status = [n nfsStatus];
	if (res.status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- readdir (1)");
		return(&res);
	}

	sys_msg(debug_proc, LOG_DEBUG, "    name = %s", [[n name] value]);

	list = [n dirlist];
	nlist = [list count];

	count = JUNKSIZE;

	entrycount = 0;
	entp = &res.readdirres_u.reply.entries;

	for (i = cookie; i < nlist; i++)
	{
		v = [list objectAtIndex:i];

		if (i == 0) s = dot;
		else if (i == 1) s = dotdot;
		else s = [v name];

		count += ENTRYSIZE;
		count += [s length];
		if (count > args->count)
		{
			sys_msg(debug_proc, LOG_DEBUG, "        BREAK");
			break;
		}

		sys_msg(debug_proc, LOG_DEBUG, "        %4u: %u %s",
			cookie, [v nodeID], [s value]);

		*entp = (struct entry *) malloc(sizeof(struct entry));
		bzero(*entp, sizeof(struct entry));

		(*entp)->fileid = [v nodeID];
		(*entp)->name = [s value];
		*(unsigned int*)((*entp)->cookie) = ++cookie;
		(*entp)->nextentry = NULL;
		entp = &(*entp)->nextentry;
	}

	if (i < nlist) res.readdirres_u.reply.eof = FALSE;
	else res.readdirres_u.reply.eof = TRUE;

	sys_msg(debug_proc, LOG_DEBUG, "    eof = %s",
		res.readdirres_u.reply.eof ? "TRUE" : "FALSE");

	[list release];

	sys_msg(debug_proc, LOG_DEBUG, "<- readdir");
	return(&res);
}
	
statfsres *
nfsproc_statfs_2_svc(nfs_fh *fh, struct svc_req *req)
{
	static statfsres res;

	sys_msg(debug_proc, LOG_DEBUG, "-> statfs");

	res.status = NFS_OK;
	res.statfsres_u.reply.tsize = 512;
	res.statfsres_u.reply.bsize = 512;
	res.statfsres_u.reply.blocks = 0;
	res.statfsres_u.reply.bfree = 0;
	res.statfsres_u.reply.bavail = 0;

	sys_msg(debug_proc, LOG_DEBUG, "<- statfs");
	return(&res);
}

/*
 * These routines do nothing - they should never even be called!
 */
void *
nfsproc_null_2_svc(void *x, struct svc_req *req)
{
	sys_msg(debug_proc, LOG_DEBUG, "-- null");
	return((void *)NULL);
}

attrstat *
nfsproc_setattr_2_svc(sattrargs *args, struct svc_req *req)
{
	static attrstat astat;

	sys_msg(debug_proc, LOG_DEBUG, "-- setattr");
	 astat.status = NFSERR_ROFS;
	return(&astat);
}

void *
nfsproc_root_2_svc(void *x, struct svc_req *req)
{
	sys_msg(debug_proc, LOG_DEBUG, "-- root");
	return(NULL);
}

readres *
nfsproc_read_2_svc(readargs *args, struct svc_req *req)
{
	static readres res;

	sys_msg(debug_proc, LOG_DEBUG, "-- read");
	res.status = NFSERR_ISDIR;	/* XXX: should return better error */
	return(&res);
}

void *
nfsproc_writecache_2_svc(void *x, struct svc_req *req)
{
	sys_msg(debug_proc, LOG_DEBUG, "-- writecache");
	return(NULL);
}	

attrstat *
nfsproc_write_2_svc(writeargs *args, struct svc_req *req)
{
	static attrstat res;

	sys_msg(debug_proc, LOG_DEBUG, "-- write");
	res.status = NFSERR_ROFS;	/* XXX: should return better error */
	return(&res);
}

diropres *
nfsproc_create_2_svc(createargs *args, struct svc_req *req)
{
	static diropres res;

	sys_msg(debug_proc, LOG_DEBUG, "-- create");
	res.status = NFSERR_ROFS;
	return(&res);
}

nfsstat *
nfsproc_remove_2_svc(diropargs *args, struct svc_req *req)
{
	static nfsstat status;
	struct file_handle *ifh;
	Vnode *n;
	String *s;
	uid_t uid = -2;	/* default = nobody */
	
	if (req->rq_cred.oa_flavor == AUTH_UNIX)
	{
		struct authunix_parms *aup;
		aup = (struct authunix_parms *)req->rq_clntcred;
		uid = aup->aup_uid;
	}

	ifh = (struct file_handle *) &(args->dir);
	
	sys_msg(debug_proc, LOG_DEBUG, "-> remove");
	sys_msg(debug_proc, LOG_DEBUG, "    dir fh = %s", fhtoc(&(args->dir)));
	sys_msg(debug_proc, LOG_DEBUG, "    file = %s", args->name);
	sys_msg(debug_proc, LOG_DEBUG, "	requesting uid = %ld", uid);

	if (uid != 0) {
		sys_msg(debug, LOG_ERR, "remove request from unauthorized uid %ld", uid);
		status = NFSERR_ACCES;
		return(&status);
	}
	
	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "remove for non-existent dir handle %s",
			fhtoc(&(args->dir)));
		status = NFSERR_NOENT;
	}
	else status = [n nfsStatus];
	if (status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- remove (error %d)", status);
		return(&status);
	}

	s = [String uniqueString:args->name];
	status = [n remove:s];
	[s release];

	if (status != NFS_OK)
		sys_msg(debug_proc, LOG_DEBUG, "<- remove (res=%d)", status);
	else
		sys_msg(debug_proc, LOG_DEBUG, "<- remove");
	return(&status);
}

nfsstat *
nfsproc_rename_2_svc(renameargs *args, struct svc_req *req)
{
	static nfsstat status;

	sys_msg(debug_proc, LOG_DEBUG, "-- rename");
	status = NFSERR_ROFS;
	return(&status);
}

nfsstat *
nfsproc_link_2_svc(linkargs *args, struct svc_req *req)
{
	static nfsstat status;

	sys_msg(debug_proc, LOG_DEBUG, "-- link");
	status = NFSERR_ROFS;
	return(&status);
}

nfsstat *
nfsproc_symlink_2_svc(symlinkargs *args, struct svc_req *req)
{
	static nfsstat status;
	struct file_handle *ifh;
	Vnode *n;
	struct authunix_parms *aup;
	uid_t uid = -2;	/* default = nobody */

	if (req->rq_cred.oa_flavor == AUTH_UNIX)
	{
		aup = (struct authunix_parms *)req->rq_clntcred;
		uid = aup->aup_uid;
	}

	sys_msg(debug_proc, LOG_DEBUG, "-> symlink");
	sys_msg(debug_proc, LOG_DEBUG, "    from.dir fh = %s", fhtoc(&(args->from.dir)));
	sys_msg(debug_proc, LOG_DEBUG, "    from.name = %s", args->from.name);
	sys_msg(debug_proc, LOG_DEBUG, "    to = %s", args->to);
	sys_msg(debug_proc, LOG_DEBUG, "    attributes:");
	sys_msg(debug_proc, LOG_DEBUG, "        mode = 0%o", args->attributes.mode);
	sys_msg(debug_proc, LOG_DEBUG, "        uid = 0%ld", args->attributes.uid);
	sys_msg(debug_proc, LOG_DEBUG, "        gid = 0%ld", args->attributes.gid);
	sys_msg(debug_proc, LOG_DEBUG, "        size = 0x%x", args->attributes.size);
	sys_msg(debug_proc, LOG_DEBUG, "        atime = [0x%x, 0x%x]", args->attributes.atime.seconds);
	sys_msg(debug_proc, LOG_DEBUG, "        mtime = [0x%x, 0x%x]", args->attributes.mtime.useconds);
	sys_msg(debug_proc, LOG_DEBUG, "    requesting uid = %ld", uid);
	
	if (uid != 0) {
		sys_msg(debug, LOG_ERR, "symlink request from unauthorized uid");
		status = NFSERR_ACCES;
		return(&status);
	};
	
	ifh = (struct file_handle *)&(args->from.dir);
	n = [controller vnodeWithID:ifh->node_id];
	if (n == nil)
	{
		sys_msg(debug, LOG_ERR, "lookup for non-existent file handle %s",
			fhtoc(&(args->from.dir)));
		status = NFSERR_NOENT;
	}
	else status = [n nfsStatus];
	if (status != NFS_OK)
	{
		sys_msg(debug_proc, LOG_DEBUG, "<- symlink (error %d)", status);
		return(&status);
	}

	status = [n symlinkWithName:args->from.name to:args->to attributes:(struct nfsv2_sattr *)&args->attributes];

	if (status) {
		sys_msg(debug_proc, LOG_DEBUG, "<- symlink");
	} else {
		sys_msg(debug_proc, LOG_DEBUG, "<- symlink (status = %d)", status);
	};
	return(&status);
}

diropres *
nfsproc_mkdir_2_svc(createargs *args, struct svc_req *req)
{
	static diropres res;

	sys_msg(debug_proc, LOG_DEBUG, "-- mkdir");
	res.status = NFSERR_ROFS;
	return(&res);
}

nfsstat *
nfsproc_rmdir_2_svc(diropargs *args, struct svc_req *req)
{
	static nfsstat status;

	sys_msg(debug_proc, LOG_DEBUG, "-- rmdir");
	status = NFSERR_ROFS;
	return(&status);
}
