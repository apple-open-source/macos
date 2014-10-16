/*
 * Copyright (c) 2006-2014 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <search.h>
#include <stdint.h>
#include <pthread.h>
#include <stdarg.h>
#include <regex.h>
#include <asl.h>
#include <asl_private.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <GSS/gssapi.h>
#include <GSS/gssapi_krb5.h>
#include <GSS/gssapi_ntlm.h>
#include <GSS/gssapi_spnego.h>
#include <GSS/gssapi_netlogon.h>
#include "gssd.h"

/*
 * Limit the number of contexts that we can have.
 * If a kernel thread gets a context from us with 
 * CONTINUE NEEDED, but never finish the context we
 * and then loops before we idle out we can end up 
 * consuming a large amount of memory.
 */
#define MAX_GSS_CONTEXTS 100

static void *rootp = (void *)0;
static pthread_mutex_t smutex;
static pthread_once_t sonce = PTHREAD_ONCE_INIT;
int ctx_counter = 0;


static void
init(void)
{
	pthread_mutex_init(&smutex, NULL);
}

static int
compare(const void *p1, const void *p2)
{
	uintptr_t v1 = (uintptr_t)p1;
	uintptr_t v2 = (uintptr_t)p2;
	if (v1 == v2)
		return (0);
	else if (v1 < v2)
		return (-1);
	return (1);
}

void
gssd_enter(void *ptr)
{
	if (ptr == NULL)
		return;

	pthread_once(&sonce, init);
	(void) pthread_mutex_lock(&smutex);
	if (ctx_counter > MAX_GSS_CONTEXTS) {
		Log("To many contexes. Exiting\n");
		/* Will exit with 0 so lanchd will start as again */
		exit(0);
	}
	
	if ((tfind(ptr, &rootp, compare) == (void *)0)) {
		if (tsearch(ptr, &rootp, compare))
			ctx_counter++;
	}
	(void) pthread_mutex_unlock(&smutex);
}

void
gssd_remove(void *ptr)
{
	if (ptr == NULL)
		return;

	pthread_once(&sonce, init);
	(void) pthread_mutex_lock(&smutex);
	if (tdelete(ptr, &rootp, compare))
		ctx_counter--;
	(void) pthread_mutex_unlock(&smutex);
}

int
gssd_check(void *ptr)
{
	int rc;
	if (ptr == (void *)0)
		return (1);

	pthread_once(&sonce, init);
	(void) pthread_mutex_lock(&smutex);
	rc = (tfind(ptr, &rootp, compare) != (void *)0);
	(void) pthread_mutex_unlock(&smutex);
	return (rc);
}

char *
buf_to_str(gss_buffer_t buf)
{
	char *s = malloc(buf->length + 1);
	uint32_t min;

	if (s) {
		memcpy(s, buf->value, buf->length);
		s[buf->length] = '\0';
	}
	(void) gss_release_buffer(&min, buf);

	return (s);
}

int
traced()
{
	struct kinfo_proc kp;
	int mib[4];
	size_t len;

	/* Fill out the first three components of the mib */
	len = 4;
	sysctlnametomib("kern.proc.pid", mib, &len);
	mib[3] = getpid();
	len = sizeof(kp);
	if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1) {
		gssd_log(ASL_LEVEL_ERR, "sysctl: %s", strerror(errno));
		return (FALSE);
	}

	return ((kp.kp_proc.p_flag & P_TRACED) == P_TRACED);
}

static void
regerr(int rerr, regex_t *re)
{
	char errbuff[60];

	(void)regerror(rerr, re, errbuff, sizeof(errbuff));
	Fatal("regex error %s\n", errbuff);
}

/*
 * Convert the oid in to a sane name if possible, if not return
 * the string constant of unkown.
 *
 */

