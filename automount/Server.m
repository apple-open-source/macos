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
#import "Server.h"
#import <sys/types.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <netdb.h>
#import <unistd.h>
#import <stdlib.h>
#import <string.h>
#import <sys/socket.h>
#import <net/if.h>
#import <sys/ioctl.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/svc.h>
#import <errno.h>
#import <nfs_prot.h>
#import "automount.h"
#import "log.h"
#import "mount.h"
#import "AMString.h"

#define MOUNT_PROG_UNKNOWN 0 
#define MOUNT_PROG_V1 1 
#define MOUNT_PROG_V2 2 
#define MOUNT_PROG_V3 3

#define NFS_PROG_UNKNOWN 0 
#define NFS_PROG_V1 1 
#define NFS_PROG_V2 2 
#define NFS_PROG_V3 3

#define LIFE_LATENCY 120
#define DEATH_LATENCY 60

#ifndef __APPLE__
#import <libc.h>
#define EBADRPC ECONNREFUSED
#define ERPCMISMATCH ECONNREFUSED
#define EAUTH ECONNREFUSED
#define EPROGUNAVAIL ECONNREFUSED
#define EPROCUNAVAIL ECONNREFUSED
#endif

extern u_short getport(struct sockaddr_in *, u_long, u_long, u_int);

static char *
mnt_strerror(int s)
{
	if (s == 0) return "No error";
	if (s == 1) return "Not owner";
	if (s == 2) return "No such file or directory";
	if (s == 5) return "I/O error";
	if (s == 13) return "Permission denied";
	if (s == 20) return "Not a directory";
	if (s == 22) return "Invalid argument";
	if (s == 63) return "Filename too long";
	if (s == 10004) return "Operation not supported";
	return "Server failure";
}

static unsigned int
rpcerr2errno(int r, int e)
{
	switch (r)
	{
		case RPC_SUCCESS: return 0;

		case RPC_CANTENCODEARGS: return EBADRPC;
		case RPC_CANTDECODERES: return EBADRPC;
		case RPC_CANTSEND: return e;
		case RPC_CANTRECV: return e;
		case RPC_TIMEDOUT: return ETIMEDOUT;

		case RPC_VERSMISMATCH: return ERPCMISMATCH;
		case RPC_AUTHERROR: return EAUTH;
		case RPC_PROGUNAVAIL: return EPROGUNAVAIL;
		case RPC_PROGVERSMISMATCH: return ERPCMISMATCH;
		case RPC_PROCUNAVAIL: return EPROCUNAVAIL;
		case RPC_CANTDECODEARGS: return EBADRPC;
		case RPC_SYSTEMERROR: return e;

		case RPC_UNKNOWNHOST: return EHOSTUNREACH;
		case RPC_UNKNOWNPROTO: return EPROTONOSUPPORT;

		case RPC_PMAPFAILURE: return e;
		case RPC_PROGNOTREGISTERED: return EPROGUNAVAIL;

		case RPC_FAILED: return e;
	}

	return EBADRPC;
}

@implementation Server

