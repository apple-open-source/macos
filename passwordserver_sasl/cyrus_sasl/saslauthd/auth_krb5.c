/* MODULE: auth_krb5 */

/* COPYRIGHT
 * Copyright (c) 1997 Messaging Direct Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MESSAGING DIRECT LTD. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MESSAGING DIRECT LTD. OR
 * ITS EMPLOYEES OR AGENTS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * END COPYRIGHT */

#ifdef __GNUC__
#ident "$Id: auth_krb5.c,v 1.5 2005/01/10 19:01:35 snsimon Exp $"
#endif

/* ok, this is  wrong but the most convenient way of doing 
 * it for now. We assume (possibly incorrectly) that if GSSAPI exists then 
 * the Kerberos 5 headers and libraries exist.   
 * What really should be done is a configure.in check for krb5.h and use 
 * that since none of this code is GSSAPI but rather raw Kerberos5.
 *
 * This function also has a bug where an alternate realm can't be
 * specified.  
 */

/* PUBLIC DEPENDENCIES */
#include "mechanisms.h"

#ifdef AUTH_KRB5
# include <krb5.h>
#endif /* AUTH_KRB5 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include "auth_krb5.h"

/* END PUBLIC DEPENDENCIES */

#define TF_DIR "/.tf"			/* Private tickets directory,   */
					/* relative to mux directory    */

char *tf_dir = NULL;

int					/* R: -1 on failure, else 0 */
auth_krb5_init (
  /* PARAMETERS */
  void					/* no parameters */
  /* END PARAMETERS */
  )
{
#ifdef AUTH_KRB5
    int rc;
    struct stat sb;

    /* Allocate memory arena for the directory pathname. */
    tf_dir = malloc(strlen(PATH_SASLAUTHD_RUNDIR) + strlen(TF_DIR)
		    + 1024 + 2);
    if (tf_dir == NULL) {
	syslog(LOG_ERR, "auth_krb5_init malloc(tf_dir) failed");
	return -1;
    }
    strcpy(tf_dir, PATH_SASLAUTHD_RUNDIR);
    strcat(tf_dir, TF_DIR);

    /* Check for an existing directory. */
    rc = lstat(tf_dir, &sb);
    if (rc == -1) {
	if (errno == ENOENT) {
	    /* Create the ticket file directory */
	    rc = mkdir(tf_dir, 0700);
	    if (rc == -1) {
		syslog(LOG_ERR, "auth_krb5_init mkdir(%s): %m",
		       tf_dir);
		free(tf_dir);
		tf_dir = NULL;
		return -1;
	    }
	} else {
	    rc = errno;
	    syslog(LOG_ERR, "auth_krb5_init: %s: %m", tf_dir);
	    free(tf_dir);
	    tf_dir = NULL;
	    return -1;
	}
    }

    /* Make sure it's not a symlink. */
    if (S_ISLNK(sb.st_mode)) {
        syslog(LOG_ERR, "auth_krb5_init: %s: is a symbolic link", tf_dir);
	free(tf_dir);
	tf_dir = NULL;
	return -1;
    }
    
    return 0;

#else
    return -1;
#endif
}

#ifdef AUTH_KRB5

#ifdef KRB5_HEIMDAL

char *					/* R: allocated response string */
auth_krb5 (
  /* PARAMETERS */
  const char *user,			/* I: plaintext authenticator */
  const char *password,			/* I: plaintext password */
  const char *service __attribute__((unused)),
  const char *realm __attribute__((unused))
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    krb5_context context;
    krb5_ccache ccache = NULL;
    krb5_principal auth_user;
    char * result;
    char tfname[2048];
    /* END VARIABLES */

    if (!user|| !password) {
	syslog(LOG_ERR, "auth_krb5: NULL password or username?");
	return strdup("NO saslauthd internal NULL password or username");
    }

    if (krb5_init_context(&context)) {
	syslog(LOG_ERR, "auth_krb5: krb5_init_context");
	return strdup("NO saslauthd internal krb5_init_context error");
    }
    
    if (krb5_parse_name (context, user, &auth_user)) {
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_parse_name");
	return strdup("NO saslauthd internal krb5_parse_name error");
    }
    
#ifdef SASLAUTHD_THREADED
    /* create a new CCACHE so we don't stomp on anything */
    snprintf(tfname,sizeof(tfname), "%s/k5cc_%d_%d", tf_dir,
	     getpid(), pthread_self());
#else
    /* create a new CCACHE so we don't stomp on anything */
    snprintf(tfname,sizeof(tfname), "%s/k5cc_%d", tf_dir, getpid());
#endif
    if (krb5_cc_resolve(context, tfname, &ccache)) {
	krb5_free_principal(context, auth_user);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_cc_resolve");
	return strdup("NO saslauthd internal error");
    }
    
    if (krb5_cc_initialize (context, ccache, auth_user)) {
	krb5_free_principal(context, auth_user);
	krb5_cc_destroy(context, ccache);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_cc_initialize");
	return strdup("NO saslauthd internal error");
    }

    if (krb5_verify_user(context, auth_user, ccache, password, 1, NULL)) {
	result = strdup("NO krb5_verify_user failed");
    } else {
	result = strdup("OK");
    }

    krb5_free_principal(context, auth_user);
    krb5_cc_destroy(context, ccache);
    krb5_free_context(context);

    return result;
}

#else /* !KRB5_HEIMDAL */