struct oid_name_entry {
	gss_OID oid;
	const char *name;
} oid_name_tbl[] = {
	{ GSS_C_NT_USER_NAME, "user name" },
	{ GSS_C_NT_MACHINE_UID_NAME, "uid" },
	{ GSS_C_NT_STRING_UID_NAME, "uid string" },
	{ GSS_C_NT_HOSTBASED_SERVICE_X, "host based service" },
	{ GSS_C_NT_HOSTBASED_SERVICE, "host based service" },
	{ GSS_C_NT_ANONYMOUS, "anonymous" },
	{ GSS_C_NT_EXPORT_NAME, "export name" },
	{ GSS_C_NT_DN, "distinguished name" },
	{ GSS_SASL_DIGEST_MD5_MECHANISM, "sasl md5 mech" },
	{ GSS_NETLOGON_MECHANISM, "netlogon mech" },
	{ GSS_C_INQ_SSPI_SESSION_KEY, "session key" },
	{ GSS_C_INQ_WIN2K_PAC_X, "Win2k PAC" },
	{ GSS_KRB5_NT_PRINCIPAL_NAME, "KRB5 principal" },
	{ GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL, "KRB5 principal referral" },
	{ GSS_KRB5_NT_PRINCIPAL, "KRB5 principal" },
	{ GSS_KRB5_NT_USER_NAME, "KRB5 user name" },
	{ GSS_KRB5_NT_MACHINE_UID_NAME, "KRB5 uid" },
	{ GSS_KRB5_NT_STRING_UID_NAME, "KRB5 uid string" },
	{ GSS_KRB5_MECHANISM, "KRB5 mech" },
	{ GSS_PKU2U_MECHANISM, "PKU2U mech" },
	{ GSS_IAKERB_MECHANISM, "IA Kerb mech" },
	/* OTHER gssapi_krb5.h oids here */
	{ GSS_NTLM_MECHANISM, "NTLM mech" },
	{ GSS_C_NT_NTLM, "NTLM name" },
	{ GSS_C_NTLM_GUEST, "NTLM guest" },
	{ GSS_NTLM_GET_SESSION_KEY_X, "NTLM session key" },
	{ GSS_SPNEGO_MECHANISM, "SPNEGO mech" },
	{ GSS_C_NT_UUID, "UUID name" },
	{ NULL, NULL }
};

char *
oid_name(gss_OID oid)
{
	uint32_t maj, min;
	gss_buffer_desc buf;
	struct oid_name_entry *oep;
	const char *name = NULL;
	char *rn;

	for (oep = oid_name_tbl; oep->name != NULL; oep++) {
		if (gss_oid_equal(oid, oep->oid)) {
			name = oep->name;
			break;
		}
	}
	if (oid == NULL)
	    name = "default (null) oid";
	if (name == NULL) {
		maj = gss_oid_to_str(&min, oid, &buf);
		if (maj != GSS_S_COMPLETE)
			rn = strdup("Bad oid");
		else
			rn = buf_to_str(&buf);
	} else
		rn = strdup(name);

	return (rn);
}

/*
 * GSSAPI ERRORS
 *
 */
static const char *gss_call_err_names[] = {
	"GSS_S_CALL_INACCESSIBLE_READ",
	"GSS_S_CALL_INACCESSIBLE_WRITE",
	"GSS_S_CALL_BAD_STRUCTURE"
};
#define CALL_ERR_SIZE (sizeof(gss_call_err_names)/sizeof(const char *))

static const char *gss_err_names[] = {
	"GSS_S_BAD_MECH",
	"GSS_S_BAD_NAME",
	"GSS_S_BAD_NAMETYPE",
	"GSS_S_BAD_BINDINGS",
	"GSS_S_BAD_STATUS",
	"GSS_S_BAD_MIC",
	"GSS_S_NO_CRED",
	"GSS_S_NO_CONTEXT",
	"GSS_S_DEFECTIVE_TOKEN",
	"GSS_S_DEFECTIVE_CREDENTIAL",
	"GSS_S_CREDENTIALS_EXPIRED",
	"GSS_S_CONTEXT_EXPIRED",
	"GSS_S_FAILURE",
	"GSS_S_BAD_QOP",
	"GSS_S_UNAUTHORIZED",
	"GSS_S_UNAVAILABLE",
	"GSS_S_DUPLICATE_ELEMENT",
	"GSS_S_NAME_NOT_MN",
};
#define ERR_SIZE (sizeof(gss_err_names)/sizeof(const char *))

static const char *gss_sup_names[] = {
	"GSS_CONTINUE_NEEDED",
	"GSS_S_DUPLICATE_TOKEN",
	"GSS_S_OLD_TOKEN",
	"GSS_S_UNSEQ_TOKEN",
	"GSS_S_GAP_TOKEN",
};
#define SUP_SIZE (sizeof(gss_sup_names)/sizeof(const char *))
#define SUP_STRING_SIZE 128

