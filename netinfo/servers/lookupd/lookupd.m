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
#import <notify.h>
#import <mach/mig_errors.h>
#import "_lu_types.h"

#define forever for (;;)

extern int getppid(void);
extern void interactive(FILE *, FILE*);

extern int _lookup_link();
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
BOOL debug_enabled = NO;
BOOL statistics_enabled = NO;
BOOL coredump_enabled = NO;
BOOL parallel_gai = YES;
BOOL lookup_local_interfaces = YES;

/* Controller.m uses this global */
BOOL shutting_down = NO;

static int configSource = configSourceAutomatic;
static char *configPath = NULL;
static char *configDomain = NULL;

static int max_priority = -1;

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
	if (setsid() < 0)
	{
		system_log(LOG_ERR, "lookupd: setsid() failed: %m");
	}
#endif
}

void
goodbye(int x)
{
	_exit(0);
}

static void
lookupd_startup()
{
	Thread *t;
	BOOL status;
	char *name;
	LUArray *config;
	LUDictionary *cglobal;
	int rf, nctoken;
	uint32_t x;

	rf = open("/dev/random", O_RDONLY, 0);
	if (rf >= 0)
	{
		read(rf, &x, sizeof(uint32_t));
		close(rf);
	}
	else
	{
		x = (getpid() << 10) + time(NULL);
	}

	srandom(x);


	rover = [[MemoryWatchdog alloc] init];

	t = [Thread currentThread];
	[t setState:ThreadStateActive];

	rpcLock = syslock_new(0);
	syslock_set_name(rpcLock, RPCLockName);

	statsLock = syslock_new(0);
	syslock_set_name(statsLock, StatsLockName);

	configManager = [[Config alloc] init];
	status = [configManager setConfigSource:configSource path:configPath domain:configDomain];

	if (max_priority == -1)
	{
		config = [configManager config];
		cglobal = [configManager configGlobal:config];
		max_priority = [configManager intForKey:"LogMaxPriority" dict:cglobal default:-1];
		[config release];
	}

	cacheAgent = [[CacheAgent alloc] init];

	name = NULL;
	if (portName == NULL)
	{
		if (debugMode) name = strdup("lookupd-debug");
		else name = strdup("lookupd");
	}
	else
	{
		if (debugMode)
		{
			name = malloc(strlen(portName) + 16);
			sprintf(name, "lookupd.%s", portName);
		}
		else name = strdup("lookupd");
	}
	
	system_log_open(name, (LOG_NOWAIT | LOG_PID), LOG_NETINFO, NULL);
	if (name != NULL) free(name);

	if (max_priority != -1) system_log_set_max_priority(max_priority);

	controller = [[Controller alloc] initWithName:portName];

	signal(SIGHUP, SIG_DFL);

	nctoken = -1;
	notify_register_signal(NETWORK_CHANGE_NOTIFICATION, SIGHUP, &nctoken);

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

	[configManager release];
	configManager = nil;

	system_log(LOG_NOTICE, "lookupd exiting");

	syslock_set_name(rpcLock, NULL);
	syslock_free(rpcLock);
	rpcLock = NULL;

	syslock_set_name(statsLock, NULL);
	syslock_free(statsLock);
	statsLock = NULL;

	[Thread shutdown];

	[rover release];

	exit(0);
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
	char *lookup_buf;
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

	datalen = 0;
	lookup_buf = NULL;

	status = _lookup_all(server_port, proc, (unit *)databuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0))
	{
		fprintf(stderr, "lookup returned NULL!\n");
		return;
	}

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);


	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
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
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
}

