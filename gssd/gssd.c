/*
 * Copyright (c) 2006-2017 Apple Inc. All rights reserved.
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

#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <sys/param.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <uuid/uuid.h>

#include <bootstrap_priv.h>
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <membership.h>
#include <netdb.h>
#include <notify.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vproc.h>
#ifdef VDEBUG
#include <time.h>
#include "/usr/local/include/vproc_priv.h"
#endif

#include <Heimdal/com_err.h>
#include <Heimdal/krb5.h>
#include <Security/Security.h>
#include <Security/SecBasePriv.h>
#include <GSS/gssapi.h>
#include <GSS/gssapi_krb5.h>
#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spnego.h>
#include <GSS/gssapi_spi.h>

#include "gssd.h"
#include "gssd/gssd_mach.h"
#include "gssd_machServer.h"

mach_port_t gssd_receive_right;

#include <os/log.h>
#include <os/log_private.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

union MaxMsgSize {
	union __RequestUnion__gssd_mach_subsystem req;
	union __ReplyUnion__gssd_mach_subsystem rep;
};
#define	MAX_GSSD_MSG_SIZE	(sizeof (union MaxMsgSize) + MAX_TRAILER_SIZE)

#define	APPLE_PREFIX  "com.apple." /* Bootstrap name prefix */
#define	MAXLABEL	256	/* Max bootstrap name */
#define	MAXTHREADS 64		/* Max number of service threads */
#define	NOBODY (uint32_t)-2	/* Default nobody user/group id */
#define	TIMEOUT	30		/* 30 seconds and then bye. */
#define	SHUTDOWN_TIMEOUT  2     /* timeout gets set to this after TERM signal */

extern int ctx_counter;

static uint32_t uid_to_gss_name(uint32_t *, uid_t, gss_OID, gss_name_t *);
static char *	get_next_kerb_component(char *);
static uint32_t gss_name_to_ucred(uint32_t *, gss_name_t, uid_t *, gid_t *, uint32_t *);
static char *	lowercase(char *);
static char *	canonicalize_host(const char *);
static const char *get_local_host_name(void);
static uint32_t str_to_svc_name(uint32_t *, const char *, gss_name_t *);
static void	gssd_init(void);
static void *	receive_message(void *);
static void	new_worker_thread(void);
static void	end_worker_thread(void);
static void	compute_new_timeout(struct timespec *);
static void *	shutdown_thread(void *);
static void *	timeout_thread(void *);
static void	vm_alloc_buffer(gss_buffer_t, uint8_t **, uint32_t *);
static uint32_t GetSessionKey(uint32_t *, gss_OID mech, gss_ctx_id_t, gssd_byte_buffer *,
			      mach_msg_type_number_t *, int);
static uint32_t badcall(char *, uint32_t *, gssd_ctx *, gssd_cred *, uint32_t *,
			gssd_byte_buffer *, mach_msg_type_number_t *,
			gssd_byte_buffer *, mach_msg_type_number_t *);

static time_t timeout = TIMEOUT; /* Seconds to wait before exiting */
static int die = 0;		/* Simulate server death. Testing only */
static int bye = 0;		/* Force clean shutdown flag. */
static int no_canon = 0;	/* Don't canonicalize host names */
static int acquire_default = 0;  /* Don't acquire default credentials in do_acquire_cred */
static  int maxthreads = MAXTHREADS;	/* Maximum number of service threads. */
static int numthreads = 0;		/* Current number of service threads */
static int kernel_only = TRUE;		/* Restricts mach_gss_lookup for kernel only */
static int realm_matching = 0;		/*
					 * If set try and find a non expired principal
					 * that is in the same realm as the service
					 */
static pthread_mutex_t numthreads_lock[1]; /* lock to protect above */
static pthread_cond_t	 numthreads_cv[1]; /* To signal when we're below max. */
static pthread_attr_t attr[1];		/* Needed to create detached threads */
static	pthread_t timeout_thr;		/* Thread sees if we've been inactive and exits */
static pthread_t shutdown_thr;		/* Thread to handle signals */

/* Counters used in debugging for init and accept context */
static atomic_int initCnt = 0;
static atomic_int initErr = 0;

static atomic_int acceptCnt = 0;
static atomic_int acceptErr = 0;

uid_t NobodyUid = NOBODY;
gid_t NobodyGid = NOBODY;

char *local_host; /* our FQDN */
long GetPWMaxRSz; /* Storage size for password entry */

sigset_t waitset[1]; /* Signals that we wait for */
sigset_t contset[1]; /* Signals that we don't exit from */

/*
 * OID table for supported mechs. This is index by the enumeration type mechtype
 * found in gss_mach_types.h.
 */
static gss_OID  mechtab[] = {
	NULL, /* Place holder for GSS_KRB5_MECHANISM */
	NULL, /* Place holder for GSS_SPNEGO_MECHANISM */
	NULL, /* Place holder for GSS_NTLM_MECHANISM */
	NULL, /* Place holder for GSS_IAKERB_MECHANISM */
	NULL
};


static kern_return_t
checkin_or_register(char *service, mach_port_t *server_port)
{
	kern_return_t kr;

	/*
	 * Check in with launchd to get the receive right.  N.B. Since
	 * we're using a host special port, or port created by launchd
	 * itself this should always succeed.
	 */

	kr = bootstrap_check_in(bootstrap_port, service, server_port);
	if (kr == BOOTSTRAP_SUCCESS)
		return (KERN_SUCCESS);

	/* This should never happen */
	Log("Could not checkin for receive right: %s\n", bootstrap_strerror(kr));

	return (kr);
}

static int
uuidstr2sessioninfo(const char *uuid_str, uid_t *uid, au_asid_t *asid)
{
	union {
		uuid_t uuid;
		struct {
			uid_t uid;
			au_asid_t asid;
		} info;
	} u;

	if (uuid_parse(uuid_str, u.uuid))
		return (-1);

	*uid = u.info.uid;
	*asid = u.info.asid;

	return (0);
}

static void
sessioninfo2uuid(uid_t uid, au_asid_t asid, uuid_t uuid)
{
	union {
		uuid_t uuid;
		struct {
			uid_t uid;
			au_asid_t asid;
		} info;
	} u;

	uuid_clear(u.uuid);
	u.info.uid = uid;
	u.info.asid = asid;
	uuid_copy(uuid, u.uuid);
}

static int
join_session(au_asid_t asid, __unused const char *instance)
{
	int err;
	au_asid_t asid2;
	mach_port_name_t session_port;

	err = audit_session_port(asid, &session_port);
	if (err) {
		Log("Could not get audit session port for %d: %s", asid, strerror(errno));
		/* %%% we should see if we can unregister the sub-job? */
		return (-1);
	}

	asid2 = audit_session_join(session_port);
	mach_port_deallocate(current_task(), session_port);

	if (asid2 != asid) {
		Log("Joined session %d but wound up in session %d", asid, asid2);
		return (-1);
	}
	return (0);
}

/*
 * Return TRUE if the audit session id is valid, FALSE otherwise
 */
static int
check_session(au_asid_t asid)
{
	int err;
	mach_port_name_t session_port;

	if (asid == AU_DEFAUDITSID || asid == AU_ASSIGN_ASID) {
		Info("Received special audit session id of %d", asid);
		return (FALSE);
	}

	err = audit_session_port(asid, &session_port);
	if (err) {
		Log("Audit session id %d is in invalid: %s", asid, strerror(errno));
		return (FALSE);
	}

	mach_port_deallocate(current_task(), session_port);
	return (TRUE);
}


au_asid_t my_asid = AU_DEFAUDITSID;
#ifdef CCOVERAGE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#endif

static void
set_identity(void)
{
	const char *instance = getenv("LaunchInstanceID");
	auditinfo_addr_t ai;
	au_asid_t asid = -1;
	uid_t euid = geteuid();

	if (getaudit_addr(&ai, sizeof(auditinfo_addr_t)))
		Debug("getaudit failed: %s", strerror(errno));
	else
		asid = ai.ai_asid;

	Debug("asid = %d euid = %d, instance = %s", ai.ai_asid,
	    euid, instance ? instance : "not set");
	if (instance && geteuid() == 0) {
		uid_t uid;

		if (uuidstr2sessioninfo(instance, &uid, &asid))
			Log("Could not parse  LaunchInstanceID: %s", instance);
		else {
#ifdef CCOVERAGE
			char path[MAXPATHLEN+1];
			snprintf(path, MAXPATHLEN, "/private/var/tmp/cc/gssd.%d/default.profraw", getpid());
			path[MAXPATHLEN] = '\0';
			chmod(path, 0666);
			chown(path, uid, 0);
#endif

			_SecSetSecuritydTargetUID(uid);
			if (join_session(asid, instance) == 0)
				setuid(uid);
		}
	}

	/* Get my actual audit session id for checkout */
	if (getaudit_addr(&ai, sizeof(auditinfo_addr_t)))
		Log("getaudit failed: %s", strerror(errno));
	else
		my_asid = ai.ai_asid;
	if (asid != my_asid || getuid() != euid)
		Info("My identity changed to asid = %d auid = %d uid = %d", ai.ai_asid, ai.ai_auid, getuid());
}

