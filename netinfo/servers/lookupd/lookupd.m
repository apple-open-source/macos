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
 * lookupd.m
 *
 * lookupd is a proxy server for all local and network information and
 * directory services.  It is called by various routines in the System
 * framework (e.g. gethostbyname()).  Using (configurable) search 
 * policies for each category of item (e.g. users, printers), lookupd
 * queries information services on behalf of the calling client.
 * Caching and negative record machanisms are used to improve ovarall
 * system performance.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 *
 * Designed and written by Marc Majka
 */

#import <NetInfo/config.h>
#import <NetInfo/system_log.h>
#import <NetInfo/project_version.h>
#import <objc/objc-runtime.h>
#import <stdio.h>
#import "Config.h"
#import "Controller.h"
#import "Thread.h"
#import "LUDictionary.h"
#import "MemoryWatchdog.h"
#import "sys.h"
#import <NetInfo/dns.h>
#import <NetInfo/dsutil.h>
#import <sys/file.h>
#import <sys/types.h>
#import <rpc/types.h>
#import <rpc/xdr.h>
#import <sys/ioctl.h>
#import <sys/resource.h>
#import <sys/signal.h>
#import <sys/wait.h>
#import <unistd.h>
#import <sys/time.h>
#import <sys/resource.h>
#import <signal.h>
#import "_lu_types.h"

#define forever for (;;)

extern int getppid(void);
extern void interactive(FILE *, FILE*);

extern int _lookup_link();
extern int _lookup_one();
extern int _lookup_all();

#ifdef _UNIX_BSD_43_
#define PID_FILE "/etc/lookupd.pid"
#define EXE_FILE "/usr/etc/lookupd"
#else
#define PID_FILE "/var/run/lookupd.pid"
#define EXE_FILE "/usr/sbin/lookupd"
#endif

static BOOL debugMode;

/*
 * GLOBALS - see LUGlobal.h
 */
id controller = nil;
id configManager = nil;
id statistics = nil;
id cacheAgent = nil;
id machRPC = nil;
id rover = nil;
syslock *rpcLock = NULL;
syslock *statsLock = NULL;
char *portName = NULL;
sys_port_type server_port = SYS_PORT_NULL;
#ifdef _SHADOW_
sys_port_type server_port_privileged = SYS_PORT_NULL;
sys_port_type server_port_unprivileged = SYS_PORT_NULL;
BOOL shadow_passwords = NO;
#endif

/* Controller.m uses this global */
BOOL shutting_down = NO;

static int configSource = configSourceAutomatic;
static char *configPath = NULL;
static char *configDomain = NULL;

#define LONG_STRING_LENGTH 8192