static const char*
gss_error(uint32_t status)
{
	status >>= GSS_C_ROUTINE_ERROR_OFFSET;
	status &= (uint32_t)GSS_C_ROUTINE_ERROR_MASK;
	if (status == 0)
		return (NULL);
	if (status > ERR_SIZE)
		return ("GSS_UNKOWN_ERROR");
	return (gss_err_names[status - 1]);
}

static const char*
gss_call_error(uint32_t status)
{
	status >>= GSS_C_CALLING_ERROR_OFFSET;
	status &= (uint32_t)GSS_C_CALLING_ERROR_MASK;
	if (status == 0)
		return (NULL);
		if (status > ERR_SIZE)
			return ("GSS_UNKOWN_CALL_ERROR");
	return (gss_call_err_names[status - 1]);
}

static char*
gss_sup_info(uint32_t status)
{

	int previous = 0;
	size_t i;
	char *str;

	status >>= GSS_C_SUPPLEMENTARY_OFFSET;
	status &= (uint32_t) GSS_C_SUPPLEMENTARY_MASK;
	if (status == 0)
		return (NULL);

	str = malloc(SUP_STRING_SIZE);
	if (str == NULL)
		Fatal("Gssd, gssd-agent out of memory");

	for (i = 0, *str = '\0'; status && i < SUP_SIZE; i++, status >>= 1)  {
		if (status & 1) {
			if (previous)
				strlcat(str, " ", SUP_STRING_SIZE);
			strlcat(str, gss_sup_names[i], SUP_STRING_SIZE);
			previous = 1;
		}
	}
	if (status)
		strlcat(str, previous ? " GSS_UNKOWN_INFO" : "GSS_UNKOWN_INFO", SUP_STRING_SIZE);

	return (str);
}

static char *
gss_alt_error(uint32_t status)
{
	const char *error, *call_err;
	char *ret, *sup_info;

	if (status == GSS_S_COMPLETE)
		return (strdup("GSS_S_COMPLETE"));

	error = gss_error(status);
	call_err = gss_call_error(status);
	sup_info = gss_sup_info(status);

	if (error && call_err && sup_info)
		asprintf(&ret, "%s (%s): %s", error, call_err, sup_info);
	else if (error && call_err && sup_info == NULL)
		asprintf(&ret, "%s (%s)", error, call_err);
	else if (error && call_err == NULL && sup_info)
		asprintf(&ret, "%s: %s", error, sup_info);
	else if (error && call_err == NULL && sup_info == NULL)
		ret = strdup(error);
	else if (error == NULL && call_err && sup_info)
		asprintf(&ret, "(%s): %s", call_err, sup_info);
	else if (error == NULL && call_err && sup_info == NULL)
		ret = strdup(call_err);
	else {
		ret = sup_info;
		sup_info = NULL;
	}

	if (sup_info)
		free(sup_info);

	return (ret);
}


/*
 * Convert the major and minor error codes for a given id into a string.
 *
 * GSS API has a function, gss_display_status, that will do this and that can handle
 * very long messages by repeatedly calling the routine with a handle returned
 * from the first call. This is too long to be useful and I have never seen a
 * case where the entire message was not retrieved on the first call. If we
 * should find more text to extract will notate that by appending a '<' code value '>'
 * at the end of the returned string. The caller will be responsible to free
 * the result. If the OID passed in is GSS_C_NO_OID then we will look up the
 * major status, else we will return the string for the minor status. If a code
 * does not map to a string we'll return the string representation of the code
 * value.
 */

/*
 * gss_display_status for minor codes that are not from the last failure on the
 * current thread are displayed unknown mech-code <blah> from mech <string of numbers>
 * for the oid in the mech in question. In the abbreviated format, "#" specifier,
 * will strip that of, since we are printing a nice readable name and its redundant.
 */

static char *nomechexp = " for mech ([0-9]+ )*[0-9]+$";
static pthread_once_t gonce = PTHREAD_ONCE_INIT;
static regex_t mre;

static void
gss_strerror_init(void)
{
	int rerr = regcomp(&mre, nomechexp, REG_EXTENDED);
	if (rerr)
		regerr(rerr, &mre);
}

