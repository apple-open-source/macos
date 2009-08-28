/*
 * Copyright (c) 2006-2009 Apple Inc. All rights reserved.
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

/*
 * gssd daemon. 
 *
 * Gssd is used to proxy requests from the kernel to set up or accept GSS
 * security contexts. The kernel makes up calls to these routines here via
 * mach messaging as defined by gssd_mach.defs. Launchd is used to set up
 * a task special port in both the start up context and in the per session 
 * context. The supplied plist that launchd uses for the start up context,
 * /System/Library/LaunchDaemons/com.apple.gssd.plist, will set the program
 * name to /usr/sbin/gssd, and in the per user session context, found at 
 * /System/Library/LaunchAgents/com.apple.gssd.plist, launchd will set the
 * program name to gssd-agent. By using a special task port, we can fetch
 * a send right from the task making a secure mount call in the kernel.
 * Launchd will own the receive right and will thus start this daemon on
 * demand as defined in the above plists. Since the daemon is invoked in
 * the correct context,  the GSS-API will be able to obtain the appropriate
 * credentials with gss acquire cred.
 *
 * This daemon will set up the context and then wait for a spell (TIMEOUT below)
 * to service any other requests. If no requests come we simply exit and
 * let launchd restart us if necessary on the next mount request. In this way
 * we are not using system resources unnecessarily and we're pretty well
 * protected from any bad consequences of any resource leaks.
 */

#include <libkern/OSAtomic.h>
#include <sys/param.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <vproc.h>

#ifdef VDEBUG
#include <time.h>
#include "/usr/local/include/vproc_priv.h"
#endif

#include <Kerberos/Kerberos.h>
#include <Kerberos/KerberosLoginPrivate.h>

#include "gssd.h"
#include "gssd_mach.h"
#include "gssd_machServer.h"

mach_port_t gssd_receive_right;

union MaxMsgSize {
	union __RequestUnion__gssd_mach_subsystem req;
	union __ReplyUnion__gssd_mach_subsystem rep;
};

#define	MAX_GSSD_MSG_SIZE	(sizeof (union MaxMsgSize) + MAX_TRAILER_SIZE)

#define	DISPLAY_ERRS(name, oid, major, minor) \
	CGSSDisplay_errs((name), (oid), (major), (minor))

#define	str_to_buf(s, b) do { \
	(b)->value = (s); (b)->length = strlen(s) + 1; \
	} while (0)

#ifndef max
#define	max(a, b) (((a)>(b))?(a):(b))
#endif

#define CAST(T,x) (T)(uintptr_t)(x)

#define	ERR(...)   syslog(LOG_ERR,  __VA_ARGS__)
#define INFO(...)  syslog(LOG_INFO, __VA_ARGS__)
#define	DEBUG(...) syslog(LOG_DEBUG, __VA_ARGS__)

#define	APPLE_PREFIX  "com.apple." /* Bootstrap name prefix */
#define	MAXLABEL	256	/* Max bootstrap name */

#define	MAXTHREADS 64		/* Max number of service threads */
#define	NOBODY (uint32_t)-2	/* Default nobody user/group id */
#define	TIMEOUT	30		/* 30 seconds and then bye. */
#define	SHUTDOWN_TIMEOUT  2     /* timeout gets set to this after TERM signal */

#define NFS_SERVICE		"nfs"
#define NFS_SERVICE_LEN		3
#define IS_NFS_SERVICE(s)	((strncmp((s), NFS_SERVICE, NFS_SERVICE_LEN) == 0) && \
				 ((s)[NFS_SERVICE_LEN] == '/' || (s)[NFS_SERVICE_LEN] == '@'))

krb5_enctype NFS_ENCTYPES[] = { 
	ENCTYPE_DES_CBC_CRC,
	ENCTYPE_DES_CBC_MD5,
	ENCTYPE_DES_CBC_MD4,
	ENCTYPE_DES3_CBC_SHA1
};

#define NUM_NFS_ENCTYPES	(sizeof(NFS_ENCTYPES)/sizeof(krb5_enctype))

static uint32_t uid_to_gss_name(uint32_t *, uid_t, gss_OID, gss_name_t *);
static char *	get_next_kerb_component(char *);
static uint32_t gss_name_to_ucred(uint32_t *, gss_name_t,
				char **, uid_t *, gid_t *, uint32_t *);
static char *	lowercase(char *);
static char *	canonicalize_host(const char *, char **);
static uint32_t str_to_svc_names(uint32_t *, const char *, gss_name_t *, uint32_t *);
static void	kerberos_init(void);
static void *	receive_message(void *);
static void 	new_worker_thread(void);
static void 	end_worker_thread(void);
static void 	compute_new_timeout(struct timespec *);
static void *	shutdown_thread(void *);
static void *	timeout_thread(void *);
static void	vm_alloc_buffer(gss_buffer_t, uint8_t **, uint32_t *);
static uint32_t GetSessionKey(uint32_t *, gss_ctx_id_t *, byte_buffer *,
			mach_msg_type_number_t *, void **);
static uint32_t badcall(char *, uint32_t *, gss_ctx *, gss_cred *,
			byte_buffer *, mach_msg_type_number_t *,
			byte_buffer *, mach_msg_type_number_t *);
static void	CGSSDisplay_errs(char*, gss_OID, OM_uint32, OM_uint32);
static void	HexLine(const char *, uint32_t *, char [80]);
static void	HexDump(const char *, uint32_t);

static time_t timeout = TIMEOUT; /* Seconds to wait before exiting */
static int debug = 0;		/* Global flag for enabling debug */
static int die = 0;		/* Simulate server death. Testing only */
static int bye = 0;		/* Force clean shutdown flag. */
static int no_canon = 0;	/* Don't canonicalize host names */

static  int maxthreads = MAXTHREADS;	/* Maximum number of service threads. */
static int numthreads = 0;		/* Current number of service threads */
static pthread_mutex_t numthreads_lock[1]; /* lock to protect above */
static pthread_cond_t	 numthreads_cv[1]; /* To signal when we're below max. */
static pthread_attr_t attr[1];		/* Needed to create detached threads */

/* Counters used in debugging for init and accept context */
static volatile int32_t initCnt = 0;
static volatile int32_t initErr = 0;

static volatile int32_t acceptCnt = 0;
static volatile int32_t acceptErr = 0;

/*
 * Kerberos globals
 */
bool kerb_init_failed = false;
krb5_context local_context;
#define NODEFAULTREALM ":NO_DEFAULT_REALM:"
char *nodefrealm[] = {
	NODEFAULTREALM,
	NULL
};
#define MAXDEFREALMLEN (strlen(NODEFAULTREALM) + 1)

char **realms = nodefrealm;
char *default_realm;

#ifdef NO_PER_USER_LAUNCHD
pthread_mutex_t acquire_cred_lock[1];
#endif

uid_t NobodyUid = NOBODY;
gid_t NobodyGid = NOBODY;

char *local_host; /* our FQDN */
long GetPWMaxRSz; /* Storage size for password entry */

sigset_t waitset[1]; /* Signals that we wait for */
sigset_t contset[1]; /* Signals that we don't exit from */

#define	GSS_cmp_oid(o1, o2) ((o1)->length == (o2)->length ? \
				memcmp((o1)->elements, (o2)->elements, (o1)->length) : 0)

/*
 * SPNEGO oid. XXX this should be published in a public header.
 * { iso(1) org(3) dod(6) internet(1) security(5)
 *  mechanism(5) spnego(2) }
 */
static gss_OID_desc spnego_mech_desc = {
	6, "\053\006\001\005\005\002"
};

/*
 * Kerberos oid. XXX this should be published in a public header.
 * Note that gss_mech_krb5 is available in gssapi/gssapi_krb5.h, but we can't
 * use that to intialize the mechtab below.
 */
static gss_OID_desc krb5_mech_desc = {
	9, "\052\206\110\206\367\022\001\002\002"
};

/*
 * OID table for supported mechs. This is index by the enumeration type mechtype
 * found in gss_mach_types.h.
 */
static gss_OID  mechtab[] = {
	&krb5_mech_desc,
	&spnego_mech_desc,
	NULL
};

static size_t
derlen(uint8_t **dptr, uint8_t *eptr)
{
	int i;
	const uint8_t *p = *dptr;
	size_t len = 0;

	if (*p &  0x80) {
		for (i = *p & 0x7f; i > 0 && (eptr == NULL || (p < eptr)); i--)
			len = (len << 8) + *++p;
	} else
		len = *p;

	*dptr = p + 1;

	return (len);
}

#define ADVANCE(p, l, e) do { \
	(p) += (l); \
	if (debug) \
		DEBUG("%s:%d Advancing %d bytes\n", __func__, __LINE__, (l)); \
	if ((p) > (e)) { \
		if (debug) \
			DEBUG("Defective at %d p = %p e = %p\n", __LINE__, (p), (e)); \
		return (GSS_S_DEFECTIVE_TOKEN); \
	} \
	} while (0)

#define CHK(p, v, e) (((p) >= (e) || *(p) != (v)) ? 0 : 1)
 
static size_t
encode_derlen(size_t len, size_t max, uint8_t *value)
{
	int i;
	size_t count, len_save = len;
	
	if (len < 0x80) {
		if (max > 0 && value)
			value[0] = len;
		return 1;
	}
	
	for (count = 0; len; count++)
		len >>= 8;

	len = len_save;
	if (value && max > count) { 
		for (i = count; i > 0; i--, len >>= 8) {
			value[i] = (len & 0xff );
		}
		value[0] = (0x80 | count);
	}
	/* Extra octet to hold the count of length bytes */
	return (count + 1);
}

#define SEQUENCE 0x30
#define CONTEXT 0xA0
#define ENUM 0x0A
#define OCTETSTRING 0x04

