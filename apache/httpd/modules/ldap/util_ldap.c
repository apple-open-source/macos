/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * util_ldap.c: LDAP things
 *
 * Original code from auth_ldap module for Apache v1.3:
 * Copyright 1998, 1999 Enbridge Pipelines Inc.
 * Copyright 1999-2001 Dave Carrigan
 */

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_mutex.h"
#include "util_ldap.h"
#include "util_ldap_cache.h"

#include <apr_strings.h>

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#if !APR_HAS_LDAP
#error mod_ldap requires APR-util to have LDAP support built in
#endif

/* Default define for ldap functions that need a SIZELIMIT but
 * do not have the define
 * XXX This should be removed once a supporting #define is
 *  released through APR-Util.
 */
#ifndef APR_LDAP_SIZELIMIT
#define APR_LDAP_SIZELIMIT -1
#endif

#ifdef LDAP_OPT_DEBUG_LEVEL
#define AP_LDAP_OPT_DEBUG LDAP_OPT_DEBUG_LEVEL
#else
#ifdef LDAP_OPT_DEBUG
#define AP_LDAP_OPT_DEBUG LDAP_OPT_DEBUG
#endif
#endif

#define AP_LDAP_HOPLIMIT_UNSET -1
#define AP_LDAP_CHASEREFERRALS_SDKDEFAULT -1
#define AP_LDAP_CHASEREFERRALS_OFF 0
#define AP_LDAP_CHASEREFERRALS_ON 1

#define AP_LDAP_CONNPOOL_DEFAULT -1
#define AP_LDAP_CONNPOOL_INFINITE -2

#if !defined(LDAP_OPT_NETWORK_TIMEOUT) && defined(LDAP_OPT_CONNECT_TIMEOUT)
#define LDAP_OPT_NETWORK_TIMEOUT LDAP_OPT_CONNECT_TIMEOUT
#endif

module AP_MODULE_DECLARE_DATA ldap_module;
static const char *ldap_cache_mutex_type = "ldap-cache";
static apr_status_t uldap_connection_unbind(void *param);

/* For OpenLDAP with the 3-arg version of ldap_set_rebind_proc(), use
 * a simpler rebind callback than the implementation in APR-util.
 * Testing for API version >= 3001 appears safe although OpenLDAP
 * 2.1.x (API version = 2004) also has the 3-arg API. */
#if APR_HAS_OPENLDAP_LDAPSDK && defined(LDAP_API_VERSION) && LDAP_API_VERSION >= 3001

#define uldap_rebind_init(p) APR_SUCCESS /* noop */

static int uldap_rebind_proc(LDAP *ld, const char *url, ber_tag_t request,
                             ber_int_t msgid, void *params)
{
    util_ldap_connection_t *ldc = params;

    return ldap_bind_s(ld, ldc->binddn, ldc->bindpw, LDAP_AUTH_SIMPLE);
}

static apr_status_t uldap_rebind_add(util_ldap_connection_t *ldc)
{
    ldap_set_rebind_proc(ldc->ldap, uldap_rebind_proc, ldc);
    return APR_SUCCESS;
}

#else /* !APR_HAS_OPENLDAP_LDAPSDK */

#define USE_APR_LDAP_REBIND
#include <apr_ldap_rebind.h>

#define uldap_rebind_init(p) apr_ldap_rebind_init(p)
#define uldap_rebind_add(ldc) apr_ldap_rebind_add((ldc)->rebind_pool, \
                                                  (ldc)->ldap, (ldc)->binddn, \
                                                  (ldc)->bindpw)
#endif

static APR_INLINE apr_status_t ldap_cache_lock(util_ldap_state_t *st, request_rec *r) { 
    apr_status_t rv = APR_SUCCESS;
    if (st->util_ldap_cache_lock) { 
        apr_status_t rv = apr_global_mutex_lock(st->util_ldap_cache_lock);
        if (rv != APR_SUCCESS) { 
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, rv, r, APLOGNO(10134) "LDAP cache lock failed");
            ap_assert(0);
        }
    }
    return rv; 
}
static APR_INLINE apr_status_t ldap_cache_unlock(util_ldap_state_t *st, request_rec *r) { 
    apr_status_t rv = APR_SUCCESS;
    if (st->util_ldap_cache_lock) { 
        apr_status_t rv = apr_global_mutex_unlock(st->util_ldap_cache_lock);
        if (rv != APR_SUCCESS) { 
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, rv, r, APLOGNO(10135) "LDAP cache lock failed");
            ap_assert(0);
        }
    }
    return rv; 
}

static void util_ldap_strdup (char **str, const char *newstr)
{
    if (*str) {
        free(*str);
        *str = NULL;
    }

    if (newstr) {
        *str = strdup(newstr);
    }
}

/*
 * Status Handler
 * --------------
 *
 * This handler generates a status page about the current performance of
 * the LDAP cache. It is enabled as follows:
 *
 * <Location /ldap-status>
 *   SetHandler ldap-status
 * </Location>
 *
 */
static int util_ldap_handler(request_rec *r)
{
    util_ldap_state_t *st;

    r->allowed |= (1 << M_GET);
    if (r->method_number != M_GET) {
        return DECLINED;
    }

    if (strcmp(r->handler, "ldap-status")) {
        return DECLINED;
    }

    st = (util_ldap_state_t *) ap_get_module_config(r->server->module_config,
            &ldap_module);

    ap_set_content_type_ex(r, "text/html; charset=ISO-8859-1", 1);

    if (r->header_only)
        return OK;

    ap_rputs(DOCTYPE_HTML_3_2
             "<html><head><title>LDAP Cache Information</title></head>\n", r);
    ap_rputs("<body bgcolor='#ffffff'><h1 align=center>LDAP Cache Information"
             "</h1>\n", r);

    util_ald_cache_display(r, st);

    return OK;
}



/* ------------------------------------------------------------------ */
/*
 * Closes an LDAP connection by unlocking it. The next time
 * uldap_connection_find() is called this connection will be
 * available for reuse.
 */
static void uldap_connection_close(util_ldap_connection_t *ldc)
{

     /* We leave bound LDAP connections floating around in our pool,
      * but always check/fix the binddn/bindpw when we take them out
      * of the pool
      */
     if (!ldc->keep) {
         uldap_connection_unbind(ldc);
         ldc->r = NULL;
     }
     else {
         /* mark our connection as available for reuse */
         ldc->freed = apr_time_now();
         ldc->r = NULL;
     }

#if APR_HAS_THREADS
     apr_thread_mutex_unlock(ldc->lock);
#endif
}


/*
 * Destroys an LDAP connection by unbinding and closing the connection to
 * the LDAP server. It is used to bring the connection back to a known
 * state after an error.
 */
static apr_status_t uldap_connection_unbind(void *param)
{
    util_ldap_connection_t *ldc = param;

    if (ldc) {
#ifdef USE_APR_LDAP_REBIND
        /* forget the rebind info for this conn */
        if (ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) {
            apr_pool_clear(ldc->rebind_pool);
        }
#endif

        if (ldc->ldap) {
            if (ldc->r) { 
                ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, ldc->r, "LDC %pp unbind", ldc); 
            }
            ldap_unbind_s(ldc->ldap);
            ldc->ldap = NULL;
        }
        ldc->bound = 0;
    }

    return APR_SUCCESS;
}

/* not presently used, not part of the API */
#if 0
/*
 * util_ldap_connection_remove frees all storage associated with the LDAP
 * connection and removes it completely from the per-virtualhost list of
 * connections
 *
 * The caller should hold the lock for this connection
 */
static apr_status_t util_ldap_connection_remove (void *param)
{
    util_ldap_connection_t *ldc = param, *l = NULL, *prev = NULL;
    util_ldap_state_t *st;

    if (!ldc) return APR_SUCCESS;

    st = ldc->st;

    uldap_connection_unbind(ldc);

#if APR_HAS_THREADS
    apr_thread_mutex_lock(st->mutex);
#endif

    /* Remove ldc from the list */
    for (l=st->connections; l; l=l->next) {
        if (l == ldc) {
            if (prev) {
                prev->next = l->next;
            }
            else {
                st->connections = l->next;
            }
            break;
        }
        prev = l;
    }

    if (ldc->bindpw) {
        free((void*)ldc->bindpw);
    }
    if (ldc->binddn) {
        free((void*)ldc->binddn);
    }

#if APR_HAS_THREADS
    apr_thread_mutex_unlock(ldc->lock);
    apr_thread_mutex_unlock(st->mutex);
#endif

    /* Destroy the pool associated with this connection */

    apr_pool_destroy(ldc->pool);

    return APR_SUCCESS;
}
#endif

static int uldap_connection_init(request_rec *r,
                                 util_ldap_connection_t *ldc)
{
    int rc = 0, ldap_option = 0;
    int version  = LDAP_VERSION3;
    apr_ldap_err_t *result = NULL;
#ifdef LDAP_OPT_NETWORK_TIMEOUT
    struct timeval connectionTimeout = {0};
#endif
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(r->server->module_config,
        &ldap_module);
    int have_client_certs = !apr_is_empty_array(ldc->client_certs);
#if !APR_HAS_SOLARIS_LDAPSDK
    /*
     * Normally we enable SSL/TLS with apr_ldap_set_option(), except
     * with Solaris LDAP, where this is broken.
     */
    int secure = APR_LDAP_NONE;
#else
    /*
     * With Solaris LDAP, we enable TSL via the secure argument
     * to apr_ldap_init(). This requires a fix from apr-util >= 1.4.0.
     *
     * Just in case client certificates ever get supported, we
     * handle those as with the other LDAP SDKs.
     */
    int secure = have_client_certs ? APR_LDAP_NONE : ldc->secure;
#endif

    /* Since the host will include a port if the default port is not used,
     * always specify the default ports for the port parameter.  This will
     * allow a host string that contains multiple hosts the ability to mix
     * some hosts with ports and some without. All hosts which do not
     * specify a port will use the default port.
     */
    apr_ldap_init(r->pool, &(ldc->ldap),
                  ldc->host,
                  APR_LDAP_SSL == ldc->secure ? LDAPS_PORT : LDAP_PORT,
                  secure, &(result));

    if (NULL == result) {
        /* something really bad happened */
        ldc->bound = 0;
        if (NULL == ldc->reason) {
            ldc->reason = "LDAP: ldap initialization failed";
        }
        return(APR_EGENERAL);
    }

    if (result->rc) {
        ldc->reason = result->reason;
        ldc->bound = 0;
        return result->rc;
    }

    if (NULL == ldc->ldap)
    {
        ldc->bound = 0;
        if (NULL == ldc->reason) {
            ldc->reason = "LDAP: ldap initialization failed";
        }
        else {
            ldc->reason = result->reason;
        }
        return(result->rc);
    }

    ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "LDC %pp init", ldc);

    if (ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) {
        /* Now that we have an ldap struct, add it to the referral list for rebinds. */
        rc = uldap_rebind_add(ldc);
        if (rc != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rc, r->server, APLOGNO(01277)
                    "LDAP: Unable to add rebind cross reference entry. Out of memory?");
            uldap_connection_unbind(ldc);
            ldc->reason = "LDAP: Unable to add rebind cross reference entry.";
            return(rc);
        }
    }

    /* always default to LDAP V3 */
    ldap_set_option(ldc->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);

    /* set client certificates */
    if (have_client_certs) {
        apr_ldap_set_option(r->pool, ldc->ldap, APR_LDAP_OPT_TLS_CERT,
                            ldc->client_certs, &(result));
        if (LDAP_SUCCESS != result->rc) {
            uldap_connection_unbind( ldc );
            ldc->reason = result->reason;
            return(result->rc);
        }
    }

    /* switch on SSL/TLS */
    if (APR_LDAP_NONE != ldc->secure
#if APR_HAS_SOLARIS_LDAPSDK
        /* See comments near apr_ldap_init() above */
        && have_client_certs
#endif
       ) {
        apr_ldap_set_option(r->pool, ldc->ldap,
                            APR_LDAP_OPT_TLS, &ldc->secure, &(result));
        if (LDAP_SUCCESS != result->rc) {
            uldap_connection_unbind( ldc );
            ldc->reason = result->reason;
            return(result->rc);
        }
    }

    /* Set the alias dereferencing option */
    ldap_option = ldc->deref;
    ldap_set_option(ldc->ldap, LDAP_OPT_DEREF, &ldap_option);

    if (ldc->ChaseReferrals != AP_LDAP_CHASEREFERRALS_SDKDEFAULT) {
        /* Set options for rebind and referrals. */
        ap_log_error(APLOG_MARK, APLOG_TRACE4, 0, r->server, APLOGNO(01278)
                "LDAP: Setting referrals to %s.",
                ((ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) ? "On" : "Off"));
        apr_ldap_set_option(r->pool, ldc->ldap,
                APR_LDAP_OPT_REFERRALS,
                (void *)((ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) ?
                    LDAP_OPT_ON : LDAP_OPT_OFF),
                &(result));
        if (result->rc != LDAP_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01279)
                    "Unable to set LDAP_OPT_REFERRALS option to %s: %d.",
                    ((ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) ? "On" : "Off"),
                    result->rc);
            result->reason = "Unable to set LDAP_OPT_REFERRALS.";
            ldc->reason = result->reason;
            uldap_connection_unbind(ldc);
            return(result->rc);
        }
    }

    if (ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) {
        if ((ldc->ReferralHopLimit != AP_LDAP_HOPLIMIT_UNSET) && ldc->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) {
            /* Referral hop limit - only if referrals are enabled and a hop limit is explicitly requested */
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01280)
                    "Setting referral hop limit to %d.",
                    ldc->ReferralHopLimit);
            apr_ldap_set_option(r->pool, ldc->ldap,
                    APR_LDAP_OPT_REFHOPLIMIT,
                    (void *)&ldc->ReferralHopLimit,
                    &(result));
            if (result->rc != LDAP_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01281)
                        "Unable to set LDAP_OPT_REFHOPLIMIT option to %d: %d.",
                        ldc->ReferralHopLimit,
                        result->rc);
                result->reason = "Unable to set LDAP_OPT_REFHOPLIMIT.";
                ldc->reason = result->reason;
                uldap_connection_unbind(ldc);
                return(result->rc);
            }
        }
    }