char *
gss_strerror(gss_OID oid, uint32_t code, uint32_t flag)
{
	uint32_t maj, min;
	uint32_t display_ctx = 0;
	int code_space;
	gss_buffer_desc msg_buf;
	char *ret_msg = NULL;
	char *tmp_msg;
	char *oidstr;
	regmatch_t match[1];
	int rerr;

	pthread_once(&gonce, gss_strerror_init);
	code_space = (oid == GSS_C_NO_OID) ? GSS_C_GSS_CODE : GSS_C_MECH_CODE;
	oidstr = (oid == GSS_C_NO_OID) ? strdup("GSSAPI") : oid_name(oid);
	if (oidstr == NULL)
		Fatal("Gssd or gssd-agent out of memory -- exiting\n");

	if (oid == GSS_C_NO_OID && flag) {
		free(oidstr);
		return (gss_alt_error(code));
	}

	maj = gss_display_status(&min, code, code_space, oid, &display_ctx, &msg_buf);
	if (maj != GSS_S_COMPLETE) {
		asprintf(&ret_msg, "%s status %d", oidstr, code);
		free(oidstr);
		return (ret_msg);
	} else {
		tmp_msg = buf_to_str(&msg_buf);
		if (tmp_msg == NULL)
			Fatal("Gssd or gssd-agent out of memory -- exiting\n");
		if (flag && (oid != GSS_C_NO_OID)) {

			rerr = regexec(&mre, tmp_msg, 1, match, 0);
			if (rerr == REG_NOMATCH)
				goto done;
			else if (rerr)
				regerr(rerr, &mre);
			else
				tmp_msg[match[0].rm_so] = '\0';
		}
	}

done:
	if (flag) {
		if (display_ctx)
			asprintf(&ret_msg, "%s: %s <%d>", oidstr, tmp_msg, code);
		else
			asprintf(&ret_msg, "%s: %s", oidstr, tmp_msg);
	} else {
		if (display_ctx)
			asprintf(&ret_msg, "%s status: %s <%d>", oidstr, tmp_msg, code);
		else
			asprintf(&ret_msg, "%s status: %s", oidstr, tmp_msg);
	}
	free(oidstr);
	free(tmp_msg);

	return (ret_msg);
}

static int foreground;
static int istraced;

#include <fcntl.h>
int
in_foreground(int ttyfd)
{
	int ttyfd2 = -1;
	pid_t tpg;

	if (ttyfd == -1) {
		ttyfd2 = open("/dev/tty", O_WRONLY);
		if (ttyfd2 == -1)
			return (FALSE);
		ttyfd = ttyfd2;
	}
	tpg = tcgetpgrp(ttyfd);

	if (ttyfd2 > -1)
		close(ttyfd2);

	if (tpg == -1)
		return (FALSE);

	return (tpg == getpgid(getpid()));
}

/*
 * Regular expression for printf fields:
 * Regexec will match an array of regmatch_t's as follows:
 * index 0 is the whole expression
 * skip the optional accessor specification
 * index 1 is the optional flags fields
 * skip the optional AltiVec/SSE vector value separator
 * index 2 is the optional minimum field width
 * index 3 is the optional precision field. (also max digits or characters to print)
 * index 4 is the optional sub-field from above; the characters after the '.'
 * index 5 is the intergral lenght type modifier (diouxXn)
 * index  is the type conversion character
 */
#define PRF_WHOLE_MATCH		0
#define PRF_ACCESSOR		-1
#define PRF_FLAGS		1
#define PRF_VSEP		-1
#define PRF_WIDTH		2
#define PRF_PREC		3
#define PRF_PREC_SPEC		4
#define PRF_LENGTH		5
#define PRF_TYPE		6

#define PRF_FIELDS		7

//static char *prfrexp = "%([0-9]+\\$)?([-#0 +']+)?([,;:_])?(\\*|[0-9]+)?(.(\\*|[0-9]*)?)?(hh?|ll?|j|t|z|q|L)?([diouxXDOUeEfFgGaACcSsp%kK])";
static char *prfrexp = "%([-#0 +']+)?(\\*|[0-9]+)?(.(\\*|[0-9]*)?)?(hh?|ll?|j|t|z|q|L)?([diouxXDOUeEfFgGaACcSsp%kK])";
static regex_t pre;

#ifndef GSSD_LOG_DEBUG
static char *gssrexp = "%([0-9]+\\$)?([-#0 +']+)?([,;:_])?(\\*|[0-9]+)?(.(\\*|[0-9]*)?)?(hh?|ll?|j|t|z|q|L)?([kK])";
static regex_t gre;
#endif

static pthread_once_t ronce = PTHREAD_ONCE_INIT;

#define GSSD_FACILITY "com.apple.gssd"
static aslclient asl = NULL;

#define ASL_INIT_FILTER ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE)