void
query_find(char *cat, char *key, char *val)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char *lookup_buf;
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

	datalen = 0;
	lookup_buf = NULL;

	status = _lookup_all(server_port, proc, (unit *)databuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0))
	{
		fprintf(stderr, "lookup returned NULL!\n");
		return;
	}

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
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
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
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
	int n, i, j, na, cat;
	kern_return_t status;
	char *k, str[16], *procname;

	procname = "query";

	/* check category */
	cat = -1;
	if (argv[1][0] != '-')
	{
		cat = [LUAgent categoryWithName:argv[1]];
		if ((cat < 0) || (cat > NCATEGORIES))
		{
			fprintf(stderr, "invalid category %s\n", argv[1]);
			return;
		}
	}
	else procname = argv[1] + 1;

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
	status = _lookup_link(server_port, procname, &proc);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find %s procedure\n", procname);
		return;
	}

	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);

	if (cat == -1) na--;

	/* Encode attribute count */
	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "xdr encoding error!\n");
		return;
	}

	if (cat != -1)
	{ 
		/* Encode "_lookup_category" attribute */
		k = copyString("_lookup_category");
		if (!xdr__lu_string(&outxdr, &k))
		{
			xdr_destroy(&outxdr);
			fprintf(stderr, "xdr encoding error!\n");
			free(k);
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

		sprintf(str, "%d", cat);
		k = str;
		if (!xdr__lu_string(&outxdr, &k))
		{
			xdr_destroy(&outxdr);
			fprintf(stderr, "xdr encoding error!\n");
			return;
		}
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

	status = _lookup_all(server_port, proc, (unit *)databuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &listbuf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "query failed!\n");
		return;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	
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
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "can't find lookup procedure\n");
		return;
	}

	listproc = 1;

	if (streq(str, "-configuration")) str = "config";

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

	status = _lookup_all(server_port, proc, (unit *)databuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &listbuf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		fprintf(stderr, "lookup failed!\n");
		return;
	}	

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;

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
	int i, pid, qp, fp, status;
	BOOL customName, initialStartup;
	struct rlimit rlim;
	sys_port_type old_port;

	objc_setMultithreaded(YES);
	
	pid = -1;
	portName = DefaultName;
	debugMode = NO;
	customName = NO;
	initialStartup = NO;

	server_port = SYS_PORT_NULL;
	old_port = SYS_PORT_NULL;

	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, goodbye);

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
				exit(0);
			}
		}

		else if (streq(argv[i], "-startup")) initialStartup = YES;

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
				exit(0);
			}
			portName = argv[++i];
			if (streq(portName, "-")) portName = NULL;
		}

		else if (streq(argv[i], "-l"))
		{
			if (((argc - i) - 1) < 1) 
			{
				fprintf(stderr,"usage: lookupd -l max_syslog_priority\n");
				exit(0);
			}
			max_priority = atoi(argv[++i]);
		}

		else if (streq(argv[i], "-c"))
		{
			if (((argc - i) - 1) < 1) 
			{
				fprintf(stderr,"usage: lookupd -c source [[domain] path]\n");
				exit(0);
			}

			i++;
			if (streq(argv[i], "default")) configSource = configSourceDefault;
			else if (streq(argv[i], "netinfo"))
			{
				configSource = configSourceNetInfo;
				if (((argc - i) - 2) < 1)
				{
					fprintf(stderr,"usage: lookupd -c netinfo domain path\n");
					exit(0);
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
					exit(0);
				}

				configPath = argv[++i];
			}
			else
			{
				fprintf(stderr, "Unknown config source.  Must be one of:\n");
				fprintf(stderr, "    default\n");
				fprintf(stderr, "    netinfo \n");
				fprintf(stderr, "    file\n");
				exit(0);
			}
		}

		else if ((qp < 0) && (fp < 0))
		{
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			exit(0);
		}
	}

	if (qp >= 0)
	{
		i = (argc - qp) - 1;
		if (i == 0)
		{
			fprintf(stderr, "usage: lookupd -q category [-a key [val ...]] ...\n");
			exit(0);
		}

		if (portName == NULL)
		{
			fprintf(stderr, "Can't query without a port\n");
			exit(0);
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
			exit(0);
		}

		if (portName == NULL)
		{
			fprintf(stderr, "Can't find without a port\n");
			exit(0);
		}

		query_find(argv[fp + 1], argv[fp + 2], argv[fp + 3]);
		exit(0);
	}
	
	if (debugMode)
	{
		if (customName)
		{
			if (getuid() != 0)
			{
				fprintf(stderr,"\nWarning: lookupd -D %s should run as root\n\n", portName);
			}

			status = sys_server_status(portName);
			if (status == SERVER_STATUS_ACTIVE)
			{
				fprintf(stderr, "lookupd -D %s is already running!\n", portName);
				exit(0);
			}
		}
	}
	else
	{
		status = sys_server_status(portName);
		if (status == SERVER_STATUS_ACTIVE)
		{
			fprintf(stderr, "lookupd is already running!\n");
			system_log(LOG_ERR, "lookupd is already running!\n");
			exit(0);
		}
		else if (status == SERVER_STATUS_INACTIVE) initialStartup = YES;
	}

	if (debugMode)
	{
		statistics_enabled = YES;

		lookupd_startup();
		if (controller == nil)
		{
			fprintf(stderr, "controller didn't init!\n");
			exit(0);
		}

		printf("lookupd version %s (%s)\n", _PROJECT_VERSION_, _PROJECT_BUILD_INFO_);
		if (portName != NULL) [controller startServerThread];
		interactive(stdin, stdout);
		shutting_down = YES;

		lookupd_shutdown(0);
	}

	if (initialStartup)
	{
		pid = fork();
		if (pid > 0)
		{
			forever sleep(1);
		}
	}

	detach();
	closeall();

	if (!debugMode) writepid();

	if (coredump_enabled)
	{
		rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
		setrlimit(RLIMIT_CORE, &rlim);
	}

	lookupd_startup();
	if (controller == nil)
	{
		system_log(LOG_EMERG, "controller didn't init!");
		kill(getppid(), SIGTERM);
		exit(0);
	}

	if (initialStartup) kill(getppid(), SIGTERM);

	[controller serverLoop];

	system_log(LOG_DEBUG, "serverLoop ended");

	lookupd_shutdown(-1);
	exit(-1);
}
