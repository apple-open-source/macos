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

/*
 * MIG server procedure implementation
 * Copyright (C) 1989 by NeXT, Inc.
 */

#import <rpc/types.h>
#import <rpc/xdr.h>
#import <netinfo/lookup_types.h>
#import <NetInfo/system_log.h>
#import "MachRPC.h"
#import "LUPrivate.h"
#import "Thread.h"

typedef struct lookup_node
{
	char *name;
	int returns_list;
} lookup_node;

static lookup_node _lookup_links[] =
{ 
	{ "getpwent", 1 },
	{ "getpwent_A", 1 },

	{ "getpwuid", 0 },
	{ "getpwuid_A", 0 },

	{ "getpwnam", 0 },
	{ "getpwnam_A", 0 },

	{ "setpwent", 1 },

	{ "getgrent", 1 },
	{ "getgrgid", 0 },
	{ "getgrnam", 0 },
	{ "initgroups", 0 },

	{ "gethostent", 1 },
	{ "gethostbyname", 0 },
	{ "gethostbyaddr", 0 },

	{ "getipv6nodebyname", 0 },
	{ "getipv6nodebyaddr", 0 },

	{ "getnetent", 1 },
	{ "getnetbyname", 0 },
	{ "getnetbyaddr", 0 },

	{ "getservent", 1 },
	{ "getservbyname", 0 },
	{ "getservbyport", 0 },

	{ "getprotoent", 1 },
	{ "getprotobyname", 0 },
	{ "getprotobynumber", 0 },

	{ "getrpcent", 1 },
	{ "getrpcbyname", 0 },
	{ "getrpcbynumber", 0 },

	{ "getfsent", 1 },
	{ "getfsbyname", 0 },

	{ "prdb_get", 1 },
	{ "prdb_getbyname", 0 },

	{ "bootparams_getent", 1 },
	{ "bootparams_getbyname", 0 },

	{ "bootp_getbyip", 0 },
	{ "bootp_getbyether", 0 },

	{ "alias_getbyname", 0 },
	{ "alias_getent", 1 },
	{ "alias_setent", 1 },

	{ "innetgr", 0 },
	{ "getnetgrent", 1 },

	{ "find", 0 },
	{ "list", 1 },
	{ "query", 1 },

	{ "checksecurityopt", 0 },
	{ "checknetwareenbl", 0 },
	{ "setloginuser", 0 },
	{ "_getstatistics", 0 },
	{ "_invalidatecache", 0 },
	{ "_suspend", 0 },

	{ "dns_proxy", 0 },
	
	{ "getaddrinfo", 0 },
	{ "getnameinfo", 0 }
};

#define LOOKUP_NPROCS  (sizeof(_lookup_links)/sizeof(_lookup_links[0]))

char *proc_name(int procno)
{
	if ((procno < 0) || (procno >= LOOKUP_NPROCS))
	{
		return "-UNKNOWN-";
	}
	return _lookup_links[procno].name;
}

kern_return_t __lookup_link
(
	sys_port_type server,
	lookup_name name,
	int *procno
)
{
	int i;

	for (i = 0; i < LOOKUP_NPROCS; i++)
	{
		if (!strcmp(name, _lookup_links[i].name))
		{
			*procno = i;
#ifdef DEBUG
			system_log(LOG_DEBUG, "_lookup_link(%s) = %d", name, i);
#endif
			return KERN_SUCCESS;
		}
	}

#ifdef DEBUG
	system_log(LOG_NOTICE, "_lookup_link(%s) failed", name);
#endif
	return KERN_FAILURE;
}

kern_return_t __lookup_all
(
	sys_port_type server,
	int procno,
	inline_data indata,
	unsigned inlen,
	ooline_data *outdata,
	unsigned *outlen
)
{
	BOOL status;
	kern_return_t kstatus;
	char *replybuf;
	char *vmbuffer;
	unsigned int replylen;
	Thread *t;
	
#ifdef DEBUG
	system_log(LOG_DEBUG, "_lookup_all[%d]", procno);
#endif

	if (procno < 0 || procno >= LOOKUP_NPROCS)
	{
		system_log(LOG_NOTICE, "_lookup_all[%d] unknown procedure", procno);
		return KERN_FAILURE;
	}

	t = [Thread currentThread];
	[t setData:NULL];
	[t setDataLen:0];

	inlen *= BYTES_PER_XDR_UNIT;

	replybuf = NULL;
	replylen = 0;

	*outlen = 0;

	status = [machRPC process:procno
		inData:(char *)indata
		inLength:inlen
		outData:&replybuf
		outLength:&replylen];

	if (!status)
	{
#ifdef DEBUG
		system_log(LOG_NOTICE, "_lookup_all(%s) failed", proc_name(procno));
#endif
		return KERN_FAILURE;
	}

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmbuffer, replylen, TRUE);
	if (kstatus != KERN_SUCCESS) return kstatus;

	[t setData:vmbuffer];
	[t setDataLen:replylen];

	memmove(vmbuffer, replybuf, replylen);
	free(replybuf);

	*outdata = vmbuffer;
	*outlen = replylen / BYTES_PER_XDR_UNIT;

	return KERN_SUCCESS;
}