static void
gssd_log_init(void)
{
	int rerr;


	/* Check if were in the forground */
	foreground = in_foreground(2);
	istraced = traced();

	asl = asl_open(getprogname(), GSSD_FACILITY, 0);
	asl_set_filter(asl, ASL_INIT_FILTER);

	rerr = regcomp(&pre, prfrexp, REG_EXTENDED);
	if (rerr)
		regerr(rerr, &pre);
#ifndef GSSD_LOG_DEBUG
	rerr = regcomp(&gre, gssrexp, REG_EXTENDED | REG_NOSUB);
	if (rerr)
		regerr(rerr, &gre);
#endif
}

void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	asl_vlog(asl, NULL, ASL_LEVEL_ERR, fmt, ap);
	va_end(ap);
	exit(1);
}

static int debug = 0;
static void (*disable_timeout)(int);

/*
 * set a callback to disable the processes timeout.
 * If the argument is zero reenable the time out
 * else disable the timeout.
 */
void set_debug_level_init(void (*dto)(int))
{
	disable_timeout = dto;
}

int get_debug_level(void)
{
	return (debug);
}

/*
 * Set the debug level and filter mask to allow asl debug messages.
 * This routine is called at startup and from the signal handler thread.
 * That thread registers for notifications from syslog the master or remote
 * filter have been install/removed.
 *
 * - debug level 0 - turn off debugging and do not send messages syslogd
 * - debug level 1 - set debugging to 1 and log to syslog at info level
 * - debug level >= 2 set the debugging level and log to syslogd at debug level
 *
 * - debug level -1, set the debug level to what the asl log level allows.
 *   N.B. If the active asl log level is debug and the current debug level is
 *   greater than 1, log at the current debug level. If the previous active filter
 *   was remote and the current active filter is local turn off debug. This would
 *   indicate that someone explicitly is turning off debuging with syslog -c gssd off.
 *
 *   Normally seting a debug level greater than zero will stop gssd from timing out.
 *   The exception is that we are turning debugging on because of a master filter notification.
 *   in that case we leave the time out setting alone. As long as the master filter is
 *   active and is greater than notice, we will log to syslog in each startup of gssd.
 *
 *   Limitations: Only one filter is active at a time. The filter priorities are
 *	remote > master > local.
 *	Raising the debug level with SIGUSR1 will have no apparent effect if the
 *	remote or master filter is the active filter and if the filter does not have
 *	debug set. If info is set you will see  only info messages.
 */
void
set_debug_level(int debug_level)
{
	int filter, local, master, remote, active = 0;
	int status;
	static int last_active = 0;

	pthread_once(&ronce, gssd_log_init);
	if (debug_level < 0) {
		/*
		 * We've got notified by syslog that our filter mask have changed.
		 * Use that to determine the debug level.
		 */
		status = asl_get_filter(asl, &local, &master, &remote, &active);
		if (status) {
			Log("asl_get_filter failed\n");
			return;
		}

		//Log("l = %x m = %x r = %x active = %d, last_active = %d", local, master, remote, active, last_active);
		switch (active) {
			case 0:	filter = local;
				break;
			case 1: filter = master;
				/*
				 * If someone turns on global debugging we don't
				 * alter whether we time out or not. If global
				 * debugging was in effect when gssd started, we
				 * will continue to send debug output as long as
				 * interest in global debugging is in effect.
				 * If global debugging is enabled after explicit debugging
				 * was enabled either with a remote filter or signal then
				 * we should keep the current timout behavor until the user
				 * turns off explicit debugging.
				 */
				break;
			case 2: filter = remote;
				break;
			default:
				Log("Unkown active ASL filter %d", active);
				return;
		}
		if (ASL_FILTER_MASK(ASL_LEVEL_DEBUG) & filter)
			debug_level = maximum(debug, 2);
		else if (ASL_FILTER_MASK(ASL_LEVEL_INFO) & filter)
			debug_level = 1;
		else
			debug_level = 0;
		if (last_active == 2 && active == 0) {
			/*
			 * We got here because the user gave the command
			 * syslog -c gssd off, so the user as explicitly
			 * told us, she is no longer interested in debugging.
			 * turn debugging off.
			 */
			debug_level = 0;
		}
		last_active = active;
	}
	if (debug == debug_level)
		return; /* Nothing has changed. */

	if (active == 0) {
		switch (debug_level) {
		case 0:	/* Debug has been turned off. */
			local = ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE);
			break;
		case 1:	/* Debug hs been set to 1 turn on INFO level loging. */
			local = ASL_FILTER_MASK_UPTO(ASL_LEVEL_INFO) | ASL_FILTER_MASK_TUNNEL;
			break;
		default:	/* Anything else turn on DEBUG level logging. */
			local = ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG) | ASL_FILTER_MASK_TUNNEL;
			break;
		}
		asl_set_filter(asl, local);
	}

	debug = debug_level;
	if (debug == 0)
		gssd_log(ASL_LEVEL_NOTICE, "Leaving debug mode");
	if (active != 1 && disable_timeout)
		disable_timeout(debug != 0);
}