/*XXX All of the #ifdef's need to be removed once apr-util 1.2 is released */
#ifdef APR_LDAP_OPT_VERIFY_CERT
    apr_ldap_set_option(r->pool, ldc->ldap, APR_LDAP_OPT_VERIFY_CERT,
                        &(st->verify_svr_cert), &(result));
#else
#if defined(LDAPSSL_VERIFY_SERVER)
    if (st->verify_svr_cert) {
        result->rc = ldapssl_set_verify_mode(LDAPSSL_VERIFY_SERVER);
    }
    else {
        result->rc = ldapssl_set_verify_mode(LDAPSSL_VERIFY_NONE);
    }
#elif defined(LDAP_OPT_X_TLS_REQUIRE_CERT)
    /* This is not a per-connection setting so just pass NULL for the
       Ldap connection handle */
    if (st->verify_svr_cert) {
        int i = LDAP_OPT_X_TLS_DEMAND;
        result->rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &i);
    }
    else {
        int i = LDAP_OPT_X_TLS_NEVER;
        result->rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &i);
    }
#endif
#endif

#ifdef LDAP_OPT_NETWORK_TIMEOUT
    if (st->connectionTimeout > 0) {
        connectionTimeout.tv_sec = st->connectionTimeout;
    }

    if (connectionTimeout.tv_sec > 0) {
        rc = apr_ldap_set_option(r->pool, ldc->ldap, LDAP_OPT_NETWORK_TIMEOUT,
                                 (void *)&connectionTimeout, &(result));
        if (APR_SUCCESS != rc) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(01282)
                             "LDAP: Could not set the connection timeout");
        }
    }
#endif

#ifdef LDAP_OPT_TIMEOUT
    /*
     * LDAP_OPT_TIMEOUT is not portable, but it influences all synchronous ldap
     * function calls and not just ldap_search_ext_s(), which accepts a timeout
     * parameter.
     * XXX: It would be possible to simulate LDAP_OPT_TIMEOUT by replacing all
     * XXX: synchronous ldap function calls with asynchronous calls and using
     * XXX: ldap_result() with a timeout.
     */
    if (st->opTimeout) {
        rc = apr_ldap_set_option(r->pool, ldc->ldap, LDAP_OPT_TIMEOUT,
                                 st->opTimeout, &(result));
        if (APR_SUCCESS != rc) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(01283)
                             "LDAP: Could not set LDAP_OPT_TIMEOUT");
        }
    }
#endif

    return(rc);
}

static int uldap_ld_errno(util_ldap_connection_t *ldc)
{
    int ldaprc;
#ifdef LDAP_OPT_ERROR_NUMBER
    if (LDAP_SUCCESS == ldap_get_option(ldc->ldap, LDAP_OPT_ERROR_NUMBER, &ldaprc)) return ldaprc;
#endif
#ifdef LDAP_OPT_RESULT_CODE
    if (LDAP_SUCCESS == ldap_get_option(ldc->ldap, LDAP_OPT_RESULT_CODE, &ldaprc)) return ldaprc;
#endif
    return LDAP_OTHER;
}

/*
 * Replacement function for ldap_simple_bind_s() with a timeout.
 * To do this in a portable way, we have to use ldap_simple_bind() and
 * ldap_result().
 *
 * Returns LDAP_SUCCESS on success; and an error code on failure
 */
static int uldap_simple_bind(util_ldap_connection_t *ldc, char *binddn,
                             char* bindpw, struct timeval *timeout)
{
    LDAPMessage *result;
    int rc;
    int msgid = ldap_simple_bind(ldc->ldap, binddn, bindpw);
    if (msgid == -1) {
        ldc->reason = "LDAP: ldap_simple_bind() failed";
        return uldap_ld_errno(ldc);
    }
    rc = ldap_result(ldc->ldap, msgid, 0, timeout, &result);
    if (rc == -1) {
        ldc->reason = "LDAP: ldap_simple_bind() result retrieval failed";
        /* -1 is LDAP_SERVER_DOWN in openldap, use something else */
        return uldap_ld_errno(ldc);
    }
    else if (rc == 0) {
        ldc->reason = "LDAP: ldap_simple_bind() timed out";
        rc = LDAP_TIMEOUT;
    } else if (ldap_parse_result(ldc->ldap, result, &rc, NULL, NULL, NULL,
                                 NULL, 1) == -1) {
        ldc->reason = "LDAP: ldap_simple_bind() parse result failed";
        return uldap_ld_errno(ldc);
    }
    else { 
        ldc->last_backend_conn = ldc->r->request_time;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, ldc->r, "LDC %pp bind", ldc);
    }
    return rc;
}

/*
 * Connect to the LDAP server and binds. Does not connect if already
 * connected (i.e. ldc->ldap is non-NULL.) Does not bind if already bound.
 *
 * Returns LDAP_SUCCESS on success; and an error code on failure
 */
static int uldap_connection_open(request_rec *r,
                                 util_ldap_connection_t *ldc)
{
    int rc = 0;
    int failures = 0;
    int new_connection = 0;
    util_ldap_state_t *st;

    /* sanity check for NULL */
    if (!ldc) {
        return -1;
    }

    /* If the connection is already bound, return
    */
    if (ldc->bound && !ldc->must_rebind)
    {
        ldc->reason = "LDAP: connection open successful (already bound)";
        return LDAP_SUCCESS;
    }

    /* create the ldap session handle
    */
    if (NULL == ldc->ldap)
    {
       new_connection = 1;
       rc = uldap_connection_init( r, ldc );
       if (LDAP_SUCCESS != rc)
       {
           return rc;
       }
    }


    st = (util_ldap_state_t *)ap_get_module_config(r->server->module_config,
                                                   &ldap_module);

    /* loop trying to bind up to st->retries times if LDAP_SERVER_DOWN or LDAP_TIMEOUT
     * are returned.  Close the connection before the first retry, and then on every
     * other retry.
     *
     * On Success or any other error, break out of the loop.
     *
     * NOTE: Looping is probably not a great idea. If the server isn't
     * responding the chances it will respond after a few tries are poor.
     * However, the original code looped and it only happens on
     * the error condition.
     */

    while (failures <= st->retries) {
        if (failures > 0 && st->retry_delay > 0) {
            apr_sleep(st->retry_delay);
        }
        rc = uldap_simple_bind(ldc, (char *)ldc->binddn, (char *)ldc->bindpw,
                               st->opTimeout);

        if (rc == LDAP_SUCCESS) break;

        failures++;

        if (AP_LDAP_IS_SERVER_DOWN(rc)) {
             ap_log_rerror(APLOG_MARK, APLOG_TRACE2, 0, r,
                          "ldap_simple_bind() failed with server down "
                          "(try %d)", failures);
        }
        else if (rc == LDAP_TIMEOUT) {
            ap_log_rerror(APLOG_MARK, APLOG_INFO, 0, r, APLOGNO(01284)
                          "ldap_simple_bind() timed out on %s "
                          "connection, dropped by firewall?",
                          new_connection ? "new" : "reused");
        }
        else {
            /* Other errors not retryable */
            break;
        }

        if (!(failures % 2)) {
            ap_log_rerror(APLOG_MARK, APLOG_TRACE2, 0, r,
                          "attempt to re-init the connection");
            uldap_connection_unbind(ldc);
            if (LDAP_SUCCESS != uldap_connection_init(r, ldc)) {
                /* leave rc as the initial bind return code */
                break;
            }
        }
    }

    /* free the handle if there was an error
    */
    if (LDAP_SUCCESS != rc)
    {
        uldap_connection_unbind(ldc);
        ldc->reason = "LDAP: ldap_simple_bind() failed";
    }
    else {
        ldc->bound = 1;
        ldc->must_rebind = 0;
        ldc->reason = "LDAP: connection open successful";
    }

    return(rc);
}


/*
 * Compare client certificate arrays.
 *
 * Returns 1 on compare failure, 0 otherwise.
 */
static int compare_client_certs(apr_array_header_t *srcs,
                                apr_array_header_t *dests)
{
    int i = 0;
    struct apr_ldap_opt_tls_cert_t *src, *dest;

    /* arrays both NULL? if so, then equal */
    if (srcs == NULL && dests == NULL) {
        return 0;
    }

    /* arrays different length or either NULL? If so, then not equal */
    if (srcs == NULL || dests == NULL || srcs->nelts != dests->nelts) {
        return 1;
    }

    /* run an actual comparison */
    src = (struct apr_ldap_opt_tls_cert_t *)srcs->elts;
    dest = (struct apr_ldap_opt_tls_cert_t *)dests->elts;
    for (i = 0; i < srcs->nelts; i++) {
        if ((strcmp(src[i].path, dest[i].path)) ||
            (src[i].type != dest[i].type) ||
            /* One is passwordless? If so, then not equal */
            ((src[i].password == NULL) ^ (dest[i].password == NULL)) ||
            (src[i].password != NULL && dest[i].password != NULL &&
             strcmp(src[i].password, dest[i].password))) {
            return 1;
        }
    }

    /* if we got here, the cert arrays were identical */
    return 0;

}


/*
 * Find an existing ldap connection struct that matches the
 * provided ldap connection parameters.
 *
 * If not found in the cache, a new ldc structure will be allocated
 * from st->pool and returned to the caller.  If found in the cache,
 * a pointer to the existing ldc structure will be returned.
 */
static util_ldap_connection_t *
            uldap_connection_find(request_rec *r,
                                  const char *host, int port,
                                  const char *binddn, const char *bindpw,
                                  deref_options deref, int secure)
{
    struct util_ldap_connection_t *l, *p; /* To traverse the linked list */
    int secureflag = secure;
    apr_time_t now = apr_time_now();

    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(r->server->module_config,
        &ldap_module);
    util_ldap_config_t *dc =
        (util_ldap_config_t *) ap_get_module_config(r->per_dir_config, &ldap_module);

#if APR_HAS_THREADS
    /* mutex lock this function */
    apr_thread_mutex_lock(st->mutex);
#endif

    if (secure < APR_LDAP_NONE) {
        secureflag = st->secure;
    }

    /* Search for an exact connection match in the list that is not
     * being used.
     */
    for (l=st->connections,p=NULL; l; l=l->next) {
#if APR_HAS_THREADS
        if (APR_SUCCESS == apr_thread_mutex_trylock(l->lock)) {
#endif
        if (   (l->port == port) && (strcmp(l->host, host) == 0)
            && ((!l->binddn && !binddn) || (l->binddn && binddn
                                             && !strcmp(l->binddn, binddn)))
            && ((!l->bindpw && !bindpw) || (l->bindpw && bindpw
                                             && !strcmp(l->bindpw, bindpw)))
            && (l->deref == deref) && (l->secure == secureflag)
            && !compare_client_certs(dc->client_certs, l->client_certs))
        {
            if (st->connection_pool_ttl > 0) {
                if (l->bound && (now - l->last_backend_conn) > st->connection_pool_ttl) {
                    ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
                                  "Removing LDAP connection last used %" APR_TIME_T_FMT " seconds ago",
                                  (now - l->last_backend_conn) / APR_USEC_PER_SEC);
                    l->r = r;
                    uldap_connection_unbind(l);
                    /* Go ahead (by falling through) and use it, so we don't create more just to unbind some other old ones */
                }
                ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, 
                              "Reuse %s LDC %pp", 
                              l->bound ? "bound" : "unbound", l);
            }
            break;
        }
#if APR_HAS_THREADS
            /* If this connection didn't match the criteria, then we
             * need to unlock the mutex so it is available to be reused.
             */
            apr_thread_mutex_unlock(l->lock);
        }