static void
writepid(void)
{
	FILE *fp;

	fp = fopen(PID_FILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

static void
closeall(void)
{
	int i;

	for (i = getdtablesize() - 1; i >= 0; i--) close(i);

	open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}

static void
detach(void)
{
#ifdef _UNIX_BSD_43_
	int ttyfd;
#endif

	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

#ifdef _UNIX_BSD_43_
	ttyfd = open("/dev/tty", O_RDWR, 0);
	if (ttyfd > 0)
	{
		ioctl(ttyfd, TIOCNOTTY, NULL);
		close(ttyfd);
	}

	setpgrp(0, getpid());
#else
	if (setsid() < 0) system_log(LOG_ERR, "lookupd: setsid() failed: %m");
#endif
}

void
parentexit(int x)
{
	exit(0);
}

void
goodbye(int x)
{
	exit(1);
}

void
handleSIGHUP()
{
	Thread *t;

	/* Ignore HUP if already restarting */
	if (shutting_down) return;

	system_log(LOG_ERR, "Caught SIGHUP - restarting");
	shutting_down = YES;

	t = [[Thread alloc] init];
	[t setName:"Knock Knock"];
	[t setState:ThreadStateActive];
	[t shouldTerminate:YES];
	[t run:@selector(lookupdMessage) context:controller];
}

static void
lookupd_startup()
{
	Thread *t;
	BOOL status;
	struct timeval tv;
	char *name;

	gettimeofday(&tv, NULL);
	srandom((getpid() << 10) + tv.tv_usec);

	rover = [[MemoryWatchdog alloc] init];

	t = [Thread currentThread];
	[t setState:ThreadStateActive];

	rpcLock = syslock_new(0);
	statsLock = syslock_new(0);

	configManager = [[Config alloc] init];
	status = [configManager setConfigSource:configSource path:configPath domain:configDomain];

	cacheAgent = [[CacheAgent alloc] init];

	name = portName;
	if (portName == NULL) name = DefaultName;
	
	system_log_open(name, (LOG_NOWAIT | LOG_PID), LOG_NETINFO, NULL);

	controller = [[Controller alloc] initWithName:portName];

	if (!status)
	{
		if (debugMode)
		{
			fprintf(stderr, "WARNING: configuration initialization failed\n");
			fprintf(stderr, "using default configuration\n");
		}
		else
		{
			system_log(LOG_ERR, "configuration initialization failed: using defaults");
		}
	}
}

static void
lookupd_shutdown(int status)
{
	[controller release];

	[cacheAgent release];
	cacheAgent = nil;

	dns_shutdown();

	[configManager release];
	configManager = nil;

	system_log(LOG_NOTICE, "lookupd exiting");
	
	syslock_free(rpcLock);
	rpcLock = NULL;

	syslock_free(statsLock);
	statsLock = NULL;

	[Thread shutdown];

	[rover release];

	exit(status);
}

/*
 * Restart everything.
 */
void
restart()
{
#ifdef _SHADOW_
	char *Argv[7], portstr2[32];
#else
	char *Argv[5];
#endif
	char pidstr[32], portstr1[32];
	int pid;

	if (debugMode) lookupd_shutdown(0);

	system_log(LOG_NOTICE, "Restarting lookupd");

#ifdef _SHADOW_
	sprintf(pidstr,  "%d", getpid());
	sprintf(portstr1, "%d", server_port_unprivileged);
	sprintf(portstr2, "%d", server_port_privileged);

	Argv[0] = "lookupd";
	Argv[1] = "-r";
	Argv[2] = portstr1;
	Argv[3] = portstr2;
	Argv[4] = pidstr;
	Argv[5] = shadow_passwords ? NULL : "-u";
	Argv[6] = NULL;
#else
	sprintf(pidstr,  "%d", getpid());
	sprintf(portstr1, "%d", server_port);

	Argv[0] = "lookupd";
	Argv[1] = "-r";
	Argv[2] = portstr1;
	Argv[3] = pidstr;
	Argv[4] = NULL;
#endif

	pid = fork();
	if (pid > 0)
	{
		signal(SIGTERM, parentexit);
		forever [[Thread currentThread] sleep:1];
	}

	execv(EXE_FILE, Argv);
}

int
print_dictionary(XDR *inxdr)
{
	int i, nkeys, j, nvals;
	char *str;

	if (!xdr_int(inxdr, &nkeys))
	{
		fprintf(stderr, "xdr decoding error!\n");
		return -1;
	}

	for (i = 0; i < nkeys; i++)
	{
		str = NULL;
		if (!xdr_string(inxdr, &str, LONG_STRING_LENGTH))
		{
			fprintf(stderr, "xdr error decoding key\n");
			return -1;
		}

		printf("%s:", str);
		free(str);

		if (!xdr_int(inxdr, &nvals))
		{
			fprintf(stderr, "xdr error decoding value list length\n");
			return -1;
		}

		for (j = 0; j < nvals; j++)
		{
			str = NULL;
			if (!xdr_string(inxdr, &str, LONG_STRING_LENGTH))
			{
				fprintf(stderr, "xdr error decoding value\n");
				return -1;
			}
		
			printf(" %s", str);
			free(str);
		}
		printf("\n");
	}

	return 0;
}

void
query_util(char *str)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	unit lookup_buf[MAX_INLINE_UNITS * BYTES_PER_XDR_UNIT];
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i;
	kern_return_t status;

	server_port = lookupd_port(portName);

	status = KERN_SUCCESS;

	if (streq(str, "-statistics"))
	{
		status = _lookup_link(server_port, "_getstatistics", &proc);
	}
	else if (streq(str, "-flushcache"))
	{
		status = _lookup_link(server_port, "_invalidatecache", &proc);
	}
	else return;

	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find lookup procedure\n");
		return;
	}
	
	server_port = lookupd_port(portName);
	
	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);
	
	if (!xdr__lu_string(&outxdr, &str))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	datalen = MAX_INLINE_UNITS * BYTES_PER_XDR_UNIT;

	if (_lookup_one(server_port, proc, (unit *)databuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);


	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		fprintf(stderr, "xdr decoding error!\n");
		return;
	}

	for (i = 0; i < n; i++)
	{
		printf("\n");
		print_dictionary(&inxdr);
	}
	if (n > 0) printf("\n");

	xdr_destroy(&inxdr);
}