static void
g_vlog(int level, const char *fmt, va_list ap)
{
	int saved_errno = errno;

	if (foreground) {
		vfprintf(stderr, fmt, ap);
		fflush(stderr);
	} else {
		if (istraced && isatty(1)) {
			va_list ap2;
			va_copy(ap2, ap);
			vprintf(fmt, ap2);
			fflush(stdout);
			va_end(ap2);
		}
		asl_vlog(asl, NULL, level, fmt, ap);
	}

	errno = saved_errno;
}

static void
g_log(int level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	g_vlog(level, fmt, ap);
	va_end(ap);
}

#ifndef GSSD_LOG_DEBUG
static int
has_gss_conv(const char *fmt)
{
	int rerr;

	rerr = regexec(&gre, fmt, 0, NULL, 0);

	if (rerr == REG_NOMATCH)
		return (FALSE);
	if (rerr)
		regerr(rerr, &gre);

	return (TRUE);
}
#endif

static int
vslprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	size_t n = strlen(buf);
	int rv;

	if (n >= size)
		return (0);

	size -= n;
	rv = vsnprintf(&buf[n], size, fmt, ap);
	return (((size_t)rv > size) ? (int)size : rv);
}

static int
slprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int rv;

	va_start(ap, fmt);
	rv = vslprintf(buf, size, fmt, ap);
	va_end(ap);

	return (rv);
}

static size_t
strlncat(char *buf, const char *s, size_t bufsize, size_t slen)
{
	char *d;
	size_t n = strlen(buf);
	size_t i;

	if (slen + n >= bufsize)
		Fatal("copy to big\n");

	d = &buf[n];
	for (i = 0; i < slen && *s && (n + i < bufsize - 1); i++) {
		*d++ = *s++;
	}
	*d = '\0';

	return (d - buf);
}

static size_t
strlncpy(char *buf, const char *s, size_t bufsize, size_t slen)
{
	*buf = '\0';
	return (strlncat(buf, s, bufsize, slen));
}

#define MAX_FMT 256
#define MAX_LOG_MESSAGE 1024

/*
 * Advance the va_list given a printf style format.
 * We will make a copy of our argment list and pass that to slprintf.
 * We then need to advance ap by the same amount.
 */
static void
va_next(va_list *ap, const char *fmt, regmatch_t match[PRF_FIELDS])
{
	char ftype;
	char tlen[3];
	regoff_t i, j;

	ftype = fmt[match[PRF_TYPE].rm_so];
	for (i = match[PRF_LENGTH].rm_so, j = 0; i < match[PRF_LENGTH].rm_eo; i++, j++) {
		tlen[j] = fmt[i];
	}
	tlen[j] = '\0';
	if (match[PRF_WIDTH].rm_eo - match[PRF_WIDTH].rm_so == 1 &&
	    fmt[match[PRF_WIDTH].rm_so] == '*')
		va_arg(*ap, int);
	if (match[PRF_PREC_SPEC].rm_eo - match[PRF_PREC_SPEC].rm_so == 1 &&
	    fmt[match[PRF_PREC_SPEC].rm_so] == '*')
		va_arg(*ap, int);

	switch(ftype) {
	/* Shamelessly assume unsigned sizes are the same size as signed ones. */
	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
		if (tlen[0] == 'h' && tlen[1] == 'h')
			va_arg(*ap, int);
		else if (tlen[0] == 'l' && tlen[1] == 'l')
			va_arg(*ap, long long);
		else if (*tlen == 'l')
			va_arg(*ap, long);
		else if (*tlen == 'j')
			va_arg(*ap, intmax_t);
		else if (*tlen == 't')
			va_arg(*ap, ptrdiff_t);
		else if (*tlen == 'z')
			va_arg(*ap, size_t);
		else if (*tlen == 'q')
			va_arg(*ap, quad_t);
		else
			/* tlen in NULL or invalid and ignored */
			va_arg(*ap, int);
		break;
	case 'D':
	case 'O':
	case 'U':
		va_arg(*ap, long);
		break;
	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
		if (*tlen == 'L')
			va_arg(*ap, long double);
		else
			va_arg(*ap, double);
		break;
	case 'c':
		if (*tlen == 'l' && tlen[1] == '\0')
			va_arg(*ap, wchar_t);
		else
			va_arg(*ap, int);
		break;
	case 's':
		if (*tlen == 'l' && tlen[1] == '\0')
			va_arg(*ap, wchar_t *);
		else
			va_arg(*ap, char *);
		break;
	case 'C':
		va_arg(*ap, wchar_t);
		break;
	case 'S':
		va_arg(*ap, wchar_t);
		break;
	case 'p':
		va_arg(*ap, void *);
		break;
	case '%':
	default:
		break;
	}
}

