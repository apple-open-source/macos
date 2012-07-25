/*-
 * This pam_krb5 module contains code that is:
 *   Copyright (c) Derrick J. Brashear, 1996. All rights reserved.
 *   Copyright (c) Frank Cusack, 1999-2001. All rights reserved.
 *   Copyright (c) Jacques A. Vidrine, 2000-2001. All rights reserved.
 *   Copyright (c) Nicolas Williams, 2001. All rights reserved.
 *   Copyright (c) Perot Systems Corporation, 2001. All rights reserved.
 *   Copyright (c) Mark R V Murray, 2001.  All rights reserved.
 *   Copyright (c) Networks Associates Technology, Inc., 2002-2005.
 *       All rights reserved.
 *   Copyright (c) 2008-2009 Apple Inc. All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KRB5_DEPRECATED_FUNCTION(x) /* no warnings for now :`( */

#include <Heimdal/krb5.h>
#include <Heimdal/com_err.h>

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>

#define	PAM_SM_AUTH
#define	PAM_SM_ACCOUNT
#define	PAM_SM_PASSWORD

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

/* _krb5_kcm_get_initial_ticket is SPI, so define it here for now */
krb5_error_code _krb5_kcm_get_initial_ticket(krb5_context context,
					     krb5_ccache id,
					     krb5_principal client,
					     krb5_principal server,
					     const char *password);


#define	COMPAT_HEIMDAL
/* #define	COMPAT_MIT  */

static int	verify_krb_v5_tgt(krb5_context, krb5_ccache, char *, int);
static const	char *compat_princ_component(krb5_context, krb5_principal, int);
static void	compat_free_data_contents(krb5_context, krb5_data *);

#define USER_PROMPT		"Username: "
#define PASSWORD_PROMPT		"Password:"
#define NEW_PASSWORD_PROMPT	"New Password:"

#define PAM_OPT_CCACHE		"ccache"
#define PAM_OPT_DEBUG		"debug"
#define PAM_OPT_DEFAULT_PRINCIPAL	"default_principal"
#define PAM_OPT_FORWARDABLE	"forwardable"
#define PAM_OPT_NO_FORWARDABLE	"noforward"
#define PAM_OPT_NO_CCACHE	"no_ccache"
#define PAM_OPT_USE_KCMINIT	"use_kcminit"
#define PAM_OPT_REUSE_CCACHE	"reuse_ccache"
#define PAM_OPT_USE_FIRST_PASS	"use_first_pass"

#define PAM_OPT_AUTH_AS_SELF	"auth_as_self"
#define PAM_OPT_DEBUG		"debug"

#define	PAM_LOG(...) \
	openpam_log(PAM_LOG_DEBUG, __VA_ARGS__)

#define	PAM_VERBOSE_ERROR(...) \
openpam_log(PAM_LOG_ERROR, __VA_ARGS__)

#ifdef COMPAT_MIT
#define krb5_get_err_text(c,e) error_message(e)
#endif

#include "Common.h"

static const char *password_key = "KRB5PWD";
static const char *user_key = "KRB5USER";