#endif
        p = l;
    }

    /* If nothing found, search again, but we don't care about the
     * binddn and bindpw this time.
     */
    if (!l) {
        for (l=st->connections,p=NULL; l; l=l->next) {
#if APR_HAS_THREADS
            if (APR_SUCCESS == apr_thread_mutex_trylock(l->lock)) {

#endif
            if ((l->port == port) && (strcmp(l->host, host) == 0) &&
                (l->deref == deref) && (l->secure == secureflag) &&
                !compare_client_certs(dc->client_certs, l->client_certs))
            {
                if (st->connection_pool_ttl > 0) {
                    if (l->bound && (now - l->last_backend_conn) > st->connection_pool_ttl) {
                        ap_log_rerror(APLOG_MARK, APLOG_TRACE1, 0, r,
                                "Removing LDAP connection last used %" APR_TIME_T_FMT " seconds ago",
                                (now - l->last_backend_conn) / APR_USEC_PER_SEC);
                        l->r = r;
                        uldap_connection_unbind(l);
                        /* Go ahead (by falling through) and use it, so we don't create more just to unbind some other old ones */
                    }
                    ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, 
                                  "Reuse %s LDC %pp (will rebind)", 
                                   l->bound ? "bound" : "unbound", l);
                }

                /* the bind credentials have changed */
                l->must_rebind = 1;
                util_ldap_strdup((char**)&(l->binddn), binddn);
                util_ldap_strdup((char**)&(l->bindpw), bindpw);

                break;
            }
#if APR_HAS_THREADS
                /* If this connection didn't match the criteria, then we
                 * need to unlock the mutex so it is available to be reused.
                 */
                apr_thread_mutex_unlock(l->lock);
            }
#endif
            p = l;
        }
    }

/* artificially disable cache */
/* l = NULL; */

    /* If no connection was found after the second search, we
     * must create one.
     */
    if (!l) {
        apr_pool_t *newpool;
        if (apr_pool_create(&newpool, NULL) != APR_SUCCESS) {
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0, r, APLOGNO(01285)
                          "util_ldap: Failed to create memory pool");
#if APR_HAS_THREADS
            apr_thread_mutex_unlock(st->mutex);
#endif
            return NULL;
        }
        apr_pool_tag(newpool, "util_ldap_connection");

        /*
         * Add the new connection entry to the linked list. Note that we
         * don't actually establish an LDAP connection yet; that happens
         * the first time authentication is requested.
         */

        /* create the details of this connection in the new pool */
        l = apr_pcalloc(newpool, sizeof(util_ldap_connection_t));
        l->pool = newpool;
        l->st = st;

#if APR_HAS_THREADS
        apr_thread_mutex_create(&l->lock, APR_THREAD_MUTEX_DEFAULT, l->pool);
        apr_thread_mutex_lock(l->lock);
#endif
        l->bound = 0;
        l->host = apr_pstrdup(l->pool, host);
        l->port = port;
        l->deref = deref;
        util_ldap_strdup((char**)&(l->binddn), binddn);
        util_ldap_strdup((char**)&(l->bindpw), bindpw);
        l->ChaseReferrals = dc->ChaseReferrals;
        l->ReferralHopLimit = dc->ReferralHopLimit;

        /* The security mode after parsing the URL will always be either
         * APR_LDAP_NONE (ldap://) or APR_LDAP_SSL (ldaps://).
         * If the security setting is NONE, override it to the security
         * setting optionally supplied by the admin using LDAPTrustedMode
         */
        l->secure = secureflag;

        /* save away a copy of the client cert list that is presently valid */
        l->client_certs = apr_array_copy_hdr(l->pool, dc->client_certs);

        /* whether or not to keep this connection in the pool when it's returned */
        l->keep = (st->connection_pool_ttl == 0) ? 0 : 1;

#ifdef USE_APR_LDAP_REBIND
        if (l->ChaseReferrals == AP_LDAP_CHASEREFERRALS_ON) {
            if (apr_pool_create(&(l->rebind_pool), l->pool) != APR_SUCCESS) {
                ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0, r, APLOGNO(01286)
                              "util_ldap: Failed to create memory pool");
#if APR_HAS_THREADS
                apr_thread_mutex_unlock(st->mutex);
#endif
                return NULL;
            }
            apr_pool_tag(l->rebind_pool, "util_ldap_rebind");
        }
#endif

        if (p) {
            p->next = l;
        }
        else {
            st->connections = l;
        }
    }

#if APR_HAS_THREADS
    apr_thread_mutex_unlock(st->mutex);
#endif
    l->r = r;
    return l;
}

/* ------------------------------------------------------------------ */

/*
 * Compares two DNs to see if they're equal. The only way to do this correctly
 * is to search for the dn and then do ldap_get_dn() on the result. This should
 * match the initial dn, since it would have been also retrieved with
 * ldap_get_dn(). This is expensive, so if the configuration value
 * compare_dn_on_server is false, just does an ordinary strcmp.
 *
 * The lock for the ldap cache should already be acquired.
 */
static int uldap_cache_comparedn(request_rec *r, util_ldap_connection_t *ldc,
                                 const char *url, const char *dn,
                                 const char *reqdn, int compare_dn_on_server)
{
    int result = 0;
    util_url_node_t *curl;
    util_url_node_t curnode;
    util_dn_compare_node_t *node;
    util_dn_compare_node_t newnode;
    int failures = 0;
    LDAPMessage *res, *entry;
    char *searchdn;

    util_ldap_state_t *st = (util_ldap_state_t *)
                            ap_get_module_config(r->server->module_config,
                                                 &ldap_module);

    /* get cache entry (or create one) */
    ldap_cache_lock(st, r);

    curnode.url = url;
    curl = util_ald_cache_fetch(st->util_ldap_cache, &curnode);
    if (curl == NULL) {
        curl = util_ald_create_caches(st, url);
    }
    ldap_cache_unlock(st, r);

    /* a simple compare? */
    if (!compare_dn_on_server) {
        /* unlock this read lock */
        if (strcmp(dn, reqdn)) {
            ldc->reason = "DN Comparison FALSE (direct strcmp())";
            return LDAP_COMPARE_FALSE;
        }
        else {
            ldc->reason = "DN Comparison TRUE (direct strcmp())";
            return LDAP_COMPARE_TRUE;
        }
    }

    if (curl) {
        /* no - it's a server side compare */
        ldap_cache_lock(st, r);

        /* is it in the compare cache? */
        newnode.reqdn = (char *)reqdn;
        node = util_ald_cache_fetch(curl->dn_compare_cache, &newnode);
        if (node != NULL) {
            /* If it's in the cache, it's good */
            /* unlock this read lock */
            ldap_cache_unlock(st, r);
            ldc->reason = "DN Comparison TRUE (cached)";
            return LDAP_COMPARE_TRUE;
        }

        /* unlock this read lock */
        ldap_cache_unlock(st, r);
    }

start_over:
    if (failures > st->retries) {
        return result;
    }

    if (failures > 0 && st->retry_delay > 0) {
        apr_sleep(st->retry_delay);
    }

    /* make a server connection */
    if (LDAP_SUCCESS != (result = uldap_connection_open(r, ldc))) {
        /* connect to server failed */
        return result;
    }

    /* search for reqdn */
    result = ldap_search_ext_s(ldc->ldap, (char *)reqdn, LDAP_SCOPE_BASE,
                               "(objectclass=*)", NULL, 1,
                               NULL, NULL, st->opTimeout, APR_LDAP_SIZELIMIT, &res);
    if (AP_LDAP_IS_SERVER_DOWN(result))
    {
        ldc->reason = "DN Comparison ldap_search_ext_s() "
                      "failed with server down";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }
    if (result == LDAP_TIMEOUT && failures == 0) {
        /*
         * we are reusing a connection that doesn't seem to be active anymore
         * (firewall state drop?), let's try a new connection.
         */
        ldc->reason = "DN Comparison ldap_search_ext_s() "
                      "failed with timeout";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }
    if (result != LDAP_SUCCESS) {
        /* search for reqdn failed - no match */
        ldc->reason = "DN Comparison ldap_search_ext_s() failed";
        return result;
    }

    ldc->last_backend_conn = r->request_time;
    entry = ldap_first_entry(ldc->ldap, res);
    searchdn = ldap_get_dn(ldc->ldap, entry);

    ldap_msgfree(res);
    if (strcmp(dn, searchdn) != 0) {
        /* compare unsuccessful */
        ldc->reason = "DN Comparison FALSE (checked on server)";
        result = LDAP_COMPARE_FALSE;
    }
    else {
        if (curl) {
            /* compare successful - add to the compare cache */
            ldap_cache_lock(st, r);
            newnode.reqdn = (char *)reqdn;
            newnode.dn = (char *)dn;

            node = util_ald_cache_fetch(curl->dn_compare_cache, &newnode);
            if (   (node == NULL)
                || (strcmp(reqdn, node->reqdn) != 0)
                || (strcmp(dn, node->dn) != 0))
            {
                util_ald_cache_insert(curl->dn_compare_cache, &newnode);
            }
            ldap_cache_unlock(st, r);
        }
        ldc->reason = "DN Comparison TRUE (checked on server)";
        result = LDAP_COMPARE_TRUE;
    }
    ldap_memfree(searchdn);
    return result;

}

/*
 * Does an generic ldap_compare operation. It accepts a cache that it will use
 * to lookup the compare in the cache. We cache two kinds of compares
 * (require group compares) and (require user compares). Each compare has a
 * different cache node: require group includes the DN; require user does not
 * because the require user cache is owned by the
 *
 */
static int uldap_cache_compare(request_rec *r, util_ldap_connection_t *ldc,
                               const char *url, const char *dn,
                               const char *attrib, const char *value)
{
    int result = 0;
    util_url_node_t *curl;
    util_url_node_t curnode;
    util_compare_node_t *compare_nodep;
    util_compare_node_t the_compare_node;
    apr_time_t curtime = 0; /* silence gcc -Wall */
    int failures = 0;

    util_ldap_state_t *st = (util_ldap_state_t *)
                            ap_get_module_config(r->server->module_config,
                                                 &ldap_module);

    /* get cache entry (or create one) */
    ldap_cache_lock(st, r);
    curnode.url = url;
    curl = util_ald_cache_fetch(st->util_ldap_cache, &curnode);
    if (curl == NULL) {
        curl = util_ald_create_caches(st, url);
    }
    ldap_cache_unlock(st, r);

    if (curl) {
        /* make a comparison to the cache */
        ldap_cache_lock(st, r);
        curtime = apr_time_now();

        the_compare_node.dn = (char *)dn;
        the_compare_node.attrib = (char *)attrib;
        the_compare_node.value = (char *)value;
        the_compare_node.result = 0;
        the_compare_node.sgl_processed = 0;
        the_compare_node.subgroupList = NULL;

        compare_nodep = util_ald_cache_fetch(curl->compare_cache,
                                             &the_compare_node);

        if (compare_nodep != NULL) {
            /* found it... */
            if (curtime - compare_nodep->lastcompare > st->compare_cache_ttl) {
                /* ...but it is too old */
                util_ald_cache_remove(curl->compare_cache, compare_nodep);
            }
            else {
                /* ...and it is good */
                if (LDAP_COMPARE_TRUE == compare_nodep->result) {
                    ldc->reason = "Comparison true (cached)";
                }
                else if (LDAP_COMPARE_FALSE == compare_nodep->result) {
                    ldc->reason = "Comparison false (cached)";
                }
                else if (LDAP_NO_SUCH_ATTRIBUTE == compare_nodep->result) {
                    ldc->reason = "Comparison no such attribute (cached)";
                }
                else {
                    ldc->reason = apr_psprintf(r->pool, 
                                              "Comparison undefined: (%d): %s (adding to cache)", 
                                              result, ldap_err2string(result));
                }

                /* record the result code to return with the reason... */
                result = compare_nodep->result;
                /* and unlock this read lock */
                ldap_cache_unlock(st, r);

                ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, 
                              "ldap_compare_s(%pp, %s, %s, %s) = %s (cached)", 
                              ldc->ldap, dn, attrib, value, ldap_err2string(result));
                return result;
            }
        }
        /* unlock this read lock */
        ldap_cache_unlock(st, r);
    }