static const char *
fmt_parse(char *out_buffer, char **obp, const char *ofp, va_list *ap)
{
	int rerr;
	regmatch_t match[PRF_FIELDS];
	char tconv;
	size_t mlen;
	const char *mstr;
	char kfmt[MAX_FMT];

	rerr = regexec(&pre, ofp, PRF_FIELDS, match, 0);
	if (rerr == REG_NOMATCH)
		return (NULL);  /* Invalid format return NULL */
	else if (rerr)
		regerr(rerr, &pre); /* Fatal error doesn't return, should never happen */

	tconv = ofp[match[PRF_TYPE].rm_so];
	mlen = (size_t)(match[PRF_WHOLE_MATCH].rm_eo - match[PRF_WHOLE_MATCH].rm_so);
	mstr = &ofp[match[PRF_WHOLE_MATCH].rm_so];
	if (mlen >= MAX_FMT)
		return (NULL);
	if (tconv == 'k' || tconv == 'K') {
		regoff_t i;
		gss_OID oid;
		char *gss_error_string;
		uint32_t flag = 0;
		uint32_t code;
		int prec = -1, width = -1;

		// handle 'k|K' format stream fromt ap to output buffer
		for (i = match[PRF_FLAGS].rm_so; i < match[PRF_FLAGS].rm_eo; i++) {
			if (ofp[i] == '#') {
				flag++;
				break;
			}
		}
		if (match[PRF_WIDTH].rm_eo - match[PRF_WIDTH].rm_so == 1 &&
		    ofp[match[PRF_WIDTH].rm_so] == '*')
			width = va_arg(*ap, int);

		if (match[PRF_PREC_SPEC].rm_eo - match[PRF_PREC_SPEC].rm_so == 1 &&
		    ofp[match[PRF_PREC_SPEC].rm_so] == '*')
			prec = va_arg(*ap, int);

		oid = (tconv == 'k') ? va_arg(*ap, gss_OID) : GSS_C_NO_OID;
		code = va_arg(*ap, uint32_t);
		gss_error_string = gss_strerror(oid, code, flag);
		kfmt[0] = '\0';
		strlncpy(kfmt, mstr, MAX_FMT, mlen);
		// Change the 'k' to 's'
		kfmt[strnlen(kfmt, MAX_FMT) - 1 ] = 's';
		if (width > -1 && prec > -1)
			*obp += slprintf(out_buffer, MAX_LOG_MESSAGE, kfmt, width, prec, gss_error_string);
		else if (width > -1)
			*obp += slprintf(out_buffer, MAX_LOG_MESSAGE, kfmt, width, gss_error_string);
		else if (prec > -1)
			*obp += slprintf(out_buffer, MAX_LOG_MESSAGE, kfmt, prec, gss_error_string);
		else
			*obp += slprintf(out_buffer, MAX_LOG_MESSAGE, kfmt, gss_error_string);
		free(gss_error_string);
	} else {
		va_list ap2;
		va_copy(ap2, *ap);
		strlncpy(kfmt, mstr, MAX_FMT, mlen);
		*obp += vslprintf(out_buffer, MAX_LOG_MESSAGE, kfmt, ap2);
		va_end(ap2);
		va_next(ap, ofp, match); /* Eat the arguments printed */
	}
	ofp += match[PRF_WHOLE_MATCH].rm_eo;

	return (ofp);
}