/* returns 0 for failure, 1 for success */
static int k5support_verify_tgt(krb5_context context, 
				krb5_ccache ccache) 
{
    krb5_principal server;
    krb5_data packet;
    krb5_keyblock *keyblock = NULL;
    krb5_auth_context auth_context = NULL;
    krb5_error_code k5_retcode;
    char thishost[BUFSIZ];
    int result = 0;
    
    if (krb5_sname_to_principal(context, NULL, NULL,
				KRB5_NT_SRV_HST, &server)) {
	return 0;
    }
    
    if (krb5_kt_read_service_key(context, NULL, server, 0,
				 0, &keyblock)) {
	goto fini;
    }
    
    if (keyblock) {
	krb5_free_keyblock(context, keyblock);
    }
    
    /* this duplicates work done in krb5_sname_to_principal
     * oh well.
     */
    if (gethostname(thishost, BUFSIZ) < 0) {
	goto fini;
    }
    thishost[BUFSIZ-1] = '\0';
    
#ifdef KRB5_HEIMDAL
    krb5_data_zero(&packet);
#endif
    k5_retcode = krb5_mk_req(context, &auth_context, 0, "host", 
			     thishost, NULL, ccache, &packet);
    
    if (auth_context) {
	krb5_auth_con_free(context, auth_context);
	auth_context = NULL;
    }
    
    if (k5_retcode) {
	goto fini;
    }
    
    if (krb5_rd_req(context, &auth_context, &packet, 
		    server, NULL, NULL, NULL)) {
	goto fini;
    }
    
    if (auth_context) {
	krb5_auth_con_free(context, auth_context);
	auth_context = NULL;
    }

    /* all is good now */
    result = 1;
 fini:
#ifndef KRB5_HEIMDAL
    krb5_free_data_contents(context, &packet);
#endif
    krb5_free_principal(context, server);
    
    return result;
}

/* FUNCTION: auth_krb5 */

/* SYNOPSIS
 * Authenticate against Kerberos IV.
 * END SYNOPSIS */

char *					/* R: allocated response string */
auth_krb5 (
  /* PARAMETERS */
  const char *user,			/* I: plaintext authenticator */
  const char *password,			/* I: plaintext password */
  const char *service __attribute__((unused)),
  const char *realm __attribute__((unused))
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    krb5_context context;
    krb5_ccache ccache = NULL;
    krb5_principal auth_user;
    krb5_creds creds;
    krb5_get_init_creds_opt opts;
    char * result;
    char tfname[40];
    /* END VARIABLES */

    if (!user|| !password) {
	syslog(LOG_ERR, "auth_krb5: NULL password or username?");
	return strdup("NO saslauthd internal error");
    }

    if (krb5_init_context(&context)) {
	syslog(LOG_ERR, "auth_krb5: krb5_init_context");
	return strdup("NO saslauthd internal error");
    }
    
    if (krb5_parse_name (context, user, &auth_user)) {
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_parse_name");
	return strdup("NO saslauthd internal error");
    }
    
    /* create a new CCACHE so we don't stomp on anything */
    snprintf(tfname,sizeof(tfname), "/tmp/k5cc_%d", getpid());
    if (krb5_cc_resolve(context, tfname, &ccache)) {
	krb5_free_principal(context, auth_user);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_cc_resolve");
	return strdup("NO saslauthd internal error");
    }
    
    if (krb5_cc_initialize (context, ccache, auth_user)) {
	krb5_free_principal(context, auth_user);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_cc_initialize");
	return strdup("NO saslauthd internal error");
    }
    
    krb5_get_init_creds_opt_init(&opts);
    /* 15 min should be more than enough */
    krb5_get_init_creds_opt_set_tkt_life(&opts, 900); 
    if (krb5_get_init_creds_password(context, &creds, 
				     auth_user, password, NULL, NULL, 
				     0, NULL, &opts)) {
	krb5_cc_destroy(context, ccache);
	krb5_free_principal(context, auth_user);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_get_init_creds_password");
	return strdup("NO saslauthd internal error");
    }
    
    /* at this point we should have a TGT. Let's make sure it is valid */
    if (krb5_cc_store_cred(context, ccache, &creds)) {
	krb5_free_principal(context, auth_user);
	krb5_cc_destroy(context, ccache);
	krb5_free_context(context);
	syslog(LOG_ERR, "auth_krb5: krb5_cc_store_cred");
	return strdup("NO saslauthd internal error");
    }
    
    if (!k5support_verify_tgt(context, ccache)) {
	syslog(LOG_ERR, "auth_krb5: k5support_verify_tgt");
	result = strdup("NO saslauthd internal error");
	goto fini;
    }
    
    /* 
     * fall through -- user is valid beyond this point  
     */
    
    result = strdup("OK");
 fini:
/* destroy any tickets we had */
    krb5_free_cred_contents(context, &creds);
    krb5_free_principal(context, auth_user);
    krb5_cc_destroy(context, ccache);
    krb5_free_context(context);

    return result;
}

#endif /* KRB5_HEIMDAL */

#else /* ! AUTH_KRB5 */

char *
auth_krb5 (
  const char *login __attribute__((unused)),
  const char *password __attribute__((unused)),
  const char *service __attribute__((unused)),
  const char *realm __attribute__((unused))
  )
{
    return NULL;
}

#endif /* ! AUTH_KRB5 */

/* END FUNCTION: auth_krb5 */

/* END MODULE: auth_krb5 */
