/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>

#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gssd_mach.h"

typedef struct {
	int total;
	pthread_mutex_t lock[1];
} counter, *counter_t;

#define	MAXCOUNTERS 6
#define	gss_init_errors (&counters[0])
#define	gss_accept_errors (&counters[1])
#define	server_errors (&counters[2])
#define	server_deaths (&counters[3])
#define	key_missmatches (&counters[4])
#define	successes (&counters[5])
#define	DISPLAY_ERRS(name, major, minor) \
		CGSSDisplay_errs((name), (major), (minor))
#define	MAXHOSTNAME 256
#define	MAXRETRIES 3
#define	TIMEOUT 100  // 100 microseconds.

counter counters[MAXCOUNTERS];

static void	waittime(void);
void		*server(void *);
void		*client(void *);
static void	deallocate(void *, uint32_t);
static void	server_done();
static void	waitall(void);
static void	inc_counter(counter_t);
static void	report_errors(void);

static void	CGSSDisplay_errs(char* rtnName, OM_uint32 maj, OM_uint32 min);
static void	HexLine(const char *, uint32_t *, char [80]);
void		HexDump(const char *, uint32_t, int);

typedef struct s_channel {
	int client;
	int failure;
	pthread_mutex_t lock[1];
	pthread_cond_t cv[1];
	byte_buffer ctoken;
	mach_msg_type_number_t ctokenCnt;
	byte_buffer stoken;
	mach_msg_type_number_t stokenCnt;
	byte_buffer clnt_skey;
	mach_msg_type_number_t clnt_skeyCnt;
} *channel_t;

#define CHANNEL_CLOSED 0x1000000
#define CHANNEL_FAILED(c) ((c)->failure & (~CHANNEL_CLOSED))

int read_channel(int d, channel_t chan);
int write_channel(int d, channel_t chan);
int close_channel(int d, channel_t chan);

#define ERR(...)   fprintf(stderr,  __VA_ARGS__)
#define DEBUG(...) fprintf(stdout, __VA_ARGS__)

static char *optstrs[] = {
	"			if no host is specified, use the local host",
	"[-C]			don't canonicalize the host name",
	"[-D]			don't use the default credential",
	"[-e]			exit on mach rpc errors",
	"[-f]			flags for init sec context",
	"[-h]			print this usage message",
	"[-H]			don't access home directory",
	"[-i]			run interactively",
	"[-k]			use kerberos service principal name, otherwise",
	"			use host base service name",
	"[-M retries]		max retries before giving up on server death",
	"[-m krb5 | spnego]	mech to use, defaults to krb5",
	"[-n n]			number of experiments to run",
	"[-p principal]		use princial for client",
	"[-r realm]		use realm for kerberos",
	"[-s n]			number of concurent servers (and clients) to run",
	"[-t usecs]		averge time to wait in the client",
	"			This is a random time beteen 0 and 2*usecs",
	"[-u user]		creditials to run as",
	"[-U]			don't bring up UI.",
	"[-v]			verbose flag. May be repeated",
#ifdef TASK_GSSD_PORT
	"",
#else
	"[-N bootstrap label]	bootstrap name",
#endif
};
	
	
static void
Usage(const char *prog)
{
	unsigned int i;

	ERR("Usage: %s [options] [host]\n", prog);
	for (i = 0; i < sizeof(optstrs)/sizeof(char *); i++) 
		ERR("\t%s\n", optstrs[i]);			
			
	exit(EXIT_FAILURE);
}

int timeout = TIMEOUT; 
int verbose = 0;
int max_retries = MAXRETRIES;
int exitonerror = 0;
int interactive = 0;
uint32_t uid;
uint32_t flags;
uint32_t gssd_flags = (GSSD_HOME_ACCESS_OK | GSSD_UI_OK);
char *principal="";
char svcname[1024];
mach_port_t mp;
pthread_cond_t num_servers_cv[1];
pthread_mutex_t num_servers_lock[1];
int num_servers;
mechtype mech = DEFAULT_MECH;