void
gssd_log(int log_level, const char *fmt, ...)
{
	int saved_errno = errno;
	const char *ofp = fmt;
	char output_buffer[MAX_LOG_MESSAGE];
	char *obp = output_buffer;
	va_list ap;

	pthread_once(&ronce, gssd_log_init);

	va_start(ap, fmt);
#ifndef GSSD_LOG_DEBUG
	if (!has_gss_conv(fmt) && 0) {
		g_vlog(log_level, fmt, ap);
		va_end(ap);
		return;
	}
#endif

	*obp = '\0';
	while (*ofp) {
		if (*ofp != '%' ) {
			if (obp - output_buffer < MAX_LOG_MESSAGE - 1) {
				*obp++ = *ofp++;
				*obp = '\0';
			}
		} else {
			ofp = fmt_parse(output_buffer, &obp, ofp, &ap);
			if (ofp == NULL) {
				g_log(ASL_LEVEL_ERR, "Invalid log format %s skipping ...\n", fmt);
				return;
			}
		}
	}

	va_end(ap);

	if (*output_buffer) {
		size_t len = strlen(output_buffer);
		g_log(log_level, output_buffer[len -1] == '\n' ? "%s" : "%s\n", output_buffer);
	}

	errno = saved_errno;
}

#if 0
/*
 * Display the major and minor GSS return codes from routine.
 */
void
display_GSS_err(char* rtnName, gss_OID mech, OM_uint32 maj, OM_uint32 min, int display_debug)
{
	OM_uint32 msg_context = 0;
	OM_uint32 min_stat = 0;
	OM_uint32 maj_stat = 0;
	gss_buffer_desc errBuf;
	char *str;
	int count = 1;

	if (maj == GSS_S_NO_CRED)
		display_debug = 1;

	MSG(display_debug, "Error returned by %s:\n", rtnName);
	do {
		maj_stat = gss_display_status(&min_stat, maj, GSS_C_GSS_CODE,
					      mech, &msg_context, &errBuf);
		str = (maj_stat == GSS_S_COMPLETE) ? buf_to_str(&errBuf) : NULL;
		if (count == 1)
			MSG(display_debug, "\tMajor error = %d: %s\n", maj, str ? str : "");
		else
			MSG(display_debug, "\t\t%s\n", str ? str : "");
		free(str);
		++count;
	} while (msg_context != 0);

	count = 1;
	msg_context = 0;
	do {
		maj_stat = gss_display_status (&min_stat, min, GSS_C_MECH_CODE,
					       mech, &msg_context, &errBuf);
		str = (maj_stat == GSS_S_COMPLETE) ? buf_to_str(&errBuf) : NULL;
		if (count == 1)
			MSG(display_debug, "\tMinor error = %d: %s\n", min, str ? str : "");
		else
			MSG(display_debug, "\t\t%s\n", str ? str : "");
		free(str);
		++count;
	} while (msg_context != 0);
}
#endif

static const char HexChars[16] = {
	'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

/*
 * Dump 16 bytes or bufSize bytes (<16) to line buf in hex followed by
 * character representation.
 */
static void
HexLine(const char *buf, size_t *bufSize, char linebuf[80])
{
	const char *bptr = buf;
	size_t	limit;
	size_t	i;
	char	*cptr = linebuf;

	memset(linebuf, 0, 80);

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
void
HexDump(const char *inBuffer, size_t inLength)
{
	size_t currentSize = inLength;
	char linebuf[80];

	while(currentSize > 0)
	{
		HexLine(inBuffer, &currentSize, linebuf);
		gssd_log(ASL_LEVEL_DEBUG, "\t%s", linebuf);
		inBuffer += 16;
	}
}

#if 0
/* LogToMessageTracer.
 * Currently not used, but we may want this in the future.
 * At any rate this apparently is how it is done.
 */

void LogToMessageTracer(const char *domain, const char *signature,
						const char *optResult, const char *optValue,
						const char *fmt,...)
{
	aslmsg m;
	va_list ap;

	if ( (domain == NULL) || (signature == NULL) || (fmt == NULL) ) {
		/* domain, signature and msg are required */
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, "com.apple.message.signature", signature);

	if (optResult != NULL) {
		asl_set(m, "com.apple.message.result", optResult);
	}
	if (optValue != NULL) {
		asl_set(m, "com.apple.message.value", optValue);
	}

	va_start(ap, fmt);
	asl_vlog(NULL, m, ASL_LEVEL_NOTICE, fmt, ap);
	va_end(ap);

	asl_free(m);
}
#endif