start_over:
    if (failures > st->retries) {
        return result;
    }

    if (failures > 0 && st->retry_delay > 0) {
        apr_sleep(st->retry_delay);
    }

    if (LDAP_SUCCESS != (result = uldap_connection_open(r, ldc))) {
        /* connect failed */
        return result;
    }

    result = ldap_compare_s(ldc->ldap,
                            (char *)dn,
                            (char *)attrib,
                            (char *)value);
    if (AP_LDAP_IS_SERVER_DOWN(result)) {
        /* connection failed - try again */
        ldc->reason = "ldap_compare_s() failed with server down";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }
    if (result == LDAP_TIMEOUT && failures == 0) {
        /*
         * we are reusing a connection that doesn't seem to be active anymore
         * (firewall state drop?), let's try a new connection.
         */
        ldc->reason = "ldap_compare_s() failed with timeout";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }

    ldc->last_backend_conn = r->request_time;
    ldc->reason = "Comparison complete";
    if ((LDAP_COMPARE_TRUE == result) ||
        (LDAP_COMPARE_FALSE == result) ||
        (LDAP_NO_SUCH_ATTRIBUTE == result)) {
        if (curl) {
            /* compare completed; caching result */
            ldap_cache_lock(st, r);
            the_compare_node.lastcompare = curtime;
            the_compare_node.result = result;
            the_compare_node.sgl_processed = 0;
            the_compare_node.subgroupList = NULL;

            /* If the node doesn't exist then insert it, otherwise just update
             * it with the last results
             */
            compare_nodep = util_ald_cache_fetch(curl->compare_cache,
                                                 &the_compare_node);
            if (   (compare_nodep == NULL)
                || (strcmp(the_compare_node.dn, compare_nodep->dn) != 0)
                || (strcmp(the_compare_node.attrib,compare_nodep->attrib) != 0)
                || (strcmp(the_compare_node.value, compare_nodep->value) != 0))
            {
                void *junk;

                junk = util_ald_cache_insert(curl->compare_cache,
                                             &the_compare_node);
                if (junk == NULL) {
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01287)
                                  "cache_compare: Cache insertion failure.");
                }
            }
            else {
                compare_nodep->lastcompare = curtime;
                compare_nodep->result = result;
            }
            ldap_cache_unlock(st, r);
        }

        if (LDAP_COMPARE_TRUE == result) {
            ldc->reason = "Comparison true (adding to cache)";
        }
        else if (LDAP_COMPARE_FALSE == result) {
            ldc->reason = "Comparison false (adding to cache)";
        }
        else if (LDAP_NO_SUCH_ATTRIBUTE == result) {
            ldc->reason = "Comparison no such attribute (adding to cache)";
        }
        else {
            ldc->reason = apr_psprintf(r->pool, 
                                       "Comparison undefined: (%d): %s (adding to cache)", 
                                        result, ldap_err2string(result));
        }
    }

    ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, 
                  "ldap_compare_s(%pp, %s, %s, %s) = %s", 
                  ldc->ldap, dn, attrib, value, ldap_err2string(result));
    return result;
}


static util_compare_subgroup_t* uldap_get_subgroups(request_rec *r,
                                                    util_ldap_connection_t *ldc,
                                                    const char *url,
                                                    const char *dn,
                                                    char **subgroupAttrs,
                                                    apr_array_header_t *subgroupclasses)
{
    int failures = 0;
    int result = LDAP_COMPARE_FALSE;
    util_compare_subgroup_t *res = NULL;
    LDAPMessage *sga_res, *entry;
    struct mod_auth_ldap_groupattr_entry_t *sgc_ents;
    apr_array_header_t *subgroups = apr_array_make(r->pool, 20, sizeof(char *));
    util_ldap_state_t *st = (util_ldap_state_t *)
                            ap_get_module_config(r->server->module_config,
                                                 &ldap_module);

    sgc_ents = (struct mod_auth_ldap_groupattr_entry_t *) subgroupclasses->elts;

    if (!subgroupAttrs) {
        return res;
    }

start_over:
    /*
     * 3.B. The cache didn't have any subgrouplist yet. Go check for subgroups.
     */
    if (failures > st->retries) {
        return res;
    }

    if (failures > 0 && st->retry_delay > 0) {
        apr_sleep(st->retry_delay);
    }


    if (LDAP_SUCCESS != (result = uldap_connection_open(r, ldc))) {
        /* connect failed */
        return res;
    }

    /* try to do the search */
    result = ldap_search_ext_s(ldc->ldap, (char *)dn, LDAP_SCOPE_BASE,
                               NULL, subgroupAttrs, 0,
                               NULL, NULL, NULL, APR_LDAP_SIZELIMIT, &sga_res);
    if (AP_LDAP_IS_SERVER_DOWN(result)) {
        ldc->reason = "ldap_search_ext_s() for subgroups failed with server"
                      " down";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }
    if (result == LDAP_TIMEOUT && failures == 0) {
        /*
         * we are reusing a connection that doesn't seem to be active anymore
         * (firewall state drop?), let's try a new connection.
         */
        ldc->reason = "ldap_search_ext_s() for subgroups failed with timeout";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }

    /* if there is an error (including LDAP_NO_SUCH_OBJECT) return now */
    if (result != LDAP_SUCCESS) {
        ldc->reason = "ldap_search_ext_s() for subgroups failed";
        return res;
    }

    ldc->last_backend_conn = r->request_time;
    entry = ldap_first_entry(ldc->ldap, sga_res);

    /*
     * Get values for the provided sub-group attributes.
     */
    if (subgroupAttrs) {
        int indx = 0, tmp_sgcIndex;

        while (subgroupAttrs[indx]) {
            char **values;
            int val_index = 0;

            /* Get *all* matching "member" values from this group. */
            values = ldap_get_values(ldc->ldap, entry, subgroupAttrs[indx]);

            if (values) {
                val_index = 0;
                /*
                 * Now we are going to pare the subgroup members of this group
                 * to *just* the subgroups, add them to the compare_nodep, and
                 * then proceed to check the new level of subgroups.
                 */
                while (values[val_index]) {
                    /* Check if this entry really is a group. */
                    tmp_sgcIndex = 0;
                    result = LDAP_COMPARE_FALSE;
                    while ((tmp_sgcIndex < subgroupclasses->nelts)
                           && (result != LDAP_COMPARE_TRUE)) {
                        result = uldap_cache_compare(r, ldc, url,
                                                     values[val_index],
                                                     "objectClass",
                                                     sgc_ents[tmp_sgcIndex].name
                                                     );

                        if (result != LDAP_COMPARE_TRUE) {
                            tmp_sgcIndex++;
                        }
                    }
                    /* It's a group, so add it to the array.  */
                    if (result == LDAP_COMPARE_TRUE) {
                        char **newgrp = (char **) apr_array_push(subgroups);
                        *newgrp = apr_pstrdup(r->pool, values[val_index]);
                    }
                    val_index++;
                }
                ldap_value_free(values);
            }
            indx++;
        }
    }

    ldap_msgfree(sga_res);

    if (subgroups->nelts > 0) {
        /* We need to fill in tmp_local_subgroups using the data from LDAP */
        int sgindex;
        char **group;
        res = apr_pcalloc(r->pool, sizeof(util_compare_subgroup_t));
        res->subgroupDNs  = apr_palloc(r->pool,
                                       sizeof(char *) * (subgroups->nelts));
        for (sgindex = 0; (group = apr_array_pop(subgroups)); sgindex++) {
            res->subgroupDNs[sgindex] = apr_pstrdup(r->pool, *group);
        }
        res->len = sgindex;
    }

    return res;
}


/*
 * Does a recursive lookup operation to try to find a user within (cached)
 * nested groups. It accepts a cache that it will use to lookup previous
 * compare attempts. We cache two kinds of compares (require group compares)
 * and (require user compares). Each compare has a different cache node:
 * require group includes the DN; require user does not because the require
 * user cache is owned by the
 *
 * DON'T CALL THIS UNLESS YOU CALLED uldap_cache_compare FIRST!!!!!
 *
 *
 * 1. Call uldap_cache_compare for each subgroupclass value to check the
 *    generic, user-agnostic, cached group entry. This will create a new generic
 *    cache entry if there
 *    wasn't one. If nothing returns LDAP_COMPARE_TRUE skip to step 5 since we
 *    have no groups.
 * 2. Lock The cache and get the generic cache entry.
 * 3. Check if there is already a subgrouplist in this generic group's cache
 *    entry.
 *    A. If there is, go to step 4.
 *    B. If there isn't:
 *       i)   Use ldap_search to get the full list
 *            of subgroup "members" (which may include non-group "members").
 *       ii)  Use uldap_cache_compare to strip the list down to just groups.
 *       iii) Lock and add this stripped down list to the cache of the generic
 *            group.
 * 4. Loop through the sgl and call uldap_cache_compare (using the user info)
 *    for each
 *    subgroup to see if the subgroup contains the user and to get the subgroups
 *    added to the
 *    cache (with user-afinity, if they aren't already there).
 *    A. If the user is in the subgroup, then we'll be returning
 *       LDAP_COMPARE_TRUE.
 *    B. if the user isn't in the subgroup (LDAP_COMPARE_FALSE via
 *       uldap_cache_compare) then recursively call this function to get the
 *       sub-subgroups added...
 * 5. Cleanup local allocations.
 * 6. Return the final result.
 */

static int uldap_cache_check_subgroups(request_rec *r,
                                       util_ldap_connection_t *ldc,
                                       const char *url, const char *dn,
                                       const char *attrib, const char *value,
                                       char **subgroupAttrs,
                                       apr_array_header_t *subgroupclasses,
                                       int cur_subgroup_depth,
                                       int max_subgroup_depth)
{
    int result = LDAP_COMPARE_FALSE;
    util_url_node_t *curl;
    util_url_node_t curnode;
    util_compare_node_t *compare_nodep;
    util_compare_node_t the_compare_node;
    util_compare_subgroup_t *tmp_local_sgl = NULL;
    int sgl_cached_empty = 0, sgindex = 0, base_sgcIndex = 0;
    struct mod_auth_ldap_groupattr_entry_t *sgc_ents =
            (struct mod_auth_ldap_groupattr_entry_t *) subgroupclasses->elts;
    util_ldap_state_t *st = (util_ldap_state_t *)
                            ap_get_module_config(r->server->module_config,
                                                 &ldap_module);

    /*
     * Stop looking at deeper levels of nested groups if we have reached the
     * max. Since we already checked the top-level group in uldap_cache_compare,
     * we don't need to check it again here - so if max_subgroup_depth is set
     * to 0, we won't check it (i.e. that is why we check < rather than <=).
     * We'll be calling uldap_cache_compare from here to check if the user is
     * in the next level before we recurse into that next level looking for
     * more subgroups.
     */
    if (cur_subgroup_depth >= max_subgroup_depth) {
        return LDAP_COMPARE_FALSE;
    }

    /*
     * 1. Check the "groupiness" of the specified basedn. Stopping at the first
     *    TRUE return.
     */
    while ((base_sgcIndex < subgroupclasses->nelts)
           && (result != LDAP_COMPARE_TRUE)) {
        result = uldap_cache_compare(r, ldc, url, dn, "objectClass",
                                     sgc_ents[base_sgcIndex].name);
        if (result != LDAP_COMPARE_TRUE) {
            base_sgcIndex++;
        }
    }

    if (result != LDAP_COMPARE_TRUE) {
        ldc->reason = "DN failed group verification.";
        return result;
    }

    /*
     * 2. Find previously created cache entry and check if there is already a
     *    subgrouplist.
     */
    ldap_cache_lock(st, r);
    curnode.url = url;
    curl = util_ald_cache_fetch(st->util_ldap_cache, &curnode);
    ldap_cache_unlock(st, r);

    if (curl && curl->compare_cache) {
        /* make a comparison to the cache */
        ldap_cache_lock(st, r);

        the_compare_node.dn = (char *)dn;
        the_compare_node.attrib = (char *)"objectClass";
        the_compare_node.value = (char *)sgc_ents[base_sgcIndex].name;
        the_compare_node.result = 0;
        the_compare_node.sgl_processed = 0;
        the_compare_node.subgroupList = NULL;

        compare_nodep = util_ald_cache_fetch(curl->compare_cache,
                                             &the_compare_node);

        if (compare_nodep != NULL) {
            /*
             * Found the generic group entry... but the user isn't in this
             * group or we wouldn't be here.
             */
            if (compare_nodep->sgl_processed) {
                if (compare_nodep->subgroupList) {
                    /* Make a local copy of the subgroup list */
                    int i;
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01288)
                                  "Making local copy of SGL for "
                                  "group (%s)(objectClass=%s) ",
                                  dn, (char *)sgc_ents[base_sgcIndex].name);
                    tmp_local_sgl = apr_pcalloc(r->pool,
                                                sizeof(util_compare_subgroup_t));
                    tmp_local_sgl->len = compare_nodep->subgroupList->len;
                    tmp_local_sgl->subgroupDNs =
                        apr_palloc(r->pool,
                                   sizeof(char *) * compare_nodep->subgroupList->len);
                    for (i = 0; i < compare_nodep->subgroupList->len; i++) {
                        tmp_local_sgl->subgroupDNs[i] =
                            apr_pstrdup(r->pool,
                                        compare_nodep->subgroupList->subgroupDNs[i]);
                    }
                }
                else {
                    sgl_cached_empty = 1;
                }
            }
        }
        ldap_cache_unlock(st, r);
    }

    if (!tmp_local_sgl && !sgl_cached_empty) {
        /* No Cached SGL, retrieve from LDAP */
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01289)
                      "no cached SGL for %s, retrieving from LDAP", dn);
        tmp_local_sgl = uldap_get_subgroups(r, ldc, url, dn, subgroupAttrs,
                                            subgroupclasses);
        if (!tmp_local_sgl) {
            /* No SGL aailable via LDAP either */
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01290) "no subgroups for %s",
                          dn);
        }

      if (curl && curl->compare_cache) {
        /*
         * Find the generic group cache entry and add the sgl we just retrieved.
         */
        ldap_cache_lock(st, r);

        the_compare_node.dn = (char *)dn;
        the_compare_node.attrib = (char *)"objectClass";
        the_compare_node.value = (char *)sgc_ents[base_sgcIndex].name;
        the_compare_node.result = 0;
        the_compare_node.sgl_processed = 0;
        the_compare_node.subgroupList = NULL;

        compare_nodep = util_ald_cache_fetch(curl->compare_cache,
                                             &the_compare_node);

        if (compare_nodep == NULL) {
            /*
             * The group entry we want to attach our SGL to doesn't exist.
             * We only got here if we verified this DN was actually a group
             * based on the objectClass, but we can't call the compare function
             * while we already hold the cache lock -- only the insert.
             */
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01291)
                          "Cache entry for %s doesn't exist", dn);
            the_compare_node.result = LDAP_COMPARE_TRUE;
            util_ald_cache_insert(curl->compare_cache, &the_compare_node);
            compare_nodep = util_ald_cache_fetch(curl->compare_cache,
                                                 &the_compare_node);
            if (compare_nodep == NULL) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01292)
                              "util_ldap: Couldn't retrieve group entry "
                              "for %s from cache",
                              dn);
            }
        }

        /*
         * We have a valid cache entry and a locally generated SGL.
         * Attach the SGL to the cache entry
         */
        if (compare_nodep && !compare_nodep->sgl_processed) {
            if (!tmp_local_sgl) {
                /* We looked up an SGL for a group and found it to be empty */
                if (compare_nodep->subgroupList == NULL) {
                    compare_nodep->sgl_processed = 1;
                }
            }
            else {
                util_compare_subgroup_t *sgl_copy =
                    util_ald_sgl_dup(curl->compare_cache, tmp_local_sgl);
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01293)
                             "Copying local SGL of len %d for group %s into cache",
                             tmp_local_sgl->len, dn);
                if (sgl_copy) {
                    if (compare_nodep->subgroupList) {
                        util_ald_sgl_free(curl->compare_cache,
                                          &(compare_nodep->subgroupList));
                    }
                    compare_nodep->subgroupList = sgl_copy;
                    compare_nodep->sgl_processed = 1;
                }
                else {
                    ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, APLOGNO(01294)
                                 "Copy of SGL failed to obtain shared memory, "
                                 "couldn't update cache");
                }
            }
        }
        ldap_cache_unlock(st, r);
      }
    }

    /*
     * tmp_local_sgl has either been created, or copied out of the cache
     * If tmp_local_sgl is NULL, there are no subgroups to process and we'll
     * return false
     */
    result = LDAP_COMPARE_FALSE;
    if (!tmp_local_sgl) {
        return result;
    }

    while ((result != LDAP_COMPARE_TRUE) && (sgindex < tmp_local_sgl->len)) {
        const char *group = NULL;
        group = tmp_local_sgl->subgroupDNs[sgindex];
        /*
         * 4. Now loop through the subgroupList and call uldap_cache_compare
         * to check for the user.
         */
        result = uldap_cache_compare(r, ldc, url, group, attrib, value);
        if (result == LDAP_COMPARE_TRUE) {
            /*
             * 4.A. We found the user in the subgroup. Return
             * LDAP_COMPARE_TRUE.
             */
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01295)
                          "Found user %s in a subgroup (%s) at level %d of %d.",
                          r->user, group, cur_subgroup_depth+1,
                          max_subgroup_depth);
        }
        else {
            /*
             * 4.B. We didn't find the user in this subgroup, so recurse into
             * it and keep looking.
             */
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01296)
                          "User %s not found in subgroup (%s) at level %d of "
                          "%d.", r->user, group, cur_subgroup_depth+1,
                          max_subgroup_depth);
            result = uldap_cache_check_subgroups(r, ldc, url, group, attrib,
                                                 value, subgroupAttrs,
                                                 subgroupclasses,
                                                 cur_subgroup_depth+1,
                                                 max_subgroup_depth);
        }
        sgindex++;
    }

    return result;
}