static int
check_audit(audit_token_t atok, int kernonly)
{
	uid_t uid, euid, ruid;
	gid_t egid, rgid;
	pid_t pid;
	au_asid_t asid;
	int ok;
	static audit_token_t kern_audit_token = KERNEL_AUDIT_TOKEN_VALUE;

	audit_token_to_au32(atok, &uid, &euid, &egid, &ruid, &rgid, &pid, &asid, NULL);
	DEBUG(9, "Received audit token: uid = %d, euid = %d, egid = %d, ruid = %d rgid = %d, pid = %d, asid = %d atid = %d",
	      uid, euid, egid, ruid, rgid, pid, asid, atok.val[7]);

	ok = (memcmp(&atok, &kern_audit_token, sizeof (audit_token_t)) == 0);
	if (!ok && !kernonly) {
		Debug("gssd asid = %d gssd uid = %d  remote pid = %d remote asid = %d remote euid = %d",
		      my_asid, getuid(), pid, asid, euid);
		ok = (asid == my_asid || (euid && euid == getuid()));
	}
	if (!ok)
		Log("Process %d in session %d as user %d was denied by gssd[%d] for session %d as user %d", pid, asid, euid, getpid(), my_asid, getuid());

	return (ok);
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
 *		change stdio to /dev/null.
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

char label_buf[MAXLABEL];
char *bname = label_buf;

int main(int argc, char *argv[])
{
	kern_return_t kr;
	int error;
	int ch;
	int debug_opt = 0;

	/* Tell logd we're special */
	os_log_set_client_type(OS_LOG_CLIENT_TYPE_LOGD_DEPENDENCY, 0);
	/* If launchd is redirecting these to files they'll be blocked */
	/* buffered. Probably not what you want. */
	setlinebuf(stdout);
	setlinebuf(stderr);

	/* Figure out our bootstrap name based on what we are called. */
	setprogname(argv[0]);
	strlcpy(label_buf, APPLE_PREFIX, sizeof(label_buf));
	strlcat(label_buf, getprogname(), sizeof(label_buf));

	while ((ch = getopt(argc, argv, "b:Cdhm:n:Rt:DT")) != -1) {
		switch (ch) {
		case 'C':
			no_canon = 1;
			break;
		case 'd':	/* Debug */
			debug_opt++;
			break;
		case 'm':
			maxthreads = atoi(optarg);
			if (maxthreads < 1)
				maxthreads = MAXTHREADS;
			break;
		case 'b':
		case 'n':
			bname = optarg;
			break;
		case 't':
			timeout = atoi(optarg);
			if (timeout < 10)
				timeout = TIMEOUT;
			break;
		case 'D':
			acquire_default = 1;
			break;
		case 'R':
			realm_matching = 1;
			break;
		case 'T':
			kernel_only = FALSE;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			Log("usage: %s [-Cdht] [-m threads] "
				"[-n bootstrap name]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

/*
 *	Currently we don't do anything else with argc, argv.
 *
 *	argc -= optind;
 *	argv += optind;
 */
	kr = checkin_or_register(bname, &gssd_receive_right);
	if (kr != KERN_SUCCESS)
		exit(EXIT_FAILURE);

	sigemptyset(waitset);
	sigaddset(waitset, SIGQUIT);
	if (!traced() && !in_foreground(2))
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

	/* Set initial debug_level */
	set_debug_level(debug_opt);
	/* Check to see if the master asl filter is set */
	set_debug_level(-1);

	/* Set our session, uid and securityd lookup target if needed */
	set_identity();

	gssd_init();

	/* Create signal handling thread */
	error = pthread_create(&shutdown_thr, attr, shutdown_thread, NULL);
	if (error) {
		Log("unable to create shutdown thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

	/* Create time out thread */
	error = pthread_create(&timeout_thr, NULL, timeout_thread, NULL);
	if (error) {
		Log("unable to create time out thread: %s", strerror(error));
		exit(EXIT_FAILURE);
	}

#ifdef VDEBUG
	{
		time_t now;
		if (debug == 2)
			vproc_transaction_begin(NULL);

		now = time(NULL);
		DEBUG(3, "starting %s with transaction count = %lu, "
			"standby count = %lu\n", ctime(&now),
			(unsigned long)_vproc_transaction_count(),
			(unsigned long)_vproc_standby_count());
	}
#endif

	/*
	 * Kick off a thread to wait for a message. Shamelessly stolen from
	 * automountd.
	 */
	new_worker_thread();

	/* Wait for time out */
	pthread_join(timeout_thr, NULL);

	DEBUG(3, "Time out exiting. Number of threads is %d\n", numthreads);

	pthread_attr_destroy(attr);

	Debug("Total %d init_sec_context errors out of %d calls\n", initErr, initCnt);
	Debug("Total %d accept_sec_context errors out of %d calls\n", acceptErr, acceptCnt);
	Debug("Total entries left = %d\n", ctx_counter);
#ifdef VDEBUG
	DEBUG(3, "exiting with transaction count = %lu, "
		"standby count = %lu\n",
		(unsigned long) _vproc_transaction_count(),
		(unsigned long) _vproc_standby_count());
#endif

	return (0);
}

static int
get_local_realms(krb5_realm **realms)
{
	int error;
	krb5_context kctx;

	if (realms == NULL)
		return (FALSE);
	*realms = NULL;
	error = krb5_init_context(&kctx);
	if (error) {
		Log("Could not get kerberos context");
		krb5_free_context(kctx);
		return (FALSE);
	}
	error = krb5_get_default_realms(kctx, realms);
	krb5_free_context(kctx);
	if (error) {
		Log("Could not get kerbose default realms");
		return (FALSE);
	}
	return (TRUE);
}

static void
free_local_realms(krb5_realm *realms)
{
	int error;
	krb5_context kctx;

	if (realms == NULL)
		return;

	error = krb5_init_context(&kctx);
	if (error) {
		Log("Could not get kerberos context");
		return;
	}
	(void )krb5_free_host_realm(kctx, realms);
	krb5_free_context(kctx);
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
	uint32_t major;
	size_t len;
	size_t realmlen;
	krb5_realm *realms = NULL;
	krb5_realm default_realm = NULL;
	int rc;

	*minor = 0;

	rc  = getpwuid_r(uid, &pwent, pwbuf, sizeof(pwbuf), &pwd);
	if (rc != 0 || pwd == NULL)
		return (GSS_S_UNAUTHORIZED);

	if (get_local_realms(&realms))
		default_realm = *realms;

	realmlen = default_realm ? strlen(default_realm)  : 0;
	len = strlen(pwd->pw_name) + 1 + realmlen + 1;
	len = maximum(len, 10);  /* max string rep for uids */
	if ((pwd->pw_uid == 0 && gss_oid_equal(oid, GSS_KRB5_NT_PRINCIPAL_NAME)) ||
	    gss_oid_equal(oid, GSS_C_NT_HOSTBASED_SERVICE))
		len = maximum(len, 5 + strlen(get_local_host_name()) + 1 + realmlen + 1);
	if ((princ_str = malloc(len)) == NULL) {
		free_local_realms(realms);
		return (GSS_S_FAILURE);
	}
	if (gss_oid_equal(oid, GSS_KRB5_NT_PRINCIPAL_NAME)) {
		if (pwd->pw_uid == 0) {
			/* use the host principal */
			if (default_realm)
				snprintf(princ_str, len,
					 "host/%s@%s", get_local_host_name(), default_realm);
			else
				snprintf(princ_str, len, "host/%s", get_local_host_name());
		} else {
			if (default_realm)
				snprintf(princ_str, len,
					 "%s@%s", pwd->pw_name, default_realm);
			else
				snprintf(princ_str, len, "%s", pwd->pw_name);
		}
	}
	else if (gss_oid_equal(oid, GSS_C_NT_USER_NAME))
		snprintf(princ_str, len, "%s", pwd->pw_name);
	else if (gss_oid_equal(oid, GSS_C_NT_STRING_UID_NAME))
		snprintf(princ_str, len, "%d", pwd->pw_uid);
	else if (gss_oid_equal(oid, GSS_C_NT_MACHINE_UID_NAME))
		memcpy(princ_str, &pwd->pw_uid, sizeof(pwd->pw_uid));
	else if (gss_oid_equal(oid, GSS_C_NT_HOSTBASED_SERVICE) && pwd->pw_uid == 0)
		snprintf(princ_str, len, "host@%s", get_local_host_name());
	else {
		free(princ_str);
		free_local_realms(realms);
		return (GSS_S_FAILURE);
	}

	str_to_buf(princ_str, &buf_name);

	Debug("importing name %s\n", princ_str);

	major = gss_import_name(minor, &buf_name, oid, name);

	free(princ_str);
	free_local_realms(realms);

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

	s = str;
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
 * Note just return the primary group, we let the kernel's kauth mechanism do
 * dynamic group lookups.
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
		*ngroups = 1;
		*gids = pwd->pw_gid;
	}
	return (false);
}

/*
 * Given a gss_name_t convert it to a local uid. We use an optional list
 * of kerberos realm names to try if name can't be resolve to a passwd
 * entry directly after converting it to a display name.
 */
static uint32_t
gss_name_to_ucred_1(uint32_t *minor, gss_name_t name,
		    uid_t *uid, gid_t *gids, uint32_t *ngroups)
{
	uint32_t major;
	char *name_str = NULL;
	gss_buffer_desc buf;
	gss_OID oid = GSS_C_NO_OID;
	char **rlm, *this_realm, *uname;
	bool gotname;
	krb5_realm *realms = NULL;

	*minor = 0;

	/*
	 * Convert name to text string and fetch the name type.
	 */
	major = gss_display_name(minor, name, &buf, &oid);
	if (major != GSS_S_COMPLETE)
		return (major);

	name_str = buf_to_str(&buf);
	if (name_str == NULL)
		return (GSS_S_FAILURE);

	uname = name_str;

	/*
	 * See if we get lucky and the string version of the name
	 * can be found.
	 */

	if ((gotname = getucred(uname, uid, gids, ngroups)))
		goto out;

	if (gss_oid_equal(oid, GSS_KRB5_NT_PRINCIPAL_NAME)) {
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
		if (!get_local_realms(&realms))
			goto out;
		for(rlm = realms; rlm && *rlm; rlm++) {
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
		Info("Directory Service could not map %s to unix credentials. Directory Service problem?\n", uname);
	else
		Info("Directory Service mapped %s to uid %d", uname, *uid);

	free(uname);
	free_local_realms(realms);

	/*
	 * If we could not find a mapping for the principal then they are mapped to nobody.
	 * This is important for service principals that probably don't have a mapping. Particularly during
	 * mounting.
	 */
	return (GSS_S_COMPLETE);
}

/*
 * Given a gss_name_t convert it to a local uid.
 */
static uint32_t
gss_name_to_ucred(uint32_t *min, gss_name_t name,
		  uid_t *uid, gid_t *gids, uint32_t *ngroups)
{
	uint32_t maj, ms;
	gss_buffer_desc xname;
	uuid_t uu;
	int ret;
	int type;
	struct passwd *pwd, pwent;
	char pwdbuf[GetPWMaxRSz];
	*uid = NobodyUid;
	*gids = NobodyGid;
	*ngroups = 1;

	maj = gss_export_name(min, name, &xname);
	if (maj != GSS_S_COMPLETE)
		return (maj);

	ret = mbr_identifier_to_uuid(ID_TYPE_GSS_EXPORT_NAME, xname.value, xname.length, uu);
	(void) gss_release_buffer(&ms, &xname);

	if (ret) {
		Debug("mbr_identifier_to_uid: failed to map export name to uuid: reason %d\n", ret);
		return (gss_name_to_ucred_1(min, name, uid, gids, ngroups));
	}

	ret = mbr_uuid_to_id(uu, uid, &type);
	if (ret || type != ID_TYPE_UID) {
		Info("gssapi: failed to turn uuid into uid: %d", ret);
		return (GSS_S_FAILURE);
	}

	ret = getpwuid_r(*uid, &pwent, pwdbuf, sizeof(pwdbuf), &pwd);
	if (ret) {
		Info("Look up of uid %d failed. Reason %d: %s\n", *uid, errno,
		     strerror(errno));
		return (GSS_S_FAILURE);
	}

	if (pwd) {
		/* We let kauth in the kernel do dynamic group mappings */
		*ngroups = 1;
		*gids = pwd->pw_gid;
	} else {
		Log("Directory Service could not find uid %d.\n", *uid);
		return (GSS_S_FAILURE);
	}

	return (GSS_S_COMPLETE);
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
 * Turn a hostname into a FQDN if we can.
 *
 * N.B. In our current normal operation this routine  should never be called.
 */
static char *
canonicalize_host(const char *host)
{
	struct addrinfo ai, *res;
	int error;
	char *fqdn;
	static volatile atomic_flag called = ATOMIC_FLAG_INIT;

	if (!atomic_flag_test_and_set(&called))
	       Log("Canonicalized_host called");
	memset(&ai, 0, sizeof(struct addrinfo));
	ai.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
	ai.ai_family = PF_UNSPEC;
	error = getaddrinfo(host, NULL, &ai, &res);
	if (error) {
		Info("Could not lookup host %s error = %s\n", host, gai_strerror(error));
		return (NULL);
	}
	fqdn = strdup(lowercase(res->ai_canonname));
	if (fqdn == NULL) {
		Log("Could not allocate hostname in canonicalize_host\n");
		return (NULL);
	}
	freeaddrinfo(res);

	return (fqdn);
}

/*
 * get_local_host_name
 *
 * Return the FQDN, lowercase of our host name
 * N.B.: In normal operation we should never need to call this routine.
 */
static void
get_local_host_name_really(void)
{
	char hostbuf[MAXHOSTNAMELEN];

	Log("Calling get_local_host_name");
	gethostname(hostbuf, MAXHOSTNAMELEN);
	local_host = canonicalize_host(hostbuf);
	if ( local_host == NULL) {
		Info("Could not canonicalize our host name in gssd_init\n");
		local_host = strdup(lowercase(hostbuf));
	}
}

static pthread_once_t gethostonce = PTHREAD_ONCE_INIT;

static const char *
get_local_host_name(void)
{
	if (pthread_once(&gethostonce, get_local_host_name_really)) {
		Log("Could not get local host name!");
		return (NULL);
	}
	return (local_host);
}

/*
 * Given the service name, host name and realm, construct the kerberos gss
 * service name.
 */
static uint32_t
construct_service_name(uint32_t *minor, const char *service, char *host,
		       const char *realm, bool lcase, gss_name_t *svcname)
{
	size_t len;
	char *s;
	gss_buffer_desc name_buf;
	uint32_t major;

	if (lcase)
		lowercase(host);
	len = strlen(service) + strlen(host) + strlen(realm) + 3;
	s = malloc(len);
	if (s == NULL) {
		Log("Out of memory");
		return (GSS_S_FAILURE);
	}
	strlcpy(s, service, len);
	strlcat(s, "/", len);
	strlcat(s, host, len);
	strlcat(s, "@", len);
	strlcat(s, realm, len);

	str_to_buf(s, &name_buf);

	Info("Importing kerberos principal service name %s\n", s);

	major = gss_import_name(minor, &name_buf,
				GSS_KRB5_NT_PRINCIPAL_NAME, svcname);
	free(s);
	return (major);
}

static uint32_t
construct_hostbased_service_name(uint32_t *minor, const char *service, const char *host, gss_name_t *svcname)
{
	size_t len;
	char *s;
	gss_buffer_desc name_buf;
	uint32_t major;

	len = strlen(service) + strlen(host) + 2;
	s = malloc(len);
	if (s == NULL) {
		Log("Out of memory");
		return (GSS_S_FAILURE);
	}
	strlcpy(s, service, len);
	strlcat(s, "@", len);
	strlcat(s, host, len);

	str_to_buf(s, &name_buf);

	Info("Importing host based service name %s\n", s);

	major = gss_import_name(minor, &name_buf, GSS_C_NT_HOSTBASED_SERVICE, svcname);

	Debug("gss_import_name returned %#K", major);

	free(s);
	return (major);
}

/*
 * str_to_svc_name: Given a string representation of a service name, convert it
 * into a  gss service name of name type GSS_KRB5_NT_PRINCIPAL_NAME.
 *
 * We return GSS_S_COMPLETE if we can produce the  service name.
 */

#define LKDCPREFIX "LKDC:"

static uint32_t
str_to_svc_name(uint32_t *minor, const char *svcstr, gss_name_t *svcname)
{
	uint32_t major __unused /* To make the static analyser happy */;
	char *realm = NULL /* default_realm */, *host;
	char lhost[MAXHOSTNAMELEN+1];
	char *s, *p, *service;
	char *fqdn = NULL;
	int is_lkdc;
	krb5_realm *realms = NULL;

	*minor = 0;
	major = GSS_S_FAILURE;

	if (svcstr == NULL) {
		Log("Null service name string\n");
		return (GSS_S_FAILURE);
	}
	DEBUG(3, "%s\n", svcstr);
	service = strdup(svcstr);
	if (service == NULL) {
		Log("Out of memory\n");
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
		strlcpy(lhost, get_local_host_name(), sizeof(lhost));
		host = lhost;
	} else if (*p == '@') {
		/* Have a host based service name */
		/* Terminate service part of name */
		*p = '\0';

		s = get_next_kerb_component(host);
		if (s != NULL) {
			Info("Invalid host name part %s\n", host);
			free(service);
			return (GSS_S_BAD_NAME);
		}
		major = construct_hostbased_service_name(minor, service, host, svcname);

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
			free(service);
			return (major);
		}
		/* Nope so set the realm to be the default and fall through */
		if (get_local_realms(&realms))
			realm = *realms;
	}
	if (realm == NULL) {
		free(service);
		/*
		 * Force exit in SHUTDOWN_TIMEOUT. Perhaps
		 * we'll pickup a default on next start up.
		 */
		kill(getpid(), SIGTERM);
		return (GSS_S_BAD_NAME);
	}

	/*
	 * At this point we have a service, host, and realm.
	 * We should try and canonicalize the host instance.
	 * N.B. In practice we should not get here. For NFS there
	 * is no realm part. Service name is construted from the
	 * mount on part from the mount structure in the kernel.
	 * SMB does not go through this routine at all.
	 */

	is_lkdc = (strncmp(realm, LKDCPREFIX, strlen(LKDCPREFIX)) == 0);
	if (!is_lkdc) {
		fqdn = canonicalize_host(host);
		if (fqdn)
			host = fqdn;
	}

	/* Don't lowercase an LKDC instance */
	major = construct_service_name(minor, service, host, realm, !is_lkdc, svcname);

	free(service);
	free_local_realms(realms);

	return (major);
}

/*
 * Given the name and name type, convert the name to a gss_name_t. If the name type
 * is a mechanism specific (currently one of the kerberos or NTLM name types), we will set the mechtype
 * passed in to be that mechanism type. We do this so that we will acquire that mechanism specific
 * credential in do_acquire_cred. This is important when the mechanism being used is SPNEGO and
 * we end up trying to use the wrong credential.
 */
static uint32_t
blob_to_name(uint32_t *min, gssd_nametype nt, gssd_byte_buffer name, uint32_t size, gssd_mechtype *mech, char **strrep, char **oidnt, gss_name_t *gname)
{
	uint32_t maj;
	gss_buffer_desc name_buf = { size, name };
	gss_OID name_type;
	*min = GSS_S_COMPLETE;

	switch (nt) {
		case GSSD_EXPORT:
			name_type = GSS_C_NT_EXPORT_NAME;
			break;
		case GSSD_ANONYMOUS:
			name_type = GSS_C_NT_ANONYMOUS;
			if (*mech == GSSD_SPNEGO_MECH)
				*mech = GSSD_NTLM_MECH;
			break;
		case GSSD_HOSTBASED:
			name_type = GSS_C_NT_HOSTBASED_SERVICE;
			break;
		case GSSD_USER:
			name_type = GSS_C_NT_USER_NAME;
			break;
		case GSSD_MACHINE_UID:
			name_type = GSS_C_NT_MACHINE_UID_NAME;
			break;
		case GSSD_STRING_UID:
			name_type = GSS_C_NT_STRING_UID_NAME;
			break;
		case GSSD_KRB5_PRINCIPAL:
			name_type = GSS_KRB5_NT_PRINCIPAL_NAME;
			*mech = GSSD_KRB5_MECH;
			break;
		case GSSD_UUID:
			name_type = GSS_C_NT_UUID;
			*mech = GSSD_IAKERB_MECH;
			break;
		case GSSD_KRB5_REFERRAL:
			name_type = GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL;
			*mech = GSSD_KRB5_MECH;
			break;
		case GSSD_NTLM_PRINCIPAL:
			name_type = GSS_C_NT_NTLM;
			*mech = GSSD_NTLM_MECH;
			break;
		case GSSD_NTLM_BLOB:
		default:
			return (GSS_S_BAD_NAMETYPE);
	}

	maj = gss_import_name(min, &name_buf, name_type, gname);

	if (maj != GSS_S_COMPLETE || get_debug_level() > 1) {
		char *ntstr = oid_name(name_type);
		Info("gss_import_name returned %#K; %#k for %.*s using %s name type",
		     maj, mechtab[*mech], *min, size, name, ntstr);
		free(ntstr);
	}
	if (maj == GSS_S_COMPLETE && strrep) {
		uint32_t dmaj, dmin;
		gss_buffer_desc dbuf;
		gss_OID oid;

		dmaj = gss_display_name(&dmin, *gname, &dbuf, &oid);
		DEBUG(3, "gss_display_name returned %#K", dmaj);
		*strrep  = (dmaj == GSS_S_COMPLETE) ? buf_to_str(&dbuf) : strdup("unknown");
		if (oidnt)
			*oidnt = oid_name(oid);
	}

	return (maj);
}

static uint32_t
blob_to_svcname(uint32_t *min, gssd_nametype nt, gssd_byte_buffer svc_princ, uint32_t size,
		gssd_mechtype mech, gss_name_t *svcname)
{
	*min = GSS_S_COMPLETE;

	switch (nt) {
	case GSSD_STRING_NAME:
		return (str_to_svc_name(min, (char *)svc_princ, svcname));
	default:
		return (blob_to_name(min, nt, svc_princ, size, &mech, NULL, NULL, svcname));
	}
}

/*
 * Figure out who nobody is and how big a buffer we need to fetch password entries.
 * If we're logging at a debug level print out the default realm if we can.
 */
static void
gssd_init(void)
{
	struct passwd *pwent;
	struct group *grent;

	/* Set up mech table */
	mechtab[GSSD_KRB5_MECH] = GSS_KRB5_MECHANISM;
	mechtab[GSSD_SPNEGO_MECH] = GSS_SPNEGO_MECHANISM;
	mechtab[GSSD_NTLM_MECH] = GSS_NTLM_MECHANISM;
	mechtab[GSSD_IAKERB_MECH] = GSS_IAKERB_MECHANISM;

	/*
	 * Turn off home directory access during startup.
	 * XXX Will need a more flexible policy to handle
	 * apps that may want home dir access.
	 */
	krb5_set_home_dir_access(NULL, FALSE);

	pwent = getpwnam("nobody");
	NobodyUid = pwent ? pwent->pw_uid : NOBODY;
	grent = getgrnam("nobody");
	NobodyGid = grent ? grent->gr_gid : NOBODY;

	/* Figure out how big a buffer we need for getting pwd entries */
	GetPWMaxRSz = sysconf(_SC_GETPW_R_SIZE_MAX);
	GetPWMaxRSz = (GetPWMaxRSz == -1) ? 512 : GetPWMaxRSz;

	Debug("Starting with pid = %d\n\n\n", getpid());
	if (get_debug_level()) {
		krb5_realm *realms = NULL;
		krb5_realm drealm = NULL;

		if (get_local_realms(&realms))
			drealm = *realms;
		Info("Kerberos default realm is %s\n\n",
		     drealm ? drealm : "No realm");
		free_local_realms(realms);
	}
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


#ifdef VDEBUG
		DEBUG(3, "Enter receive_message %p with transaction count = %lu, "
			"standby count = %lu\n", pthread_self(),
			_vproc_transaction_count(), _vproc_standby_count());
#endif
	pthread_setname_np("mach_msg_server thread");
	kr = mach_msg_server_once(gssd_mach_server, MAX_GSSD_MSG_SIZE,
			gssd_receive_right,
			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));


#ifdef VDEBUG
		DEBUG(3, "Leaving receive_message %p with transaction count = %lu, "
			"standby count = %lu\n", pthread_self(),
			_vproc_transaction_count(), _vproc_standby_count());
#endif

	if (kr != KERN_SUCCESS)  {
		Log("mach_msg_server(mp): %s\n", mach_error_string(kr));
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
#define MAXTHREADNAME 24

static void
new_worker_thread(void)
{
	pthread_t thread;
	char thread_name[MAXTHREADNAME];
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
		Info("unable to create worker thread: %s", strerror(error));
		numthreads--;
	}

out:

	snprintf(thread_name, sizeof (thread_name), "worker thread %d", numthreads);
	thread_name[MAXTHREADNAME - 1] = '\0';
	pthread_setname_np(thread_name);
	DEBUG(3, "Starting %s\n", thread_name);

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

	if (get_debug_level() > 2) {
		char thread_name[MAXTHREADNAME];
		pthread_getname_np(pthread_self(), thread_name, sizeof thread_name);
		DEBUG(3, "Ending %s. Number of worker threads running is %d\n", thread_name, numthreads);
	}

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
	sigset_t quitset[1];

	pthread_setname_np("Signal thread");

	sigemptyset(quitset);
	sigaddset(quitset, SIGQUIT);

	do {
		int debug_level = get_debug_level();

		if (sigwait(waitset, &sig))
			Log("sigwait failed %s", strerror(errno));

		Debug("Received signal %d\n", sig);
		switch (sig) {
		case SIGQUIT:
			if (get_debug_level() > 1)
				die = 1;
			else {
				pthread_sigmask(SIG_UNBLOCK, quitset, NULL);
				raise(SIGQUIT);
			}
			break;
		case SIGUSR1:
			debug_level++;
			break;
		case SIGUSR2:
			if (debug_level)
				debug_level--;
			break;
		case SIGHUP:
			debug_level = !debug_level;
			break;
		}
		set_debug_level(debug_level);
	} while (sigismember(contset, sig) || sig == 0);

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

	pthread_setname_np("Timeout thread");
	(void) pthread_mutex_lock(numthreads_lock);

	/*
	 * Note that we have an extra thread running waiting for a mach message,
	 * the first of which was started in main. Hence we have the test below for
	 * greater than one instead of zero.
	 */
	while (bye ? (rv == 0 && numthreads > 1) : (rv == 0 || get_debug_level() || numthreads > 1)) {
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

		set_debug_level(-1);

		DEBUG(4, "timeout_thread: rv = %s %d\n",
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
		Log("Could not allocate vm in vm_alloc_buffer\n");
		return;
	}
	*len = (uint32_t) buf->length;
	memcpy(*value, buf->value, *len);
}

/*
 * Extract the session key from a completed gss context. Currently the only
 * supported mechanism is kerberos and NTLM. Note the extracted key has been vm_allocated
 * and will be released by mig. (See gssd_mach.defs)
 * XXX this is extraordinarily yuckie.
 */


static gss_OID kerb_mechs[] = {
	GSS_KRB5_MECHANISM,
	GSS_IAKERB_MECHANISM,
	GSS_PKU2U_MECHANISM,
	NULL
};

static bool
is_kerberos_key_mech(gss_const_OID mech)
{
	gss_OID *p;

	for (p = kerb_mechs; *p; p++) {
		if (gss_oid_equal(mech, *p))
			return (true);
	}

	return (false);
}

static uint32_t
GetSessionKey(uint32_t *minor, gss_OID mech, gss_ctx_id_t ctx,
	      gssd_byte_buffer *skey, mach_msg_type_number_t *skeyCnt, int lucid)
{
	gss_krb5_lucid_context_v1_t *lucid_ctx = NULL;
	gss_krb5_lucid_key_t *key;
	void  *some_lucid_ctx;
	uint32_t maj_stat, min_stat;
	uint32_t vers;
	gss_buffer_desc buf;

	*skey = NULL;
	*skeyCnt = 0;
	*minor = 0;

	if (gss_oid_equal(mech, GSS_NTLM_MECHANISM)) {
		gss_buffer_set_t keys;
		maj_stat = gss_inquire_sec_context_by_oid(minor, ctx, GSS_NTLM_GET_SESSION_KEY_X, &keys);
		if (maj_stat != GSS_S_COMPLETE)
			return (maj_stat);

		if (keys->count) {
			if (keys->count > 1)
				Info("GetSessionKey received multiple keys. Using first key of %d keys\n", (uint32_t)keys->count);
			vm_alloc_buffer(&keys->elements[0], skey, skeyCnt);
			if (*skey == NULL) {
				Log("Out of memory in GetSessionKey\n");
				return (GSS_S_FAILURE);
			}
		}
		(void)gss_release_buffer_set(&min_stat, &keys);
		return (GSS_S_COMPLETE);

	} else if (is_kerberos_key_mech(mech)) {
		DEBUG(4, "Calling  gss_krb5_export_lucid_sec_context\n");
		maj_stat = gss_krb5_export_lucid_sec_context(minor, &ctx,
							     1, &some_lucid_ctx);
		DEBUG(3, "gss_krb5_export_lucid_sec_context returned %#K; %#k", maj_stat, mech, *minor);

		if (maj_stat != GSS_S_COMPLETE) {
			return (maj_stat);
		}

		vers = ((gss_krb5_lucid_context_version_t *)some_lucid_ctx)->version;
		switch (vers) {
			case 1:
				lucid_ctx = (gss_krb5_lucid_context_v1_t *)some_lucid_ctx;
				break;
			default:
				Log("Lucid version %d is unsupported\n", vers);
				(void) gss_krb5_free_lucid_sec_context(&min_stat, lucid_ctx);
				return (GSS_S_UNAVAILABLE);
		}
		DEBUG(4, "vers = %d, protocol = %d\n",  vers, lucid_ctx->protocol);

		switch (lucid_ctx->protocol) {
		case 0:
			DEBUG(4, "Got rfc1964\n");
			key = &lucid_ctx->rfc1964_kd.ctx_key;
			break;
		case 1:
			key = lucid_ctx->cfx_kd.have_acceptor_subkey ?
				&lucid_ctx->cfx_kd.acceptor_subkey :
			&lucid_ctx->cfx_kd.ctx_key;
			break;
		default:
			(void) gss_krb5_free_lucid_sec_context(&min_stat, lucid_ctx);
			return (GSS_S_CALL_BAD_STRUCTURE);  /* should never happen. */
		}

		DEBUG(4, "lucid key type = %d\n", key->type);
		if (lucid) {
			Info("exporting lucid context\n");
			if (!make_lucid_stream(lucid_ctx, &buf.length, &buf.value)) {
				Log("Could not encode lucid context\n");
				free(buf.value);
				return (GSS_S_FAILURE);
			}
		} else {
			buf.length = key->length;
			buf.value  = key->data;
		}

		vm_alloc_buffer(&buf, skey, skeyCnt);
		if (*skey == NULL) {
			Log("Out of memory in GetSessionKey\n");
			if (lucid)
				free(buf.value);
			return (GSS_S_FAILURE);
		}

		if (lucid)
			free(buf.value);
		(void) gss_krb5_free_lucid_sec_context(&min_stat, lucid_ctx);
		return (GSS_S_COMPLETE);
	}

	maj_stat = gss_oid_to_str(&min_stat, mech, &buf);
	if (maj_stat == GSS_S_COMPLETE) {
		char *oidstr = buf_to_str(&buf);
		Info("Unsupported mechanism for key extraction: %s\n", oidstr);
		free(oidstr);
	} else {
		Info("Unsupported mechanism for key extraction.\n");
	}

	return (GSS_S_COMPLETE);
}

/*
 * If we get a call and the verifier does not match, clear out the args for
 * the client.
 */
static uint32_t
badcall(char *rtn, uint32_t *minor_stat,
	gssd_ctx *gss_context, gssd_cred *cred_handle, uint32_t *gssd_flags,
	gssd_byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken, mach_msg_type_number_t *otokenCnt)
{

	if (!gssd_check(CAST(void *, *gss_context)))
	    Info("Bad context found %p\n", (void *)(uintptr_t)*gss_context);
	if (!gssd_check(CAST(void *, *cred_handle)))
	    Info("Bad cred handle found %p\n", (void *)(uintptr_t)*cred_handle);
	Log("%s request not addressed to us\n", rtn);
	*minor_stat = 0;
	*gss_context = CAST(gssd_ctx, GSS_C_NO_CONTEXT);
	*cred_handle = CAST(gssd_cred, GSS_C_NO_CREDENTIAL);
	*gssd_flags = 0;
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
gss_name_to_kprinc(uint32_t *minor, gss_name_t name, krb5_principal *princ, krb5_context kctx)
{
	uint32_t major, m;
	gss_name_t kname = GSS_C_NO_NAME;
	gss_buffer_desc dname;
	char *strname = NULL;

	*minor = 0;
	major = gss_canonicalize_name(minor, name, GSS_KRB5_MECHANISM, &kname);
	if (major != GSS_S_COMPLETE)
		return (major);

	major = gss_display_name(minor, kname, &dname, NULL);
	(void) gss_release_name(&m, &kname);
	if (major != GSS_S_COMPLETE)
		return (major);

	strname = buf_to_str(&dname);
	if (strname == NULL) {
		return (GSS_S_FAILURE);
	}

	DEBUG(3, "parsing %s\n", strname);
	*minor = krb5_parse_name(kctx, strname, princ);

	major = (uint32_t) (*minor ? GSS_S_FAILURE : GSS_S_COMPLETE);
	free(strname);

	return (major);
}

static uint32_t
gssd_set_allowable_keytypes(uint32_t *minor, gss_cred_id_t cred_handle, gss_name_t service, uint32_t keytypesCnt, int32_t *keytypes)
{
	krb5_context kctx;
	krb5_principal kprinc = NULL, kservice = NULL;
	krb5_ccache ccache = NULL;
	krb5_creds mcred, rcred;
	gss_name_t name;
	uint32_t major, m;

	/* Initialize creds */
	krb5_cc_clear_mcred(&mcred);
	krb5_cc_clear_mcred(&rcred);

	/* Set the allowable key types for this credential to be used when getting service tickets */
	major = gss_krb5_set_allowable_enctypes(minor, cred_handle, keytypesCnt, keytypes);
	if (service == GSS_C_NO_NAME || major != GSS_S_COMPLETE) {
		if (major != GSS_S_COMPLETE)
			Log("Could not set enctypes for cred");
		return (major);
	}

	/*
	 * Check if the service ticket for "service" has already been fetched.
	 * If it has and the key type is not one we want we need to remove that
	 * credential from the cache backing this cred_handle. So that a fetch of a new
	 * service credential with one of the supported key types.
	 */

	major = gss_inquire_cred_by_mech(minor, cred_handle, GSS_KRB5_MECHANISM, &name, NULL, NULL, NULL);
	if (major != GSS_S_COMPLETE) {
		Info("gss_inquire_cred failed %K, minor = %#k", major, GSS_KRB5_MECHANISM, *minor);
		return (major);
	}
	*minor = (uint32_t)krb5_init_context(&kctx);
	if (*minor) {
		Info("Could not establish krb5 context");
		gss_release_name(&m, &name);
		return (GSS_S_FAILURE);
	}

	major = gss_name_to_kprinc(minor, name, &kprinc, kctx);
	if (major != GSS_S_COMPLETE) {
		Info("gss_name_to_kprinc failed: %K minor = %#k", major, GSS_KRB5_MECHANISM, *minor);
		goto out;
	}
	major = gss_name_to_kprinc(minor, service, &kservice, kctx);
	if (major != GSS_S_COMPLETE) {
		Info("gss_name_to_kprinc failed for service: %K minor = %#k", major, GSS_KRB5_MECHANISM, *minor);
		goto out;
	}
	if (kprinc)
		*minor = krb5_cc_cache_match(kctx, kprinc, &ccache);
	else
		*minor = krb5_cc_default(kctx, &ccache);
	if (*minor)  {
		major = GSS_S_FAILURE;
		goto out;
	}

	mcred.server = kservice;
	*minor = (uint32_t)krb5_cc_retrieve_cred(kctx, ccache, 0, &mcred, &rcred);

	if (*minor) {
		if (*minor == (uint32_t)KRB5_CC_NOTFOUND) {
			*minor = 0;
			Debug("Did not find credential");
		} else {
			major = GSS_S_FAILURE;
			Debug("krb_cc_retrieve_cred failed %d", (int)*minor);
		}
		goto out;
	}
	Debug("Found matching credential");

	/* See if the credential session keytype matches one of our keytypes */
	for (uint32_t i = 0; i < keytypesCnt; i++) {
		/* If we have a match we're done */
		if (rcred.session.keytype == keytypes[i]) {
			Debug("Found matching keytype %d", keytypes[i]);
			goto out;
		}
	}

	/*
	 * We have a matched service but the key type dosen't match
	 * one in our list. Lets attempt to remove it.
	 */
	*minor = krb5_cc_remove_cred(kctx, ccache, 0, &mcred);
	if (*minor) {
		Info("Could not remove credential from cache");
		major = GSS_S_FAILURE;
	} else {
		Debug("Removed matching credential");
	}
out:
	gss_release_name(&m, &name);
	if (kprinc)
		krb5_free_principal(kctx, kprinc);
	if (kservice)
		krb5_free_principal(kctx, kservice);
	if (ccache)
		krb5_cc_close(kctx, ccache);
	if (rcred.server)
		krb5_free_cred_contents(kctx, &rcred);
	krb5_free_context(kctx);

	return (major);
}

/*
 * krb5_find_principal
 *
 * Given a kerberos service principal return the name of
 * a principal in the credential cache collection that
 * is not expired and is in the same realm as the server
 * principal. If no or only expired principals are found
 * return NULL. In addition in the flags field we return
 * KFCN_ALIVE if there is at least one non expired cache
 * in the collection. And KFCN_EXPIRED if the only matching
 * credential found is expired. So if the flags field
 * equals KFCN_EXPIRED, we just return expired and not bother
 * driving on in find_realm_principal. Note that KFCN_EXPIRED
 * being set means that the return principal name is NULL.
 */

#define KFCN_ALIVE 1
#define KFCN_EXPIRED 2

static char*
krb5_find_principal(krb5_context kcontext, krb5_principal sprinc, int *flags)
{
	krb5_error_code error, err;
	krb5_cc_cache_cursor cursor;
	krb5_ccache ccache;
	krb5_principal ccache_princ;
	char *kname = NULL;
	time_t ltime;
	const char *msg = NULL;
	int cnt = 0;
	*flags = 0;

	err = krb5_cc_cache_get_first(kcontext, NULL, &cursor);
	if (err) {
		msg = krb5_get_error_message(kcontext, err);
		Info("Could not get cache collection cursor %s\n", msg);
		krb5_free_error_message(kcontext, msg);
		return (NULL);
	}
	while (!(error = krb5_cc_cache_next(kcontext, cursor, &ccache))) {
		int isdead = 0;
		cnt += 1;

		err = krb5_cc_get_principal(kcontext, ccache, &ccache_princ);
		if (err) {
			krb5_cc_close(kcontext, ccache);
			msg = krb5_get_error_message(kcontext, err);
			Info("krb5_cc_get_principal error: %s\n", msg);
			krb5_free_error_message(kcontext, msg);
			continue;
		}

		err = krb5_cc_get_lifetime(kcontext, ccache, &ltime);

		if (ltime <= 0) {
			if (err && err != KRB5_CC_END) {
				msg = krb5_get_error_message(kcontext, err);
				Info("krb5_cc_get_lifetime error: %s\n", msg);
				krb5_free_error_message(kcontext, msg);
			}
			isdead = 1;
		} else {
			*flags |= KFCN_ALIVE;
		}

		if (krb5_realm_compare(kcontext, sprinc, ccache_princ)) {
			(void) krb5_unparse_name(kcontext, ccache_princ, &kname);
			krb5_free_principal(kcontext, ccache_princ);
			Info("Found  %d: principal %s lifetime %ld\n",
			     cnt, kname ? kname : "could not get principal name", ltime);
			if (!isdead) {
				krb5_cc_close(kcontext, ccache);
				*flags &= ~KFCN_EXPIRED;
				break;
			} else {
				*flags |= KFCN_EXPIRED;
				free(kname);
				kname = NULL;
			}
		} else {
			(void) krb5_free_principal(kcontext, ccache_princ);
		}

		krb5_cc_close(kcontext, ccache);
	}
	if (error && error != KRB5_CC_END) {
		msg = krb5_get_error_message(kcontext, error);
		Log("Could not iterate through cache collections: %s\n", msg);
		krb5_free_error_message(kcontext, msg);
	}
	(void) krb5_cc_cache_end_seq_get(kcontext, cursor);

	return (kname);
}

/*
 * find_realm_principal:
 * Given a service principal try and find a non-expired principal in the cache
 * in the same realm so that calls to gss_init_sec_context will work.
 * Currently this only groks kerberos. This is useful if credentials have
 * been acquired (kinit'ed) that aren't in the service's realm after the
 * a TGT has been acquired in the service's realm. Cross realm relationships may
 * not do what you want here, so this has been made optional. To enable this
 * the launchd plist file will need to be modified to add the realm matching option
 * of "-R".
 */
static uint32_t
find_realm_principal(uint32_t *minor, gss_name_t sname,  char **name, size_t *size)
{
	krb5_principal sprinc;
	uint32_t major;
	char *pname = NULL;
	krb5_context kctx;
	int error, flags;

	*minor = 0;
	*size = 0;
	*name = NULL;

	if (!realm_matching)
		return (GSS_S_COMPLETE);

	error = krb5_init_context(&kctx);
	if (error) {
		Log("Can't get kerberos context");
		return (GSS_S_FAILURE);
	}

	major = gss_name_to_kprinc(minor, sname, &sprinc, kctx);
	if (major != GSS_S_COMPLETE) {
		krb5_free_context(kctx);
		Debug("Could not convert gss name to kerberos principal %#K %#k\n", major, GSS_KRB5_MECHANISM, *minor);
		return (major);
	}

	pname = krb5_find_principal(kctx, sprinc, &flags);
	krb5_free_principal(kctx, sprinc);
	krb5_free_context(kctx);
	Debug("Using principal <%s> flags = %d\n", pname ? pname : "Default", flags);
	if (flags == KFCN_EXPIRED)
		return (GSS_S_CREDENTIALS_EXPIRED);
	if (pname)
		*size = strlen(pname);
	*name = pname;

	return (GSS_S_COMPLETE);
}

static uint32_t
do_acquire_cred(uint32_t *, gssd_nametype, gssd_byte_buffer, uint32_t, gssd_mechtype, gss_cred_id_t *);

static uint32_t
do_acquire_cred_v1(uint32_t *minor, char *principal, gssd_mechtype mech, gss_name_t sname, uint32_t uid,
		   gss_cred_id_t *cred_handle, uint32_t flags)
{
	uint32_t major = GSS_S_FAILURE, mstat;
	gss_buffer_desc buf_name;
	gss_name_t clnt_gss_name;
	gss_OID_set mechset = GSS_C_NULL_OID_SET;
	gss_OID name_type = GSS_KRB5_NT_PRINCIPAL_NAME;
	size_t size;

	if (principal == NULL) {
		major = find_realm_principal(minor, sname,  &principal, &size);
		if (major) {
			free(principal);
			return (major);
		}
		if (principal) {
			major =  do_acquire_cred(minor, GSSD_KRB5_PRINCIPAL, (gssd_byte_buffer)principal,
						 (uint32_t)size, GSSD_KRB5_MECH, cred_handle);
			free(principal);
			return (major);
		}
	}

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

		Info("importing name %s with Kerberos\n", principal);

	retry:
		major = gss_import_name(minor, &buf_name, name_type, &clnt_gss_name);
		if (major == GSS_S_COMPLETE) {
			char  *nt_oid;
			major = gss_acquire_cred(
						 minor,
						 clnt_gss_name,
						 GSS_C_INDEFINITE,
						 mechset,
						 GSS_C_INITIATE,
						 cred_handle,
						 NULL, NULL);
			nt_oid = oid_name(name_type);
			Info("gss_acuire_cred for %s using %s, returned: %K; %#k", principal, nt_oid, major, mechtab[mech], *minor);
			free(nt_oid);
			if (major == GSS_S_COMPLETE) {
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
			(void) gss_release_name(&mstat, &clnt_gss_name);
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
					 cred_handle,
					 NULL, NULL);

		if (major == GSS_S_COMPLETE) {
			Info("Using default credential %p\n", *(gss_cred_id_t *)cred_handle);
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
				 cred_handle,
				 NULL, NULL);
	Info("Trying to aquire cred with uid %d. Returned %#K; %#k", uid, major, mechtab[mech], *minor);

	/* Done with the name */
	(void) gss_release_name(&mstat, &clnt_gss_name);
done:
	if (mechset != GSS_C_NULL_OID_SET)
		gss_release_oid_set(&mstat, &mechset);

	return (major);
}

static uint32_t
do_acquire_cred(uint32_t *minor_stat, gssd_nametype nt, gssd_byte_buffer name, uint32_t size,
		gssd_mechtype mech, gss_cred_id_t *handle)
{
	uint32_t maj, min, nmaj;
	gss_OID_set mechset = GSS_C_NULL_OID_SET;
	gss_name_t gname = GSS_C_NO_NAME;
	char *mech_name= NULL;
	char *princ_name = NULL;
	char *oid_nt = NULL;

	*minor_stat = GSS_S_COMPLETE;

	if (handle == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_CALL_INACCESSIBLE_WRITE);

	maj = gss_create_empty_oid_set(minor_stat, &mechset);
	if (maj != GSS_S_COMPLETE)
		return (maj);

	/*
	 * Convert the name blob to a gss_name_t, giving back the string representation for
	 * the name and the name type oid passed in. In addition if the name type was a
	 * mech specific name type adjust the mech to reflect that. That mechanism will
	 * then be added as the only member to the mech set below, and thus we will only
	 * acquire credentials for that mech. This is important for SPNEGO, if we don't do
	 * that, then SPNEGO may try mechanism we are not interested in.
	 */
	nmaj = blob_to_name(minor_stat, nt, name, size, &mech, &princ_name, &oid_nt, &gname);

	maj = gss_add_oid_set_member(minor_stat, mechtab[mech], &mechset);
	if (maj != GSS_S_COMPLETE)
		goto done;

	/* If we can't convert to a gss_name_t try the default with the possibly adjusted mech type */
	if (nmaj != GSS_S_COMPLETE)
		goto do_default;

	mech_name = oid_name(mechtab[mech]);
	Info("Acquiring credentials for %s with %s name type using %s mechanism",
	     princ_name, oid_nt, mech_name ? mech_name : "Unknown");
	free(mech_name);

	maj = gss_acquire_cred(minor_stat,
			       gname,
			       GSS_C_INDEFINITE,
			       mechset,
			       GSS_C_INITIATE,
			       handle,
			       NULL, NULL);

	(void)gss_release_name(&min, &gname);
	Info("Acquiring passed in credentials %K; %#k", maj, mechtab[mech], *minor_stat);
	if (maj == GSS_S_COMPLETE)
		goto done;

do_default:
	if (!acquire_default)
		goto done;

	/* Use the default in gss_init_sec_context */
	maj = gss_acquire_cred(
			       minor_stat,
			       GSS_C_NO_NAME,
			       GSS_C_INDEFINITE,
			       mechset,
			       GSS_C_INITIATE,
			       handle,
			       NULL, NULL);

	if (maj == GSS_S_COMPLETE) {
		Info("Using default credential %p\n", (void *) *handle);
	} else {
		Info("Using null credential\n");
		*handle = GSS_C_NO_CREDENTIAL;
		maj = GSS_S_COMPLETE;
	}
done:
	if (mechset != GSS_C_NULL_OID_SET)
		(void) gss_release_oid_set(&min, &mechset);
	if (gname != GSS_C_NO_NAME)
		(void) gss_release_name(&min, &gname);
	free(princ_name);
	free(oid_nt);

	return (maj);
}

/*
 * gssd_context type and routines to hold the underlying gss context as well
 * as the service name and the vproc_transaction handle. The latter keeps
 * gssd "dirty" when we have an oustanding context setup.
 */
typedef struct {
	gss_ctx_id_t gss_cntx;
	gss_name_t   svc_name;
	vproc_transaction_t trans_handle;
} gssd_context, *gssd_context_t;

static gssd_ctx
gssd_set_context(gss_ctx_id_t ctx, gss_name_t svc_name)
{
	gssd_context_t g;

	g = malloc(sizeof (gssd_context));
	if (g == NULL)
		return (CAST(gssd_ctx, GSS_C_NO_CONTEXT));
	gssd_enter(g);

	g->gss_cntx = ctx;
	g->svc_name = svc_name;
	g->trans_handle = vproc_transaction_begin(NULL);

	return (CAST(gssd_ctx, g));
}

static gss_ctx_id_t
gssd_get_context(gssd_ctx ctx, gss_name_t *svc_name)
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

static uint32_t
svc_mach_gss_init_sec_context_common(
				     gssd_mechtype mech,
				     gssd_byte_buffer itoken, mach_msg_type_number_t itokenCnt,
				     gss_name_t svcid,
				     uint32_t flags,
				     uint32_t *gssd_flags,
				     gss_ctx_id_t  *context,
				     gss_cred_id_t cred_handle,
				     uint32_t *ret_flags,
				     gssd_byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
				     gssd_byte_buffer *otoken, mach_msg_type_number_t *otokenCnt,
				     gssd_dstring  displayname,
				     uint32_t *minor_stat)
{
	gss_buffer_desc intoken = {itokenCnt, itoken};
	gss_buffer_desc outtoken = {0, NULL};
	gss_buffer_desc name_buf;
	gss_name_t source;
	gss_OID mech_oid;
	uint32_t major_stat;
	uint32_t major, minor;
	int  lucid = ((*gssd_flags & GSSD_LUCID_CONTEXT) == GSSD_LUCID_CONTEXT);

	Debug("Using mech = %d\n", mech);
	DEBUG(3, "\tcred_handle = %p\n", cred_handle);
	DEBUG(3, "\tgss_context = %p\n", context);

	if (!context)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_CALL_INACCESSIBLE_WRITE);
	DEBUG(3, "\t*gss_context = %p\n", (void *)*context);
	Debug("itokenCnt = %d\n", itokenCnt);
	HEXDUMP(2, (char *)itoken, (itokenCnt > 80) ? 80 : itokenCnt);
	if (die) {
		Debug("Forced server death\n");
		_exit(0);
	}

	*gssd_flags = 0;

	major_stat = gss_init_sec_context(
					  minor_stat,
					  cred_handle,		/* User's credential handle */
					  context,		/* Context handle */
					  svcid,		/* Target name */
					  mechtab[mech],	/* Use the requested mech */
					  flags,		/* Request flag bits */
					  0,			/* Time requirement */
					  NULL,		/* Channel bindings */
					  &intoken,		/* Token from context acceptor */
					  &mech_oid,		/* Actual mech types */
					  &outtoken,		/* Token for the context acceptor */
					  ret_flags,		/* Returned flag bits */
					  NULL);		/* Time valid */

	if (major_stat == GSS_S_COMPLETE || major_stat == GSS_S_CONTINUE_NEEDED) {
		/* Set up OOL buffer for the otoken */
		vm_alloc_buffer(&outtoken, otoken, otokenCnt);
		/* and release our gss buffer */
		gss_release_buffer(&minor, &outtoken);
	}

	if (major_stat == GSS_S_COMPLETE) {
		/*
		 * If requeseted return a display representation to the caller.
		 */
		if (displayname) {
			major = gss_inquire_context(&minor, *context, &source,
						    NULL, NULL, NULL, NULL, NULL, NULL);
			if (major == GSS_S_COMPLETE) {
				major = gss_display_name(&minor, source, &name_buf, NULL);
				if (major == GSS_S_COMPLETE) {
					char *s = buf_to_str(&name_buf);
					strlcpy(displayname, s, MAX_DISPLAY_STR);
					free(s);
				}
				gss_release_name(&minor, &source);
			}
		}

		if (gss_oid_equal(mech_oid, GSS_NTLM_MECHANISM)) {
			gss_buffer_set_t data;

			major = gss_inquire_sec_context_by_oid(&minor, *context, GSS_C_NTLM_GUEST, &data);
			if (major == GSS_S_COMPLETE) {
				uint32_t guest_flag = *(uint32_t *)data->elements->value;
				if (guest_flag) {
					*gssd_flags |= GSSD_GUEST_ONLY;
					DEBUG(3, "\tContext is NTLM simple file sharing %x\n", guest_flag);
				} else {
					DEBUG(3, "\tContext is NOT NTLM simple file sharing\n");
				}
				(void) gss_release_buffer_set(&minor, &data);
			} else {
				Info("gss_inquire_sec_context_by_oid returned %K; %#k", major, mechtab[mech], minor);
			}
		}

		/*
		 * Fetch the (sub)session key from the context
		 */
		Debug("lucid flag is %sset\n", lucid ? "" : "not ");
		major_stat = GetSessionKey(minor_stat, mech_oid, *context,
					   skey, skeyCnt, lucid);

		Debug("Client key: length = %d\n", *skeyCnt);
		HEXDUMP(2, (char *) *skey, *skeyCnt);
	}


	initCnt++;
	if (major_stat != GSS_S_CONTINUE_NEEDED && major_stat != GSS_S_COMPLETE)
		initErr++;

	DEBUG(3, "cred = %p\n", cred_handle);
	DEBUG(3, "\tgss_context = %p\n", context ? *context : NULL);
	Debug("%sotokenCnt = %d\n", get_debug_level() > 2 ? "\t" : "", *otokenCnt);
	HEXDUMP(2, (char *)*otoken, (*otokenCnt > 80) ? 80 : *otokenCnt);
	DEBUG(3, "Returning from init %d errors out of a total %d calls\n", initErr, initCnt);



	return (major_stat);
}

/*
 * Mig dispatch routine for gss_init_sec_context.
 */
kern_return_t
svc_mach_gss_init_sec_context(
	mach_port_t server,
	gssd_mechtype mech,
	gssd_byte_buffer itoken, mach_msg_type_number_t itokenCnt,
	uint32_t uid,
	gssd_string princ_namestr,
	gssd_string svc_namestr,
	uint32_t flags,
	uint32_t gssd_flags,
	gssd_ctx *gss_context,
	gssd_cred *cred_handle,
	audit_token_t atok,
	uint32_t *ret_flags,
	gssd_byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken, mach_msg_type_number_t *otokenCnt,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	kern_return_t kstat;

	kstat = svc_mach_gss_init_sec_context_v2(server,
						 mech,
						 itoken,
						 itokenCnt,
						 uid,
						 GSSD_STRING_NAME,
						 (gssd_byte_buffer) princ_namestr,
						 (uint32_t) strlen(princ_namestr) + 1,
						 GSSD_STRING_NAME,
						 (gssd_byte_buffer) svc_namestr,
						 (uint32_t) strlen(svc_namestr) + 1,
						 flags,
						 &gssd_flags,
						 gss_context,
						 cred_handle,
						 atok,
						 ret_flags,
						 skey,
						 skeyCnt,
						 otoken,
						 otokenCnt,
						 NULL,
						 major_stat,
						 minor_stat);
	return (kstat);
}

kern_return_t
svc_mach_gss_init_sec_context_v2(
	mach_port_t server __attribute__((unused)),
	gssd_mechtype mech,
	gssd_byte_buffer itoken,
	mach_msg_type_number_t itokenCnt,
	uint32_t uid,
	gssd_nametype clnt_nt,
	gssd_byte_buffer clnt_princ,
	mach_msg_type_number_t clnt_princCnt,
	gssd_nametype svc_nt,
	gssd_byte_buffer svc_princ,
	mach_msg_type_number_t svc_princCnt,
	uint32_t flags,
	uint32_t *gssd_flags,
	gssd_ctx *gss_context,
	gssd_cred *cred_handle,
	audit_token_t atok,
	uint32_t *ret_flags,
	gssd_byte_buffer *skey,
	mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken,
	mach_msg_type_number_t *otokenCnt,
	gssd_dstring displayname,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	kern_return_t kr;

	kr = svc_mach_gss_init_sec_context_v3(server,
					      mech,
					      itoken, itokenCnt,
					      uid,
					      clnt_nt,
					      clnt_princ, clnt_princCnt,
					      svc_nt,
					      svc_princ, svc_princCnt,
					      flags,
					      NULL, 0,
					      gssd_flags,
					      gss_context,
					      cred_handle,
					      atok,
					      ret_flags,
					      skey, skeyCnt,
					      otoken, otokenCnt,
					      displayname,
					      major_stat,
					      minor_stat);

	return (kr);
}

kern_return_t
svc_mach_gss_init_sec_context_v3(
	mach_port_t server __attribute__((unused)),
	gssd_mechtype mech,
	gssd_byte_buffer itoken,
	mach_msg_type_number_t itokenCnt,
	uint32_t uid,
	gssd_nametype clnt_nt,
	gssd_byte_buffer clnt_princ,
	mach_msg_type_number_t clnt_princCnt,
	gssd_nametype svc_nt,
	gssd_byte_buffer svc_princ,
	mach_msg_type_number_t svc_princCnt,
	uint32_t flags,
	gssd_etype_list etypes,
	mach_msg_type_number_t etypesCnt,
	uint32_t *gssd_flags,
	gssd_ctx *gss_context,
	gssd_cred *cred_handle,
	audit_token_t atok,
	uint32_t *ret_flags,
	gssd_byte_buffer *skey,
	mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken,
	mach_msg_type_number_t *otokenCnt,
	gssd_dstring displayname,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	gss_name_t svc_gss_name;
	gss_ctx_id_t g_cntx = GSS_C_NO_CONTEXT;
	uint32_t i;
	uint32_t mstat;   /* Minor status for cleaning up. */
	vproc_transaction_t gssd_vproc_handle;
	kern_return_t kr = KERN_SUCCESS;
	Debug("Enter uid = %d flags = %8.0x", uid, *gssd_flags);

	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	/* Initialize all output paramaters for MIG */
	*major_stat = GSS_S_COMPLETE;
	*minor_stat = 0;
	if (displayname)
		*displayname = '\0';
	*otokenCnt = *skeyCnt = 0;
	*skey = *otoken = NULL;
	*ret_flags = 0;

	svc_gss_name = GSS_C_NO_NAME;

	if (!check_audit(atok, FALSE)) {
		kr = KERN_NO_ACCESS;
		goto out;
	}

	if (gss_context == NULL || cred_handle == NULL ||
	    !gssd_check(CAST(void *, *gss_context)) || !gssd_check(CAST(void *, *cred_handle))) {
		*major_stat = badcall("svc_mach_gss_init_context",
				      minor_stat, gss_context, cred_handle,
				      gssd_flags,
				      skey, skeyCnt,
				      otoken, otokenCnt);

		kr = KERN_SUCCESS;
		goto out;
	}
	if (*gss_context)
		g_cntx = gssd_get_context(*gss_context, &svc_gss_name);

	/*
	 * Below currently doesn't do anything since the mach defs file has
	 * the major_stat as an out parameter, so *major_stat is always going
	 * to be 0 (GSS_S_COMPLETE). If we ever rev the protocol we should change
	 * that. It's not so bad since gss_init_sec_context will note that the
	 * context is invalid and we will destroy the context on returning.
	 */
	if (*major_stat != GSS_S_CONTINUE_NEEDED && *major_stat != GSS_S_COMPLETE) {
		kr = KERN_SUCCESS;
		goto done;
	}
	if (*gssd_flags & GSSD_RESTART) {
		if (g_cntx != GSS_C_NO_CONTEXT) {
			Debug("Restarting, deleting context %p", g_cntx);
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);
			if (g_cntx != GSS_C_NO_CONTEXT) {
				Debug("gss_delete_sec_context did not clear context");
				g_cntx = GSS_C_NO_CONTEXT;
			}

		}
		if (*cred_handle) {
			gssd_remove(CAST(void *, *cred_handle));
			(void) gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
		}
	}

	if (svc_gss_name == GSS_C_NO_NAME) {
		char *dname;
		gss_buffer_desc bufname;
		uint32_t maj, min;
		gss_OID oid;
		char *oname;

		*major_stat = blob_to_svcname(minor_stat, svc_nt, svc_princ, svc_princCnt,
			mech, &svc_gss_name);

		if (*major_stat != GSS_S_COMPLETE) {
			Info("Could not determine service principal name: %#K", *major_stat);
			goto done;
		}

		maj = gss_display_name(&min, svc_gss_name, &bufname, &oid);
		if (maj != GSS_S_COMPLETE)
			Info("Cannot determine target name: %K", maj);
		else {
			dname = buf_to_str(&bufname);
			oname = oid_name(oid);
			Info("Server principal name %s as %s", dname, oname);
			free(dname);
			free(oname);
		}
	}
	if (CAST(gss_cred_id_t, *cred_handle) == GSS_C_NO_CREDENTIAL) {
		if (clnt_nt == GSSD_STRING_NAME)
			*major_stat = do_acquire_cred_v1(minor_stat, (char *)clnt_princ, mech,
							 svc_gss_name, uid, (gss_cred_id_t *)cred_handle, *gssd_flags);
		else {
			*major_stat = do_acquire_cred(minor_stat, clnt_nt,
						      clnt_princ, clnt_princCnt,
						      mech, (gss_cred_id_t *) cred_handle);
		}
		if (*major_stat != GSS_S_COMPLETE) {
			/* Release the name here. */
			(void)gss_release_name(&mstat, &svc_gss_name);
			goto done;
		}

		/*
		 * if the service supplies an array of kerberos etypes then limit the
		 * etypes for kerberos to that set. Only NFS uses this currently.
		 */
		Debug("enctypes count = %d\n", etypesCnt);
		if (etypesCnt) {
			if (get_debug_level() > 1) {
				char num[4];
				char buf[80];
				size_t j;
				buf[0] = '\0';
				for(i = 0; i < etypesCnt; i++) {
					j = snprintf(num, sizeof (num), "%d ", etypes[i]);
					if (j >= sizeof(num))
						break;
					strlcat(buf, num, sizeof(buf));

				}
				Debug("enctypes:%s\n", buf);
			}
			*major_stat = gssd_set_allowable_keytypes(minor_stat, *(gss_cred_id_t *)cred_handle, svc_gss_name,
								  etypesCnt, etypes);
			if (*major_stat != GSS_S_COMPLETE) {
				Log("Could not set enctypes for serive %K %#k\n", *major_stat, mechtab[mech], *minor_stat);
				/* Ok to fail, we may not have selected a kerberos credential */
				*major_stat = 0;
			}
		}
		gssd_enter(CAST(void *, *cred_handle));
	}

	*major_stat = GSS_S_BAD_NAME;

	*major_stat = svc_mach_gss_init_sec_context_common(
		mech,
		itoken,
		itokenCnt,
		svc_gss_name,
		flags,
		gssd_flags,
		&g_cntx,
		CAST(gss_cred_id_t, *cred_handle),
		ret_flags,
		skey,
		skeyCnt,
		otoken,
		otokenCnt,
		displayname,
		minor_stat);


	if (*major_stat == GSS_S_CONTINUE_NEEDED) {
		*gss_context = gssd_set_context(g_cntx, svc_gss_name);
		if (*gss_context == 0)
			*major_stat = GSS_S_FAILURE;
	}

done:
	Info("svc_mach_gss_init_sec_context_common %K; %#k", *major_stat, mechtab[mech], *minor_stat);
	if (*major_stat != GSS_S_CONTINUE_NEEDED) {
		/* We're done so free what we allocated */
		gssd_remove(CAST(void *, *cred_handle));
		(void) gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
		if (g_cntx != GSS_C_NO_CONTEXT)
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);
		(void)gss_release_name(&mstat, &svc_gss_name);
		*gss_context = (gssd_ctx)GSS_C_NO_CONTEXT;
	}

out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);

	Debug("Exit");

	return (kr);

}

/*
 * Mig dispatch routine for gss_accept_sec_context.
 */
kern_return_t
svc_mach_gss_accept_sec_context(
	mach_port_t test_port,
	gssd_byte_buffer itoken, mach_msg_type_number_t itokenCnt,
	gssd_string svc_namestr,
	uint32_t gssd_flags,
	gssd_ctx *gss_context,
	gssd_cred *cred_handle,
	audit_token_t atok,
	uint32_t *ret_flags,
	uint32_t *uid,
	gssd_gid_list gids, mach_msg_type_number_t *gidsCnt,
	gssd_byte_buffer *skey, mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken, mach_msg_type_number_t *otokenCnt,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	kern_return_t kr;

	kr = svc_mach_gss_accept_sec_context_v2(test_port,
						itoken,
						itokenCnt,
						GSSD_STRING_NAME,
						(gssd_byte_buffer)svc_namestr,
						(uint32_t) strlen(svc_namestr) + 1,
						&gssd_flags,
						gss_context,
						cred_handle,
						atok,
						ret_flags,
						uid,
						gids,
						gidsCnt,
						skey,
						skeyCnt,
						otoken,
						otokenCnt,
						major_stat,
						minor_stat);
	return (kr);
}

kern_return_t
svc_mach_gss_accept_sec_context_v2(
	mach_port_t server __attribute__((unused)),
	gssd_byte_buffer itoken,
	mach_msg_type_number_t itokenCnt,
	gssd_nametype svc_nt __attribute__((unused)),
	gssd_byte_buffer svc_princ __attribute__((unused)),
	mach_msg_type_number_t svc_princCnt __attribute__((unused)),
	uint32_t *inout_gssd_flags __attribute__((unused)),
	gssd_ctx *gss_context,
	gssd_cred *cred_handle,
	audit_token_t atok,
	uint32_t *ret_flags,
	uint32_t *uid,
	gssd_gid_list gids,
	mach_msg_type_number_t *gidsCnt,
	gssd_byte_buffer *skey,
	mach_msg_type_number_t *skeyCnt,
	gssd_byte_buffer *otoken,
	mach_msg_type_number_t *otokenCnt,
	uint32_t *major_stat,
	uint32_t *minor_stat)
{
	gss_ctx_id_t g_cntx = GSS_C_NO_CONTEXT;
	gss_name_t princ;
	gss_OID oid;
	uint32_t mstat;    /* Minor status to clean up with. */
	kern_return_t kr = KERN_SUCCESS;
	vproc_transaction_t gssd_vproc_handle;

	Debug("Enter");
	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	/* Initailize all output parameters to sane values for MIG */
	*major_stat = *minor_stat = 0;
	*otokenCnt = *skeyCnt = 0;
	*skey = *otoken = NULL;
	/* Set the uid/gid to nobody to be safe */
	*uid = NobodyUid;
	*gidsCnt = 1;
	*gids = NobodyGid;
	*ret_flags = 0;

	if (!check_audit(atok, FALSE)) {
		kr = KERN_NO_ACCESS;
		goto out;
	}

	*inout_gssd_flags = 0;

	if (die) {
		Debug("Forced server death\n");
		_exit(0);
	}

	if (!gssd_check(CAST(void *, *gss_context)) || !gssd_check(CAST(void *, *cred_handle))) {
		*major_stat = badcall("svc_mach_gss_accept_sec_context",
				      minor_stat, gss_context, cred_handle,
				      inout_gssd_flags,
				      skey, skeyCnt, otoken, otokenCnt);

		end_worker_thread();
		vproc_transaction_end(NULL, gssd_vproc_handle);

		return (KERN_SUCCESS);
	}

	g_cntx = gssd_get_context(*gss_context, NULL);
	gss_buffer_desc intoken = {itokenCnt, itoken};
	gss_buffer_desc outtoken = {0, NULL};;

	DEBUG(4, "minor_stat = %d\n", (int) *minor_stat);
	DEBUG(4, "\tcred = %p\n", (void *)(uintptr_t)*cred_handle);
	DEBUG(4, "\tgss_context = %p\n", g_cntx);
	DEBUG(3, "itokenCnt = %d\n", itokenCnt);
	HEXDUMP(3, (char *)itoken, (itokenCnt > 80) ? 80 : itokenCnt);

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

	if (*major_stat == GSS_S_CONTINUE_NEEDED || *major_stat == GSS_S_COMPLETE) {
		vm_alloc_buffer(&outtoken, otoken, otokenCnt);
		gss_release_buffer(&mstat, &outtoken);
	}

	if (*major_stat == GSS_S_COMPLETE ) {
		/*
		 * Turn the principal name into UNIX creds
		 */
		*major_stat = gss_name_to_ucred(minor_stat, princ,
						uid, gids, gidsCnt);
		if (*major_stat != GSS_S_COMPLETE) {
			kr = KERN_FAILURE;
			goto done;
		}
		/*
		 * Fetch the (sub)session key from the context
		 */
		*major_stat = GetSessionKey(minor_stat, oid, g_cntx,
					    skey, skeyCnt, 1);

		Debug("Server key length = %d\n", *skeyCnt);
		HEXDUMP(2, (char *) *skey, *skeyCnt);
	} else if (*major_stat == GSS_S_CONTINUE_NEEDED) {
		*gss_context = gssd_set_context(g_cntx, NULL);
		if (*gss_context == 0)
			*major_stat = GSS_S_FAILURE;

		/*
		 * Register our context handle
		 */
		gssd_enter(CAST(void *, *gss_context));
	}
	if (*major_stat == GSS_S_COMPLETE || *major_stat == GSS_S_CONTINUE_NEEDED) {
		DEBUG(3, "otokenCnt = %d", *otokenCnt);
		HEXDUMP(3, (char *)*otoken, (*otokenCnt > 80) ? 80 : *otokenCnt);
	}
done:
	gss_release_name(&mstat, &princ);
	if (*major_stat != GSS_S_CONTINUE_NEEDED) {
		gssd_remove(CAST(void *, *cred_handle));
		(void)gss_release_cred(&mstat, (gss_cred_id_t *) cred_handle);
		if (g_cntx != GSS_C_NO_CONTEXT)
			(void) gss_delete_sec_context(&mstat, &g_cntx, GSS_C_NO_BUFFER);
	}

	acceptCnt++;
	if (*major_stat != GSS_S_CONTINUE_NEEDED && *major_stat != GSS_S_COMPLETE)
		acceptErr++;
	DEBUG(3, "Returning from accept %d erros of of %d total calls\n", acceptErr, acceptCnt);

	Info("gss_accept_sec_context %K; %#k", *major_stat, oid, *minor_stat);
out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);

	Debug("Exit");

	return (kr);
}


#define MSG(f, ...) do {\
	if (f) { \
		Debug(__VA_ARGS__); \
	} else { \
		Log(__VA_ARGS__); \
	} \
} while (0)



/*
 * Mig dispatch routine to log GSS-API errors
 */
kern_return_t
svc_mach_gss_log_error(
	mach_port_t test_port __attribute__((unused)),
	gssd_string mnt,
	uint32_t uid,
	gssd_string source,
	uint32_t major,
	uint32_t minor,
	audit_token_t atok)
{
	OM_uint32 msg_context = 0;
	OM_uint32 min_stat = 0;
	OM_uint32 maj_stat = 0;
	gss_buffer_desc errBuf;
	char msgbuf[1024];
	char *errStr;
	int full = 0;
	vproc_transaction_t gssd_vproc_handle;
	kern_return_t kr = KERN_SUCCESS;

	Debug("Enter");
	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	if (!check_audit(atok, FALSE)) {
		kr = KERN_NO_ACCESS;
		goto out;
	}

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
		errStr = buf_to_str(&errBuf);
		if (maj_stat != GSS_S_COMPLETE)
			goto done;
		full = strlcat(msgbuf, " - ", sizeof(msgbuf)) >= sizeof(msgbuf) ||
		    strlcat(msgbuf, errStr, sizeof(msgbuf)) >= sizeof(msgbuf);
		free(errStr);
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
		errStr = buf_to_str(&errBuf);
		if (maj_stat != GSS_S_COMPLETE) {
			free(errStr);
			goto done;
		}
		full = strlcat(msgbuf, " - ", sizeof(msgbuf)) >= sizeof(msgbuf) ||
		    strlcat(msgbuf, errStr, sizeof(msgbuf)) >= sizeof(msgbuf);
		free(errStr);
		if (full)
			goto done;
	} while (msg_context != 0);

done:
	MSG((major == GSS_S_NO_CRED), "%s", msgbuf);

out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);

	return (kr);
}

kern_return_t
svc_mach_gss_hold_cred(mach_port_t server __unused,
		       gssd_mechtype mech,
		       gssd_nametype nt,
		       gssd_byte_buffer princ,
		       mach_msg_type_number_t princCnt,
		       audit_token_t atok,
		       uint32_t *major_stat,
		       uint32_t *minor_stat)
{
	gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
	uint32_t m;
	vproc_transaction_t gssd_vproc_handle;
	kern_return_t kr = KERN_SUCCESS;

	Debug("Enter");
	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	*major_stat = *minor_stat = 0;

	if (!check_audit(atok, FALSE)) {
		kr = KERN_NO_ACCESS;
		goto out;
	}

	*major_stat = do_acquire_cred(minor_stat, nt, princ, princCnt, mech, &cred);
	if (*major_stat != GSS_S_COMPLETE || cred == GSS_C_NO_CREDENTIAL)
		goto out;
	*major_stat = gss_cred_hold(minor_stat, cred);
	(void) gss_release_cred(&m, &cred);

out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);

	return (kr);
}

kern_return_t
svc_mach_gss_unhold_cred(mach_port_t server __unused,
			 gssd_mechtype mech,
			 gssd_nametype nt,
			 gssd_byte_buffer princ,
			 mach_msg_type_number_t princCnt,
			 audit_token_t atok,
			 uint32_t *major_stat,
			 uint32_t *minor_stat)
{
	gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
	uint32_t m;
	vproc_transaction_t gssd_vproc_handle;
	kern_return_t kr = KERN_SUCCESS;

	Debug("Enter");
	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	*major_stat = *minor_stat = 0;

	if (!check_audit(atok, FALSE)) {
		kr = KERN_NO_ACCESS;
		goto out;
	}

	*major_stat = do_acquire_cred(minor_stat, nt, princ, princCnt, mech, &cred);
	if (*major_stat != GSS_S_COMPLETE || cred == GSS_C_NO_CREDENTIAL)
		goto out;
	*major_stat = gss_cred_unhold(minor_stat, cred);
	(void) gss_release_cred(&m, &cred);

out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);
	return (kr);
}