int main(int argc, char *argv[])
{
	char *bname = NULL;
	int i, j, ch;
	int error;
	int num = 1;
	int Servers = 1;
	int use_kerberos = 0;
	pthread_t thread;
	pthread_attr_t attr[1];
	char hostbuf[MAXHOSTNAME];
	char *host = hostbuf;
	char *realm = NULL;
	char *prog;
	struct passwd *pent;
	kern_return_t kr;

	uid = getuid();
	prog = strrchr(argv[0], '/');
	prog = prog ? prog + 1 : argv[0];

	while ((ch = getopt(argc, argv, "CDefhHikN:n:M:m:p:r:s:t:u:Uv")) != -1) {
		switch (ch) {
		case 'C':
			gssd_flags |= GSSD_NO_CANON;
			break;
		case 'D':
			gssd_flags |= GSSD_NO_DEFAULT;
			break;
		case 'e':
			exitonerror = 1;
			break;
		case 'f':
			flags |= atoi(optarg);
			break;
		case 'H':
			gssd_flags &= ~GSSD_HOME_ACCESS_OK;
			break;
		case 'i':
			interactive = 1;
			break;
		case 'k':
			use_kerberos = 1;
			break;
		case 'N':
			bname = optarg;
			break;
		case 'n':
			num = atoi(optarg);
			break;
		case 'M':
			max_retries = atoi(optarg);
			break;
		case 'm':
			if (strcmp(optarg, "krb5") == 0)
				mech = KRB5_MECH;
			else if (strcmp(optarg, "spnego") == 0)
				mech = SPNEGO_MECH;
			else {
				ERR("Unavailable gss mechanism %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			principal = optarg;
			break;
		case 'r':
			realm = optarg;
			break;
		case 's':
			Servers = atoi(optarg);
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case 'u':
			pent = getpwnam(optarg);
			if (pent)
				uid = pent->pw_uid;
			else
				ERR("Could no find user %s\n", optarg);
			break;
		case 'U':
			gssd_flags &= ~GSSD_UI_OK;
			break;
		case 'v':
			verbose++;
			break;
		default:
			Usage(prog);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		gethostname(hostbuf, MAXHOSTNAME);
	} else if (argc == 1) {
		host = argv[0];
	} else {
		Usage(prog);
	}

	if (principal)
		printf("Using creds for %s  host=%s\n", principal, host);
	else
		printf("Creds for uid=%d  host=%s\n", uid, host);
	if (use_kerberos) {
		strlcpy(svcname, "nfs/", sizeof(svcname));
		strlcat(svcname, host, sizeof(svcname));
		if (realm) {
			strlcat(svcname, "@", sizeof(svcname));
			strlcat(svcname, realm, sizeof(svcname));
		}
	} else {
		strlcpy(svcname, "nfs@", sizeof(svcname));
		strlcat(svcname, host, sizeof(svcname));
	}
	printf("Service name = %s\n", svcname);

	if (!bname) {
		bname = "com.apple.gssd-agent";
	}

	if (interactive) {
		printf("Hit enter to start ");
		(void) getchar();
	}

#ifdef TASK_GSSD_PORT
	kr = task_get_gssd_port(mach_task_self(), &mp);
	if (kr != KERN_SUCCESS) {
		ERR("task_get_gssd_port(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
#else
	kr = bootstrap_look_up(bootstrap_port, bname, &mp);
	if (kr != KERN_SUCCESS) {
		ERR("bootstrap_look_up(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
#endif
	if (!MACH_PORT_VALID(mp)) {
		ERR("Could not get a valid port (%d)\n", mp);
		exit(EXIT_FAILURE);
	}

	pthread_attr_init(attr);
	pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

	for (i = 0; i < MAXCOUNTERS; i++)
		pthread_mutex_init(counters[i].lock, NULL);

	pthread_mutex_init(num_servers_lock, NULL);
	pthread_cond_init(num_servers_cv, NULL);
	for (j = 0; j < MAXCOUNTERS; j++)
		counters[j].total = 0;
	for (i = 0; i < num; i++) {
		num_servers = Servers;
		for (j = 0; j < num_servers; j++) {
			error = pthread_create(&thread, attr, server, NULL);
			if (error)
				ERR("Could not start server: %s\n",
						strerror(error));
		}
		waitall();
	}
	report_errors();
			
	pthread_attr_destroy(attr);

	kr = mach_port_deallocate(mach_task_self(), mp);
	if (kr != KERN_SUCCESS) {
		ERR("Coun not delete send right!\n");
	}

	if (interactive) {
		printf("Hit enter to stop\n");
		(void) getchar();
	}

	return (0);
}

static void
waittime(void)
{
	struct timespec to;

	if (timeout == 0)
		return;

	to.tv_sec = 0;
	to.tv_nsec = (random() % (2*1000*timeout));

	nanosleep(&to, NULL);
}


static void
inc_counter(counter_t count)
{
	pthread_mutex_lock(count->lock);
	count->total++;
	pthread_mutex_unlock(count->lock);
}

static void
report_errors(void)
{
	printf("gss_init_errors %d\n", gss_init_errors->total);
	printf("gss_accept_errors %d\n", gss_accept_errors->total);
	printf("server_errors %d\n", server_errors->total);
	printf("server_deaths %d\n", server_deaths->total);
}

static void
server_done(void)
{
	pthread_mutex_lock(num_servers_lock);
	num_servers-- ;
	if (num_servers == 0)
		pthread_cond_signal(num_servers_cv);
	pthread_mutex_unlock(num_servers_lock);
}

static void
waitall(void)
{
	pthread_mutex_lock(num_servers_lock);
	while (num_servers > 0)
		pthread_cond_wait(num_servers_cv, num_servers_lock);
	pthread_mutex_unlock(num_servers_lock);
}

static void
deallocate(void *addr, uint32_t size)
{
	if (addr == NULL || size == 0)
		return;

	(void) vm_deallocate(mach_task_self(), (vm_address_t)addr, (vm_size_t)size);
}

int read_channel(int d, channel_t chan)
{
	pthread_mutex_lock(chan->lock);
	while (chan->client != d && !chan->failure)
		pthread_cond_wait(chan->cv, chan->lock);

	waittime();

	if (chan->failure) {
		pthread_mutex_unlock(chan->lock);
		return (-1);
	}

	return (0);	
}

int write_channel(int d, channel_t chan)
{
	if (chan->client != d)
		ERR("Writing out of turn\n");

	chan->client = !d;
	pthread_cond_signal(chan->cv);
	pthread_mutex_unlock(chan->lock);

	return (0);
}

int close_channel(int d, channel_t chan)
{
	int rc;

	pthread_mutex_lock(chan->lock);
	while (chan->client != d && !chan->failure)
		pthread_cond_wait(chan->cv, chan->lock);

	rc = chan->failure;

	chan->failure |= CHANNEL_CLOSED;
	chan->client = d;
	pthread_cond_signal(chan->cv);
	pthread_mutex_unlock(chan->lock);

	return (rc);
}

void *client(void *arg)
{
	channel_t channel = (channel_t)arg;
	uint32_t major_stat;
	uint32_t minor_stat;
	uint32_t rflags;
	gss_cred cred_handle = (gss_cred) (uintptr_t)GSS_C_NO_CREDENTIAL;
	gss_ctx gss_context = (gss_ctx) (uintptr_t)GSS_C_NO_CONTEXT;
	kern_return_t kr;
	int gss_error = 0;
	int retry_count = 0;

	do {
		if (read_channel(1, channel)) {
			ERR("Bad read from server\n");
			return (NULL);
		}

		if (verbose)
			DEBUG("Calling mach_gss_init_sec_context from %p\n",
				pthread_self());
	
		deallocate(channel->ctoken, channel->ctokenCnt);
		channel->ctoken = (byte_buffer)GSS_C_NO_BUFFER;
		channel->ctokenCnt = 0;
retry:
		kr = mach_gss_init_sec_context(
			mp,
			mech,
			channel->stoken, channel->stokenCnt,
			uid,
			principal,
			svcname,
			flags,
			gssd_flags,		       
			&gss_context,
			&cred_handle,
			&rflags,
			&channel->clnt_skey, &channel->clnt_skeyCnt,
			&channel->ctoken, &channel->ctokenCnt,
			&major_stat,
			&minor_stat);
	
		if (kr != KERN_SUCCESS) {
			inc_counter(server_errors);
			ERR("gsstest: %s\n", mach_error_string(kr));
			if (exitonerror)
				exit(1);
			if (kr == MIG_SERVER_DIED) {
				inc_counter(server_deaths);
				if (gss_context == (uint32_t)(uintptr_t)GSS_C_NO_CONTEXT &&
					retry_count < max_retries) {
					retry_count++;
					goto retry;
				}
			}

			channel->failure = 1;
			write_channel(1, channel);
			return (NULL);
		}
	
		gss_error = (major_stat != GSS_S_COMPLETE &&
					major_stat != GSS_S_CONTINUE_NEEDED);
		if (verbose > 1) {
			DEBUG("\tcred = 0x%0x\n", (int) cred_handle);
			DEBUG("\tclnt_gss_context = 0x%0x\n",
				(int) gss_context);
			DEBUG("\ttokenCnt = %d\n", (int) channel->ctokenCnt);
			if (verbose > 2)
				HexDump((char *) channel->ctoken,
					 (uint32_t) channel->ctokenCnt, 1);
		}	
	
		channel->failure = gss_error;
		write_channel(1, channel);
	} while (major_stat == GSS_S_CONTINUE_NEEDED);

	if (gss_error) {
		inc_counter(gss_init_errors);
		DISPLAY_ERRS("mach_gss_init_sec_context: ",
				major_stat, minor_stat);
	}

	close_channel(1, channel);
	return (NULL);
}
	
void *server(void *arg __attribute__((unused)))
{
	struct s_channel args;
	channel_t channel = &args;
	pthread_t client_thr;
	int error;
	uint32_t major_stat;
	uint32_t minor_stat;
	uint32_t rflags;
	gss_cred cred_handle = (gss_cred) (uintptr_t)GSS_C_NO_CREDENTIAL;
	gss_ctx gss_context = (gss_ctx) (uintptr_t)GSS_C_NO_CONTEXT;
	uint32_t clnt_uid;
	uint32_t clnt_gids[NGROUPS_MAX];
	uint32_t clnt_ngroups;
	byte_buffer svc_skey;
	mach_msg_type_number_t svc_skeyCnt;
	kern_return_t kr;
	int retry_count = 0;

	channel->client = 1;
	channel->failure = 0;
	pthread_mutex_init(channel->lock, NULL);
	pthread_cond_init(channel->cv, NULL);
	channel->ctoken = (byte_buffer) GSS_C_NO_BUFFER;
	channel->ctokenCnt = 0;
	channel->stoken = (byte_buffer) GSS_C_NO_BUFFER;
	channel->stokenCnt = 0;
	channel->clnt_skey = (byte_buffer) GSS_C_NO_BUFFER;
	channel->clnt_skeyCnt = 0;


	// Kick off a client;
	error = pthread_create(&client_thr, NULL, client, channel);
	if (error) {
		ERR("Could not start client: %s\n", strerror(error));
		return NULL;
	}


	do {
		if (read_channel(0, channel) == -1) {
			ERR("Bad read from client\n");
			goto out;
		}
			
		deallocate(channel->stoken, channel->stokenCnt);
		channel->stoken = (byte_buffer)GSS_C_NO_BUFFER;
		channel->stokenCnt = 0;

		if (verbose)
			DEBUG("Calling mach_gss_accept_sec_contex %p\n",
				pthread_self());
	
retry:
		kr = mach_gss_accept_sec_context(
			mp,
			channel->ctoken, channel->ctokenCnt,
			svcname,
			gssd_flags,
			&gss_context,
			&cred_handle,
			&rflags,			 
			&clnt_uid,
			clnt_gids,
			&clnt_ngroups,
			&svc_skey, &svc_skeyCnt,
			&channel->stoken, &channel->stokenCnt,
			&major_stat,
			&minor_stat);
	
		if (kr != KERN_SUCCESS) {
			inc_counter(server_errors);
			ERR("gsstest: %s\n", mach_error_string(kr));
			if (exitonerror)
				exit(1);
			if (kr == MIG_SERVER_DIED) {
				inc_counter(server_deaths);
				if (gss_context == (uint32_t)(uintptr_t)GSS_C_NO_CONTEXT &&
					retry_count < max_retries) {
					retry_count++;
					goto retry;
				}
			}

			channel->failure = 1;
			write_channel(0, channel);
			goto out;
		}
	
		error = (major_stat != GSS_S_COMPLETE &&
					major_stat != GSS_S_CONTINUE_NEEDED);

		channel->failure = error;
		write_channel(0, channel);
	} while (major_stat == GSS_S_CONTINUE_NEEDED);

	if (error) {
		inc_counter(gss_accept_errors);
		DISPLAY_ERRS("mach_gss_accept_sec_context: ",
				major_stat, minor_stat);
	}
out:
	close_channel(0, channel);

	pthread_join(client_thr, NULL);

	if (major_stat == GSS_S_COMPLETE && !CHANNEL_FAILED(channel)) {
		if (svc_skeyCnt != channel->clnt_skeyCnt ||
			memcmp(svc_skey, channel->clnt_skey, svc_skeyCnt)) {
			ERR("Session keys don't match!\n");
			ERR("\tClient key length = %d\n",
				 channel->clnt_skeyCnt);
			HexDump((char *) channel->clnt_skey,
				(uint32_t) channel->clnt_skeyCnt, 1);
			ERR("\tServer key length = %d\n", svc_skeyCnt);
			HexDump((char *) svc_skey, (uint32_t) svc_skeyCnt, 0);
			if (uid != clnt_uid)
				ERR("Wrong uid. got %d expected %d\n",
					clnt_uid, uid);
		}
		else if (verbose) {
			DEBUG("\tSession key length = %d\n", svc_skeyCnt);
			HexDump((char *) svc_skey, (uint32_t) svc_skeyCnt, 1);
			DEBUG("\tReturned uid = %d\n", uid);
		}
	} else if (verbose > 1) {
		DEBUG("Failed major status = %d\n", major_stat);
		DEBUG("Channel failure = %x\n", channel->failure);
	}

	deallocate(svc_skey, svc_skeyCnt);
	deallocate(channel->ctoken, channel->ctokenCnt);
	deallocate(channel->stoken, channel->stokenCnt);
	deallocate(channel->clnt_skey, channel->clnt_skeyCnt);
	pthread_mutex_destroy(channel->lock);
	pthread_cond_destroy(channel->cv);

	server_done();

	return (NULL);
}

static void
CGSSDisplay_errs(char* rtnName, OM_uint32 maj, OM_uint32 min)
{
	OM_uint32 msg_context = 0;
	OM_uint32 min_stat = 0;
	OM_uint32 maj_stat = 0;	
	gss_buffer_desc errBuf;
	int count = 1;
	ERR("Error returned by %s:\n", rtnName);
	do {
		maj_stat = gss_display_status(&min_stat, maj, GSS_C_GSS_CODE,
					GSS_C_NULL_OID, &msg_context, &errBuf);
		ERR("\tmajor error %d: %s\n", count, (char *)errBuf.value);
		maj_stat =  gss_release_buffer(&min_stat, &errBuf);
		count++;
	} while (msg_context != 0);
		
	count = 1;
	msg_context = 0;
	do {
		maj_stat = gss_display_status (&min_stat, min, GSS_C_MECH_CODE,
					GSS_C_NULL_OID, &msg_context, &errBuf);
		ERR("\tminor error %d: %s\n", count, (char *)errBuf.value);
		count++;
	} while (msg_context != 0);
}

static const char HexChars[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

static void
HexLine(const char *buf, uint32_t *bufSize, char linebuf[80])
{
	char 	*bptr = buf;
	int	limit;
	int	i;
        char	*cptr = linebuf;

        memset(linebuf,0,sizeof(linebuf));
        
	limit = (*bufSize > 16) ? 16 : *bufSize;
	*bufSize -= limit;
	
	for(i = 0; i < 16; i++)
	{
		if(i < limit)
		{
			*cptr++ = HexChars[(*bptr >> 4) & 0x0f];
			*cptr++ = HexChars[*bptr & 0x0f];
                        *cptr++ = ' ';
			bptr++;
		} else {
                        *cptr++ = ' ';
                        *cptr++ = ' ';
                        *cptr++ = ' ';

		}
	}
	bptr = buf;
        *cptr++ = ' ';
        *cptr++ = ' ';
        *cptr++ = ' ';
	for(i = 0; i < limit; i++)
	{
		*cptr++ = (char) (((*bptr > 0x1f) && (*bptr < 0x7f)) ? *bptr : '.');
		bptr++;
	}
        *cptr++ = '\n';
	*cptr = '\0';
}

void
HexDump(const char *inBuffer, uint32_t inLength, int debug)
{
    uint32_t currentSize = inLength;
    char linebuf[80];    
    
    while(currentSize > 0)
    {
        HexLine(inBuffer, &currentSize, linebuf);
	if (debug)
		DEBUG("%s", linebuf);
	else
		ERR("%s", linebuf);
        inBuffer += 16;
    }
}