static int uldap_cache_checkuserid(request_rec *r, util_ldap_connection_t *ldc,
                                   const char *url, const char *basedn,
                                   int scope, char **attrs, const char *filter,
                                   const char *bindpw, const char **binddn,
                                   const char ***retvals)
{
    const char **vals = NULL;
    int numvals = 0;
    int result = 0;
    LDAPMessage *res, *entry;
    char *dn;
    int count;
    int failures = 0;
    util_url_node_t *curl;              /* Cached URL node */
    util_url_node_t curnode;
    util_search_node_t *search_nodep;   /* Cached search node */
    util_search_node_t the_search_node;
    apr_time_t curtime;

    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(r->server->module_config,
        &ldap_module);

    /* Get the cache node for this url */
    ldap_cache_lock(st, r);
    curnode.url = url;
    curl = (util_url_node_t *)util_ald_cache_fetch(st->util_ldap_cache,
                                                   &curnode);
    if (curl == NULL) {
        curl = util_ald_create_caches(st, url);
    }
    ldap_cache_unlock(st, r);

    if (curl) {
        ldap_cache_lock(st, r);
        the_search_node.username = filter;
        search_nodep = util_ald_cache_fetch(curl->search_cache,
                                            &the_search_node);
        if (search_nodep != NULL) {

            /* found entry in search cache... */
            curtime = apr_time_now();

            /*
             * Remove this item from the cache if its expired. If the sent
             * password doesn't match the storepassword, the entry will
             * be removed and readded later if the credentials pass
             * authentication.
             */
            if ((curtime - search_nodep->lastbind) > st->search_cache_ttl) {
                /* ...but entry is too old */
                util_ald_cache_remove(curl->search_cache, search_nodep);
            }
            else if (   (search_nodep->bindpw)
                     && (search_nodep->bindpw[0] != '\0')
                     && (strcmp(search_nodep->bindpw, bindpw) == 0))
            {
                /* ...and entry is valid */
                *binddn = apr_pstrdup(r->pool, search_nodep->dn);
                if (attrs) {
                    int i;
                    *retvals = apr_palloc(r->pool, sizeof(char *) * search_nodep->numvals);
                    for (i = 0; i < search_nodep->numvals; i++) {
                        (*retvals)[i] = apr_pstrdup(r->pool, search_nodep->vals[i]);
                    }
                }
                ldap_cache_unlock(st, r);
                ldc->reason = "Authentication successful (cached)";
                return LDAP_SUCCESS;
            }
        }
        /* unlock this read lock */
        ldap_cache_unlock(st, r);
    }

    /*
     * At this point, there is no valid cached search, so lets do the search.
     */

    /*
     * If LDAP operation fails due to LDAP_SERVER_DOWN, control returns here.
     */
start_over:
    if (failures > st->retries) {
        return result;
    }

    if (failures > 0 && st->retry_delay > 0) {
        apr_sleep(st->retry_delay);
    }

    if (LDAP_SUCCESS != (result = uldap_connection_open(r, ldc))) {
        return result;
    }

    /* try do the search */
    result = ldap_search_ext_s(ldc->ldap,
                               (char *)basedn, scope,
                               (char *)filter, attrs, 0,
                               NULL, NULL, st->opTimeout, APR_LDAP_SIZELIMIT, &res);
    if (AP_LDAP_IS_SERVER_DOWN(result))
    {
        ldc->reason = "ldap_search_ext_s() for user failed with server down";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }

    if (result == LDAP_TIMEOUT) {
        ldc->reason = "ldap_search_ext_s() for user failed with timeout";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }


    /* if there is an error (including LDAP_NO_SUCH_OBJECT) return now */
    if (result != LDAP_SUCCESS) {
        ldc->reason = "ldap_search_ext_s() for user failed";
        return result;
    }

    /*
     * We should have found exactly one entry; to find a different
     * number is an error.
     */
    ldc->last_backend_conn = r->request_time;
    count = ldap_count_entries(ldc->ldap, res);
    if (count != 1)
    {
        if (count == 0 )
            ldc->reason = "User not found";
        else
            ldc->reason = "User is not unique (search found two "
                          "or more matches)";
        ldap_msgfree(res);
        return LDAP_NO_SUCH_OBJECT;
    }

    entry = ldap_first_entry(ldc->ldap, res);

    /* Grab the dn, copy it into the pool, and free it again */
    dn = ldap_get_dn(ldc->ldap, entry);
    *binddn = apr_pstrdup(r->pool, dn);
    ldap_memfree(dn);

    /*
     * A bind to the server with an empty password always succeeds, so
     * we check to ensure that the password is not empty. This implies
     * that users who actually do have empty passwords will never be
     * able to authenticate with this module. I don't see this as a big
     * problem.
     */
    if (!bindpw || strlen(bindpw) <= 0) {
        ldap_msgfree(res);
        ldc->reason = "Empty password not allowed";
        return LDAP_INVALID_CREDENTIALS;
    }

    /*
     * Attempt to bind with the retrieved dn and the password. If the bind
     * fails, it means that the password is wrong (the dn obviously
     * exists, since we just retrieved it)
     */
    result = uldap_simple_bind(ldc, (char *)*binddn, (char *)bindpw,
                               st->opTimeout);
    if (AP_LDAP_IS_SERVER_DOWN(result) ||
        (result == LDAP_TIMEOUT && failures == 0)) {
        if (AP_LDAP_IS_SERVER_DOWN(result))
            ldc->reason = "ldap_simple_bind() to check user credentials "
                          "failed with server down";
        else
            ldc->reason = "ldap_simple_bind() to check user credentials "
                          "timed out";
        ldap_msgfree(res);
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }

    /* failure? if so - return */
    if (result != LDAP_SUCCESS) {
        ldc->reason = "ldap_simple_bind() to check user credentials failed";
        ldap_msgfree(res);
        uldap_connection_unbind(ldc);
        return result;
    }
    else {
        /*
         * We have just bound the connection to a different user and password
         * combination, which might be reused unintentionally next time this
         * connection is used from the connection pool.
         */
        ldc->must_rebind = 1;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "LDC %pp used for authn, must be rebound", ldc);
    }

    /*
     * Get values for the provided attributes.
     */
    if (attrs) {
        int k = 0;
        int i = 0;
        while (attrs[k++]);
        vals = apr_pcalloc(r->pool, sizeof(char *) * (k+1));
        numvals = k;
        while (attrs[i]) {
            char **values;
            int j = 0;
            char *str = NULL;
            /* get values */
            values = ldap_get_values(ldc->ldap, entry, attrs[i]);
            while (values && values[j]) {
                str = str ? apr_pstrcat(r->pool, str, "; ", values[j], NULL)
                          : apr_pstrdup(r->pool, values[j]);
                j++;
            }
            ldap_value_free(values);
            vals[i] = str;
            i++;
        }
        *retvals = vals;
    }

    /*
     * Add the new username to the search cache.
     */
    if (curl) {
        ldap_cache_lock(st, r);
        the_search_node.username = filter;
        the_search_node.dn = *binddn;
        the_search_node.bindpw = bindpw;
        the_search_node.lastbind = apr_time_now();
        the_search_node.vals = vals;
        the_search_node.numvals = numvals;

        /* Search again to make sure that another thread didn't ready insert
         * this node into the cache before we got here. If it does exist then
         * update the lastbind
         */
        search_nodep = util_ald_cache_fetch(curl->search_cache,
                                            &the_search_node);
        if ((search_nodep == NULL) ||
            (strcmp(*binddn, search_nodep->dn) != 0)) {

            /* Nothing in cache, insert new entry */
            util_ald_cache_insert(curl->search_cache, &the_search_node);
        }
        else if ((!search_nodep->bindpw) ||
            (strcmp(bindpw, search_nodep->bindpw) != 0)) {

            /* Entry in cache is invalid, remove it and insert new one */
            util_ald_cache_remove(curl->search_cache, search_nodep);
            util_ald_cache_insert(curl->search_cache, &the_search_node);
        }
        else {
            /* Cache entry is valid, update lastbind */
            search_nodep->lastbind = the_search_node.lastbind;
        }
        ldap_cache_unlock(st, r);
    }
    ldap_msgfree(res);

    ldc->reason = "Authentication successful";
    return LDAP_SUCCESS;
}

/*
 * This function will return the DN of the entry matching userid.
 * It is used to get the DN in case some other module than mod_auth_ldap
 * has authenticated the user.
 * The function is basically a copy of uldap_cache_checkuserid
 * with password checking removed.
 */