void
query_find(char *cat, char *key, char *val)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	unit lookup_buf[MAX_INLINE_UNITS * BYTES_PER_XDR_UNIT];
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i;
	kern_return_t status;

	server_port = lookupd_port(portName);

	status = KERN_SUCCESS;

	status = _lookup_link(server_port, "find", &proc);

	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find lookup procedure\n");
		return;
	}
	
	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);
	
	if (!xdr__lu_string(&outxdr, &cat))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	datalen = MAX_INLINE_UNITS * BYTES_PER_XDR_UNIT;

	if (!xdr__lu_string(&outxdr, &key))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	if (!xdr__lu_string(&outxdr, &val))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	if (_lookup_one(server_port, proc, (unit *)databuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);


	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		fprintf(stderr, "xdr decoding error!\n");
		return;
	}

	for (i = 0; i < n; i++)
	{
		printf("\n");
		print_dictionary(&inxdr);
	}
	if (n > 0) printf("\n");

	xdr_destroy(&inxdr);
}

void
query_query(int argc, char *argv[])
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char *listbuf;
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i, j, na;
	kern_return_t status;
	char *k;

	/* check the "-a" options */

	na = 1;
	n = argc - 1;
	for (i = 2; i < argc; i++)
	{
		if (streq(argv[i], "-a"))
		{
			if (i == n)
			{
				/* trailing empty "-a" */
				fprintf(stderr, "trailing -a option without a key\n");
				fprintf(stderr, "usage: lookupd -q category [[-a key] val ...] ...\n");
				return;
			}
			else if (streq(argv[i+1], "-a"))
			{
				/* empty "-a" */
				fprintf(stderr, "-a option without a key\n");
				fprintf(stderr, "usage: lookupd -q category [[-a key] val ...] ...\n");
				return;
			}

			na++;
		}
		else if (i == 2)
		{
			/* no leading "-a" */
			fprintf(stderr, "no leading -a option\n");
			fprintf(stderr, "usage: lookupd -q category [[-a key] val ...] ...\n");
			return;
		}
	}

	server_port = lookupd_port(portName);
	status = _lookup_link(server_port, "query", &proc);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find query procedure\n");
		return;
	}

	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);

	/* Encode attribute count */
	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	/* Encode "_lookup_category" attribute */
	k = copyString("_lookup_category");
	if (!xdr__lu_string(&outxdr, &k))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}
	free(k);
	n = 1;
	if (!xdr_int(&outxdr, &n))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}
	if (!xdr__lu_string(&outxdr, &argv[1]))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	for (i = 2; i < argc; i++)
	{
		if (streq(argv[i], "-a"))
		{
			i++;
			if (!xdr__lu_string(&outxdr, &argv[i]))
			{
				xdr_destroy(&outxdr);
				fprintf(stderr, "xdr encoding error!\n");
				return;
			}
			n = 0;
			for (j = i + 1; (j < argc) && strcmp(argv[j], "-a"); j++) n++;
			if (!xdr_int(&outxdr, &n))
			{
				xdr_destroy(&outxdr);
				fprintf(stderr, "xdr encoding error!\n");
				return;
			}
			
		}
		else if (!xdr__lu_string(&outxdr, &argv[i]))
		{
			xdr_destroy(&outxdr);
			fprintf(stderr, "xdr encoding error!\n");
			return;
		}
	}

	listbuf = NULL;
	datalen = 0;

	if (_lookup_all(server_port, proc, (unit *)databuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &listbuf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "query failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

#ifdef _IPC_TYPED_
	datalen *= BYTES_PER_XDR_UNIT;
#endif
	
	xdrmem_create(&inxdr, listbuf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		fprintf(stderr, "xdr decoding error!\n");
		return;
	}

	for (i = 0; i < n; i++)
	{
		printf("\n");
		print_dictionary(&inxdr);
	}
	if (n > 0) printf("\n");

	xdr_destroy(&inxdr);

	vm_deallocate(sys_task_self(), (vm_address_t)listbuf, datalen);
}

void
query_list(char *str)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	char *listbuf;
	int n, i, listproc, encode_len;
	kern_return_t status;

	server_port = lookupd_port(portName);

	status = KERN_SUCCESS;
	listproc = 0;
	encode_len = 0;

	status = _lookup_link(server_port, "list", &proc);
	listproc = 1;

	if (streq(str, "-configuration")) str = "config";

	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find lookup procedure\n");
		return;
	}
	
	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);
	
	if (!xdr__lu_string(&outxdr, &str))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	datalen = MAX_INLINE_UNITS * BYTES_PER_XDR_UNIT;

	listbuf = NULL;
	datalen = 0;

	if (_lookup_all(server_port, proc, (unit *)databuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &listbuf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}	

	xdr_destroy(&outxdr);

#ifdef _IPC_TYPED_
	datalen *= BYTES_PER_XDR_UNIT;
#endif

	xdrmem_create(&inxdr, listbuf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		fprintf(stderr, "xdr decoding error!\n");
		return;
	}

	for (i = 0; i < n; i++)
	{
		printf("\n");
		print_dictionary(&inxdr);
	}
	if (n > 0) printf("\n");

	xdr_destroy(&inxdr);

	vm_deallocate(sys_task_self(), (vm_address_t)listbuf, datalen);
}