/*
 * Windows 2k is including a bogus MIC in the return token from the server
 * which fails in the gss_init_sec_context call. The mic appears to always be
 * another copy of the kerberos AP_REP token. Go figure. At any rate this
 * routine takes the input token, ASN1 decodes it and if there is a bad Mic
 * removes it and adjust the token so that it is valid again. We should move
 * this into the kerberos library when we have enough experience that this routine
 * covers all the w2k cases.
 */

static uint32_t
spnego_win2k_hack(gss_buffer_t token)
{
	uint8_t *ptr, *eptr, *response, *start, *end;
	size_t len, rlen, seqlen, seqlenbytes, negresplen, negresplenbytes, tlen;

	ptr = 	token->value;
	eptr = ptr + token->length;

	if (debug) {
		DEBUG("%s:%d token value\n", __func__, __LINE__);
		HexDump(token->value, token->length);
	}

	/* CHOICE [1] negTokenResp */
	if (!CHK(ptr, (CONTEXT | 1), eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	/* Sequence */
	if (!CHK(ptr, SEQUENCE, eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	/* Save start of first element in sequence [0] enum*/
	start = ptr;
	if (!CHK(ptr, (CONTEXT | 0), eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	if (len != 3)
		return (GSS_S_DEFECTIVE_TOKEN);
	if (!CHK(ptr, ENUM, eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	if (len != 1)
		return (GSS_S_DEFECTIVE_TOKEN);
	if (!CHK(ptr, 0x0, eptr)) /* != ACCEPT_COMPLETE */
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	/* Get the mech type accepted */
	if (!CHK(ptr, (CONTEXT | 1), eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	/* Skip past the oid bytes -- should check for kerberos? */
	ADVANCE(ptr, len, eptr);
	/* Check for the response token */
	if (!CHK(ptr, (CONTEXT | 2), eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	if (!CHK(ptr, OCTETSTRING, eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	rlen = derlen(&ptr, eptr);
	response = ptr;
	/* Skip rest of response token */
	ADVANCE(ptr, rlen, eptr);
	if (ptr == eptr) 
		/* No mic part so nothing to do */
		return (GSS_S_COMPLETE);
	end = ptr;  /* Save the end of the token */
	/* See if we have a mechMic */
	if (!CHK(ptr, (CONTEXT | 3), eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	if (!CHK(ptr, OCTETSTRING, eptr))
		return (GSS_S_DEFECTIVE_TOKEN);
	ADVANCE(ptr, 1, eptr);
	len = derlen(&ptr, eptr);
	if (len != rlen || ptr + rlen != eptr || memcmp(response, ptr, rlen) != 0) {
		if (debug)
			DEBUG("Mic does not equal response %p %p %p len = %d rlen = %d\n",
				ptr, ptr + rlen, eptr, len, rlen);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	/*
	 * Ok we have a bogus mic, lets chop it off. This is the length value
	 * of the sequence in the negTokenResp
	 */
	seqlen = end - start;
	
	/* Number of bytes to ecode the length */
	seqlenbytes = encode_derlen(seqlen, 0, 0);
	/*
	 * Length of the sequence in the negToken response. Note we add one
	 * for the sequence tag itself
	 */
	negresplen = seqlen + seqlenbytes + 1;
	negresplenbytes = encode_derlen(negresplen, 0, 0);
	/*
	 * Total negTokenResp length
	 */
	tlen = negresplen + negresplenbytes + 1; /* One for the context 1 tag */
	/*
	 * Now we do surgery on the token,
	 */
	ptr = token->value;
	*ptr++ = CONTEXT | 1;
	encode_derlen(negresplen, negresplenbytes, ptr);
	ptr += negresplenbytes;
	*ptr++ = SEQUENCE;
	encode_derlen(seqlen, seqlenbytes, ptr);
	ptr += seqlenbytes;
	memmove(ptr, start, seqlen);
	token->length = tlen;

	if (debug) {
		DEBUG("Returning token from %s\n", __func__);
		HexDump(token->value, token->length);
	}

	return (GSS_S_COMPLETE);
}
		

/*
 * This daemon is to be started by launchd, as such it follows the following
 * launchd rules:
 *	We don't:
 *		call daemon(3)
 *		call fork and having the parent process exit
 *		change uids or gids.
 *		set up the current working directory or chroot.
 *		set the session id
 * 		change stdio to /dev/null.
 *		call setrusage(2)
 *		call setpriority(2)
 *		Ignore SIGTERM.
 *	We are launched on demand
 *		and we catch SIGTERM to exit cleanly.
 *
 * In practice daemonizing in the classic unix sense would probably be ok
 * since we get invoke by traffic on a task_special_port, but we will play
 * by the rules, its even easier to boot.
 */

int main(int argc, char *argv[])
{
	char label_buf[MAXLABEL];
	char *bname = label_buf;
	char *myname;
	kern_return_t kr;
	pthread_t timeout_thr, shutdown_thr;
	int error;
	int ch;

	/* If launchd is redirecting these to files they'll be blocked */
	/* buffered. Probably not what you want. */
	setlinebuf(stdout);
	setlinebuf(stderr);

	/* Figure out our bootstrap name based on what we are called. */

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	strlcpy(label_buf, APPLE_PREFIX, sizeof(label_buf));
	strlcat(label_buf, myname, sizeof(label_buf));

	openlog(myname, LOG_PID  | LOG_NDELAY, LOG_DAEMON);

	while ((ch = getopt(argc, argv, "Cdhm:n:t:")) != -1) {
		switch (ch) {
		case 'C':
			no_canon = 1;
			break;
		case 'd':	/* Debug */
			debug++;
			break;
		case 'm':
			maxthreads = atoi(optarg);
			if (maxthreads < 1)
				maxthreads = MAXTHREADS;
			break;
		case 'n':
			bname = optarg;
			break;
		case 't':
			timeout = atoi(optarg);
			if (timeout < 1)
				timeout = TIMEOUT;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			ERR("usage: %s [-Cdht] [-m threads] "
				"[-n bootstrap name]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	sigemptyset(waitset);
	sigaddset(waitset, SIGABRT);
	sigaddset(waitset, SIGINT);
	sigaddset(waitset, SIGHUP);
	sigaddset(waitset, SIGUSR1);
	sigaddset(waitset, SIGUSR2);
	*contset = *waitset;
	sigaddset(waitset, SIGTERM);
	pthread_sigmask(SIG_BLOCK, waitset, NULL);

	(void) pthread_mutex_init(numthreads_lock, NULL);
	(void) pthread_cond_init(numthreads_cv, NULL);
	(void) pthread_attr_init(attr);
	(void) pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

	setlogmask(LOG_UPTO(debug ? LOG_DEBUG : LOG_ERR));

	kerberos_init();

	/*
	 * Check in with launchd to get the receive right.
	 * N.B. Since we're using a task special port, if launchd
	 * does not have the receive right we can't get it.
	 * And since we should always be started by launchd
	 * this should always succeed.
	 */
	kr = bootstrap_check_in(bootstrap_port, bname, &gssd_receive_right);
	if (kr != BOOTSTRAP_SUCCESS) {
		ERR("Could not checkin for receive right: %s\n", bootstrap_strerror(kr));
		exit(EXIT_FAILURE);
	}


	/* Create signal handling thread */
	error = pthread_create(&shutdown_thr, attr, shutdown_thread, NULL);
	if (error) {
		ERR("unable to create shutdown thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

	/* Create time out thread */
	error = pthread_create(&timeout_thr, NULL, timeout_thread, NULL);
	if (error) {
		ERR("unable to create time out thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

#ifdef VDEBUG
	{
		time_t now;
		if (debug == 2)
			vproc_transaction_begin(NULL);

		now = time(NULL);
		fprintf(stderr, "starting %s with transaction count = %lu, "
			"standby count = %lu\n", ctime(&now),
			(unsigned long)_vproc_transaction_count(),
			(unsigned long)_vproc_standby_count());
		fflush(stderr);
	}
#endif

	/*
	 * Kick off a thread to wait for a message. Shamelessly stolen from
	 * automountd.
	 */
	new_worker_thread();

	/* Wait for time out */
	pthread_join(timeout_thr, NULL);

	if (debug)
		DEBUG("Time out exiting. Number of threads is %d\n", numthreads);

	pthread_attr_destroy(attr);

	if (debug) {
		DEBUG("Total %d init_sec_context errors out of %d calls\n",
			initErr, initCnt);
		DEBUG("Total %d accept_sec_context errors out of %d calls\n",
			acceptErr, acceptCnt);
	}

#ifdef VDEBUG
	fprintf(stderr, "exiting with transaction count = %lu, "
		"standby count = %lu\n",
		(unsigned long) _vproc_transaction_count(),
		(unsigned long) _vproc_standby_count());
	fflush(stderr);
	sleep(30);
	fprintf(stderr, "Bye Bye\n");
#endif
	exit(kerb_init_failed);
}

/*
 * Generate a list of our local realms. Currently I only have an interface
 * to get our default realm, so that's all we get. I found a manpage on the web
 * that support the following interface for NETBSD, (2003 I think). But we,
 * don't support it.
 * XXX: We should parse all the realms in the realms section placing the
 * default as the first realm in /Library/Preferences/edu.mit.Kerberos or
 * /etc/krb5.conf.
 */
static krb5_error_code
krb5_get_default_realms(krb5_context ctx, char ***lrealms)
{
	krb5_error_code kec;
	*lrealms = (char **)calloc(2, sizeof (char *));

	if (*lrealms == NULL)
		return (ENOMEM);
	kec = krb5_get_default_realm(ctx, *lrealms);
	if (kec) {
		ERR("%s: could not get default realm: %s\n",
			__func__, error_message(kec));
		*lrealms = nodefrealm;
		kec = 0;
	}

	return (kec);
}

/*
 * Given a uid and name type convert it to a gss_name_t
 */
static uint32_t
uid_to_gss_name(uint32_t *minor, uid_t uid, gss_OID oid, gss_name_t *name)
{
	char pwbuf[GetPWMaxRSz];
	struct passwd *pwd, pwent;
	char *princ_str;
	gss_buffer_desc buf_name;
	uint32_t major, len;
	int rc;

	*minor = 0;

	rc  = getpwuid_r(uid, &pwent, pwbuf, sizeof(pwbuf), &pwd);
	if (rc != 0 || pwd == NULL)
		return (GSS_S_UNAUTHORIZED);

	len = strlen(pwd->pw_name) + 1 + strlen(default_realm) + 1;
	len = max(len, 10);  /* max string rep for uids */
	len = max(len, 5 + strlen(local_host) + 1 + strlen(default_realm) + 1);
	if ((princ_str = malloc(len)) == NULL)
		return (GSS_S_FAILURE);

	if (oid == GSS_KRB5_NT_PRINCIPAL_NAME) {
		if (pwd->pw_uid == 0)
			/* use the host principal */
			snprintf(princ_str, len,
				"host/%s@%s", local_host, default_realm);
		else
			snprintf(princ_str, len,
				"%s@%s", pwd->pw_name, default_realm);
	}
	else if (oid == GSS_C_NT_USER_NAME)
		snprintf(princ_str, len, "%s", pwd->pw_name);
	else if (oid == GSS_C_NT_STRING_UID_NAME)
		snprintf(princ_str, len, "%d", pwd->pw_uid);
	else if (oid == GSS_C_NT_MACHINE_UID_NAME)
		memcpy(princ_str, &pwd->pw_uid, sizeof(pwd->pw_uid));
	else if (oid == GSS_C_NT_HOSTBASED_SERVICE && pwd->pw_uid == 0)
		snprintf(princ_str, len, "host@%s", local_host);
	else {
		free(princ_str);
		return (GSS_S_FAILURE);
	}

	str_to_buf(princ_str, &buf_name);

	if (debug)
		DEBUG("importing name %s\n", princ_str);

	major = gss_import_name(minor, &buf_name, oid, name);

	free(princ_str);

	return (major);
}

/*
 * get_next_kerb_component. Get the next kerberos component from string.
 */
static char *
get_next_kerb_component(char *str)
{
	char *s, *p;

	/*
	 * Its possible to include "/" and "@" in the leading
	 * components of a kerberos principal name if they
	 * are back slashed escaped, as in, fo\/o\@@realm.
	 */

	s = p = str;
	do {
		p = strpbrk(s, "/@\\");
		s = (p && *p == '\\' && *(p+1)) ? p + 2 : NULL;
	} while (s);

	return (p);
}

/*
 * getucred: Given a user name return the corresponding uid and gid list.
 * Note the first gid in the list is the principal (passwd entry) gid.
 *
 * Return: True on success of False on failure. Note on failure, *uid and *gid
 * are set to nobody and *ngroups is set to 1.
 */
static bool
getucred(const char *uname, uid_t *uid, gid_t *gids, uint32_t *ngroups)
{
	struct passwd *pwd, pwent;
	char pwdbuf[GetPWMaxRSz];
	*uid = NobodyUid;
	*gids = NobodyGid;
	*ngroups = 1;
	
	(void) getpwnam_r(uname, &pwent, pwdbuf, sizeof(pwdbuf), &pwd);
	if (pwd) {
		*uid = pwd->pw_uid;
		*ngroups = NGROUPS_MAX;
		if (getgrouplist(uname, pwd->pw_gid,
			(int *)gids, (int *)ngroups) == -1) {
			/* Best we can do is just return the principal gid */
			*gids = pwd->pw_gid;
			*ngroups = 1;
		}
		return (true);
	}
	return (false);
}

/*
 * Given a gss_name_t convert it to a local uid. We supply an optional list
 * of kerberos realm names to try if name can't be resolve to a passwd
 * entry directly after converting it to a display name.
 */
static uint32_t
gss_name_to_ucred(uint32_t *minor, gss_name_t name,
	char **lrealms, uid_t *uid, gid_t *gids, uint32_t *ngroups)
{
	uint32_t major;
	char *name_str = NULL;
	gss_buffer_desc buf;
	gss_OID oid;
	char **rlm, *this_realm, *uname;
	bool gotname;

	*minor = 0;

	/*
	 * Convert name to text string and fetch the name type.
	 */
	major = gss_display_name(minor, name, &buf, &oid);
	if (major != GSS_S_COMPLETE)
		return (major);

	name_str = malloc(buf.length + 1);
	if (name_str == NULL) {
		gss_release_buffer(minor, &buf);
		return (GSS_S_FAILURE);
	}
	memcpy(name_str, buf.value, buf.length);
	name_str[buf.length] = '\0';
	uname = name_str;

	/*
	 * See if we get lucky and the string version of the name
	 * can be found. XXX Should we try this only for GSS_C_NT_USER_NAME
	 * and GSS_KRB5_NT_PRINCIPAL_NAME?
	 */

	 if ((gotname = getucred(uname, uid, gids, ngroups)))
		 goto out;

	if (oid == GSS_KRB5_NT_PRINCIPAL_NAME) {
		/*
		 * If we failed the above lookup and we're a kerberos name
		 * and if the realm of the name is one of our local realms,
		 * try looking up the first component and see if its a user we
		 * know. We ignore any instance part here, i.e., we assume
		 * user@realm and user/instance@realm are the same for all
		 * instances.
		 */
		this_realm = strrchr(name_str, '@');
		if (this_realm == NULL)
			goto out;
		this_realm++;
		for(rlm = lrealms; rlm && *rlm; rlm++) {
			if (strncmp(this_realm, *rlm, buf.length) == 0) {
				char *p;

				p = get_next_kerb_component(name_str);
				if (p)
					*p = '\0';


				gotname = getucred(uname, uid, gids, ngroups);
				goto out;
			}
		}
	}

out:
	if (!gotname)
		ERR("%s: Could not map %s to unix credentials\n", __func__, uname);
	free(uname);

	return (gotname ? GSS_S_COMPLETE : GSS_S_FAILURE);
}

static char *
lowercase(char *s)
{
	char *t;

	for (t = s; t && *t; t++)
		*t = tolower(*t);

	return (s);
}

/*
 * Turn a hostname into a FQDN if we can. Optionally do the reverse lookup
 * and return it as well in the rfqdn parameter if it is different from the
 * forward  lookup. If that parameter is NULL,  don't try the reverse lookup. 
 * at all. If the foward lookup fails we return NULL.
 * If we succeed it is the caller's responsibility to free the results.
 */
static char *
canonicalize_host(const char *host, char **rfqdn)
{
	struct hostent *hp, *rhp;
	int h_err;
	char *fqdn;

	if (rfqdn)
		*rfqdn = NULL;

	hp = getipnodebyname(host, AF_INET6, AI_DEFAULT, &h_err);
	if (hp == NULL) {
		if (debug)
			DEBUG("host look up for %s returned %d\n", host, h_err);
		return (NULL);
	}
	fqdn = strdup(lowercase(hp->h_name));
	if (fqdn == NULL) {
		ERR("Could not allocat hostname in canonicalize_host\n");
		return (NULL);
	}

	if (rfqdn) {
		if (debug)
			DEBUG("%s: Trying reverse lookup\n", __func__);
		rhp = getipnodebyaddr(hp->h_addr_list[0], hp->h_length, AF_INET6, &h_err);
		if (rhp) {
			if (strncmp(fqdn, lowercase(rhp->h_name), MAXHOSTNAMELEN) != 0) {
				*rfqdn = strdup(rhp->h_name);
				if (*rfqdn == NULL)
					ERR("Could not allocat hostname in canonicalize_host\n");
			}
			freehostent(rhp);
		}
		else {
			if (debug)
				DEBUG("reversed host look up for %s returned %d\n", host, h_err);
		}
	}

	freehostent(hp);

	return (fqdn);
}

/*
 * Given the service name, host name and realm, construct the kerberos gss
 * service name.
 */
static uint32_t
construct_service_name(uint32_t *minor, const char *service, char *host,
		       const char *realm, bool lcase, gss_name_t *svcname)
{
	int len;
	char *s;
	gss_buffer_desc name_buf;
	uint32_t major;

	if (lcase)
		lowercase(host);
	len = strlen(service) + strlen(host) + strlen(realm) + 3;
	s = malloc(len);
	if (s == NULL) {
		ERR("Out of memory in %s\n", __func__);
		return (GSS_S_FAILURE);
	}
	strlcpy(s, service, len);
	strlcat(s, "/", len);
	strlcat(s, host, len);
	strlcat(s, "@", len);
	strlcat(s, realm, len);

	str_to_buf(s, &name_buf);

	if (debug)
		DEBUG("Importing kerberos principal service name %s\n", s);

	major = gss_import_name(minor, &name_buf,
				GSS_KRB5_NT_PRINCIPAL_NAME, svcname);
	free(s);
	return (major);
}

static uint32_t
construct_hostbased_service_name(uint32_t *minor, const char *service, const char *host, gss_name_t *svcname)
{
	int len;
	char *s;
	gss_buffer_desc name_buf;
	uint32_t major;
	
	len = strlen(service) + strlen(host) + 2;
	s = malloc(len);
	if (s == NULL) {
		ERR("Out of memory in %s\n", __func__);
		return (GSS_S_FAILURE);
	}
	strlcpy(s, service, len);
	strlcat(s, "@", len);
	strlcat(s, host, len);
	
	str_to_buf(s, &name_buf);
	
	if (debug)
		DEBUG("Importing host based service name %s\n", s);
		
	major = gss_import_name(minor, &name_buf, GSS_C_NT_HOSTBASED_SERVICE, svcname);

	free(s);
	return (major);
}

/*
 * str_to_svc_name: Given a string representation of a service name, convert it
 * into a set of  gss service names of name type GSS_KRB5_NT_PRINCIPAL_NAME.
 *
 * We get up to three names, lowercase of the forward canonicalization of the 
 * host name, lowercase of the host name itself, and the lowercase of the reverse
 * canonicalization of the host name.
 *
 * name_count is an in/out parameter that says what the size is of the svcname
 * array coming in and the number of gss names found coming out.
 * if name_count is one, no canonicalization is done
 * if name_count is two, return the lowercase of the forward canonicalization
 * 	followed by the non canonicalized host name
 * if name_count is three, the two elements above followed by the lowercase of
 * 	the reverse lookup.
 *
 * We return GSS_S_COMPLETE if we can produce at least one service name.
 */

#define LKDCPREFIX "LKDC:"

static uint32_t
str_to_svc_names(uint32_t *minor, const char *svcstr,
	gss_name_t *svcname, uint32_t *name_count)
{
	uint32_t major, first_major;
	char *realm = NULL /* default_realm */, *host;
	char *s, *p, *service;
	char *fqdn = NULL, *rfqdn = NULL;
	uint32_t count = *name_count;
	int is_lkdc;
	
	*minor = 0;
	major = GSS_S_FAILURE;
	*name_count = 0;
	if (debug) 
		DEBUG("%s: %s count = %d\n", __func__, svcstr, count);

	service = strdup(svcstr);
	if (service == NULL) {
		ERR("%s: Out of memory\n", __func__);
		return (GSS_S_FAILURE);
	}

	p = get_next_kerb_component(service);

	/*set host part */
	host = p + 1;

	if (p == NULL || *p == '\0') {
		/*
		 * We only have the service name so we (this host)
		 * must be our instance.
		 */
		host = local_host;
		
	} else if (*p == '@') {
		/* Have a host based service name */
		/* Terminate service part of name */
		*p = '\0';

		s = get_next_kerb_component(host);
		if (s != NULL) {
			ERR("%s: Invalid host name part %s\n", __func__, host);
			free(service);
			return (GSS_S_BAD_NAME);
		}
		major = construct_hostbased_service_name(minor, service, host, svcname);
		if (major == GSS_S_COMPLETE)
			*name_count = 1;
		return (major);
	} else if (*p == '/') {
		/* We have a kerberos instance thus a kerberos principal type */
		/* Terminate service part of name */
		*p = '\0';

		/* See if we have a realm */
		s = host;
		do {
			s = get_next_kerb_component(s+1);
			if (s && (*s == '@')) {
				realm = s + 1;
				*s = '\0';	/* terminate host instance */
				break;
			}
		} while (s);
	} else {
		/* Should never happen */
		free(service);
		return (GSS_S_BAD_NAME);
	}

	if (realm == NULL) {
		/*
		 * Try this as a host based service name first, since
		 * host base service name will get canonicalized, looked up in the domain realms
		 * section and then tried for referrals
		 */
		major = construct_hostbased_service_name(minor, service, host, svcname);
		if (major == GSS_S_COMPLETE) {
			*name_count = 1;
			return (major);
		}
		/* Nope so set the realm to be the default and fall through */
		realm = default_realm;
	}
	if (strncmp(realm, NODEFAULTREALM, MAXDEFREALMLEN) == 0) {
		free(service);
		/* 
		 * Force exit in SHUTDOWN_TIMEOUT. Perhaps 
		 * we'll pickup a default on next start up.
		 */
		kill(getpid(), SIGTERM);
		return (GSS_S_BAD_NAME);
	}
	

	is_lkdc = (strncmp(realm, LKDCPREFIX, strlen(LKDCPREFIX)) == 0);
	/* Don't lowercase an LKDC instance */
	major = construct_service_name(minor, service, host, realm, !is_lkdc, &svcname[*name_count]);
	if (major == GSS_S_COMPLETE)
		*name_count += 1;
	first_major = major;
	
	/* Don't waste time trying to canonicalize local KDCs */
	if (count == 1 || is_lkdc)
		goto done;

	fqdn = canonicalize_host(host, (count == 3) ? &rfqdn : NULL);
	if (fqdn && strncmp(fqdn, host, MAXHOSTNAMELEN) != 0) {
		major = construct_service_name(minor, service, fqdn, realm, true, &svcname[*name_count]);
		if (major == GSS_S_COMPLETE)
			*name_count += 1;
	}

	if (rfqdn && *name_count < count) {
		major = construct_service_name(minor, service, rfqdn, realm, true, &svcname[*name_count]);
		if (major == GSS_S_COMPLETE)
			*name_count += 1;
		free(rfqdn);
	}

done:
	free(service);

	return (*name_count ? GSS_S_COMPLETE : first_major);
}

/*
 * Set up kerberos, and determine our local realms. Figure out who nobody is
 * and how big a buffer we need to fetch pass word entries.
 */
static void
kerberos_init(void)
{
	int error;
	struct passwd *pwent;
	struct group *grent;
	char hostbuf[MAXHOSTNAMELEN];

#ifndef NO_PER_USER_LAUNCHD
	/*
	 * Never touch the home directory, or automounting home dirs
	 * is going to be very unhappy. We really never need to look into
	 * the home directory for file systems. The only thing we give up
	 * is if the user has there own  kerberos config file
	 * (~/Library/Preferences/edu.mit.Kerberos) to override the system,
	 * but frankly that's just to bad.
	 */
	__KLSetHomeDirectoryAccess(false);
#endif
	kerb_init_failed = false;
	error = krb5_init_context(&local_context);
	if (error) {
		ERR("gssd: Kerberos: %s\n", error_message(error));
		kerb_init_failed = true;
	} 
	if (kerb_init_failed == false &&
		krb5_get_default_realms(local_context, &realms)) {
		ERR("gssd: no Kerberos realm\n");
		kerb_init_failed = true;
	}
	default_realm = *realms; /* our first local realm is our default */
#ifdef NO_PER_USER_LAUNCHD
	pthread_mutex_init(acquire_cred_lock, NULL);
#endif
	pwent = getpwnam("nobody");
	NobodyUid = pwent ? pwent->pw_uid : NOBODY;
	grent = getgrnam("nobody");
	NobodyGid = grent ? grent->gr_gid : NOBODY;

	gethostname(hostbuf, MAXHOSTNAMELEN);
	local_host = canonicalize_host(hostbuf, NULL);
	if ( local_host == NULL) {
		ERR("Could not canonicalize our host name in kerberos_init\n");
		local_host = lowercase(hostbuf);
	}

	/* Figure out how big a buffer we need for getting pwd entries */
	GetPWMaxRSz = sysconf(_SC_GETPW_R_SIZE_MAX);
	GetPWMaxRSz = (GetPWMaxRSz == -1) ? 512 : GetPWMaxRSz;

	if (debug)
		 DEBUG("\tKerberos default realm is %s for %s\n",
				default_realm, local_host);

	/*
	 * Never bring up user interface (UI) for root. 
	 * Roots credentials typically are in this hosts keytab 
	 * and it makes no sense to bring up UI, which might give 
	 * some random user access to system preferences.
	 */
	 if (geteuid() == 0)
		 __KLSetPromptMechanism(klPromptMechanism_None);
}

/*
 * Receive one message. Note that mach_msg_server_once will call
 * the appropriate dispatch routine, which in turn will call new_worker_thread()
 * and that will fire us up again to wait for the next message.
 */
static void *
receive_message(void *arg __attribute__((unused)))
{
	kern_return_t kr;

	if (debug) {
		DEBUG("Enter receive_message %p\n", pthread_self());
#ifdef VDEBUG		
		fprintf(stderr, "Enter receive_message %p with transaction count = %lu, "
			"standby count = %lu\n", pthread_self(),
			_vproc_transaction_count(), _vproc_standby_count());
		fflush(stderr);
#endif
	}

	kr = mach_msg_server_once(gssd_mach_server, MAX_GSSD_MSG_SIZE,
			gssd_receive_right,
			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	
	if (debug) {
		DEBUG("Leaving receive_message %p\n", pthread_self());
#ifdef VDEBUG
		fprintf(stderr, "Leaving receive_message %p with transaction count = %lu, "
			"standby count = %lu\n", pthread_self(),
			_vproc_transaction_count(), _vproc_standby_count());
		fflush(stderr);
#endif
	}	

	if (kr != KERN_SUCCESS)  {
		ERR("mach_msg_server(mp): %s\n", mach_error_string(kr));
		exit(1);
	}

	return (NULL);
}


/*
 * Wait until we have fewer than the maximum number of worker threads,
 * and then create one running receive_message() thread.
 *
 * Called by the dispatch routines just before processing a message,
 * so we're listening for messages even while processing a message,
 * as long as we aren't out of threads.
 */
static void
new_worker_thread(void)
{
        pthread_t thread;
        int error;

        (void) pthread_mutex_lock(numthreads_lock);

        while (bye == 0 && numthreads >= maxthreads) {
                (void) pthread_cond_wait(numthreads_cv, numthreads_lock);
        }
	if (bye)
		goto out;
        numthreads++;
	error = pthread_create(&thread, attr, receive_message, NULL);
	if (error) {
		ERR("unable to create worker thread: %s", strerror(error));
		numthreads--;
	}

out:
        (void) pthread_mutex_unlock(numthreads_lock);
}

/*
 * This worker thread is terminating; reduce the count of worker threads,
 * and, if it's dropped below the maximum, wake up anybody waiting for
 * it to drop below the maximum.
 *
 * Called by the dispatch routines just before returning.
 */
static void
end_worker_thread(void)
{
        (void) pthread_mutex_lock(numthreads_lock);
        numthreads--;
        if (numthreads < maxthreads)
                pthread_cond_signal(numthreads_cv);

	if (debug)
		DEBUG("Thread %p ending numthreads = %d\n",
					pthread_self(), numthreads);

        (void) pthread_mutex_unlock(numthreads_lock);
}


/*
 * Thread that handles signals for us and will tell the timeout thread to
 * shut us down if we get a signal that we don't continue for. We set a global
 * variable bye and the timeout value to SHUTDOWN_TIMEOUT and wake every
 * body up. Threads block in new_worker_thread will see bye is set and exit.
 * We set timeout to SHUTDOWN_TIMEOUT for the timeout thread, so that threads
 * executing dispatch routines have an opportunity to finish.
 */
static void*
shutdown_thread(void *arg __attribute__((unused)))
{
	int sig;

	do {
		sigwait(waitset, &sig);
		switch (sig) {
		case SIGABRT:
			die = 1;
			break;
		case SIGUSR1:
			debug++;
			if (debug == 1)
				setlogmask(LOG_UPTO(LOG_DEBUG));
			DEBUG("Debug level is now %d\n", debug);
			break;
		case SIGUSR2:
			if (debug > 0)
				debug--;
			if (debug == 0)
				setlogmask(LOG_UPTO(LOG_ERR));
			break;
		case SIGHUP:
			debug = !debug;
			setlogmask(LOG_UPTO(debug ? LOG_DEBUG : LOG_ERR));
			break;
		}
	} while (sigismember(contset, sig));

	pthread_mutex_lock(numthreads_lock);
	bye = 1;
	/*
 	 * Wait a little bit for dispatch threads to complete.
	 */
	timeout = SHUTDOWN_TIMEOUT;
	/*
	 * Force the timeout_thread and all the rest to to wake up and exit.
	 */
	pthread_cond_broadcast(numthreads_cv);
	pthread_mutex_unlock(numthreads_lock);

	return (NULL);
}

static void
compute_new_timeout(struct timespec *new)
{
	struct timeval current;

	gettimeofday(&current, NULL);
	new->tv_sec = current.tv_sec + timeout;
	new->tv_nsec = 1000 * current.tv_usec;
}

static void*
timeout_thread(void *arg __attribute__((unused)))
{
	int rv = 0;
	struct timespec exittime;

	(void) pthread_mutex_lock(numthreads_lock);

	/*
	 * Note that we have an extra thread running waiting for a mach message,
	 * the first of which was started in main. Hence we have the test below for
	 * greater than one instead of zero.
	 */
	while (bye ? (rv == 0 && numthreads > 1) : (rv == 0 || numthreads > 1)) {
		if (bye < 2)
			compute_new_timeout(&exittime);
		/*
		 * If the shutdown thread has told us to exit (bye == 1),
		 * then increment bye so that we will exit after at most
		 * SHUTDOWN_TIMEOUT from the time we were signaled. When
		 * we come back around the loop bye will be greater or 
		 * equal to two and we will not update our absolute exit time.
		 */
		if (bye)
			bye++;
		rv = pthread_cond_timedwait(numthreads_cv,
					numthreads_lock, &exittime);
		if (debug)
			DEBUG("timeout_thread: rv = %s %d\n",
			rv ? strerror(rv) : "signaled", numthreads);
	}

	(void) pthread_mutex_unlock(numthreads_lock);


	return (NULL);
}

/*
 * vm_alloc_buffer: Copy the contents of the gss_buf_t to vm_allocated
 * memory at *value. The mig routines will automatically deallocate this
 * memory.
 */

static void
vm_alloc_buffer(gss_buffer_t buf, uint8_t **value, uint32_t *len)
{
	kern_return_t kr;

	*value = NULL;
	*len = 0;

	if (buf->length == 0)
		return;
	kr = vm_allocate(mach_task_self(),
			(vm_address_t *)value, buf->length, VM_FLAGS_ANYWHERE);
	if (kr != KERN_SUCCESS) {
		ERR("Could not allocate vm in vm_alloc_buffer\n");
		return;
	}
	*len = buf->length;
	memcpy(*value, buf->value, *len);
}

/*
 * Extract the session key from a completed gss context. Currently the only
 * supported mechanism is kerberos. Note the extracted key has been vm_allocated
 * and will be released by mig. (See gssd_mach.defs)
 */
static uint32_t
GetSessionKey(uint32_t *minor, gss_ctx_id_t *ctx,
		byte_buffer *skey, mach_msg_type_number_t *skeyCnt, void **lctx)
{
	gss_krb5_lucid_context_v1_t *lucid_ctx;
	gss_krb5_lucid_key_t *key;
	void  *some_lucid_ctx;
	uint32_t major;
	uint32_t vers;
	gss_buffer_desc buf;

	*lctx = NULL;
	*skey = NULL;
	*skeyCnt = 0;
	*minor = 0;

	if (debug)
		DEBUG("Calling  gss_krb5_export_lucid_sec_context\n");
	major = gss_krb5_export_lucid_sec_context(minor, ctx,
					     1, &some_lucid_ctx);
	if (major != GSS_S_COMPLETE)
		return (major);

	*lctx = some_lucid_ctx;
	vers = ((gss_krb5_lucid_context_version_t *)some_lucid_ctx)->version;
	switch (vers) {
	case 1:
		lucid_ctx = (gss_krb5_lucid_context_v1_t *)some_lucid_ctx;
		break;
	default:
		ERR("Lucid version %d is unsupported\n", vers);
		return (GSS_S_UNAVAILABLE);
	}

	if (debug)
		DEBUG("vers = %d, protocol = %d\n",  vers, lucid_ctx->protocol);

	switch (lucid_ctx->protocol) {
	case 0:
		key = &lucid_ctx->rfc1964_kd.ctx_key;
		break;
	case 1:
		key = lucid_ctx->cfx_kd.have_acceptor_subkey ?
			&lucid_ctx->cfx_kd.acceptor_subkey :
			&lucid_ctx->cfx_kd.ctx_key;
		break;
	default:
		return (GSS_S_CALL_BAD_STRUCTURE);  /* should never happen. */
	}

	buf.length = key->length;
	buf.value  = key->data;

	vm_alloc_buffer(&buf, skey, skeyCnt);
	if (skey == NULL) {
		ERR("Out of memory in GetSessionKey\n");
		return (GSS_S_FAILURE);
	}

	return (GSS_S_COMPLETE);
}

/*
 * If we get a call and the verifier does not match, clear out the args for
 * the client.
 */
static uint32_t
badcall(char *rtn, uint32_t *minor_stat,
	gss_ctx *gss_context, gss_cred *cred_handle,
	byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	byte_buffer *otoken, mach_msg_type_number_t *otokenCnt)
{

	ERR("%s request not addressed to us\n", rtn);
	*minor_stat = 0;
	*gss_context = CAST(gss_ctx, GSS_C_NO_CONTEXT);
	*cred_handle = CAST(gss_cred, GSS_C_NO_CREDENTIAL);
	*skey = NULL;
	*skeyCnt = 0;
	*otoken = NULL;
	*otokenCnt = 0;

	return (GSS_S_CALL_BAD_STRUCTURE);
}

/*
 * Convert a gss_name_t to a krb5_principal 
 */
static uint32_t
gss_name_to_kprinc(uint32_t *minor, gss_name_t name, krb5_principal *princ)
{
	uint32_t major, m;
	gss_name_t kname = GSS_C_NO_NAME;
	gss_buffer_desc dname;
	char *strname = NULL;

	*minor = 0;
	major = gss_canonicalize_name(minor, name, &krb5_mech_desc, &kname);
	if (major != GSS_S_COMPLETE)
		return (major);

	major = gss_display_name(minor, kname, &dname, NULL);
	if (major != GSS_S_COMPLETE) {
		(void) gss_release_name(&m, &kname);
		return (major);
	}

	strname = malloc(dname.length + 1);
	if (strname == NULL) {
		(void) gss_release_name(&m, &kname);
		(void) gss_release_buffer(&m, &dname);
		return (GSS_S_FAILURE);

	}
	strlcpy(strname, dname.value, dname.length+1);
	(void) gss_release_buffer(&m, &dname);
	(void) gss_release_name(&m, &kname);
	
	if (debug)
		DEBUG("%s: parsing %s\n", __func__, strname);
	*minor = krb5_parse_name(local_context, strname, princ);

	major = *minor ? GSS_S_FAILURE : GSS_S_COMPLETE;
	free(strname);

	return (major);
}

/*
 * krb5_find_cache_name(krb5_principal princ)
 *
 * Given a kerberos principal find the best cache name to use.
 */
#define CacheDone(e) (((e) == ccIteratorEnd) || ((e) == KRB5_CC_END))

static char*
krb5_find_cache_name(krb5_principal sprinc)
{
	krb5_error_code error;
	cc_context_t cc_context;
	cc_ccache_iterator_t cc_iterator;
	cc_ccache_t cc_cache;
	cc_string_t cc_name;
	krb5_ccache ccache;
	krb5_principal ccache_princ;
	char *cname = NULL;
	const char *cc_type = NULL;

	error = cc_initialize(&cc_context, ccapi_version_4, NULL, NULL);
	if (error) {
		INFO("Could not initialize the credentials cache\n");
		return (NULL);
	}
	error = cc_context_new_ccache_iterator(cc_context, &cc_iterator);
	if (error) {
		INFO("Could not iterate of credentials cache\n");
		cc_context_release(cc_context);
		return (NULL);
	}
	while (!(error = cc_ccache_iterator_next(cc_iterator, &cc_cache))) {
		krb5_error_code err;

		if (cc_ccache_get_name(cc_cache, &cc_name)) {
			INFO("%s: Could not get cache name, skipping\n", __func__);
			cc_ccache_release(cc_cache);
			continue;
		}
		if (debug)
			DEBUG("%s: Found ccache %s\n", __func__, cc_name->data);
		err = krb5_cc_resolve(local_context, cc_name->data, &ccache);
		cc_ccache_release(cc_cache);
		if (err) {
			INFO("%s: krb5_cc_resolve error: %s\n", __func__, error_message(err));
			cc_string_release(cc_name);
			continue;
		}

		cc_type = krb5_cc_get_type(local_context, ccache);
		err = krb5_cc_get_principal(local_context, ccache, &ccache_princ);
		krb5_cc_close(local_context, ccache);
		if (err) {
			INFO("%s: krb5_cc_get_principal error: %s\n", __func__, error_message(err));
			cc_string_release(cc_name);
			continue;
		}

		if (krb5_realm_compare(local_context, sprinc, ccache_princ)) {
			size_t len = strlen(cc_name->data) + 1;
			len +=  cc_type ? strlen(cc_type) + 1 : 0;
			cname = malloc(len);
			if (cname == NULL)
				ERR("%s: Could not duplicate cache name\n", __func__);
			else {
				if (cc_type && *cc_type)
					snprintf(cname, len, "%s:%s", cc_type, cc_name->data);
				else
					snprintf(cname, len, "%s", cc_name->data);
			}
			cc_string_release(cc_name);
			krb5_free_principal(local_context, ccache_princ);
			break;
		}
		krb5_free_principal(local_context, ccache_princ);
		cc_string_release(cc_name);
	}
	if (error && !CacheDone(error))
		ERR("%s: Could not iterate through cache collections: %s\n", __func__,
			error_message(error));

	cc_ccache_iterator_release(cc_iterator);
	cc_context_release(cc_context);

	return (cname);
}

/*
 * set_principal_identity:
 * Given a service principal try and set the default identity so that
 * calls to gss_init_sec_context will work.
 * Currently this only groks kerberos.
 */
static void
set_principal_identity(gss_name_t sname)
{
	krb5_principal sprinc;
	uint32_t major, minor;
	char *cname;
	
	major = gss_name_to_kprinc(&minor, sname, &sprinc);
	if (major != GSS_S_COMPLETE) {
		DEBUG("%s: Could not convert gss name to kerberos principal\n", __func__);
		return;
	}

	cname = krb5_find_cache_name(sprinc);
	krb5_free_principal(local_context, sprinc);
	if (debug) 
		DEBUG("%s: Using ccache <%s>\n", __func__, cname ? cname : "Default");
	if (cname) {
		major = gss_krb5_ccache_name(&minor, cname, NULL);
		if (debug)
			DEBUG("gss_krb5_ccache_name returned %d:%d\n", major, minor);
		free(cname);
	}

}


static uint32_t
do_acquire_cred(uint32_t *minor, char *principal, mechtype mech, gss_name_t sname, uint32_t uid,
	gss_cred *cred_handle, uint32_t flags)
{
	uint32_t major = GSS_S_FAILURE, mstat;
	gss_buffer_desc buf_name;
	gss_name_t clnt_gss_name;
	gss_OID_set mechset = GSS_C_NULL_OID_SET;
	gss_OID name_type = GSS_KRB5_NT_PRINCIPAL_NAME;

#ifdef NO_PER_USER_LAUNCHD	 
	int allow_home = FALSE;

	/*
	 * We need to serialize access to acquiring creds
	 * since different threads may or may not want to look
	 * in home dirs or allow UI.
	 * Note the UI calls are actually broken for gss, so
	 * if you don't wan't UI you must try the default.
	 * If you specify GSSD_NO_DEFAULT and not GSSD_UI_OK, you
	 * will fail.
	 */
	pthread_mutex_lock(acquire_cred_lock);
	if ((flags & GSSD_HOME_ACCESS_OK) == 0) {
		allow_home = __KLAllowHomeDirectoryAccess();
		if (allow_home)
			__KLSetHomeDirectoryAccess(!allow_home);
	}
#endif
	if ((flags & GSSD_UI_OK) == 0)
		__KLSetPromptMechanism(klPromptMechanism_None);

	set_principal_identity(sname);
	major = gss_create_empty_oid_set(minor, &mechset);
	if (major != GSS_S_COMPLETE)
		goto done;
	major = gss_add_oid_set_member(minor, mechtab[mech], &mechset);
	if (major != GSS_S_COMPLETE)
		goto done;

	/*
	 * If we've been passed a principal name then try that first with Kerberos.
	 * Since using GSS_C_NT_USER_NAME might work, but throw away instance and realm
	 * info. It seems easier just to try and not call gss_inquire_names_for_mech
	 */
	 if (principal && *principal) {
		str_to_buf(principal, &buf_name);


		
		if (debug)
			 DEBUG("importing name %s with Keberos\n", principal);

retry:
		major = gss_import_name(minor, &buf_name, name_type, &clnt_gss_name);

		if (major == GSS_S_COMPLETE) {
			major = gss_acquire_cred(
					minor,
					clnt_gss_name,
					GSS_C_INDEFINITE,
					mechset,
					GSS_C_INITIATE,
					(gss_cred_id_t *) cred_handle,
					NULL, NULL);

			if (major == GSS_S_COMPLETE) {
				if (debug)
					DEBUG("Using credentials for %s\n", principal);
				/* Done with the name */
				(void) gss_release_name(&mstat, &clnt_gss_name);
				goto done;
			}
		}
		
		/* 
		 * We could call gss_inquire_names_for_mech and try all supported name types
		 * but it seems likely the only name type of interest would be GSS_C_NT_USER_NAME.
		 */
		if (name_type == GSS_KRB5_NT_PRINCIPAL_NAME) {
			name_type = GSS_C_NT_USER_NAME;
			DEBUG("importing name %s as a user name\n", principal);
			goto retry;
		}
	 }

	if (!(flags & GSSD_NO_DEFAULT)) {
		/* Try default */
		major = gss_acquire_cred(
					minor,
					GSS_C_NO_NAME,
					GSS_C_INDEFINITE,
					mechset,
					GSS_C_INITIATE,
					(gss_cred_id_t *) cred_handle,
					NULL, NULL);

		if (major == GSS_S_COMPLETE) {
			if (debug)
				DEBUG("Using default credential %p\n", *(gss_cred_id_t *)cred_handle);
			goto done;
		}
	}

	/* See if uid will work */
	major = uid_to_gss_name(minor, (uid_t) uid,
				GSS_C_NT_USER_NAME, &clnt_gss_name);
	if (major != GSS_S_COMPLETE)
		return (major);

	major = gss_acquire_cred(
				minor,
				clnt_gss_name,
				GSS_C_INDEFINITE,
				mechset,
				GSS_C_INITIATE,
				(gss_cred_id_t *) cred_handle,
				NULL, NULL);

	/* Done with the name */
	(void) gss_release_name(&mstat, &clnt_gss_name);
done:
	if (mechset != GSS_C_NULL_OID_SET)
		gss_release_oid_set(&mstat, &mechset);
	
#ifdef NO_PER_USER_LAUNCHD
	if ((flags & GSSD_HOME_ACCESS) == 0 && allow_home)
		__KLSetHomeDirectoryAccess(allow_home);

	if ((flags & GSSD_UI_OK) == 0 && getuid() != 0)
		__KLSetPromptMechanism(klPromptMechanism_Autodetect);

	pthread_mutex_unlock(acquire_cred_lock);
#endif
	return (major);
}

/*
 * gssd_context type and routines to hold the underlying gss context as well
 * as the service name
 *
 * The reason we do this is on the initial call to gss_init_sec_context is that the
 * service name can generate up to two extra service names to try.
 * See str_to_svc_names above. Now we need to store the found name
 * where we can retrieve it on the next call if we return CONTINUE_NEEDED and
 * an easy way to do that is to construct our own context data structure to wrap
 * the real gss context and the service name used.
 *
 * You might be wondering why not just call str_to_svc_names again and not
 * worry about another level of context wrapping. Apart from the added work
 * of generating the candidate names and finding the "right" name again when we go
 * through the loop calling gss_init_sec_context, it won't work unless the first
 * name is the chosen name. When we pass in the address of the context to
 * gss_init_sec_context, on error gss will happily delete the context and set
 * our context now to be GSS_C_NO_CONTEXT.
 *
 * So let us say we generate 3 candidate service names and the second one will actually
 * work. The first time around gss_init_sec_context will fail and set our passed
 * in context to GSS_C_NO_CONTEXT  and on the second call succeed, but
 * gss_init_sec_context will think this is an initial context (since the context
 * is NULL) and create a new one and return to the caller CONTINUE_NEEDED. Oops
 * we're in an infinite loop at this point, since the server will receive a valid
 * initial token and around we go.
 */
typedef struct {
	gss_ctx_id_t gss_cntx;
	gss_name_t   svc_name;
	vproc_transaction_t trans_handle;
} gssd_context, *gssd_context_t;

static gss_ctx
gssd_set_context(gss_ctx_id_t ctx, gss_name_t svc_name)
{
	gssd_context_t g;

	g = malloc(sizeof (gssd_context));
	if (g == NULL)
		return (CAST(gss_ctx, GSS_C_NO_CONTEXT));
	gssd_enter(g);

	g->gss_cntx = ctx;
	g->svc_name = svc_name;
	g->trans_handle = vproc_transaction_begin(NULL);

	return (CAST(gss_ctx, g));
}

static gss_ctx_id_t
gssd_get_context(gss_ctx ctx, gss_name_t *svc_name)
{
	gssd_context_t g;
	gss_ctx_id_t gss_context;

	if (!ctx) {
		if (svc_name)
			*svc_name = GSS_C_NO_NAME;
		return (GSS_C_NO_CONTEXT);
	}
	g = CAST(gssd_context_t, ctx);
	if (svc_name)
		*svc_name = g->svc_name;
	gss_context = g->gss_cntx;
	vproc_transaction_end(NULL, g->trans_handle);
	gssd_remove(g);
	free(g);

	return (gss_context);
}

#define MAX_SVC_NAMES 3

/*
 * Mig dispatch routine for gss_init_sec_context.
 */
kern_return_t
svc_mach_gss_init_sec_context(
	mach_port_t test_port __attribute__((unused)),
	mechtype mech,
	byte_buffer itoken, mach_msg_type_number_t itokenCnt,
	uint32_t uid,
	string_t princ_namestr,
	string_t svc_namestr,
	uint32_t flags,
	uint32_t gssd_flags,
	gss_ctx *gss_context,
	gss_cred *cred_handle,
	uint32_t *ret_flags,
	byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	byte_buffer *otoken, mach_msg_type_number_t *otokenCnt,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	gss_name_t svc_gss_name[MAX_SVC_NAMES];
	gss_ctx_id_t g_cntx = GSS_C_NO_CONTEXT;
	uint32_t i, gnames = MAX_SVC_NAMES, name_index = MAX_SVC_NAMES;
	void *lucid_ctx = NULL;
	uint32_t mstat;   /* Minor status for cleaning up. */
	vproc_transaction_t gssd_vproc_handle;
	uint32_t only_1des = ((gssd_flags & GSSD_NFS_1DES) != 0);

	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	if (kerb_init_failed) {
		if (debug)
			DEBUG("%s: is exiting since kerbos init failed\n", __func__);
		end_worker_thread();
		vproc_transaction_end(NULL, gssd_vproc_handle);
		
		kill(getpid(), SIGTERM);  /* Force exit after return in SHUTDOWN_TIMEOUT*/
		return (KERN_NOT_SUPPORTED);
	}
		
	if (debug) {
		DEBUG("svc_mach_gss_init_sec_context:\n");
		DEBUG("	cred_handle = %p\n", CAST(gss_cred_id_t, *cred_handle));
		DEBUG("	uid = %d\n", (int) uid);
		DEBUG(" flags = %0x\n", flags);
		DEBUG(" gssd flags = %0x\n", gssd_flags);
		if (die) {
			DEBUG("Forced server death\n");
			_exit(0);
		}
	}

	if (!gssd_check(CAST(void *, *gss_context)) || !gssd_check(CAST(void *, *cred_handle))) {
		*major_stat = badcall("svc_mach_gss_init_context",
				      minor_stat, gss_context, cred_handle,
				      skey, skeyCnt,
				      otoken, otokenCnt);
		
		end_worker_thread();
		vproc_transaction_end(NULL, gssd_vproc_handle);
		
		return KERN_SUCCESS;
	}

	if (*gss_context == CAST(gss_ctx, GSS_C_NO_CONTEXT)) {

		if (no_canon || (gssd_flags & GSSD_NO_CANON))
			gnames = 1;
		*major_stat = str_to_svc_names(minor_stat,
			svc_namestr, svc_gss_name, &gnames);
		if (*major_stat != GSS_S_COMPLETE)
			goto done;
	}
	else {
		gnames = 1;
		g_cntx = gssd_get_context(*gss_context, svc_gss_name);
		if ((gssd_flags & GSSD_RESTART) && g_cntx != GSS_C_NO_CONTEXT)
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);
	}
	if (*cred_handle &&  (gssd_flags & GSSD_RESTART)) {
		gssd_remove(CAST(void *, *cred_handle));
		(void) gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
	}	
	if (CAST(gss_cred_id_t, *cred_handle) == GSS_C_NO_CREDENTIAL || (gssd_flags & GSSD_RESTART)) {
		*major_stat = do_acquire_cred(minor_stat, princ_namestr, mech,
			*svc_gss_name, uid, cred_handle, gssd_flags);
		if (*major_stat != GSS_S_COMPLETE)
			goto done;
		/* Currently NFS only supports a subset of the Kerberos enctypes */
		if (IS_NFS_SERVICE(svc_namestr)) {
			*major_stat = gss_krb5_set_allowable_enctypes
				(minor_stat, *(gss_cred_id_t *)cred_handle,
				 NUM_NFS_ENCTYPES - only_1des, NFS_ENCTYPES);
			if (*major_stat != GSS_S_COMPLETE) {
				ERR("Could not set enctypes for NFS\n");
				goto done;
			}
		}
		gssd_enter(CAST(void *, *cred_handle));
	}

	gss_buffer_desc intoken = {itokenCnt, itoken};
	gss_buffer_desc outtoken = {0, NULL};
	if ((gssd_flags & GSSD_WIN2K_HACK) && itokenCnt > 0)
		spnego_win2k_hack(&intoken);

	if (debug) {
		DEBUG("Calling gss_init_sec_context\n");
		DEBUG("Using mech = %d\n", mech);
		DEBUG("\tminor_stat = %d\n", (int) *minor_stat);
		DEBUG("\tcred = %p\n", *cred_handle);
		DEBUG("\tgss_context = %p\n", *gss_context);
		DEBUG("Flags = %0x  gss flags = %0x\n", flags, gssd_flags);
		DEBUG("\titokenCnt = %d\n", itokenCnt);
		HexDump((char *)itoken, (itokenCnt > 80) ? 80 : itokenCnt);
	}

	*major_stat = GSS_S_BAD_NAME;
	for (i = 0; i < gnames; i++) {

		*major_stat = gss_init_sec_context(
			minor_stat,
			CAST(gss_cred_id_t, *cred_handle),	/* User's credential handle */
			&g_cntx,		/* Context handle */
			svc_gss_name[i],
			mechtab[mech],		/* Use the requested mech */
			flags,
			0,			/* Time requirement */
			NULL,			/* Channel bindings */
			&intoken,		/* Token from context acceptor */
			NULL,			/* Actual mech types */
			&outtoken,		/* Token for the context acceptor */
			ret_flags,		/* Returned flag bits */
			NULL);			/* Time valid */

		if (*major_stat == GSS_S_COMPLETE ||
		    *major_stat == GSS_S_CONTINUE_NEEDED)
			break;
	}
	name_index = i;

	/* Done with the names */
	for (i = 0; i < gnames; i++)
		if (i != name_index)
			(void)gss_release_name(&mstat, &svc_gss_name[i]);

	vm_alloc_buffer(&outtoken, otoken, otokenCnt);
	gss_release_buffer(&mstat, &outtoken);

	if (*major_stat == GSS_S_COMPLETE) {
		/*
		 * Fetch the (sub)session key from the context
		 */
		*major_stat = GetSessionKey(minor_stat, &g_cntx,
					skey, skeyCnt, &lucid_ctx);

		if (debug) {
			DEBUG("Client key length = %d\n", *skeyCnt);
			HexDump((char *) *skey, *skeyCnt);
		}
	} else if (*major_stat == GSS_S_CONTINUE_NEEDED) {
		*gss_context = gssd_set_context(g_cntx, svc_gss_name[name_index]);
		if (*gss_context == 0)
			*major_stat = GSS_S_FAILURE;
	}

done:
	if (*major_stat != GSS_S_CONTINUE_NEEDED) {
		/* We're done so free what we allocated */
		gssd_remove(CAST(void *, *cred_handle));
		(void) gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
		if (lucid_ctx)
			(void) gss_krb5_free_lucid_sec_context(&mstat, lucid_ctx);
		else if (g_cntx != GSS_C_NO_CONTEXT)
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);

		if (*gss_context)
			gssd_remove(CAST(void *, *gss_context));
		if (name_index < gnames)
			(void)gss_release_name(&mstat, &svc_gss_name[name_index]);
	}
	if (debug) {
		OSAtomicIncrement32(&initCnt);
		if (*major_stat != GSS_S_CONTINUE_NEEDED &&
						*major_stat != GSS_S_COMPLETE)
			OSAtomicIncrement32(&initErr);
		DEBUG("Returning from init (%d/%d)\n", initErr, initCnt);
	}

	if (debug || (*major_stat != GSS_S_CONTINUE_NEEDED &&
					*major_stat != GSS_S_COMPLETE))
		DISPLAY_ERRS("svc_mach_gss_init_sec_context", mechtab[mech],
						*major_stat, *minor_stat);
	if (debug) {
		DEBUG("Returning from svc_mach_gss_init_sec_context");
		DEBUG("\tcred = %p\n", *cred_handle);
		DEBUG("\tgss_context = %p\n", *gss_context);
		DEBUG("\totokenCnt = %d\n", *otokenCnt);
		HexDump((char *)*otoken, (*otokenCnt > 80) ? 80 : *otokenCnt);
	}
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);
	

	return (KERN_SUCCESS);
}

/*
 * Mig dispatch routine for gss_accept_sec_context.
 */
kern_return_t
svc_mach_gss_accept_sec_context(
	mach_port_t test_port __attribute__((unused)),
	byte_buffer itoken, mach_msg_type_number_t itokenCnt,
	string_t svc_namestr,
	uint32_t gssd_flags __attribute__((unused)),
	gss_ctx *gss_context,
	gss_cred *cred_handle,
	uint32_t *ret_flags,
	uint32_t *uid,
	gid_list gids, mach_msg_type_number_t *gidsCnt,
	byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	byte_buffer *otoken, mach_msg_type_number_t *otokenCnt,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	gss_ctx_id_t g_cntx = GSS_C_NO_CONTEXT;
	gss_name_t princ;
	gss_OID oid;
	void *lucid_ctx = NULL;
	uint32_t mstat;    /* Minor status to clean up with. */
	kern_return_t kr = KERN_SUCCESS;
	vproc_transaction_t gssd_vproc_handle;

	gssd_vproc_handle = vproc_transaction_begin(NULL);	
	new_worker_thread();

	if (kerb_init_failed) {
		if (debug)
			DEBUG("%s: is exiting since kerbos init failed\n", __func__);
		end_worker_thread();
		kill(getpid(), SIGTERM); /* Exit after return in SHUTDOWN_TIMEOUT */
		return (KERN_NOT_SUPPORTED);
	}
		

	/* Set the uid to nobody to be safe */
	*uid = NobodyUid;

	if (debug) {
		DEBUG("svc_mach_gss_accept_sec_context:\n");
		if (die) {
			DEBUG("Forced server death\n");
			_exit(0);
		}
	}

	DEBUG("Checking context and cred %llx %llX\n", *gss_context, *cred_handle);
	if (!gssd_check(CAST(void *, *gss_context)) || !gssd_check(CAST(void *, *cred_handle))) {
		*major_stat = badcall("svc_mach_gss_accept_sec_context",
				minor_stat, gss_context, cred_handle,
				skey, skeyCnt, otoken, otokenCnt);

		end_worker_thread();
		vproc_transaction_end(NULL, gssd_vproc_handle);
		
		return (KERN_SUCCESS);
	}

	DEBUG("Getting context from %llx\n", *gss_context);
	g_cntx = gssd_get_context(*gss_context, NULL);
	DEBUG("Context is %p\n", g_cntx);
	
	if (*cred_handle == CAST(gss_cred, GSS_C_NO_CREDENTIAL)) {
		uint32_t i, gnames;
		gss_name_t svc_gss_name[2];

		if (debug)
			DEBUG("\tgss_acquire_cred for service: %s\n",
								svc_namestr);

		gnames = (no_canon || (gssd_flags & GSSD_NO_CANON)) ? 1 : 2;
		*major_stat = str_to_svc_names(minor_stat, svc_namestr,
						svc_gss_name, &gnames);
		if (*major_stat != GSS_S_COMPLETE)
			goto done;

		for (i = 0; i < gnames; i++) {
			*major_stat = gss_acquire_cred(minor_stat,
					svc_gss_name[i],
					GSS_C_INDEFINITE,
					GSS_C_NULL_OID_SET,
					GSS_C_ACCEPT,
					(gss_cred_id_t *) cred_handle,
					NULL, NULL);
			if (*major_stat == GSS_S_COMPLETE)
				break;
		}
		
		for (i = 0; i < gnames; i++)
			gss_release_name(&mstat, &svc_gss_name[i]);
		if (*major_stat != GSS_S_COMPLETE)
			goto done;
		gssd_enter(CAST(void *, *cred_handle));
	}

	gss_buffer_desc intoken = {itokenCnt, itoken};
	gss_buffer_desc outtoken = {0, NULL};;
	*major_stat = 0;
	*minor_stat = 0;

	if (debug) {
		DEBUG("Calling gss_accept_sec_context\n");
		DEBUG("\tminor_stat = %d\n", (int) *minor_stat);
		DEBUG("\tcred = %p\n", *cred_handle);
		DEBUG("\tgss_context = %p\n", g_cntx);
		DEBUG("\titokenCnt = %d\n", itokenCnt);
	}

	*major_stat = gss_accept_sec_context(
		minor_stat,
		&g_cntx,			// Context handle
		CAST(gss_cred_id_t, *cred_handle),	// Acceptor's credential handle
		&intoken,			// Token from context initiator
		GSS_C_NO_CHANNEL_BINDINGS,	// Channel bindings
		&princ,				// Context initiator's name
		&oid,				// Mech types
		&outtoken,			// Token for context initiator
		ret_flags,			// Flags out
		NULL,				// Time requirement
		NULL);				// Delegated creds

	vm_alloc_buffer(&outtoken, otoken, otokenCnt);
	gss_release_buffer(&mstat, &outtoken);

	if (*major_stat == GSS_S_COMPLETE) {
		/*
		 * Turn the principal name into UNIX creds
		 */
		*major_stat = gss_name_to_ucred(minor_stat, princ, realms,
			uid, gids, gidsCnt);
		if (*major_stat != GSS_S_COMPLETE) {
			kr = KERN_FAILURE;
			goto done;
		}
		/*
		 * Fetch the (sub)session key from the context
		 */
		*major_stat = GetSessionKey(minor_stat, &g_cntx,
					    skey, skeyCnt, &lucid_ctx);

		if (debug) {
			DEBUG("Server key length = %d\n", *skeyCnt);
			HexDump((char *) *skey, *skeyCnt);
			DEBUG("Returning uid = %d\n", *uid);
		}
	} else if (*major_stat == GSS_S_CONTINUE_NEEDED) {
		*gss_context = gssd_set_context(g_cntx, NULL);
		if (*gss_context == 0)
			*major_stat = GSS_S_FAILURE;

		/*
		 * Register our context handle
		 */
		gssd_enter(CAST(void *, *gss_context));
	}

done:
	gss_release_name(&mstat, &princ);
	if (*major_stat != GSS_S_CONTINUE_NEEDED) {
		gssd_remove(CAST(void *, *cred_handle));
		(void)gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
		if (lucid_ctx)
			(void) gss_krb5_free_lucid_sec_context(&mstat, lucid_ctx);
		else if (g_cntx != GSS_C_NO_CONTEXT)
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);

		if (*gss_context)
			gssd_remove(CAST(void *, *gss_context));
	}
	if (debug) {
		OSAtomicIncrement32(&acceptCnt);
		if (*major_stat != GSS_S_CONTINUE_NEEDED &&
					*major_stat != GSS_S_COMPLETE)
			OSAtomicIncrement32(&acceptErr);
		DEBUG("Returning from accept (%d/%d)\n", acceptErr, acceptCnt);
	}

	if (*major_stat != GSS_S_CONTINUE_NEEDED &&
				*major_stat != GSS_S_COMPLETE)
		DISPLAY_ERRS("svc_mach_gss_accept_sec_context", oid,
						*major_stat, *minor_stat);
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);
	
	return (kr);
}

/*
 * Mig dispatch routine to log GSS-API errors
 */
kern_return_t
svc_mach_gss_log_error(
	mach_port_t test_port __attribute__((unused)),
	string_t mnt,
	uint32_t uid,
	string_t source,
	uint32_t major,
	uint32_t minor)
{
	OM_uint32 msg_context = 0;
	OM_uint32 min_stat = 0;
	OM_uint32 maj_stat = 0;
	gss_buffer_desc errBuf;
	char msgbuf[1024];
	int full = 0;

	new_worker_thread();

	(void) snprintf(msgbuf, sizeof(msgbuf), "nfs %s Kerberos: %s, uid=%d",
		source, mnt, uid);

	/*
	 * Start with the major error string(s)
	 * The strings are concatenated into a fixed size log
	 * message buffer.  If the messages exceed the buffer
	 * size then we truncate.
	 */
	do {
		if (major == GSS_S_FAILURE)	// more info in minor msg
			break;
		maj_stat = gss_display_status(&min_stat, major, GSS_C_GSS_CODE,
					GSS_C_NULL_OID, &msg_context, &errBuf);
		if (maj_stat != GSS_S_COMPLETE)
			goto done;
		full = strlcat(msgbuf, " - ", sizeof(msgbuf)) >= sizeof(msgbuf) ||
		    strlcat(msgbuf, (char *) errBuf.value, sizeof(msgbuf)) >= sizeof(msgbuf);
		(void) gss_release_buffer(&min_stat, &errBuf);
		if (full)
			goto done;
	} while (msg_context != 0);

	/*
	 * Append any minor error string(s)
	 */
	msg_context = 0;
	do {
		maj_stat = gss_display_status (&min_stat, minor, GSS_C_MECH_CODE,
					GSS_C_NULL_OID, &msg_context, &errBuf);
		if (maj_stat != GSS_S_COMPLETE)
			goto done;
		full = strlcat(msgbuf, " - ", sizeof(msgbuf)) >= sizeof(msgbuf) ||
		    strlcat(msgbuf, (char *) errBuf.value, sizeof(msgbuf)) >= sizeof(msgbuf);
		(void) gss_release_buffer(&min_stat, &errBuf);
		if (full)
			goto done;
	} while (msg_context != 0);

done:
	syslog(LOG_ERR, "%s", msgbuf);

	end_worker_thread();

	return (KERN_SUCCESS);
}

/*
 * Display the major and minor GSS return codes from routine.
 */
static void
CGSSDisplay_errs(char* rtnName, gss_OID mech, OM_uint32 maj, OM_uint32 min)
{
	OM_uint32 msg_context = 0;
	OM_uint32 min_stat = 0;
	OM_uint32 maj_stat = 0;
	gss_buffer_desc errBuf;
	int count = 1;

	ERR("Error returned by %s:\n", rtnName);
	do {
		maj_stat = gss_display_status(&min_stat, maj, GSS_C_GSS_CODE,
					mech, &msg_context, &errBuf);
		if (count == 1)
			ERR("\tMajor error = %d: %s\n", maj, (char *)errBuf.value);
		else
			ERR("\t\t%s\n", (char *)errBuf.value);
		(void)gss_release_buffer(&min_stat, &errBuf);
		++count;
	} while (msg_context != 0);

	count = 1;
	msg_context = 0;
	do {
		maj_stat = gss_display_status (&min_stat, min, GSS_C_MECH_CODE,
					mech, &msg_context, &errBuf);
		if (count == 1)
			ERR("\tMinor error = %d: %s\n", min, (char *)errBuf.value);
		else
			ERR("\t\t%s\n", (char *)errBuf.value);
		(void)gss_release_buffer(&min_stat, &errBuf);
		++count;
	} while (msg_context != 0);
}

static const char HexChars[16] = {
	'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

/*
 * Dump 16 bytes or bufSize bytes (<16) to line buf in hex followed by
 * character representation.
 */
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

/*
 * Dump the supplied buffer in hex.
 */
static void
HexDump(const char *inBuffer, uint32_t inLength)
{
    uint32_t currentSize = inLength;
    char linebuf[80];

    while(currentSize > 0)
    {
        HexLine(inBuffer, &currentSize, linebuf);
	DEBUG("%s", linebuf);
        inBuffer += 16;
    }
}