static int uldap_cache_getuserdn(request_rec *r, util_ldap_connection_t *ldc,
                                 const char *url, const char *basedn,
                                 int scope, char **attrs, const char *filter,
                                 const char **binddn, const char ***retvals)
{
    const char **vals = NULL;
    int numvals = 0;
    int result = 0;
    LDAPMessage *res, *entry;
    char *dn;
    int count;
    int failures = 0;
    util_url_node_t *curl;              /* Cached URL node */
    util_url_node_t curnode;
    util_search_node_t *search_nodep;   /* Cached search node */
    util_search_node_t the_search_node;
    apr_time_t curtime;

    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(r->server->module_config,
        &ldap_module);

    /* Get the cache node for this url */
    ldap_cache_lock(st, r);
    curnode.url = url;
    curl = (util_url_node_t *)util_ald_cache_fetch(st->util_ldap_cache,
                                                   &curnode);
    if (curl == NULL) {
        curl = util_ald_create_caches(st, url);
    }
    ldap_cache_unlock(st, r);

    if (curl) {
        ldap_cache_lock(st, r);
        the_search_node.username = filter;
        search_nodep = util_ald_cache_fetch(curl->search_cache,
                                            &the_search_node);
        if (search_nodep != NULL) {

            /* found entry in search cache... */
            curtime = apr_time_now();

            /*
             * Remove this item from the cache if its expired.
             */
            if ((curtime - search_nodep->lastbind) > st->search_cache_ttl) {
                /* ...but entry is too old */
                util_ald_cache_remove(curl->search_cache, search_nodep);
            }
            else {
                /* ...and entry is valid */
                *binddn = apr_pstrdup(r->pool, search_nodep->dn);
                if (attrs) {
                    int i;
                    *retvals = apr_palloc(r->pool, sizeof(char *) * search_nodep->numvals);
                    for (i = 0; i < search_nodep->numvals; i++) {
                        (*retvals)[i] = apr_pstrdup(r->pool, search_nodep->vals[i]);
                    }
                }
                ldap_cache_unlock(st, r);
                ldc->reason = "Search successful (cached)";
                return LDAP_SUCCESS;
            }
        }
        /* unlock this read lock */
        ldap_cache_unlock(st, r);
    }

    /*
     * At this point, there is no valid cached search, so lets do the search.
     */

    /*
     * If LDAP operation fails due to LDAP_SERVER_DOWN, control returns here.
     */
start_over:
    if (failures > st->retries) {
        return result;
    }

    if (failures > 0 && st->retry_delay > 0) {
        apr_sleep(st->retry_delay);
    }

    if (LDAP_SUCCESS != (result = uldap_connection_open(r, ldc))) {
        return result;
    }

    /* try do the search */
    result = ldap_search_ext_s(ldc->ldap,
                               (char *)basedn, scope,
                               (char *)filter, attrs, 0,
                               NULL, NULL, st->opTimeout, APR_LDAP_SIZELIMIT, &res);
    if (AP_LDAP_IS_SERVER_DOWN(result))
    {
        ldc->reason = "ldap_search_ext_s() for user failed with server down";
        uldap_connection_unbind(ldc);
        failures++;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE5, 0, r, "%s (attempt %d)", ldc->reason, failures);
        goto start_over;
    }

    /* if there is an error (including LDAP_NO_SUCH_OBJECT) return now */
    if (result != LDAP_SUCCESS) {
        ldc->reason = "ldap_search_ext_s() for user failed";
        return result;
    }

    /*
     * We should have found exactly one entry; to find a different
     * number is an error.
     */
    ldc->last_backend_conn = r->request_time;
    count = ldap_count_entries(ldc->ldap, res);
    if (count != 1)
    {
        if (count == 0 )
            ldc->reason = "User not found";
        else
            ldc->reason = "User is not unique (search found two "
                          "or more matches)";
        ldap_msgfree(res);
        return LDAP_NO_SUCH_OBJECT;
    }

    entry = ldap_first_entry(ldc->ldap, res);

    /* Grab the dn, copy it into the pool, and free it again */
    dn = ldap_get_dn(ldc->ldap, entry);
    *binddn = apr_pstrdup(r->pool, dn);
    ldap_memfree(dn);

    /*
     * Get values for the provided attributes.
     */
    if (attrs) {
        int k = 0;
        int i = 0;
        while (attrs[k++]);
        vals = apr_pcalloc(r->pool, sizeof(char *) * (k+1));
        numvals = k;
        while (attrs[i]) {
            char **values;
            int j = 0;
            char *str = NULL;
            /* get values */
            values = ldap_get_values(ldc->ldap, entry, attrs[i]);
            while (values && values[j]) {
                str = str ? apr_pstrcat(r->pool, str, "; ", values[j], NULL)
                          : apr_pstrdup(r->pool, values[j]);
                j++;
            }
            ldap_value_free(values);
            vals[i] = str;
            i++;
        }
        *retvals = vals;
    }

    /*
     * Add the new username to the search cache.
     */
    if (curl) {
        ldap_cache_lock(st, r);
        the_search_node.username = filter;
        the_search_node.dn = *binddn;
        the_search_node.bindpw = NULL;
        the_search_node.lastbind = apr_time_now();
        the_search_node.vals = vals;
        the_search_node.numvals = numvals;

        /* Search again to make sure that another thread didn't ready insert
         * this node into the cache before we got here. If it does exist then
         * update the lastbind
         */
        search_nodep = util_ald_cache_fetch(curl->search_cache,
                                            &the_search_node);
        if ((search_nodep == NULL) ||
            (strcmp(*binddn, search_nodep->dn) != 0)) {

            /* Nothing in cache, insert new entry */
            util_ald_cache_insert(curl->search_cache, &the_search_node);
        }
        /*
         * Don't update lastbind on entries with bindpw because
         * we haven't verified that password. It's OK to update
         * the entry if there is no password in it.
         */
        else if (!search_nodep->bindpw) {
            /* Cache entry is valid, update lastbind */
            search_nodep->lastbind = the_search_node.lastbind;
        }
        ldap_cache_unlock(st, r);
    }

    ldap_msgfree(res);

    ldc->reason = "Search successful";
    return LDAP_SUCCESS;
}

/*
 * Reports if ssl support is enabled
 *
 * 1 = enabled, 0 = not enabled
 */
static int uldap_ssl_supported(request_rec *r)
{
   util_ldap_state_t *st = (util_ldap_state_t *)ap_get_module_config(
                                r->server->module_config, &ldap_module);

   return(st->ssl_supported);
}


/* ---------------------------------------- */
/* config directives */


static const char *util_ldap_set_cache_bytes(cmd_parms *cmd, void *dummy,
                                             const char *bytes)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->cache_bytes = atol(bytes);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01297)
                 "ldap cache: Setting shared memory cache size to "
                 "%" APR_SIZE_T_FMT " bytes.",
                 st->cache_bytes);

    return NULL;
}

static const char *util_ldap_set_cache_file(cmd_parms *cmd, void *dummy,
                                            const char *file)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    if (file) {
        st->cache_file = ap_server_root_relative(st->pool, file);
    }
    else {
        st->cache_file = NULL;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01298)
                 "LDAP cache: Setting shared memory cache file to %s.",
                 st->cache_file);

    return NULL;
}

static const char *util_ldap_set_cache_ttl(cmd_parms *cmd, void *dummy,
                                           const char *ttl)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->search_cache_ttl = atol(ttl) * 1000000;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01299)
                 "ldap cache: Setting cache TTL to %ld microseconds.",
                 st->search_cache_ttl);

    return NULL;
}

static const char *util_ldap_set_cache_entries(cmd_parms *cmd, void *dummy,
                                               const char *size)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->search_cache_size = atol(size);
    if (st->search_cache_size < 0) {
        st->search_cache_size = 0;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01300)
                 "ldap cache: Setting search cache size to %ld entries.",
                 st->search_cache_size);

    return NULL;
}

static const char *util_ldap_set_opcache_ttl(cmd_parms *cmd, void *dummy,
                                             const char *ttl)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->compare_cache_ttl = atol(ttl) * APR_USEC_PER_SEC;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01301)
                 "ldap cache: Setting operation cache TTL to %ld microseconds.",
                 st->compare_cache_ttl);

    return NULL;
}

static const char *util_ldap_set_opcache_entries(cmd_parms *cmd, void *dummy,
                                                 const char *size)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->compare_cache_size = atol(size);
    if (st->compare_cache_size < 0) {
        st->compare_cache_size = 0;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01302)
                 "ldap cache: Setting operation cache size to %ld entries.",
                 st->compare_cache_size);

    return NULL;
}


/**
 * Parse the certificate type.
 *
 * The type can be one of the following:
 * CA_DER, CA_BASE64, CA_CERT7_DB, CA_SECMOD, CERT_DER, CERT_BASE64,
 * CERT_KEY3_DB, CERT_NICKNAME, KEY_DER, KEY_BASE64
 *
 * If no matches are found, APR_LDAP_CA_TYPE_UNKNOWN is returned.
 */
static int util_ldap_parse_cert_type(const char *type)
{
    /* Authority file in binary DER format */
    if (0 == strcasecmp("CA_DER", type)) {
        return APR_LDAP_CA_TYPE_DER;
    }

    /* Authority file in Base64 format */
    else if (0 == strcasecmp("CA_BASE64", type)) {
        return APR_LDAP_CA_TYPE_BASE64;
    }

    /* Netscape certificate database file/directory */
    else if (0 == strcasecmp("CA_CERT7_DB", type)) {
        return APR_LDAP_CA_TYPE_CERT7_DB;
    }

    /* Netscape secmod file/directory */
    else if (0 == strcasecmp("CA_SECMOD", type)) {
        return APR_LDAP_CA_TYPE_SECMOD;
    }

    /* Client cert file in DER format */
    else if (0 == strcasecmp("CERT_DER", type)) {
        return APR_LDAP_CERT_TYPE_DER;
    }

    /* Client cert file in Base64 format */
    else if (0 == strcasecmp("CERT_BASE64", type)) {
        return APR_LDAP_CERT_TYPE_BASE64;
    }

    /* Client cert file in PKCS#12 format */
    else if (0 == strcasecmp("CERT_PFX", type)) {
        return APR_LDAP_CERT_TYPE_PFX;
    }

    /* Netscape client cert database file/directory */
    else if (0 == strcasecmp("CERT_KEY3_DB", type)) {
        return APR_LDAP_CERT_TYPE_KEY3_DB;
    }

    /* Netscape client cert nickname */
    else if (0 == strcasecmp("CERT_NICKNAME", type)) {
        return APR_LDAP_CERT_TYPE_NICKNAME;
    }

    /* Client cert key file in DER format */
    else if (0 == strcasecmp("KEY_DER", type)) {
        return APR_LDAP_KEY_TYPE_DER;
    }

    /* Client cert key file in Base64 format */
    else if (0 == strcasecmp("KEY_BASE64", type)) {
        return APR_LDAP_KEY_TYPE_BASE64;
    }

    /* Client cert key file in PKCS#12 format */
    else if (0 == strcasecmp("KEY_PFX", type)) {
        return APR_LDAP_KEY_TYPE_PFX;
    }

    else {
        return APR_LDAP_CA_TYPE_UNKNOWN;
    }

}


/**
 * Set LDAPTrustedGlobalCert.
 *
 * This directive takes either two or three arguments:
 * - certificate type
 * - certificate file / directory / nickname
 * - certificate password (optional)
 *
 * This directive may only be used globally.
 */
static const char *util_ldap_set_trusted_global_cert(cmd_parms *cmd,
                                                     void *dummy,
                                                     const char *type,
                                                     const char *file,
                                                     const char *password)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    apr_finfo_t finfo;
    apr_status_t rv;
    int cert_type = 0;
    apr_ldap_opt_tls_cert_t *cert;

    if (err != NULL) {
        return err;
    }

    /* handle the certificate type */
    if (type) {
        cert_type = util_ldap_parse_cert_type(type);
        if (APR_LDAP_CA_TYPE_UNKNOWN == cert_type) {
           return apr_psprintf(cmd->pool, "The certificate type %s is "
                                          "not recognised. It should be one "
                                          "of CA_DER, CA_BASE64, CA_CERT7_DB, "
                                          "CA_SECMOD, CERT_DER, CERT_BASE64, "
                                          "CERT_KEY3_DB, CERT_NICKNAME, "
                                          "KEY_DER, KEY_BASE64", type);
        }
    }
    else {
        return "Certificate type was not specified.";
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01303)
                      "LDAP: SSL trusted global cert - %s (type %s)",
                       file, type);

    /* add the certificate to the global array */
    cert = (apr_ldap_opt_tls_cert_t *)apr_array_push(st->global_certs);
    cert->type = cert_type;
    cert->path = file;
    cert->password = password;

    /* if file is a file or path, fix the path */
    if (cert_type != APR_LDAP_CA_TYPE_UNKNOWN &&
        cert_type != APR_LDAP_CERT_TYPE_NICKNAME) {

        cert->path = ap_server_root_relative(cmd->pool, file);
        if (cert->path &&
            ((rv = apr_stat (&finfo, cert->path, APR_FINFO_MIN, cmd->pool))
                != APR_SUCCESS))
        {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, cmd->server, APLOGNO(01304)
                         "LDAP: Could not open SSL trusted certificate "
                         "authority file - %s",
                         cert->path == NULL ? file : cert->path);
            return "Invalid global certificate file path";
        }
    }

    return(NULL);
}


/**
 * Set LDAPTrustedClientCert.
 *
 * This directive takes either two or three arguments:
 * - certificate type
 * - certificate file / directory / nickname
 * - certificate password (optional)
 */