int
main(int argc, char *argv[])
{
	int i, pid, qp, fp;
	BOOL restarting;
	BOOL customName;
	struct rlimit rlim;
	sys_task_port_type old_lu;
#ifdef _SHADOW_
	sys_port_type old_port_privileged;
	sys_port_type old_port_unprivileged;
#else
	sys_port_type old_port;
#endif

	objc_setMultithreaded(YES);
	
	pid = -1;
	restarting = NO;
	portName = DefaultName;
	debugMode = NO;
	customName = NO;

	server_port = SYS_PORT_NULL;
#ifdef _SHADOW_
	server_port_unprivileged = SYS_PORT_NULL;
	old_port_unprivileged = SYS_PORT_NULL;
	server_port_privileged = SYS_PORT_NULL;
	old_port_privileged = SYS_PORT_NULL;
	shadow_passwords = YES;
#else
	old_port = SYS_PORT_NULL;
#endif

	/* Clean up and re-initialize state on SIGHUP */
	signal(SIGHUP, handleSIGHUP);

	qp = -1;
	fp = -1;

	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-q")) qp = i;

		else if (streq(argv[i], "-f")) fp = i;

		else if (streq(argv[i], "-a"))
		{
			if (qp < 0)
			{
				fprintf(stderr, "usage: lookupd -q category [-a key [val ...]] ...\n");
				exit(1);
			}
		}

		else if (streq(argv[i], "-flushcache")) qp = i - 1;

		else if (streq(argv[i], "-statistics")) qp = i - 1;

		else if (streq(argv[i], "-configuration")) qp = i - 1;

		else if (streq(argv[i], "-d"))
		{
			debugMode = YES;
			portName = NULL;
		}

		else if (streq(argv[i], "-D"))
		{
			debugMode = YES;
			customName = YES;
			if (((argc - i) - 1) < 1) 
			{
				fprintf(stderr,"usage: lookupd -D name\n");
				exit(1);
			}
			portName = argv[++i];
			if (streq(portName, "-")) portName = NULL;
		}

		else if (streq(argv[i], "-r"))
		{
			if (((argc - i) - 1) < 2) 
			{
#ifdef _SHADOW_
				fprintf(stderr,"usage: lookupd -r unprivport privport pid\n");
#else
				fprintf(stderr,"usage: lookupd -r port pid\n");
#endif
				exit(1);
			}

			restarting = YES;

#ifdef _SHADOW_
			old_port_unprivileged = (sys_port_type)atoi(argv[++i]);
			old_port_privileged = (sys_port_type)atoi(argv[++i]);
#else
			old_port = (sys_port_type)atoi(argv[++i]);
#endif
			pid = atoi(argv[++i]);
		}

#ifdef _SHADOW_
		else if (streq(argv[i], "-u")) shadow_passwords = NO;
#endif

		else if (streq(argv[i], "-c"))
		{
			if (((argc - i) - 1) < 1) 
			{
				fprintf(stderr,"usage: lookupd -c source [[domain] path]\n");
				exit(1);
			}

			i++;
			if (streq(argv[i], "default")) configSource = configSourceDefault;
			else if (streq(argv[i], "configd")) configSource = configSourceConfigd;
			else if (streq(argv[i], "netinfo"))
			{
				configSource = configSourceNetInfo;
				if (((argc - i) - 2) < 1)
				{
					fprintf(stderr,"usage: lookupd -c netinfo domain path\n");
					exit(1);
				}

				configDomain = argv[++i];
				configPath = argv[++i];
			}
			else if (streq(argv[i], "file"))
			{
				configSource = configSourceFile;
				if (((argc - i) - 1) < 1)
				{
					fprintf(stderr,"usage: lookupd -c file path\n");
					exit(1);
				}

				configPath = argv[++i];
			}
			else
			{
				fprintf(stderr, "Unknown config source.  Must be one of:\n");
				fprintf(stderr, "    default\n");
				fprintf(stderr, "    configd\n");
				fprintf(stderr, "    netinfo \n");
				fprintf(stderr, "    file\n");
				exit(1);
			}
		}

		else if ((qp < 0) && (fp < 0))
		{
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			exit(1);
		}
	}

	if (qp >= 0)
	{
		i = (argc - qp) - 1;
		if (i == 0)
		{
			fprintf(stderr, "usage: lookupd -q category [-a key [val ...]] ...\n");
			exit(1);
		}

		if (portName == NULL)
		{
			fprintf(stderr, "Can't query without a port\n");
			exit(1);
		}

		if (i == 1)
		{
			if (streq(argv[qp + 1], "-statistics")) query_util(argv[qp + 1]);
			else if (streq(argv[qp + 1], "-flushcache")) query_util(argv[qp + 1]);
			else query_list(argv[qp + 1]);
		}
		else query_query(argc - qp, argv + qp);
		exit(0);
	}

	if (fp >= 0)
	{
		i = (argc - fp) - 1;
		if (i != 3)
		{
			fprintf(stderr, "usage: lookupd -f category key val\n");
			exit(1);
		}

		if (portName == NULL)
		{
			fprintf(stderr, "Can't find without a port\n");
			exit(1);
		}

		query_find(argv[fp + 1], argv[fp + 2], argv[fp + 3]);
		exit(0);
	}

	if (restarting && debugMode)
	{
		fprintf(stderr, "Can't restart in debug mode\n");
		exit(1);
	}

	if ((!restarting) && (lookupd_port(portName) != SYS_PORT_NULL))
	{
		if (debugMode)
		{
			if (customName)
			{
				fprintf(stderr, "lookupd -D %s is already running!\n",
					portName);
			}
			else
			{
				fprintf(stderr, "lookupd -d is already running!\n");
			}
		}
		else
		{
			fprintf(stderr, "lookupd is already running!\n");
			system_log(LOG_ERR, "lookupd is already running!\n");
		}
		exit(1);
	}

	if (debugMode)
	{
		lookupd_startup();
		if (controller == nil)
		{
			fprintf(stderr, "controller didn't init!\n");
			exit(1);
		}

		printf("lookupd version %s (%s)\n", _PROJECT_VERSION_, _PROJECT_BUILD_INFO_);
		if (portName != NULL) [controller startServerThread];
		interactive(stdin, stdout);
		shutting_down = YES;

		lookupd_shutdown(0);
	}

	if (restarting)
	{
		if (sys_task_for_pid(sys_task_self(), pid, &old_lu) != KERN_SUCCESS)
		{
			system_log(LOG_EMERG, "Can't get port for PID %d", pid);
			exit(1);
		}
#ifdef _SHADOW_
		if (sys_port_extract_receive_right(old_lu, old_port_unprivileged, &server_port_unprivileged)
			!= KERN_SUCCESS || 
		    sys_port_extract_receive_right(old_lu, old_port_privileged, &server_port_privileged)
			!= KERN_SUCCESS || 
		    port_set_allocate(task_self(), &server_port)
			!= KERN_SUCCESS || 
		    port_set_add(task_self(), server_port, server_port_unprivileged)
			!= KERN_SUCCESS || 
		    port_set_add(task_self(), server_port, server_port_privileged)
			!= KERN_SUCCESS)
#else
		if (sys_port_extract_receive_right(old_lu, old_port, &server_port)
			!= KERN_SUCCESS)
#endif
		{
			system_log(LOG_EMERG, "Can't grab port rights");
			kill(pid, SIGKILL);
			exit(1);
		}
	}
	else
	{
		pid = fork();
		if (pid > 0)
		{
			signal(SIGTERM, parentexit);
			forever sleep(1);
		}

		detach();
	}

	closeall();

	if (!debugMode) writepid();

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
	signal(SIGTERM, goodbye);

	lookupd_startup();
	if (controller == nil)
	{
		system_log(LOG_EMERG, "controller didn't init!");
		kill(getppid(), SIGTERM);
		exit(1);
	}

	kill(getppid(), SIGTERM);

	[controller serverLoop];

	system_log(LOG_DEBUG, "serverLoop ended");

	/* We get here if the sighup handler got hit. */
	restart();

	lookupd_shutdown(-1);
	exit(-1);
}