+ (BOOL)isMyAddress:(unsigned int)a mask:(unsigned int)m
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[1024];
	int offset, addrlen;
	int sock;
	unsigned int me;

	me = htonl(INADDR_LOOPBACK);
	if ((a & m) == (me & m)) return YES;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return NO;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		close(sock);
		return NO;
	}

	addrlen = sizeof(struct ifreq) - IFNAMSIZ;
	offset = 0;

	while (offset <= ifc.ifc_len)
	{
		ifr = (struct ifreq *)(ifc.ifc_buf + offset);
#ifdef __APPLE__
		offset += IFNAMSIZ;
		if (ifr->ifr_addr.sa_len > addrlen) offset += ifr->ifr_addr.sa_len;
		else offset += addrlen;
#else
		offset += sizeof(struct ifreq);
#endif
		if (ifr->ifr_addr.sa_family != AF_INET) continue;
		if (ioctl(sock, SIOCGIFFLAGS, ifr) < 0) continue;

		if ((ifr->ifr_flags & IFF_UP) && (!(ifr->ifr_flags & IFF_LOOPBACK)))
		{
			me = ((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr;
			if ((a & m) == (me & m))
			{
				close(sock);
				return YES;
			}
		}
	}

	close(sock);
	return NO;
}

+ (BOOL)isMyAddress:(String *)addr
{
	if (addr == nil) return NO;
	return [Server isMyAddress:inet_addr([addr value]) mask:(unsigned int)-1];
}

+ (BOOL)isMyNetwork:(String *)net
{
	char *s, *p;
	unsigned int bits, a, i, x, m;

	if (net == nil) return NO;

	s = malloc([net length] + 1);
	strcpy(s, [net value]);
	p = strchr(s, '/');
	if (p == NULL) 
	{
		free(s);
		return [Server isMyAddress:net];
	}

	*p = '\0';
	p++;
	bits = atoi(p);
	if (bits == 0)
	{
		free(s);
		return [Server isMyAddress:net];
	}

	a = inet_addr(s);
	free(s);

	bits = 33 - bits;
	m = 0;
	for (i = 1, x = 1; i < bits; i++, x *= 2) m |= x;
	m = ~m;

	return [Server isMyAddress:a mask:m];
}

- (Server *)initWithName:(String *)servername
{
	char hn[1024];
	char *p;

	[super init];

	if (servername == nil)
	{
		[self release];
		return nil;
	}

	pings = 5;
	timeout = 4;
	address = 0;

	gethostname(hn, 1024);
	isLocalHost = NO;
	if (!strcmp("localhost", [servername value])) isLocalHost = YES;
	if ((!isLocalHost) && (!strcmp(hn, [servername value]))) isLocalHost = YES;
	if (!isLocalHost)
	{
		p = strchr(hn, '.');
		if (p != NULL)
		{
			*p = '\0';
			if (!strcmp(hn, [servername value])) isLocalHost = YES;
		}
	}
	if (isLocalHost) address = htonl(INADDR_LOOPBACK);

	myname = [servername retain];
	[self reset];

	return self;
}

- (void)setTimeout:(unsigned int)t
{
	if (t == 0) timeout = 1;
	else timeout = t;
}

- (String *)name
{
	return myname;
}

- (BOOL)isLocalHost
{
	return isLocalHost;
}

- (unsigned long)address
{
	struct hostent *h;

	if (address != 0) return address;

	h = gethostbyname([myname value]);
	if (h == NULL) return 0;

	bcopy(h->h_addr, &address, sizeof(unsigned long));

	return address;
}

- (void)reset
{
	int i;

	for (i = 0; i < 4; i++)
	{
		port[i] = 0;
		lastTime[i] = 0;
		isDead[i][0] = NO;
		isDead[i][1] = NO;
	}
}

- (unsigned int)getNFSPort:(unsigned short *)p
	version:(unsigned int)v
	protocol:(unsigned int)proto
{
	struct sockaddr_in sin;
	struct timeval tv;
	CLIENT *cl;
	int i, s, delta, px;
	unsigned long addr;
	BOOL mort;

	if (!((v == 2) || (v == 3))) return ERPCMISMATCH;

	gettimeofday(&tv, NULL);
	delta = tv.tv_sec - lastTime[v];
	if (delta > LIFE_LATENCY) port[v] = 0;

	if (port[v] != 0)
	{
		*p = port[v];
		return 0;
	}

	if (mountClient[v] != NULL)
	{
		cl = (CLIENT *)mountClient[v];
		auth_destroy(cl->cl_auth);
		clnt_destroy(cl);
	}

	mountClient[v] = NULL;

	px = 0;
	if (proto == IPPROTO_TCP) px = 1;
	mort = isDead[v][px];

	if (mort && (delta < DEATH_LATENCY))
	{
		sys_msg(debug, LOG_INFO,
			"Assuming %s NFS_V%d/%s is still unavailable (%d seconds remain)",
			[myname value], v, (px == 0) ? "UDP" : "TCP", DEATH_LATENCY - delta);
		return EHOSTDOWN;
	}

	isDead[v][px] = NO;

	addr = [self address];
	if (addr == 0)
	{
		sys_msg(debug, LOG_ERR, "Can't find address for %s", [myname value]);
		isDead[v][px] = YES;
		return 0;
	}
	
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	
	sin.sin_port = 0;

	port[v] = getport(&sin, NFS_PROGRAM, v, proto);

	lastTime[v] = tv.tv_sec;

	if (port[v] == 0)
	{
		sys_msg(debug, LOG_NOTICE, "Can't get NFS_V%d/%s port for %s",
			v, (proto == IPPROTO_TCP) ? "TCP" : "UDP", [myname value]);
		isDead[v][px] = YES;

		if (rpc_createerr.cf_stat == RPC_PMAPFAILURE)
		{
			for (i = 0; i < 4; i++)
			{
				isDead[i][0] = YES;
				isDead[i][1] = YES;
			}
			return EHOSTDOWN;
		}
		return rpcerr2errno(rpc_createerr.cf_stat, ECONNREFUSED);
	}

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;

	tv.tv_sec = timeout / pings;
	if (tv.tv_sec == 0) tv.tv_sec = 1;
	tv.tv_usec = 0;

	s = RPC_ANYSOCK;

	cl = NULL;

	/*
	 * Use MOUNTPROG V1 for NFS V2.
	 * Use MOUNTPROG V3 for NFS V3.
	 */
	i = 1;
	if (v == 3) i = 3;
	
	if (proto == IPPROTO_TCP)
		cl = clnttcp_create(&sin, MOUNTPROG, i, &s, 0, 0);
	else
		cl = clntudp_create(&sin, MOUNTPROG, i, tv, &s);

	if (cl == NULL)
	{
		sys_msg(debug, LOG_ERR, "Can't create MOUNTPROG client for server %s: %s",
			inet_ntoa(sin.sin_addr), clnt_spcreateerror("clntudp_create"));
		return rpc_createerr.cf_error.re_errno;
	}

	cl->cl_auth = authunix_create_default();
	mountClient[v] = (void *)cl;

	*p = port[v];
	return 0;
}

- (unsigned int)getHandle:(void *)fh
	size:(int *)s
	port:(unsigned short *)p
	forFile:(String *)filename
	version:(unsigned int)v
	protocol:(unsigned int)proto
{
	int status;
	struct timeval tv;
	char *dir;
	fhstatus mount_res_2;
	mountres3 mount_res_3;
	unsigned int mount_status;
	CLIENT *cl;
	struct rpc_err rpcerr;
	
	if (!((v == 2) || (v == 3))) return ERPCMISMATCH;

	status = [self getNFSPort:p version:v protocol:proto];
	if (status != 0) return status;

	tv.tv_sec = timeout;
	if (tv.tv_sec == 0) tv.tv_sec = 1;
	tv.tv_usec = 0;

	status = RPC_FAILED;
	cl = (CLIENT *)mountClient[v];
	if (cl == NULL) return status;

	mount_status = (unsigned int)-1;
	dir = [filename value];

	memset(&mount_res_2, 0, sizeof(fhstatus));
	memset(&mount_res_3, 0, sizeof(mountres3));

	if (v == 2)
	{
		status = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath, &dir,
			xdr_fhstatus, &mount_res_2, tv);
		if (status == RPC_SUCCESS) mount_status = mount_res_2.fhs_status;
	}
	else
	{
		status = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath, &dir,
			xdr_mountres3, &mount_res_3, tv);
		if (status == RPC_SUCCESS) mount_status = mount_res_3.fhs_status;
	}

	if (status != RPC_SUCCESS)
	{
		sys_msg(debug, LOG_ERR, "RPC mount version %d %s:%s status: %s",
			v, [myname value], [filename value], clnt_sperrno(status));
		clnt_geterr(cl, &rpcerr);
		return rpcerr2errno(rpcerr.re_status, ECONNABORTED);
	}

	if (mount_status != 0)
	{
		sys_msg(debug, LOG_ERR, "mount (NFSV%d) %s:%s - %s",
			v, [myname value], [filename value], mnt_strerror(mount_status));
		if (v == 2)
		{
			(void)clnt_freeres(cl, xdr_fhstatus, &mount_res_2);
		}
		else
		{
			(void)clnt_freeres(cl, xdr_mountres3, &mount_res_3);
		}	
		return mount_status;
	}

	if (v == 2)
	{
		*s = FHSIZE;
		memmove(fh, mount_res_2.fhstatus_u.fhs_fhandle, *s);
		(void)clnt_freeres(cl, xdr_fhstatus, &mount_res_2);
	}
	else
	{
		*s = mount_res_3.mountres3_u.mountinfo.fhandle.fhandle3_len;
		memmove(fh, mount_res_3.mountres3_u.mountinfo.fhandle.fhandle3_val, *s);
		(void)clnt_freeres(cl, xdr_mountres3, &mount_res_3);
	}	

	return 0;
}

- (void)dealloc
{
	CLIENT *cl;
	int i;

	for (i = 0; i < 4; i++)
	{
		if (mountClient[i] != NULL)
		{
			cl = (CLIENT *)mountClient[i];
			auth_destroy(cl->cl_auth);
			clnt_destroy(cl);
		}
	}

	[super dealloc];
}

@end