static const char *util_ldap_set_trusted_client_cert(cmd_parms *cmd,
                                                     void *config,
                                                     const char *type,
                                                     const char *file,
                                                     const char *password)
{
    util_ldap_config_t *dc =  config;
    apr_finfo_t finfo;
    apr_status_t rv;
    int cert_type = 0;
    apr_ldap_opt_tls_cert_t *cert;

    /* handle the certificate type */
    if (type) {
        cert_type = util_ldap_parse_cert_type(type);
        if (APR_LDAP_CA_TYPE_UNKNOWN == cert_type) {
            return apr_psprintf(cmd->pool, "The certificate type \"%s\" is "
                                           "not recognised. It should be one "
                                           "of CA_DER, CA_BASE64, "
                                           "CERT_DER, CERT_BASE64, "
                                           "CERT_NICKNAME, CERT_PFX, "
                                           "KEY_DER, KEY_BASE64, KEY_PFX",
                                           type);
        }
        else if ( APR_LDAP_CA_TYPE_CERT7_DB == cert_type ||
                 APR_LDAP_CA_TYPE_SECMOD == cert_type ||
                 APR_LDAP_CERT_TYPE_PFX == cert_type ||
                 APR_LDAP_CERT_TYPE_KEY3_DB == cert_type) {
            return apr_psprintf(cmd->pool, "The certificate type \"%s\" is "
                                           "only valid within a "
                                           "LDAPTrustedGlobalCert directive. "
                                           "Only CA_DER, CA_BASE64, "
                                           "CERT_DER, CERT_BASE64, "
                                           "CERT_NICKNAME, KEY_DER, and "
                                           "KEY_BASE64 may be used.", type);
        }
    }
    else {
        return "Certificate type was not specified.";
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01305)
                      "LDAP: SSL trusted client cert - %s (type %s)",
                       file, type);

    /* add the certificate to the client array */
    cert = (apr_ldap_opt_tls_cert_t *)apr_array_push(dc->client_certs);
    cert->type = cert_type;
    cert->path = file;
    cert->password = password;

    /* if file is a file or path, fix the path */
    if (cert_type != APR_LDAP_CA_TYPE_UNKNOWN &&
        cert_type != APR_LDAP_CERT_TYPE_NICKNAME) {

        cert->path = ap_server_root_relative(cmd->pool, file);
        if (cert->path &&
            ((rv = apr_stat (&finfo, cert->path, APR_FINFO_MIN, cmd->pool))
                != APR_SUCCESS))
        {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, cmd->server, APLOGNO(01306)
                         "LDAP: Could not open SSL client certificate "
                         "file - %s",
                         cert->path == NULL ? file : cert->path);
            return "Invalid client certificate file path";
        }

    }

    return(NULL);
}


/**
 * Set LDAPTrustedMode.
 *
 * This directive sets what encryption mode to use on a connection:
 * - None (No encryption)
 * - SSL (SSL encryption)
 * - STARTTLS (TLS encryption)
 */
static const char *util_ldap_set_trusted_mode(cmd_parms *cmd, void *dummy,
                                              const char *mode)
{
    util_ldap_state_t *st =
    (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                              &ldap_module);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01307)
                      "LDAP: SSL trusted mode - %s",
                       mode);

    if (0 == strcasecmp("NONE", mode)) {
        st->secure = APR_LDAP_NONE;
    }
    else if (0 == strcasecmp("SSL", mode)) {
        st->secure = APR_LDAP_SSL;
    }
    else if (   (0 == strcasecmp("TLS", mode))
             || (0 == strcasecmp("STARTTLS", mode))) {
        st->secure = APR_LDAP_STARTTLS;
    }
    else {
        return "Invalid LDAPTrustedMode setting: must be one of NONE, "
               "SSL, or TLS/STARTTLS";
    }

    st->secure_set = 1;
    return(NULL);
}

static const char *util_ldap_set_verify_srv_cert(cmd_parms *cmd,
                                                 void *dummy,
                                                 int mode)
{
    util_ldap_state_t *st =
    (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                              &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01308)
                      "LDAP: SSL verify server certificate - %s",
                      mode?"TRUE":"FALSE");

    st->verify_svr_cert = mode;

    return(NULL);
}


static const char *util_ldap_set_connection_timeout(cmd_parms *cmd,
                                                    void *dummy,
                                                    const char *ttl)
{
#ifdef LDAP_OPT_NETWORK_TIMEOUT
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
#endif
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

#ifdef LDAP_OPT_NETWORK_TIMEOUT
    st->connectionTimeout = atol(ttl);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01309)
                 "ldap connection: Setting connection timeout to %ld seconds.",
                 st->connectionTimeout);
#else
    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, cmd->server, APLOGNO(01310)
                 "LDAP: Connection timeout option not supported by the "
                 "LDAP SDK in use." );
#endif

    return NULL;
}


static const char *util_ldap_set_chase_referrals(cmd_parms *cmd,
                                                 void *config,
                                                 const char *arg)
{
    util_ldap_config_t *dc =  config;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01311)
                      "LDAP: Setting referral chasing %s", arg);

    if (0 == strcasecmp(arg, "on")) {
        dc->ChaseReferrals = AP_LDAP_CHASEREFERRALS_ON;
    }
    else if (0 == strcasecmp(arg, "off")) {
        dc->ChaseReferrals = AP_LDAP_CHASEREFERRALS_OFF;
    }
    else if (0 == strcasecmp(arg, "default")) {
        dc->ChaseReferrals = AP_LDAP_CHASEREFERRALS_SDKDEFAULT;
    }
    else {
        return "LDAPReferrals must be 'on', 'off', or 'default'";
    }

    return(NULL);
}

static const char *util_ldap_set_debug_level(cmd_parms *cmd,
                                             void *config,
                                             const char *arg) {
#ifdef AP_LDAP_OPT_DEBUG
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
#endif

    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

#ifndef AP_LDAP_OPT_DEBUG
    return "This directive is not supported with the currently linked LDAP library";
#else
    st->debug_level = atoi(arg);
    return NULL;
#endif
}

static const char *util_ldap_set_referral_hop_limit(cmd_parms *cmd,
                                                    void *config,
                                                    const char *hop_limit)
{
    util_ldap_config_t *dc =  config;

    dc->ReferralHopLimit = atol(hop_limit);

    if (dc->ReferralHopLimit <= 0) {
        return "LDAPReferralHopLimit must be greater than zero (Use 'LDAPReferrals Off' to disable referral chasing)";
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01312)
                 "LDAP: Limit chased referrals to maximum of %d hops.",
                 dc->ReferralHopLimit);

    return NULL;
}

static void *util_ldap_create_dir_config(apr_pool_t *p, char *d)
{
    util_ldap_config_t *dc =
        (util_ldap_config_t *) apr_pcalloc(p,sizeof(util_ldap_config_t));

    /* defaults are AP_LDAP_CHASEREFERRALS_ON and AP_LDAP_DEFAULT_HOPLIMIT */
    dc->client_certs = apr_array_make(p, 10, sizeof(apr_ldap_opt_tls_cert_t));
    dc->ChaseReferrals = AP_LDAP_CHASEREFERRALS_ON;
    dc->ReferralHopLimit = AP_LDAP_HOPLIMIT_UNSET;

    return dc;
}

static const char *util_ldap_set_op_timeout(cmd_parms *cmd,
                                            void *dummy,
                                            const char *val)
{
    long timeout;
    char *endptr;
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    timeout = strtol(val, &endptr, 10);
    if ((val == endptr) || (*endptr != '\0')) {
        return "Timeout not numerical";
    }
    if (timeout < 0) {
        return "Timeout must be non-negative";
    }

    if (timeout) {
        if (!st->opTimeout) {
            st->opTimeout = apr_pcalloc(cmd->pool, sizeof(struct timeval));
        }
        st->opTimeout->tv_sec = timeout;
    }
    else {
        st->opTimeout = NULL;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01313)
                 "ldap connection: Setting op timeout to %ld seconds.",
                 timeout);

#ifndef LDAP_OPT_TIMEOUT

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, cmd->server, APLOGNO(01314)
                 "LDAP: LDAP_OPT_TIMEOUT option not supported by the "
                 "LDAP library in use. Using LDAPTimeout value as search "
                 "timeout only." );
#endif

    return NULL;
}

static const char *util_ldap_set_conn_ttl(cmd_parms *cmd,
                                          void *dummy,
                                          const char *val)
{
    apr_interval_time_t timeout = -1;
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);

    /* Negative values mean AP_LDAP_CONNPOOL_INFINITE */
    if (val[0] != '-' &&
        ap_timeout_parameter_parse(val, &timeout, "s") != APR_SUCCESS) {
        return "LDAPConnectionPoolTTL has wrong format";
    }

    if (timeout < 0) {
        /* reserve -1 for default value */
        timeout = AP_LDAP_CONNPOOL_INFINITE;
    }
    st->connection_pool_ttl = timeout;
    return NULL;
}

static const char *util_ldap_set_retry_delay(cmd_parms *cmd,
                                             void *dummy,
                                             const char *val)
{
    apr_interval_time_t timeout;
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    if (ap_timeout_parameter_parse(val, &timeout, "s") != APR_SUCCESS) {
        return "LDAPRetryDelay has wrong format";
    }

    if (timeout < 0) {
        return "LDAPRetryDelay must be >= 0";
    }

    st->retry_delay = timeout;
    return NULL;
}

static const char *util_ldap_set_retries(cmd_parms *cmd,
                                         void *dummy,
                                         const char *val)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)ap_get_module_config(cmd->server->module_config,
                                                  &ldap_module);
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }

    st->retries = atoi(val);
    if (st->retries < 0) {
        return  "LDAPRetries must be >= 0";
    }

    return NULL;
}

static void *util_ldap_create_config(apr_pool_t *p, server_rec *s)
{
    util_ldap_state_t *st =
        (util_ldap_state_t *)apr_pcalloc(p, sizeof(util_ldap_state_t));

    /* Create a per vhost pool for mod_ldap to use, serialized with
     * st->mutex (also one per vhost).  both are replicated by fork(),
     * no shared memory managed by either.
     */
    apr_pool_create(&st->pool, p);
    apr_pool_tag(st->pool, "util_ldap_state");
#if APR_HAS_THREADS
    apr_thread_mutex_create(&st->mutex, APR_THREAD_MUTEX_DEFAULT, st->pool);
#endif

    st->cache_bytes = 500000;
    st->search_cache_ttl = 600 * APR_USEC_PER_SEC; /* 10 minutes */
    st->search_cache_size = 1024;
    st->compare_cache_ttl = 600 * APR_USEC_PER_SEC; /* 10 minutes */
    st->compare_cache_size = 1024;
    st->connections = NULL;
    st->ssl_supported = 0;
    st->global_certs = apr_array_make(p, 10, sizeof(apr_ldap_opt_tls_cert_t));
    st->secure = APR_LDAP_NONE;
    st->secure_set = 0;
    st->connectionTimeout = 10;
    st->opTimeout = apr_pcalloc(p, sizeof(struct timeval));
    st->opTimeout->tv_sec = 60;
    st->verify_svr_cert = 1;
    st->connection_pool_ttl = AP_LDAP_CONNPOOL_DEFAULT; /* no limit */
    st->retries = 3;
    st->retry_delay = 0; /* no delay */

    return st;
}

/* cache-related settings are not merged here, but in the post_config hook,
 * since the cache has not yet sprung to life
 */
static void *util_ldap_merge_config(apr_pool_t *p, void *basev,
                                    void *overridesv)
{
    util_ldap_state_t *st = apr_pcalloc(p, sizeof(util_ldap_state_t));
    util_ldap_state_t *base = (util_ldap_state_t *) basev;
    util_ldap_state_t *overrides = (util_ldap_state_t *) overridesv;

    st->pool = overrides->pool;
#if APR_HAS_THREADS
    st->mutex = overrides->mutex;
#endif

    /* The cache settings can not be modified in a
        virtual host since all server use the same
        shared memory cache. */
    st->cache_bytes = base->cache_bytes;
    st->search_cache_ttl = base->search_cache_ttl;
    st->search_cache_size = base->search_cache_size;
    st->compare_cache_ttl = base->compare_cache_ttl;
    st->compare_cache_size = base->compare_cache_size;

    st->connections = NULL;
    st->ssl_supported = 0; /* not known until post-config and re-merged */
    st->global_certs = apr_array_append(p, base->global_certs,
                                           overrides->global_certs);
    st->secure = (overrides->secure_set == 0) ? base->secure
                                              : overrides->secure;

    /* These LDAP connection settings can not be overwritten in
        a virtual host. Once set in the base server, they must
        remain the same. None of the LDAP SDKs seem to be able
        to handle setting the verify_svr_cert flag on a
        per-connection basis.  The OpenLDAP client appears to be
        able to handle the connection timeout per-connection
        but the Novell SDK cannot.  Allowing the timeout to
        be set by each vhost is of little value so rather than
        trying to make special exceptions for one LDAP SDK, GLOBAL_ONLY
        is being enforced on this setting as well. */
    st->connectionTimeout = base->connectionTimeout;
    st->opTimeout = base->opTimeout;
    st->verify_svr_cert = base->verify_svr_cert;
    st->debug_level = base->debug_level;

    st->connection_pool_ttl = (overrides->connection_pool_ttl == AP_LDAP_CONNPOOL_DEFAULT) ?
                                base->connection_pool_ttl : overrides->connection_pool_ttl;

    st->retries = base->retries;
    st->retry_delay = base->retry_delay;

    return st;
}