/*
 * authentication management
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_creds creds;
	krb5_principal princ;
	krb5_ccache ccache;
	krb5_get_init_creds_opt *opts = NULL;
	struct passwd *pwd;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	int retval, have_tickets = 0;
	const void *ccache_data;
	const char *user;
	char *pass;
	const void *sourceuser, *service;
	char *principal = NULL, *princ_name = NULL, *ccache_name, luser[32], *srvdup;

	retval = pam_get_user(pamh, &user, USER_PROMPT);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got user: %s", user);

	retval = pam_get_item(pamh, PAM_RUSER, &sourceuser);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got ruser: %s", (const char *)sourceuser);

	service = NULL;
	pam_get_item(pamh, PAM_SERVICE, &service);
	if (service == NULL)
		service = "unknown";

	PAM_LOG("Got service: %s", (const char *)service);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		return (PAM_SERVICE_ERR);
	}

	/* Get principal name */
	if (openpam_get_option(pamh, PAM_OPT_AUTH_AS_SELF))
		asprintf(&principal, "%s/%s", (const char *)sourceuser, user);
	else if (NULL == openpam_get_option(pamh, PAM_OPT_DEFAULT_PRINCIPAL))
		od_principal_for_user(pamh, user, &principal);
	else
		principal = strdup(user);
	if (principal == NULL) {
		PAM_LOG("Failed to determine Kerberos principal name.");
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Context initialised");

	/* if we are running with KCMINIT, just store the password in the kcm cache */
	if (openpam_get_option(pamh, PAM_OPT_USE_KCMINIT)) {
		PAM_LOG("Stashing kcm credentials in enviroment for kcminit: %s", principal);

		retval = pam_get_authtok(pamh, PAM_AUTHTOK, (const char **)&pass, PASSWORD_PROMPT);
		if (retval != PAM_SUCCESS) {
			PAM_LOG("no password: %s", principal);
			goto cleanup3;
		}

		retval = pam_setenv(pamh, user_key, principal, 1);
		if (retval != PAM_SUCCESS)
			goto cleanup3;

		retval = pam_setenv(pamh, password_key, pass, 1);
		if (retval != PAM_SUCCESS) {
			goto cleanup3;
		}

		free(principal);
		krb5_free_context(pam_context);

		return (PAM_IGNORE);
	}

	krbret = krb5_get_init_creds_opt_alloc(pam_context, &opts);
	if (krbret) {
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	if (NULL != openpam_get_option(pamh, PAM_OPT_NO_FORWARDABLE) &&
		NULL != openpam_get_option(pamh, PAM_OPT_FORWARDABLE)) {
		PAM_VERBOSE_ERROR("Do not set both \"%s\" and \"%s\".", PAM_OPT_FORWARDABLE, PAM_OPT_NO_FORWARDABLE);
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Created principal: %s", principal);

	krbret = krb5_parse_name(pam_context, principal, &princ);
	free(principal);
	if (krbret != 0) {
		PAM_LOG("Error krb5_parse_name(): %s",
		    krb5_get_err_text(pam_context, krbret));
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Done krb5_parse_name()");

	/* Now convert the principal name into something human readable */
	princ_name = NULL;
	krbret = krb5_unparse_name(pam_context, princ, &princ_name);
	if (krbret != 0) {
		PAM_LOG("Error krb5_unparse_name(): %s",
		    krb5_get_err_text(pam_context, krbret));
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Got principal: %s", princ_name);

	/* Get password */
	retval = pam_get_authtok(pamh, PAM_AUTHTOK, (const char **)&pass, PASSWORD_PROMPT);
	if (retval != PAM_SUCCESS)
		goto cleanup2;

	PAM_LOG("Got password");

	/* Verify the local user exists (AFTER getting the password) */
	if (strchr(user, '@')) {
		/* get a local account name for this principal */
		krbret = krb5_aname_to_localname(pam_context, princ,
		    sizeof(luser), luser);
		if (krbret != 0) {
			PAM_VERBOSE_ERROR("Kerberos 5 error");
			PAM_LOG("Error krb5_aname_to_localname(): %s",
			    krb5_get_err_text(pam_context, krbret));
			retval = PAM_USER_UNKNOWN;
			goto cleanup2;
		}

		retval = pam_set_item(pamh, PAM_USER, luser);
		if (retval != PAM_SUCCESS)
			goto cleanup2;

		PAM_LOG("PAM_USER Redone");
	}

	if (getpwnam_r(user, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) != 0 || pwd == NULL) {
		retval = PAM_USER_UNKNOWN;
		goto cleanup2;
	}

	PAM_LOG("Done getpwnam()");

	/* Get a TGT */
	if (NULL == openpam_get_option(pamh, PAM_OPT_NO_FORWARDABLE)) {
		krb5_get_init_creds_opt_set_forwardable(opts, 1);
		krb5_get_init_creds_opt_set_proxiable(opts, 1);

		PAM_LOG("Attempting to get forwardable TGT.");
		memset(&creds, 0, sizeof(krb5_creds));
		krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
			pass, NULL, pamh, 0, NULL, opts);
		if (krbret != 0) {
			krb5_get_init_creds_opt_set_forwardable(opts, 0);
			krb5_get_init_creds_opt_set_proxiable(opts, 0);
		} else {
			PAM_LOG("Have a forwardable TGT.");
			have_tickets = 1;
		}
	}

	if (!have_tickets) {
		PAM_LOG("Attempting to get non-forwardable TGT.");
		memset(&creds, 0, sizeof(krb5_creds));
		krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
			pass, NULL, pamh, 0, NULL, opts);
		if (krbret != 0) {
			PAM_VERBOSE_ERROR("Kerberos 5 error");
			PAM_LOG("Error krb5_get_init_creds_password(): %s",
				krb5_get_err_text(pam_context, krbret));
			retval = PAM_AUTH_ERR;
			goto cleanup2;
		}
	}

	PAM_LOG("Got TGT");

	/* Generate a temporary cache */
	krbret = krb5_cc_new_unique(pam_context, "FILE", NULL, &ccache);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_gen_new(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	krbret = krb5_cc_initialize(pam_context, ccache, princ);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_initialize(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	krbret = krb5_cc_store_cred(pam_context, ccache, &creds);
	if (krbret != 0) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		PAM_LOG("Error krb5_cc_store_cred(): %s",
		    krb5_get_err_text(pam_context, krbret));
		krb5_cc_destroy(pam_context, ccache);
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}
	if (0 == strcmp("FILE", krb5_cc_get_type(pam_context, ccache)))
		chown(krb5_cc_get_name(pam_context, ccache), pwd->pw_uid, pwd->pw_gid);

	PAM_LOG("Credentials stashed");

	/* Verify them */
	if ((srvdup = strdup(service)) == NULL) {
		retval = PAM_BUF_ERR;
		goto cleanup;
	}
	krbret = verify_krb_v5_tgt(pam_context, ccache, srvdup,
	    openpam_get_option(pamh, PAM_OPT_DEBUG) ? 1 : 0);
	free(srvdup);
	if (krbret == -1) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		krb5_cc_destroy(pam_context, ccache);
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash verified");

	retval = pam_get_data(pamh, "ccache", &ccache_data);
	if (retval == PAM_SUCCESS) {
		krb5_cc_destroy(pam_context, ccache);
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_AUTH_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash not pre-existing");

	asprintf(&ccache_name, "%s:%s", krb5_cc_get_type(pam_context,
		ccache), krb5_cc_get_name(pam_context, ccache));
	if (ccache_name == NULL) {
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_BUF_ERR;
		goto cleanup;
	}
	retval = pam_setenv(pamh, "krb5_ccache", ccache_name, 1);
	if (retval != 0) {
		krb5_cc_destroy(pam_context, ccache);
		PAM_VERBOSE_ERROR("Kerberos 5 error");
		retval = PAM_SERVICE_ERR;
		goto cleanup;
	}

	PAM_LOG("Credentials stash saved");

cleanup:
	krb5_free_cred_contents(pam_context, &creds);
	PAM_LOG("Done cleanup");
cleanup2:
	krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	if (princ_name)
		free(princ_name);

	if (opts)
		krb5_get_init_creds_opt_free(pam_context, opts);

	krb5_free_context(pam_context);

	PAM_LOG("Done cleanup3");

	if (retval != PAM_SUCCESS)
		PAM_LOG("Kerberos 5 refuses you");

	return (retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
#ifdef _FREEFALL_CONFIG
	return (PAM_SUCCESS);
#else

	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_principal princ = NULL;
	krb5_creds creds;
	krb5_ccache ccache_temp, ccache_perm = NULL;
	krb5_cc_cursor cursor;
	struct passwd *pwd = NULL;
	struct passwd pwdbuf;
	char pwbuffer[2 * PATH_MAX];
	int retval;
	const char *cache_type, *cache_name, *q;
	const void *user;
	const void *cache_data;
	char *cache_name_buf = NULL, *p = NULL, *cache_type_colon_name = NULL;
	int use_kcminit;

	uid_t euid;
	gid_t egid;

	if (flags & PAM_DELETE_CRED) {
		retval = PAM_SUCCESS;
		goto cleanup4;
	}

	if (flags & PAM_REFRESH_CRED) {
		retval = PAM_SUCCESS;
		goto cleanup4;
	}

	if (flags & PAM_REINITIALIZE_CRED) {
		retval = PAM_SUCCESS;
		goto cleanup4;
	}

	if (!(flags & PAM_ESTABLISH_CRED)) {
		retval = PAM_SERVICE_ERR;
		goto cleanup4;
	}

	/* If a persistent cache isn't desired, stop now. */
	if (openpam_get_option(pamh, PAM_OPT_NO_CCACHE)) {
		retval = PAM_SUCCESS;
		goto cleanup4;
	}

	PAM_LOG("Establishing credentials");

	/* Get username */
	retval = pam_get_item(pamh, PAM_USER, &user);
	if (retval != PAM_SUCCESS)
		goto cleanup4;

	PAM_LOG("Got user: %s", (const char *)user);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_secure_context() failed");
		retval = PAM_SERVICE_ERR;
		goto cleanup4;
	}

	PAM_LOG("Context initialised");

	euid = geteuid();	/* Usually 0 */
	egid = getegid();

	PAM_LOG("Got euid, egid: %d %d", euid, egid);

	use_kcminit = (openpam_get_option(pamh, PAM_OPT_USE_KCMINIT) != NULL);

	if (!use_kcminit) {
		/* Retrieve the temporary cache */
		if ((cache_data = pam_getenv(pamh, "krb5_ccache")) == NULL) {
			PAM_LOG("Error pam_getenv failed.");
			retval = PAM_IGNORE;
			goto cleanup3;
		}
		krbret = krb5_cc_resolve(pam_context, cache_data, &ccache_temp);
		if (krbret != 0) {
			PAM_LOG("Error krb5_cc_resolve(\"%s\"): %s", (const char *)cache_data,
				krb5_get_err_text(pam_context, krbret));
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}
	}
	/* Get the uid. This should exist. */
	if (getpwnam_r(user, &pwdbuf, pwbuffer, sizeof(pwbuffer), &pwd) != 0 || pwd == NULL) {
		retval = PAM_USER_UNKNOWN;
		goto cleanup3;
	}

	PAM_LOG("Done getpwnam()");

	/* Avoid following a symlink as root */
	if (0 == egid && 0 != setegid(pwd->pw_gid)) {
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}
	if (0 == euid && 0 != seteuid(pwd->pw_uid)) {
		retval = PAM_SERVICE_ERR;
		goto cleanup3;
	}

	PAM_LOG("Done setegid() & seteuid()");


	if (use_kcminit) {
		const char *principal, *password;
		krb5_principal princ = NULL;

		principal = pam_getenv(pamh, user_key);
		if (principal == NULL) {
			PAM_LOG("pam_sm_setcred: krb5 user %s doesn't have a principal", (char *)user);
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}

		password = pam_getenv(pamh, password_key);
		if (password == NULL) {
			PAM_LOG("pam_sm_setcred: krb5 user %s doesn't have a password", (char *)user);
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}

		krbret = krb5_parse_name(pam_context, principal, &princ);
		if (krbret != 0) {
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}

		krbret = krb5_cc_new_unique(pam_context, "API", NULL, &ccache_perm);
		if (krbret) {
			krb5_free_principal(pam_context, princ);
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}
		PAM_LOG("pam_sm_setcred: init credential cache");

		krbret = krb5_cc_initialize(pam_context, ccache_perm, princ);
		if (krbret) {
			krb5_free_principal(pam_context, princ);
			krb5_cc_close(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}

		PAM_LOG("pam_sm_setcred: storing credential for: %s", principal);
		krbret = _krb5_kcm_get_initial_ticket(pam_context, ccache_perm, princ, NULL, password);
		krb5_free_principal(pam_context, princ);
		if (krbret) {
			PAM_LOG("kcm init failed: %d", krbret);
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}
		retval = PAM_SUCCESS;
	} else {

		/* Initialize the new ccache */
		krbret = krb5_cc_get_principal(pam_context, ccache_temp, &princ);
		if (krbret != 0) {
			PAM_LOG("Error krb5_cc_get_principal(): %s",
				krb5_get_err_text(pam_context, krbret));
			retval = PAM_SERVICE_ERR;
			goto cleanup3;
		}

		/* Get the cache name */
		cache_name = openpam_get_option(pamh, PAM_OPT_CCACHE);
		if (cache_name == NULL) {
			krbret = krb5_cc_default(pam_context, &ccache_perm);
		}
		else {
			size_t len = (PATH_MAX + 16);
			p = calloc(len, sizeof(char));
			q = cache_name;

			if (p == NULL) {
				PAM_LOG("Error malloc(): failure");
				retval = PAM_BUF_ERR;
				goto cleanup3;
			}
			cache_name = cache_name_buf = p;

			/* convert %u and %p */
			while (*q) {
				if (*q == '%') {
					q++;
					if (*q == 'u') {
						len -= snprintf(p, len, "%d", pwd->pw_uid);
						p += strlen(p);
					}
					else if (*q == 'p') {
						len -= snprintf(p, len, "%d", getpid());
						p += strlen(p);
					}
					else {
						/* Not a special token */
						*p++ = '%';
						len--;
						q--;
					}
					q++;
				}
				else {
					*p++ = *q++;
					len--;
				}
			}

			PAM_LOG("Got cache_name: %s", cache_name);

			krbret = krb5_cc_resolve(pam_context, cache_name, &ccache_perm);
			if (krbret != 0) {
				PAM_LOG("Error krb5_cc_resolve(): %s",
					krb5_get_err_text(pam_context, krbret));
				retval = PAM_SERVICE_ERR;
				goto cleanup2;
			}
		}
		krbret = krb5_cc_initialize(pam_context, ccache_perm, princ);
		if (krbret != 0) {
			PAM_LOG("Error krb5_cc_initialize(): %s",
				krb5_get_err_text(pam_context, krbret));
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}

		PAM_LOG("Cache initialised");

		/* Prepare for iteration over creds */
		krbret = krb5_cc_start_seq_get(pam_context, ccache_temp, &cursor);
		if (krbret != 0) {
			PAM_LOG("Error krb5_cc_start_seq_get(): %s",
				krb5_get_err_text(pam_context, krbret));
			krb5_cc_destroy(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}

		PAM_LOG("Prepared for iteration");

		/* Copy the creds (should be two of them) */
		while ((krbret = krb5_cc_next_cred(pam_context, ccache_temp,
						   &cursor, &creds) == 0)) {
			krbret = krb5_cc_store_cred(pam_context, ccache_perm, &creds);
			if (krbret != 0) {
				PAM_LOG("Error krb5_cc_store_cred(): %s",
					krb5_get_err_text(pam_context, krbret));
				krb5_cc_destroy(pam_context, ccache_perm);
				krb5_free_cred_contents(pam_context, &creds);
				retval = PAM_SERVICE_ERR;
				goto cleanup2;
			}
			krb5_free_cred_contents(pam_context, &creds);
			PAM_LOG("Iteration");
		}
		krb5_cc_end_seq_get(pam_context, ccache_temp, &cursor);

		PAM_LOG("Done iterating");
	}

	/* Get the cache type and name */
	cache_type = krb5_cc_get_type(pam_context, ccache_perm);
	cache_name = krb5_cc_get_name(pam_context, ccache_perm);
	PAM_LOG("Got cache_name: %s:%s", cache_type, cache_name);

	if (0 == strcmp(cache_type, "FILE")) {
		if (chown(cache_name, pwd->pw_uid, pwd->pw_gid) == -1) {
			PAM_LOG("Error chown(): %s", strerror(errno));
			krb5_cc_destroy(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}
		PAM_LOG("Done chown()");

		if (chmod(cache_name, (S_IRUSR | S_IWUSR)) == -1) {
			PAM_LOG("Error chmod(): %s", strerror(errno));
			krb5_cc_destroy(pam_context, ccache_perm);
			retval = PAM_SERVICE_ERR;
			goto cleanup2;
		}
		PAM_LOG("Done chmod()");
	}

	asprintf(&cache_type_colon_name, "%s:%s", cache_type, cache_name);
	if (NULL == cache_type_colon_name)
		goto cleanup2;
	retval = pam_setenv(pamh, "KRB5CCNAME", cache_type_colon_name, 1);
	free(cache_type_colon_name);
	if (retval != PAM_SUCCESS) {
		PAM_LOG("Error pam_setenv(): %s", pam_strerror(pamh, retval));
		krb5_cc_destroy(pam_context, ccache_perm);
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Environment done: KRB5CCNAME=%s", cache_name);

	krb5_cc_close(pam_context, ccache_perm);

	PAM_LOG("Cache closed");

cleanup2:
	if (NULL != princ)
		krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	if (NULL != p)
		free(p);
	krb5_free_context(pam_context);
	PAM_LOG("Done cleanup3");

	seteuid(euid);
	setegid(egid);

	PAM_LOG("Done seteuid() & setegid()");

	if (cache_name_buf != NULL)
		free(cache_name_buf);
cleanup4:
	pam_unsetenv(pamh, user_key);
	pam_unsetenv(pamh, password_key);
	PAM_LOG("Done cleanup4");

	return (retval);
#endif
}

/*
 * account management
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_ccache ccache;
	krb5_principal princ;
	int retval;
	const void *user;
	const void *ccache_name;

	if (openpam_get_option(pamh, PAM_OPT_USE_KCMINIT))
		return (PAM_SUCCESS);

	retval = pam_get_item(pamh, PAM_USER, &user);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got user: %s", (const char *)user);

	retval = pam_get_data(pamh, "ccache", &ccache_name);
	if (retval != PAM_SUCCESS)
		return (PAM_SUCCESS);

	PAM_LOG("Got credentials");

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_secure_context() failed");
		return (PAM_PERM_DENIED);
	}

	PAM_LOG("Context initialised");

	krbret = krb5_cc_resolve(pam_context, (const char *)ccache_name, &ccache);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_resolve(\"%s\"): %s", (const char *)ccache_name,
		    krb5_get_err_text(pam_context, krbret));
		krb5_free_context(pam_context);
		return (PAM_PERM_DENIED);
	}

	PAM_LOG("Got ccache %s", (const char *)ccache_name);


	krbret = krb5_cc_get_principal(pam_context, ccache, &princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_cc_get_principal(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_PERM_DENIED;;
		goto cleanup;
	}

	PAM_LOG("Got principal");

	if (krb5_kuserok(pam_context, princ, (const char *)user))
		retval = PAM_SUCCESS;
	else
		retval = PAM_PERM_DENIED;
	krb5_free_principal(pam_context, princ);

	PAM_LOG("Done kuserok()");

cleanup:
	krb5_free_context(pam_context);
	PAM_LOG("Done cleanup");

	return (retval);

}

/*
 * password management
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc __unused, const char *argv[] __unused)
{
	krb5_error_code krbret;
	krb5_context pam_context;
	krb5_creds creds;
	krb5_principal princ;
	krb5_get_init_creds_opt *opts = NULL;
	krb5_data result_code_string, result_string;
	int result_code, retval;
	char *pass;
	const void *user;
	char *princ_name = NULL, *passdup;

	if (!(flags & PAM_UPDATE_AUTHTOK))
		return (PAM_AUTHTOK_ERR);

	retval = pam_get_item(pamh, PAM_USER, &user);
	if (retval != PAM_SUCCESS)
		return (retval);

	PAM_LOG("Got user: %s", (const char *)user);

	krbret = krb5_init_context(&pam_context);
	if (krbret != 0) {
		PAM_LOG("Error krb5_init_secure_context() failed");
		return (PAM_SERVICE_ERR);
	}

	PAM_LOG("Context initialised");

	krb5_get_init_creds_opt_alloc(pam_context, &opts);

	PAM_LOG("Credentials options initialised");

	/* Get principal name */
	krbret = krb5_parse_name(pam_context, (const char *)user, &princ);
	if (krbret != 0) {
		PAM_LOG("Error krb5_parse_name(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_USER_UNKNOWN;
		goto cleanup3;
	}

	/* Now convert the principal name into something human readable */
	princ_name = NULL;
	krbret = krb5_unparse_name(pam_context, princ, &princ_name);
	if (krbret != 0) {
		PAM_LOG("Error krb5_unparse_name(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_SERVICE_ERR;
		goto cleanup2;
	}

	PAM_LOG("Got principal: %s", princ_name);

	/* Get password */
	retval = pam_get_authtok(pamh, PAM_OLDAUTHTOK, (const char **)&pass, PASSWORD_PROMPT);
	if (retval != PAM_SUCCESS)
		goto cleanup2;

	PAM_LOG("Got password");

	memset(&creds, 0, sizeof(krb5_creds));
	krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
	    pass, NULL, pamh, 0, "kadmin/changepw", opts);
	if (krbret != 0) {
		PAM_LOG("Error krb5_get_init_creds_password(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_AUTH_ERR;
		goto cleanup2;
	}

	PAM_LOG("Credentials established");

	/* Now get the new password */
	for (;;) {
		retval = pam_get_authtok(pamh,
		    PAM_AUTHTOK, (const char **)&pass, NEW_PASSWORD_PROMPT);
		if (retval != PAM_TRY_AGAIN)
			break;
		pam_error(pamh, "Mismatch; try again, EOF to quit.");
	}
	if (retval != PAM_SUCCESS)
		goto cleanup;

	PAM_LOG("Got new password");

	/* Change it */
	if ((passdup = strdup(pass)) == NULL) {
		retval = PAM_BUF_ERR;
		goto cleanup;
	}
	krbret = krb5_set_password(pam_context, &creds, passdup, NULL,
	    &result_code, &result_code_string, &result_string);
	free(passdup);
	if (krbret != 0) {
		PAM_LOG("Error krb5_change_password(): %s",
		    krb5_get_err_text(pam_context, krbret));
		retval = PAM_AUTHTOK_ERR;
		goto cleanup;
	}
	if (result_code) {
		PAM_LOG("Error krb5_change_password(): (result_code)");
		retval = PAM_AUTHTOK_ERR;
		goto cleanup;
	}

	PAM_LOG("Password changed");

	if (result_string.data)
		free(result_string.data);
	if (result_code_string.data)
		free(result_code_string.data);

cleanup:
	krb5_free_cred_contents(pam_context, &creds);
	PAM_LOG("Done cleanup");
cleanup2:
	krb5_free_principal(pam_context, princ);
	PAM_LOG("Done cleanup2");
cleanup3:
	if (princ_name)
		free(princ_name);

	if (opts)
		krb5_get_init_creds_opt_free(pam_context, opts);

	krb5_free_context(pam_context);

	PAM_LOG("Done cleanup3");

	return (retval);
}

PAM_MODULE_ENTRY("pam_krb5");

/*
 * This routine with some modification is from the MIT V5B6 appl/bsd/login.c
 * Modified by Sam Hartman <hartmans@mit.edu> to support PAM services
 * for Debian.
 *
 * Verify the Kerberos ticket-granting ticket just retrieved for the
 * user.  If the Kerberos server doesn't respond, assume the user is
 * trying to fake us out (since we DID just get a TGT from what is
 * supposedly our KDC).  If the host/<host> service is unknown (i.e.,
 * the local keytab doesn't have it), and we cannot find another
 * service we do have, let her in.
 *
 * Returns 1 for confirmation, -1 for failure, 0 for uncertainty.
 */
/* ARGSUSED */
static int
verify_krb_v5_tgt(krb5_context context, krb5_ccache ccache,
    char *pam_service, int debug)
{
	krb5_error_code retval;
	krb5_principal princ;
	krb5_keyblock *keyblock;
	krb5_data packet;
	krb5_auth_context auth_context;
	char phost[BUFSIZ];
	const char *services[3], **service;

	packet.data = 0;

	/* If possible we want to try and verify the ticket we have
	 * received against a keytab.  We will try multiple service
	 * principals, including at least the host principal and the PAM
	 * service principal.  The host principal is preferred because access
	 * to that key is generally sufficient to compromise root, while the
	 * service key for this PAM service may be less carefully guarded.
	 * It is important to check the keytab first before the KDC so we do
	 * not get spoofed by a fake KDC.
	 */
	services[0] = "host";
	services[1] = pam_service;
	services[2] = NULL;
	keyblock = 0;
	retval = -1;
	for (service = &services[0]; *service != NULL; service++) {
		retval = krb5_sname_to_principal(context, NULL, *service,
		    KRB5_NT_SRV_HST, &princ);
		if (retval != 0) {
			if (debug)
				openpam_log(PAM_LOG_DEBUG,
				    "pam_krb5: verify_krb_v5_tgt(): %s: %s",
				    "krb5_sname_to_principal()",
				    krb5_get_err_text(context, retval));
			return -1;
		}

		/* Extract the name directly. */
		strncpy(phost, compat_princ_component(context, princ, 1),
		    BUFSIZ);
		phost[BUFSIZ - 1] = '\0';

		/*
		 * Do we have service/<host> keys?
		 * (use default/configured keytab, kvno IGNORE_VNO to get the
		 * first match, and ignore enctype.)
		 */
		retval = krb5_kt_read_service_key(context, NULL, princ, 0, 0,
		    &keyblock);
		if (retval != 0) {
			krb5_free_principal(context, princ);
			princ = NULL;
			continue;
		}
		break;
	}
	if (retval != 0) {	/* failed to find key */
		/* Keytab or service key does not exist */
		if (debug)
			openpam_log(PAM_LOG_DEBUG,
			    "pam_krb5: verify_krb_v5_tgt(): %s: %s",
			    "krb5_kt_read_service_key()",
			    krb5_get_err_text(context, retval));
		retval = 0;
		goto cleanup;
	}
	if (keyblock)
		krb5_free_keyblock(context, keyblock);

	/* Talk to the kdc and construct the ticket. */
	auth_context = NULL;
	retval = krb5_mk_req(context, &auth_context, 0, (char *)*service, phost,
		NULL, ccache, &packet);
	if (auth_context) {
		krb5_auth_con_free(context, auth_context);
		auth_context = NULL;	/* setup for rd_req */
	}
	if (retval) {
		if (debug)
			openpam_log(PAM_LOG_DEBUG,
			    "pam_krb5: verify_krb_v5_tgt(): %s: %s",
			    "krb5_mk_req()",
			    krb5_get_err_text(context, retval));
		retval = -1;
		goto cleanup;
	}

	/* Try to use the ticket. */
	retval = krb5_rd_req(context, &auth_context, &packet, princ, NULL,
	    NULL, NULL);
	if (retval) {
		if (debug)
			openpam_log(PAM_LOG_DEBUG,
			    "pam_krb5: verify_krb_v5_tgt(): %s: %s",
			    "krb5_rd_req()",
			    krb5_get_err_text(context, retval));
		retval = -1;
	}
	else
		retval = 1;

cleanup:
	if (packet.data)
		compat_free_data_contents(context, &packet);
	krb5_free_principal(context, princ);
	return retval;
}

#ifdef COMPAT_HEIMDAL
#ifdef COMPAT_MIT
#error This cannot be MIT and Heimdal compatible!
#endif
#endif

#ifndef COMPAT_HEIMDAL
#ifndef COMPAT_MIT
#error One of COMPAT_MIT and COMPAT_HEIMDAL must be specified!
#endif
#endif

#ifdef COMPAT_HEIMDAL
/* ARGSUSED */
static const char *
compat_princ_component(krb5_context context __unused, krb5_principal princ, int n)
{
	return princ->name.name_string.val[n];
}

/* ARGSUSED */
static void
compat_free_data_contents(krb5_context context __unused, krb5_data * data)
{
	krb5_xfree(data->data);
}
#endif

#ifdef COMPAT_MIT
static const char *
compat_princ_component(krb5_context context, krb5_principal princ, int n)
{
	return krb5_princ_component(context, princ, n)->data;
}

static void
compat_free_data_contents(krb5_context context, krb5_data * data)
{
	krb5_free_data_contents(context, data);
}
#endif
