/*
 * Copyright (c) 2006-2010 Apple Computer, Inc. All rights reserved.
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

#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include <servers/bootstrap.h>
#include <GSS/gssapi.h>
#include <GSS/gssapi_krb5.h>
#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spnego.h>

#include <asl.h>
#include <bsm/audit.h>
#include <bsm/audit_session.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gssd.h"
#include "gssd_mach.h"

/*
 * OID table for supported mechs. This is index by the enumeration type mechtype
 * found in gss_mach_types.h.
 */
static gss_OID  mechtab[] = {
	NULL, /* Place holder for GSS_KRB5_MECHANISM */
	NULL, /* Place holder for GSS_SPNEGO_MECHANISM */
	NULL, /* Place holder for GSS_NTLM_MECHANISM */
	NULL
};

int debug = 0;

#define	MAXHOSTNAME 256
#define	MAXRETRIES 3
#define	TIMEOUT 100  // 100 microseconds.

volatile int32_t gss_init_errors;
volatile int32_t gss_accept_errors;
volatile int32_t server_errors;
volatile int32_t server_deaths;
volatile int32_t key_mismatches;
volatile int32_t successes;

static void	waittime(void);
void		*server(void *);
void		*client(void *);
static void	deallocate(void *, uint32_t);
static void	server_done();
static void	waitall(void);
static void	report_errors(void);

typedef struct s_channel {
	int client;
	int failure;
	pthread_mutex_t lock[1];
	pthread_cond_t cv[1];
	gssd_byte_buffer ctoken;
	mach_msg_type_number_t ctokenCnt;
	gssd_byte_buffer stoken;
	mach_msg_type_number_t stokenCnt;
	gssd_byte_buffer clnt_skey;
	mach_msg_type_number_t clnt_skeyCnt;
} *channel_t;

#define CHANNEL_CLOSED 0x1000000
#define CHANNEL_FAILED(c) ((c)->failure & (~CHANNEL_CLOSED))

int read_channel(int d, channel_t chan);
int write_channel(int d, channel_t chan);
int close_channel(int d, channel_t chan);

static char *optstrs[] = {
	"			if no host is specified, use the local host",
	"[-b bootstrap label]	client bootstrap name",
	"[-B bootstrap label]	server bootstrap name",
	"[-C]			don't canonicalize the host name",
	"[-d]			debugging",
	"[-D]			don't use the default credential",
	"[-e]			exit on mach rpc errors",
	"[-f flags]		flags for init sec context",
	"[-h]			print this usage message",
	"[-H]			don't access home directory",
	"[-i]			run interactively",
	"[-k]			use kerberos service principal name, otherwise",
	"			use host base service name",
	"[-M retries]		max retries before giving up on server death",
	"[-m krb5 | spnego |ntlm] mech to use, defaults to krb5",
	"[-n n]			number of experiments to run",
	"[-N uid | user | krb5 | ntlm]	name type for client principal",
	"[-p principal]		use principal for client",
	"[-R]			exercise  credential refcounting and exit",
	"[-r realm]		use realm for kerberos",
	"[-s n]			number of concurrent servers (and clients) to run",
	"[-S Service principal] Service principal to use",
	"[-t usecs]		average time to wait in the client",
	"			This is a random time between 0 and 2*usecs",
	"[-u user]		credentials to run as",
	"[-V]			verbose flag. May be repeated",
	"[-v version]		use version of the protocol",
};


static void
Usage(void)
{
	unsigned int i;

	Log("Usage: %s [options] [host]\n", getprogname());
	for (i = 0; i < sizeof(optstrs)/sizeof(char *); i++)
		Log("\t%s\n", optstrs[i]);

	exit(EXIT_FAILURE);
}

int timeout = TIMEOUT;
int verbose = 0;
int max_retries = MAXRETRIES;
int exitonerror = 0;
int interactive = 0;
int version = 0;
uint32_t uid;
uint32_t flags;
uint32_t gssd_flags = GSSD_HOME_ACCESS_OK;
char *principal="";
char svcname[1024];
mach_port_t server_mp = MACH_PORT_NULL;
mach_port_t client_mp = MACH_PORT_NULL;
pthread_cond_t num_servers_cv[1];
pthread_mutex_t num_servers_lock[1];
int num_servers;
gssd_mechtype mech = GSSD_KRB5_MECH;
gssd_nametype name_type = GSSD_MACHINE_UID;