static apr_status_t util_ldap_cleanup_module(void *data)
{
    server_rec *s = data;
    util_ldap_state_t *st = (util_ldap_state_t *)ap_get_module_config(
        s->module_config, &ldap_module);

    if (st->ssl_supported) {
        apr_ldap_ssl_deinit();
    }

    return APR_SUCCESS;
}

static int util_ldap_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
                                apr_pool_t *ptemp)
{
    apr_status_t result;

    result = ap_mutex_register(pconf, ldap_cache_mutex_type, NULL,
                               APR_LOCK_DEFAULT, 0);
    if (result != APR_SUCCESS) {
        return result;
    }

    return OK;
}

static int util_ldap_post_config(apr_pool_t *p, apr_pool_t *plog,
                                 apr_pool_t *ptemp, server_rec *s)
{
    apr_status_t result;
    server_rec *s_vhost;
    util_ldap_state_t *st_vhost;

    util_ldap_state_t *st = (util_ldap_state_t *)
                            ap_get_module_config(s->module_config,
                                                 &ldap_module);

    apr_ldap_err_t *result_err = NULL;
    int rc;

    /* util_ldap_post_config() will be called twice. Don't bother
     * going through all of the initialization on the first call
     * because it will just be thrown away.*/
    if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG) {

#if APR_HAS_SHARED_MEMORY
        /*
         * If we are using shared memory caching and the cache file already
         * exists then delete it.  Otherwise we are going to run into problems
         * creating the shared memory.
         */
        if (st->cache_file && st->cache_bytes > 0) {
            char *lck_file = apr_pstrcat(ptemp, st->cache_file, ".lck",
                                         NULL);
            apr_file_remove(lck_file, ptemp);
        }
#endif
        return OK;
    }

#if APR_HAS_SHARED_MEMORY
    /*
     * initializing cache if we don't already have a shm address
     */
    if (!st->cache_shm) {
#endif
        result = util_ldap_cache_init(p, st);
        if (result != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, result, s, APLOGNO(01315)
                         "LDAP cache: could not create shared memory segment");
            return DONE;
        }

        result = ap_global_mutex_create(&st->util_ldap_cache_lock, NULL,
                                        ldap_cache_mutex_type, NULL, s, p, 0);
        if (result != APR_SUCCESS) {
            return result;
        }

        /* merge config in all vhost */
        s_vhost = s->next;
        while (s_vhost) {
            st_vhost = (util_ldap_state_t *)
                       ap_get_module_config(s_vhost->module_config,
                                            &ldap_module);
            st_vhost->util_ldap_cache = st->util_ldap_cache;
            st_vhost->util_ldap_cache_lock = st->util_ldap_cache_lock;
#if APR_HAS_SHARED_MEMORY
            st_vhost->cache_shm = st->cache_shm;
            st_vhost->cache_rmm = st->cache_rmm;
            st_vhost->cache_file = st->cache_file;
            ap_log_error(APLOG_MARK, APLOG_DEBUG, result, s, APLOGNO(01316)
                         "LDAP merging Shared Cache conf: shm=0x%pp rmm=0x%pp "
                         "for VHOST: %s", st->cache_shm, st->cache_rmm,
                         s_vhost->server_hostname);
#endif
            s_vhost = s_vhost->next;
        }
#if APR_HAS_SHARED_MEMORY
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01317)
                     "LDAP cache: LDAPSharedCacheSize is zero, disabling "
                     "shared memory cache");
    }
#endif

    /* log the LDAP SDK used
     */
    {
        apr_ldap_err_t *result = NULL;
        apr_ldap_info(p, &(result));
        if (result != NULL) {
            ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01318) "%s", result->reason);
        }
    }

    apr_pool_cleanup_register(p, s, util_ldap_cleanup_module,
                              util_ldap_cleanup_module);

    /*
     * Initialize SSL support, and log the result for the benefit of the admin.
     *
     * If SSL is not supported it is not necessarily an error, as the
     * application may not want to use it.
     */
    rc = apr_ldap_ssl_init(p,
                      NULL,
                      0,
                      &(result_err));
    if (APR_SUCCESS == rc) {
        rc = apr_ldap_set_option(ptemp, NULL, APR_LDAP_OPT_TLS_CERT,
                                 (void *)st->global_certs, &(result_err));
    }

    if (APR_SUCCESS == rc) {
        st->ssl_supported = 1;
        ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01319)
                     "LDAP: SSL support available" );
    }
    else {
        st->ssl_supported = 0;
        ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(01320)
                     "LDAP: SSL support unavailable%s%s",
                     result_err ? ": " : "",
                     result_err ? result_err->reason : "");
    }

    /* ssl_supported is really a global setting */
    s_vhost = s->next;
    while (s_vhost) {
        st_vhost = (util_ldap_state_t *)
                   ap_get_module_config(s_vhost->module_config,
                                        &ldap_module);

        st_vhost->ssl_supported = st->ssl_supported;
        s_vhost = s_vhost->next;
    }

    /* Initialize the rebind callback's cross reference list. */
    (void) uldap_rebind_init(p);

#ifdef AP_LDAP_OPT_DEBUG
    if (st->debug_level > 0) {
        result = ldap_set_option(NULL, AP_LDAP_OPT_DEBUG, &st->debug_level);
        if (result != LDAP_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01321)
                    "LDAP: Could not set the LDAP library debug level to %d:(%d) %s",
                    st->debug_level, result, ldap_err2string(result));
        }
    }
#endif

    return(OK);
}

static void util_ldap_child_init(apr_pool_t *p, server_rec *s)
{
    apr_status_t sts;
    util_ldap_state_t *st = ap_get_module_config(s->module_config,
                                                 &ldap_module);

    if (!st->util_ldap_cache_lock) return;

    sts = apr_global_mutex_child_init(&st->util_ldap_cache_lock,
              apr_global_mutex_lockfile(st->util_ldap_cache_lock), p);
    if (sts != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, sts, s, APLOGNO(01322)
                     "Failed to initialise global mutex %s in child process",
                     ldap_cache_mutex_type);
    }
}

static const command_rec util_ldap_cmds[] = {
    AP_INIT_TAKE1("LDAPSharedCacheSize", util_ldap_set_cache_bytes,
                  NULL, RSRC_CONF,
                  "Set the size of the shared memory cache (in bytes). Use "
                  "0 to disable the shared memory cache. (default: 500000)"),

    AP_INIT_TAKE1("LDAPSharedCacheFile", util_ldap_set_cache_file,
                  NULL, RSRC_CONF,
                  "Set the file name for the shared memory cache."),

    AP_INIT_TAKE1("LDAPCacheEntries", util_ldap_set_cache_entries,
                  NULL, RSRC_CONF,
                  "Set the maximum number of entries that are possible in the "
                  "LDAP search cache. Use 0 or -1 to disable the search cache "
                  "(default: 1024)"),

    AP_INIT_TAKE1("LDAPCacheTTL", util_ldap_set_cache_ttl,
                  NULL, RSRC_CONF,
                  "Set the maximum time (in seconds) that an item can be "
                  "cached in the LDAP search cache. Use 0 for no limit. "
                  "(default 600)"),

    AP_INIT_TAKE1("LDAPOpCacheEntries", util_ldap_set_opcache_entries,
                  NULL, RSRC_CONF,
                  "Set the maximum number of entries that are possible "
                  "in the LDAP compare cache. Use 0 or -1 to disable the compare cache "
                  "(default: 1024)"),

    AP_INIT_TAKE1("LDAPOpCacheTTL", util_ldap_set_opcache_ttl,
                  NULL, RSRC_CONF,
                  "Set the maximum time (in seconds) that an item is cached "
                  "in the LDAP operation cache. Use 0 for no limit. "
                  "(default: 600)"),

    AP_INIT_TAKE23("LDAPTrustedGlobalCert", util_ldap_set_trusted_global_cert,
                   NULL, RSRC_CONF,
                   "Takes three arguments; the first argument is the cert "
                   "type of the second argument, one of CA_DER, CA_BASE64, "
                   "CA_CERT7_DB, CA_SECMOD, CERT_DER, CERT_BASE64, CERT_KEY3_DB, "
                   "CERT_NICKNAME, KEY_DER, or KEY_BASE64. The second argument "
                   "specifes the file and/or directory containing the trusted CA "
                   "certificates (and global client certs for Netware) used to "
                   "validate the LDAP server. The third argument is an optional "
                   "passphrase if applicable."),

    AP_INIT_TAKE23("LDAPTrustedClientCert", util_ldap_set_trusted_client_cert,
                   NULL, OR_AUTHCFG,
                   "Takes three arguments: the first argument is the certificate "
                   "type of the second argument, one of CA_DER, CA_BASE64, "
                   "CA_CERT7_DB, CA_SECMOD, CERT_DER, CERT_BASE64, CERT_KEY3_DB, "
                   "CERT_NICKNAME, KEY_DER, or KEY_BASE64. The second argument "
                   "specifies the file and/or directory containing the client "
                   "certificate, or certificate ID used to validate this LDAP "
                   "client.  The third argument is an optional passphrase if "
                   "applicable."),

    AP_INIT_TAKE1("LDAPTrustedMode", util_ldap_set_trusted_mode,
                  NULL, RSRC_CONF,
                  "Specify the type of security that should be applied to "
                  "an LDAP connection. One of; NONE, SSL or STARTTLS."),

    AP_INIT_FLAG("LDAPVerifyServerCert", util_ldap_set_verify_srv_cert,
                  NULL, RSRC_CONF,
                  "Set to 'ON' requires that the server certificate be verified"
                  " before a secure LDAP connection can be establish.  Default"
                  " 'ON'"),

    AP_INIT_TAKE1("LDAPConnectionTimeout", util_ldap_set_connection_timeout,
                  NULL, RSRC_CONF,
                  "Specify the LDAP socket connection timeout in seconds "
                  "(default: 10)"),

    AP_INIT_TAKE1("LDAPReferrals", util_ldap_set_chase_referrals,
                  NULL, OR_AUTHCFG,
                  "Choose whether referrals are chased ['ON'|'OFF'|'DEFAULT'].  Default 'ON'"),

    AP_INIT_TAKE1("LDAPReferralHopLimit", util_ldap_set_referral_hop_limit,
                  NULL, OR_AUTHCFG,
                  "Limit the number of referral hops that LDAP can follow. "
                  "(Integer value, Consult LDAP SDK documentation for applicability and defaults"),

    AP_INIT_TAKE1("LDAPLibraryDebug", util_ldap_set_debug_level,
                  NULL, RSRC_CONF,
                  "Enable debugging in LDAP SDK (Default: off, values: SDK specific"),

    AP_INIT_TAKE1("LDAPTimeout", util_ldap_set_op_timeout,
                  NULL, RSRC_CONF,
                  "Specify the LDAP bind/search timeout in seconds "
                  "(0 = no limit). Default: 60"),
    AP_INIT_TAKE1("LDAPConnectionPoolTTL", util_ldap_set_conn_ttl,
                  NULL, RSRC_CONF,
                  "Specify the maximum amount of time a bound connection can sit "
                  "idle and still be considered valid for reuse"
                  "(0 = no pool, -1 = no limit, n = time in seconds). Default: -1"),
    AP_INIT_TAKE1("LDAPRetries", util_ldap_set_retries,
                  NULL, RSRC_CONF,
                  "Specify the number of times a failed LDAP operation should be retried "
                  "(0 = no retries). Default: 3"),
    AP_INIT_TAKE1("LDAPRetryDelay", util_ldap_set_retry_delay,
                  NULL, RSRC_CONF,
                  "Specify the delay between retries of a failed LDAP operation "
                  "(0 = no delay). Default: 0"),


    {NULL}
};

static void util_ldap_register_hooks(apr_pool_t *p)
{
    APR_REGISTER_OPTIONAL_FN(uldap_connection_open);
    APR_REGISTER_OPTIONAL_FN(uldap_connection_close);
    APR_REGISTER_OPTIONAL_FN(uldap_connection_unbind);
    APR_REGISTER_OPTIONAL_FN(uldap_connection_find);
    APR_REGISTER_OPTIONAL_FN(uldap_cache_comparedn);
    APR_REGISTER_OPTIONAL_FN(uldap_cache_compare);
    APR_REGISTER_OPTIONAL_FN(uldap_cache_checkuserid);
    APR_REGISTER_OPTIONAL_FN(uldap_cache_getuserdn);
    APR_REGISTER_OPTIONAL_FN(uldap_ssl_supported);
    APR_REGISTER_OPTIONAL_FN(uldap_cache_check_subgroups);

    ap_hook_pre_config(util_ldap_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(util_ldap_post_config,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_handler(util_ldap_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(util_ldap_child_init, NULL, NULL, APR_HOOK_MIDDLE);
}

AP_DECLARE_MODULE(ldap) = {
   STANDARD20_MODULE_STUFF,
   util_ldap_create_dir_config, /* create dir config */
   NULL,                        /* merge dir config */
   util_ldap_create_config,     /* create server config */
   util_ldap_merge_config,      /* merge server config */
   util_ldap_cmds,              /* command table */
   util_ldap_register_hooks,    /* set up request processing hooks */
};