kern_return_t
svc_mach_gss_lookup(mach_port_t server,
		    uint32_t uid,
		    int32_t asid,
		    audit_token_t atok,
		    mach_port_t *gssd_port)
{
	kern_return_t kr = KERN_NO_ACCESS;
	uuid_t uuid;
	uuid_string_t uuidstr;
	vproc_transaction_t gssd_vproc_handle;

	Debug("Enter");
	gssd_vproc_handle = vproc_transaction_begin(NULL);
	new_worker_thread();

	*gssd_port = MACH_PORT_NULL;
	if (!check_audit(atok, kernel_only))
		goto out;

	if (check_session(asid)) {
		if (asid == my_asid) {
			kr = mach_port_insert_right(mach_task_self(), server, server, MACH_MSG_TYPE_MAKE_SEND);
			if (kr == KERN_SUCCESS)
				*gssd_port = server;
		} else {
			sessioninfo2uuid((uid_t)uid, (au_asid_t)asid, uuid);
			uuid_unparse(uuid, uuidstr);
			Debug("Looking up %s for %d %d as instance %s", bname, uid, asid, uuidstr);

			kr = bootstrap_look_up3(bootstrap_port, bname, gssd_port, 0, uuid, BOOTSTRAP_SPECIFIC_INSTANCE);
			if (kr != KERN_SUCCESS)
				Log("Could not lookup per instance port %d: %s", kr, bootstrap_strerror(kr));

			Debug("bootstap_look_up3 = %d port = %d,  server port = %d", kr, *gssd_port, server);
		}
	}

out:
	end_worker_thread();
	vproc_transaction_end(NULL, gssd_vproc_handle);
	return (kr);
}

#pragma clang diagnostic pop