struct gss_name {
	gssd_nametype nt;
	gssd_byte_buffer name;
	uint32_t len;
} clientp, targetp;

static mach_port_t
get_gssd_port(void)
{
	mach_port_t mp, hgssdp;
	kern_return_t kr;
	auditinfo_addr_t ai;
	au_asid_t asid;

	if (getaudit_addr(&ai, sizeof(auditinfo_addr_t))) {
		perror("getaudit_addr");
		exit(EXIT_FAILURE);
	}
	asid = ai.ai_asid;

	if (seteuid(0)) {
		Log("Could not get privilege");
		exit(EXIT_FAILURE);
	}
	kr = host_get_gssd_port(mach_host_self(), &hgssdp);
	if (kr != KERN_SUCCESS) {
		Log("host_get_gssd_port(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
	if (seteuid(uid)) {
		Log("Could not drop privilege");
		exit(EXIT_FAILURE);
	}
	kr = mach_gss_lookup(hgssdp, uid, asid, &mp);
	if (kr != KERN_SUCCESS) {
		Log("Could not lookup port for asid = %d, uid = %d: %s\n",
		    asid, uid, mach_error_string(kr));
	}

	mach_port_deallocate(mach_host_self(), hgssdp);
	return (mp);

}

static int
do_refcount(gssd_mechtype mt, gssd_nametype nt, char *princ)
{
	kern_return_t kret;
	uint32_t M = 0, m = 0;

	printf("trying to hold credential for %s\n", princ);
	kret = mach_gss_hold_cred(client_mp, mt, nt, (uint8_t *)princ, (uint32_t)strlen(princ), &M, &m);
	if (kret == KERN_SUCCESS && M == GSS_S_COMPLETE) {
		printf("Held credential for %s\n", principal);
		if (interactive) {
			printf("Press return to release ...");
			(void)getchar();
		}
		kret = mach_gss_unhold_cred(client_mp, mt, nt, (uint8_t *)princ, (uint32_t)strlen(princ), &M, &m);
		if (kret == KERN_SUCCESS && M == GSS_S_COMPLETE)
			printf("Unheld credential for %s\n", principal);
		else {
			Log("mach_gss_unhold_cred: kret = %d: %#K %#k", kret, M, mechtab[mt], m);
			return 1;
		}
	} else {
		Log("mach_gss_hold_cred: kret = %d: %#K %#k", kret, M, mechtab[mt], m);
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char *bname_server = NULL, *bname_client = NULL;
	int i, j, ch;
	int error;
	int num = 1;
	int Servers = 1;
	int use_kerberos = 0;
	int refcnt = 0;
	pthread_t thread;
	pthread_attr_t attr[1];
	char hostbuf[MAXHOSTNAME];
	char *host = hostbuf;
	char *realm = NULL;
	char *ServicePrincipal = NULL;
	struct passwd *pent = NULL;
	kern_return_t kr;

	uid = getuid();
	if (seteuid(uid)) {
		Log("Could not drop privilege");
		exit(EXIT_FAILURE);
	}

	setprogname(argv[0]);

	/* Set up mech table */
	mechtab[GSSD_KRB5_MECH] = GSS_KRB5_MECHANISM;
	mechtab[GSSD_SPNEGO_MECH] = GSS_SPNEGO_MECHANISM;
	mechtab[GSSD_NTLM_MECH] = GSS_NTLM_MECHANISM;

	while ((ch = getopt(argc, argv, "b:B:CdDef:hHikN:n:M:m:p:r:Rs:S:t:u:v:V")) != -1) {
		switch (ch) {
		case 'b':
			bname_client = optarg;
			break;
		case 'B':
			bname_server = optarg;
			break;
		case 'C':
			gssd_flags |= GSSD_NO_CANON;
			break;
		case 'd':
			debug++;
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
		case 'M':
			max_retries = atoi(optarg);
			break;
		case 'm':
			if (strcmp(optarg, "krb5") == 0)
				mech = GSSD_KRB5_MECH;
			else if (strcmp(optarg, "spnego") == 0)
				mech = GSSD_SPNEGO_MECH;
			else if (strcmp(optarg, "ntlm") == 0)
				mech = GSSD_NTLM_MECH;
			else {
				Log("Unavailable gss mechanism %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'n':
			num = atoi(optarg);
			break;

		case 'N':
			if (strcmp(optarg, "uid") == 0)
				name_type = GSSD_MACHINE_UID;
			else if (strcmp(optarg, "suid") == 0)
				name_type = GSSD_STRING_UID;
			else if (strcmp(optarg, "user") == 0)
				name_type = GSSD_USER;
			else if (strcmp(optarg, "krb5") == 0)
				name_type = GSSD_KRB5_PRINCIPAL;
			else if (strcmp(optarg, "ntlm") == 0)
				name_type = GSSD_NTLM_PRINCIPAL;
			else {
				Log("Unsupported name type %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			principal = optarg;
			break;
		case 'r':
			realm = optarg;
			break;
		case 'R':
			refcnt = 1;
			break;
		case 's':
			Servers = atoi(optarg);
			break;
		case 'S':
			ServicePrincipal = optarg;
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case 'u':
			pent = getpwnam(optarg);
			if (pent)
				uid = pent->pw_uid;
			else
				Log("Could no find user %s\n", optarg);
			break;
		case 'V':
			verbose++;
			break;
		case 'v':
			version = atoi(optarg);
			break;
		default:
			Usage();
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
		Usage();
	}


	if (principal == NULL || *principal == '\0') {
		if (pent == NULL)
			pent = getpwuid(uid);
		principal = pent->pw_name;
		name_type = GSSD_USER;
	}

	clientp.nt = name_type;
	switch (name_type) {
		case GSSD_USER:
		case GSSD_STRING_UID:
		case GSSD_KRB5_PRINCIPAL:
		case GSSD_NTLM_PRINCIPAL:
			clientp.name = (gssd_byte_buffer) principal;
			clientp.len = (uint32_t) strlen(principal);
			break;
		default:
			Log("Unsupported name type for principal %s\n", principal);
			exit(EXIT_FAILURE);
			break;
	}
	printf("Using creds for %s  host=%s\n", principal, host);

	if (bname_client) {
		kr = bootstrap_look_up(bootstrap_port, bname_client, &client_mp);
		if (kr != KERN_SUCCESS) {
			Log("bootstrap_look_up(): %s\n", bootstrap_strerror(kr));
			exit(EXIT_FAILURE);
		}
	} else {
		client_mp = get_gssd_port();
	}

	if (!MACH_PORT_VALID(client_mp)) {
		Log("Could not get a valid client port (%d)\n", client_mp);
		exit(EXIT_FAILURE);
	}

	if (refcnt)
		return do_refcount(mech, name_type, principal);

	if (ServicePrincipal)
		strlcpy(svcname, ServicePrincipal, sizeof(svcname));
	else if (use_kerberos) {
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

	if (!use_kerberos) {
		targetp.nt = GSSD_HOSTBASED;
		targetp.name = (gssd_byte_buffer)svcname;
		targetp.len = (uint32_t) strlen(svcname);
	}
	printf("Service name = %s\n", svcname);

	if (bname_server) {
		kr = bootstrap_look_up(bootstrap_port, bname_server, &server_mp);
		if (kr != KERN_SUCCESS) {
			Log("bootstrap_look_up(): %s\n", bootstrap_strerror(kr));
			exit(EXIT_FAILURE);
		}
	} else {
		server_mp = get_gssd_port();
	}

	if (!MACH_PORT_VALID(server_mp)) {
		Log("Could not get a valid server port (%d)\n", server_mp);
		exit(EXIT_FAILURE);
	}

	if (interactive) {
		printf("Hit enter to start ");
		(void) getchar();
	}

	pthread_attr_init(attr);
	pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

	pthread_mutex_init(num_servers_lock, NULL);
	pthread_cond_init(num_servers_cv, NULL);

	for (i = 0; i < num; i++) {
		num_servers = Servers;
		for (j = 0; j < num_servers; j++) {
			error = pthread_create(&thread, attr, server, NULL);
			if (error)
				Log("Could not start server: %s\n",
						strerror(error));
		}
		waitall();
	}
	report_errors();

	pthread_attr_destroy(attr);

	kr = mach_port_deallocate(mach_task_self(), client_mp);
	if (kr != KERN_SUCCESS) {
		Log("Could not delete send right!\n");
	}

	kr = mach_port_deallocate(mach_task_self(), server_mp);
	if (kr != KERN_SUCCESS) {
		Log("Could not delete send right!\n");
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
report_errors(void)
{
	printf("gss_init_errors %d\n", gss_init_errors);
	printf("gss_accept_errors %d\n", gss_accept_errors);
	printf("server_errors %d\n", server_errors);
	printf("server_deaths %d\n", server_deaths);
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
		Log("Writing out of turn\n");

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
	uint32_t inout_gssd_flags;
	gssd_cred cred_handle = (gssd_cred) (uintptr_t)GSS_C_NO_CREDENTIAL;
	gssd_ctx gss_context = (gssd_ctx) (uintptr_t)GSS_C_NO_CONTEXT;
	kern_return_t kr;
	int gss_error = 0;
	int retry_count = 0;
	char display_name[128];

	do {
		if (read_channel(1, channel)) {
			Log("Bad read from server\n");
			return (NULL);
		}

		if (verbose)
			Debug("Calling mach_gss_init_sec_context from %p\n",
				(void *) pthread_self());

		deallocate(channel->ctoken, channel->ctokenCnt);
		channel->ctoken = (gssd_byte_buffer)GSS_C_NO_BUFFER;
		channel->ctokenCnt = 0;
retry:
		switch (version) {
		case 0:
		case 1:
			kr = mach_gss_init_sec_context(
						       client_mp,
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
			break;
		case 2:
			inout_gssd_flags = gssd_flags;
			kr = mach_gss_init_sec_context_v2(
							  client_mp,
							  mech,
							  channel->stoken, channel->stokenCnt,
							  uid,
							  clientp.nt,
							  clientp.name,
							  clientp.len,
							  targetp.nt,
							  targetp.name,
							  targetp.len,
							  flags,
							  &inout_gssd_flags,
							  &gss_context,
							  &cred_handle,
							  &rflags,
							  &channel->clnt_skey, &channel->clnt_skeyCnt,
							  &channel->ctoken, &channel->ctokenCnt,
							  display_name,
							  &major_stat,
							  &minor_stat);
			if (verbose && kr == KERN_SUCCESS && major_stat ==  GSS_S_COMPLETE)
				Debug("Got client identity of '%s'\n", display_name);
			break;
		default:
			Log("Unsupported version %d\n", version);
			exit(1);
			break;
		}

		if (kr != KERN_SUCCESS) {
			OSAtomicIncrement32(&server_errors);
			Log("gsstest client: %s\n", mach_error_string(kr));
			if (exitonerror)
				exit(1);
			if (kr == MIG_SERVER_DIED) {
				OSAtomicIncrement32(&server_deaths);
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
			Debug("\tcred = 0x%0x\n", (int) cred_handle);
			Debug("\tclnt_gss_context = 0x%0x\n",
				(int) gss_context);
			Debug("\ttokenCnt = %d\n", (int) channel->ctokenCnt);
			if (verbose > 2)
				HexDump((char *) channel->ctoken,
					 (uint32_t) channel->ctokenCnt);
		}

		channel->failure = gss_error;
		write_channel(1, channel);
	} while (major_stat == GSS_S_CONTINUE_NEEDED);


	if (gss_error) {
		OSAtomicIncrement32(&gss_init_errors);
		Log("mach_gss_int_sec_context: %#K %#k\n", major_stat, mechtab[mech], minor_stat);
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
	uint32_t major_stat = GSS_S_FAILURE;
	uint32_t minor_stat;
	uint32_t rflags;
	uint32_t inout_gssd_flags;
	gssd_cred cred_handle = (gssd_cred) (uintptr_t)GSS_C_NO_CREDENTIAL;
	gssd_ctx gss_context = (gssd_ctx) (uintptr_t)GSS_C_NO_CONTEXT;
	uint32_t clnt_uid;
	uint32_t clnt_gids[NGROUPS_MAX];
	uint32_t clnt_ngroups;
	gssd_byte_buffer svc_skey = NULL;
	mach_msg_type_number_t svc_skeyCnt = 0;
	kern_return_t kr;
	int retry_count = 0;

	channel->client = 1;
	channel->failure = 0;
	pthread_mutex_init(channel->lock, NULL);
	pthread_cond_init(channel->cv, NULL);
	channel->ctoken = (gssd_byte_buffer) GSS_C_NO_BUFFER;
	channel->ctokenCnt = 0;
	channel->stoken = (gssd_byte_buffer) GSS_C_NO_BUFFER;
	channel->stokenCnt = 0;
	channel->clnt_skey = (gssd_byte_buffer) GSS_C_NO_BUFFER;
	channel->clnt_skeyCnt = 0;

	// Kick off a client;
	error = pthread_create(&client_thr, NULL, client, channel);
	if (error) {
		Log("Could not start client: %s\n", strerror(error));
		return NULL;
	}


	do {
		if (read_channel(0, channel) == -1) {
			Log("Bad read from client\n");
			goto out;
		}

		deallocate(channel->stoken, channel->stokenCnt);
		channel->stoken = (gssd_byte_buffer)GSS_C_NO_BUFFER;
		channel->stokenCnt = 0;

		if (verbose)
			Debug("Calling mach_gss_accept_sec_contex %p\n",
				(void *) pthread_self());

retry:		switch (version) {
		case 0:
		case 1:
			kr = mach_gss_accept_sec_context(
				server_mp,
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
			break;
		case 2:
			inout_gssd_flags = gssd_flags;
			kr = mach_gss_accept_sec_context_v2(
				server_mp,
				channel->ctoken, channel->ctokenCnt,
				GSSD_STRING_NAME,
				(uint8_t *)svcname,
				(uint32_t) strlen(svcname)+1,
				&inout_gssd_flags,
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
			break;
		default:
			Log("Unsupported version %d\n", version);
			exit(1);
			break;
		}

		if (kr != KERN_SUCCESS) {
			OSAtomicIncrement32(&server_errors);
			Log("gsstest server: %s\n", mach_error_string(kr));
			if (exitonerror)
				exit(1);
			if (kr == MIG_SERVER_DIED) {
				OSAtomicIncrement32(&server_deaths);
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
		OSAtomicIncrement32(&gss_accept_errors);
		Log("mach_gss_accept_sec_context: %#K %#k", major_stat, mechtab[mech], minor_stat);
	}
out:
	close_channel(0, channel);

	pthread_join(client_thr, NULL);

	if (major_stat == GSS_S_COMPLETE && !CHANNEL_FAILED(channel)) {
		if (svc_skeyCnt != channel->clnt_skeyCnt ||
			memcmp(svc_skey, channel->clnt_skey, svc_skeyCnt)) {
			Log("Session keys don't match!\n");
			Log("\tClient key length = %d\n",
				 channel->clnt_skeyCnt);
			HexDump((char *) channel->clnt_skey,
				(uint32_t) channel->clnt_skeyCnt);
			Log("\tServer key length = %d\n", svc_skeyCnt);
			HexDump((char *) svc_skey, (uint32_t) svc_skeyCnt);
			if (uid != clnt_uid)
				Log("Wrong uid. got %d expected %d\n",
					clnt_uid, uid);
		}
		else if (verbose) {
			Debug("\tSession key length = %d\n", svc_skeyCnt);
			HexDump((char *) svc_skey, (uint32_t) svc_skeyCnt);
			Debug("\tReturned uid = %d\n", uid);
		}
	} else if (verbose > 1) {
		Debug("Failed major status = %d\n", major_stat);
		Debug("Channel failure = %x\n", channel->failure);
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