kern_return_t __lookup_one
(
	sys_port_type server,
	int procno,
	inline_data indata,
	unsigned inlen,
	inline_data outdata,
	unsigned *outlen
)
{
	BOOL status;
	char *replybuf;
	unsigned int replylen;

#ifdef DEBUG
	system_log(LOG_DEBUG, "_lookup_one[%d]", procno);
#endif

	if (procno < 0 || procno >= LOOKUP_NPROCS)
	{
		system_log(LOG_NOTICE, "_lookup_one[%d] unknown procedure", procno);
		return KERN_FAILURE;
	}

	if (_lookup_links[procno].returns_list)
	{
		system_log(LOG_NOTICE, "_lookup_one(%s) bad procedure type", proc_name(procno));
		return KERN_FAILURE;
	}

	inlen *= BYTES_PER_XDR_UNIT;

	replybuf = NULL;
	replylen = 0;

	status = [machRPC process:procno
		inData:(char *)indata
		inLength:inlen
		outData:&replybuf
		outLength:&replylen];

	if (!status)
	{
#ifdef DEBUG
		system_log(LOG_NOTICE, "_lookup_one(%s) failed", proc_name(procno));
#endif
		return KERN_FAILURE;
	}

	if ((replylen / BYTES_PER_XDR_UNIT) > (*outlen))
	{
		system_log(LOG_ERR, "_lookup_one(%s) reply buffer size %u, %u required", proc_name(procno), (*outlen), replylen / BYTES_PER_XDR_UNIT);
		free(replybuf);
		return KERN_FAILURE;
	}

	memmove(outdata, replybuf, replylen);
	free(replybuf);
	*outlen = replylen / BYTES_PER_XDR_UNIT;

	return KERN_SUCCESS;
}

kern_return_t __lookup_ooall
(
	sys_port_type server,
	int procno,
	ooline_data indata,
	unsigned inlen,
	ooline_data *outdata,
	unsigned *outlen
)
{
	BOOL status;
	kern_return_t kstatus;
	char *replybuf;
	char *vmbuffer;
	unsigned int replylen;

#ifdef DEBUG
	system_log(LOG_DEBUG, "_lookup_ooall[%d]", procno);
#endif

	if (procno < 0 || procno >= LOOKUP_NPROCS)
	{
		system_log(LOG_NOTICE, "_lookup_ooall[%d] unknown procedure", procno);
		vm_deallocate(sys_task_self(), (vm_address_t)indata, inlen * UNIT_SIZE);
		return KERN_FAILURE;
	}

	inlen *= BYTES_PER_XDR_UNIT;

	replybuf = NULL;
	replylen = 0;

	*outlen = 0;

	status = [machRPC process:procno
		inData:(char *)indata
		inLength:inlen
		outData:&replybuf
		outLength:&replylen];

	if (!status)
	{
#ifdef DEBUG
		system_log(LOG_NOTICE, "_lookup_all(%s) failed", proc_name(procno));
#endif
		return KERN_FAILURE;
	}

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmbuffer, replylen, TRUE);
	if (kstatus != KERN_SUCCESS) return kstatus;

	memmove(vmbuffer, replybuf, replylen);
	free(replybuf);

	*outdata = vmbuffer;
	*outlen = replylen / BYTES_PER_XDR_UNIT;

	return KERN_SUCCESS;
}

kern_return_t __lookup_link_secure
(
	sys_port_type server,
	lookup_name name,
	int *procno
)
{
	return __lookup_link(server, name, procno);
}

kern_return_t __lookup_all_secure
(
	sys_port_type server,
	int procno,
	inline_data indata,
	unsigned inlen,
	ooline_data *outdata,
	unsigned *outlen
)
{
	return __lookup_all(server, procno, indata, inlen, outdata, outlen);
}

kern_return_t __lookup_one_secure
(
	sys_port_type server,
	int procno,
	inline_data indata,
	unsigned inlen,
	inline_data outdata,
	unsigned *outlen
)
{
	return __lookup_one(server, procno, indata, inlen, outdata, outlen);
}

kern_return_t __lookup_ooall_secure
(
	sys_port_type server,
	int procno,
	ooline_data indata,
	unsigned inlen,
	ooline_data *outdata,
	unsigned *outlen
)
{
	return __lookup_ooall(server, procno, indata, inlen, outdata, outlen);
}
