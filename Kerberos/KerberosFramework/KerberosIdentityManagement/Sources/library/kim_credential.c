/*
 * $Header$
 *
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <krb5.h>
#include "kim_private.h"

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_iterator_create (kim_credential_iterator_t *out_credential_iterator,
                                            kim_ccache_t               in_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_credential_iterator == NULL) { err = param_error (1, "out_credential_iterator", "NULL"); }
    if (in_ccache               == NULL) { err = param_error (2, "in_ccache", "NULL"); }
    
    if (!err) {
    }
        
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_iterator_next (kim_credential_iterator_t  in_credential_iterator,
                                          kim_credential_t          *out_credential)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential_iterator == NULL) { err = param_error (1, "in_credential_iterator", "NULL"); }
    if (out_credential         == NULL) { err = param_error (2, "out_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_credential_iterator_free (kim_credential_iterator_t *io_credential_iterator)
{
    if (io_credential_iterator && *io_credential_iterator) {
        free (*io_credential_iterator);
        *io_credential_iterator = NULL;
    }
}

#pragma mark -

/* ------------------------------------------------------------------------ */

struct kim_credential_opaque {
    krb5_context context;
    krb5_creds *creds;
};

struct kim_credential_opaque kim_credential_initializer = { NULL, NULL };

/* ------------------------------------------------------------------------ */

static inline kim_error_t kim_credential_allocate (kim_credential_t *out_credential)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (out_credential == NULL) { err = param_error (1, "out_credential", "NULL"); }
    
    if (!err) {
        credential = malloc (sizeof (*credential));
        if (!credential) { err = os_error (errno); }
    }
    
    if (!err) {
        *credential = kim_credential_initializer;
        *out_credential = credential;
        credential = NULL;
    }
    
    kim_credential_free (&credential);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_create_new (kim_credential_t *out_credential,
                                       kim_identity_t    in_client_identity,
                                       kim_options_t     in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (out_credential == NULL) { err = param_error (1, "out_credential", "NULL"); }
    
    if (!err) {
        err = kim_credential_allocate (&credential);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&credential->context));
    }
    
    if (!err) {
    }
    
    if (!err) {
        *out_credential = credential;
        credential = NULL;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_create_from_keytab (kim_credential_t *out_credential,
                                               kim_identity_t    in_identity,
                                               kim_options_t     in_options,
                                               kim_string_t      in_keytab)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    krb5_keytab keytab = NULL;
    krb5_creds creds;
    kim_boolean_t creds_allocated = FALSE;
    krb5_principal principal = NULL;
    kim_time_t start_time = 0;
    kim_string_t service_name = NULL;
    krb5_get_init_creds_opt init_cred_options;
    krb5_address **addresses = NULL;
    
    if (out_credential == NULL) { err = param_error (1, "out_credential", "NULL"); }
    
    if (!err) {
        err = kim_credential_allocate (&credential);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&credential->context));
    }
    
    if (!err) {
        kim_options_t options = in_options;

        if (!options) {
            err = kim_options_create (&options);
        }
        
        if (!err) {
            err = kim_options_get_init_cred_options (options, credential->context,
                                                     &start_time, &service_name,
                                                     &init_cred_options, &addresses);
        }
        
        if (options != in_options) { kim_options_free (&options); }
    }
    
    if (!err) {
        if (in_keytab) {
            err = krb5_error (krb5_kt_resolve (credential->context, in_keytab, &keytab));
        } else {
            err = krb5_error (krb5_kt_default (credential->context, &keytab));
        }
    }
    
    if (!err) {
        if (in_identity) {
            err = kim_identity_get_krb5_principal (in_identity, credential->context, &principal);
        } else {
            krb5_kt_cursor cursor = NULL;
            krb5_keytab_entry entry;
            kim_boolean_t entry_allocated = FALSE;
            
            err = krb5_error (krb5_kt_start_seq_get (credential->context, keytab, &cursor));
            
            if (!err) {
                err = krb5_error (krb5_kt_next_entry (credential->context, keytab, &entry, &cursor));
                entry_allocated = (err == KIM_NO_ERROR); /* remember to free later */
            }
            
            if (!err) {
                err = krb5_error (krb5_copy_principal (credential->context, entry.principal, &principal));
            }
            
            if (entry_allocated) { krb5_free_keytab_entry_contents (credential->context, &entry); }
            if (cursor  != NULL) { krb5_kt_end_seq_get (credential->context, keytab, &cursor); }
        }
    }

    if (!err) {
        err = krb5_error (krb5_get_init_creds_keytab (credential->context, &creds, principal, keytab, 
                                                      start_time, (char *) service_name, &init_cred_options));        
    }
    
    if (!err) {
        err = krb5_error (krb5_copy_creds (credential->context, &creds, &credential->creds));
    }
    
    if (!err) {
        *out_credential = credential;
        credential = NULL;
    }
    
    if (addresses != NULL) { krb5_free_addresses (credential->context, addresses); }
    if (principal != NULL) { krb5_free_principal (credential->context, principal); }
    if (creds_allocated  ) { krb5_free_cred_contents (credential->context, &creds); }
    kim_string_free (&service_name);
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_create_from_krb5_creds (kim_credential_t *out_credential,
                                                   krb5_creds       *in_krb5_creds,
                                                   krb5_context      in_krb5_context)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (out_credential  == NULL) { err = param_error (1, "out_credential", "NULL"); }
    if (in_krb5_creds   == NULL) { err = param_error (2, "in_krb5_creds", "NULL"); }
    if (in_krb5_context == NULL) { err = param_error (3, "in_krb5_context", "NULL"); }
    
    if (!err) {
        err = kim_credential_allocate (&credential);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&credential->context));
    }
    
    if (!err) {
        err = krb5_error (krb5_copy_creds (credential->context, in_krb5_creds, &credential->creds));
    }
    
    if (!err) {
        *out_credential = credential;
        credential = NULL;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_copy (kim_credential_t *out_credential,
                                 kim_credential_t  in_credential)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (out_credential == NULL) { err = param_error (1, "out_credential", "NULL"); }
    if (in_credential  == NULL) { err = param_error (2, "in_credential", "NULL"); }
    
    if (!err) {
        err = kim_credential_allocate (&credential);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&credential->context));
    }
    
    if (!err) {
        err = krb5_error (krb5_copy_creds (credential->context, in_credential->creds, &credential->creds));
    }
    
    if (!err) {
        *out_credential = credential;
        credential = NULL;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_krb5_creds (kim_credential_t   in_credential,
                                           krb5_context       in_krb5_context,
                                           krb5_creds       **out_krb5_creds)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential   == NULL) { err = param_error (1, "in_credential", "NULL"); }
    if (in_krb5_context == NULL) { err = param_error (2, "in_krb5_context", "NULL"); }
    if (out_krb5_creds  == NULL) { err = param_error (3, "out_krb5_creds", "NULL"); }
    
    if (!err) {
        err = krb5_error (krb5_copy_creds (in_krb5_context, in_credential->creds, out_krb5_creds));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_client_identity (kim_credential_t  in_credential,
                                                kim_identity_t   *out_client_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential       == NULL) { err = param_error (1, "in_credential", "NULL"); }
    if (out_client_identity == NULL) { err = param_error (2, "out_client_identity", "NULL"); }
    
    if (!err) {
        err = kim_identity_create_from_krb5_principal (out_client_identity,
                                                       in_credential->context,
                                                       in_credential->creds->client);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_service_identity (kim_credential_t  in_credential,
                                                 kim_identity_t   *out_service_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential        == NULL) { err = param_error (1, "in_credential", "NULL"); }
    if (out_service_identity == NULL) { err = param_error (2, "out_service_identity", "NULL"); }
    
    if (!err) {
        err = kim_identity_create_from_krb5_principal (out_service_identity,
                                                       in_credential->context,
                                                       in_credential->creds->server);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_is_tgt (kim_credential_t  in_credential,
                                   kim_boolean_t     *out_is_tgt)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t service = NULL;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    if (out_is_tgt    == NULL) { err = param_error (2, "out_is_tgt", "NULL"); }
    
    if (!err) {
        err = kim_credential_get_service_identity (in_credential, &service);
    }
    
    if (!err) {
        err = kim_identity_is_tgt_service (service, out_is_tgt);
    }
    
    kim_identity_free (&service);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_state (kim_credential_t           in_credential,
                                      kim_credential_state_t    *out_state)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_time_t expiration_time = 0;
    kim_time_t start_time = 0;
    krb5_timestamp now = 0;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    if (out_state     == NULL) { err = param_error (2, "out_state", "NULL"); }
    
    if (!err) {
        err = kim_credential_get_expiration_time (in_credential, &expiration_time);
    }
    
    if (!err) {
        err = kim_credential_get_start_time (in_credential, &start_time);
    }
    
    if (!err) {
        krb5_int32 usec;
        
        err = krb5_error (krb5_us_timeofday (in_credential->context, &now, &usec));
    }
    
    if (!err) {
        *out_state = kim_credentials_state_valid;
        
        if (expiration_time <= now) {
            *out_state = kim_credentials_state_expired;
        
        } else if ((in_credential->creds->ticket_flags & TKT_FLG_POSTDATED) && 
                   (in_credential->creds->ticket_flags & TKT_FLG_INVALID)) {
            if (start_time > now) { 
                *out_state = kim_credentials_state_not_yet_valid;
            } else {
                *out_state = kim_credentials_state_needs_validation;
            }
            
        } else if (in_credential->creds->addresses) { /* ticket contains addresses */
            krb5_address **laddresses = NULL;
            
            krb5_error_code code = krb5_os_localaddr (in_credential->context, &laddresses);
            if (!code) { laddresses = NULL; }
            
            if (laddresses) { /* assume valid if the local host has no addresses */
                kim_boolean_t found_match = FALSE;
                kim_count_t i = 0;
                
                for (i = 0; in_credential->creds->addresses[0]; i++) {
                    if (krb5_address_search (in_credential->context, 
                                             in_credential->creds->addresses[i], laddresses)) {
                        found_match = TRUE;
                        break;
                    }
                }
                
                if (!found_match) {
                    *out_state = kim_credentials_state_address_mismatch;
                }
            }
            
            if (laddresses != NULL) { krb5_free_addresses (in_credential->context, laddresses); }
        }
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_start_time (kim_credential_t  in_credential,
                                           kim_time_t       *out_start_time)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    
    if (!err) {
        *out_start_time = (in_credential->creds->times.starttime ? 
                           in_credential->creds->times.starttime :
                           in_credential->creds->times.authtime);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_expiration_time (kim_credential_t  in_credential,
                                                kim_time_t       *out_expiration_time)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    
    if (!err) {
        *out_expiration_time = in_credential->creds->times.endtime;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_get_renewal_expiration_time (kim_credential_t  in_credential,
                                                        kim_time_t       *out_renewal_expiration_time)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    
    if (!err) {
        *out_renewal_expiration_time = in_credential->creds->times.renew_till;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_store (kim_credential_t  in_credential,
                                  kim_identity_t    in_client_identity,
                                  kim_ccache_t     *out_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_verify (kim_credential_t in_credential,
                                   kim_identity_t   in_service_identity,
                                   kim_string_t     in_keytab,
                                   kim_boolean_t    in_fail_if_no_service_key)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_credential == NULL) { err = param_error (1, "in_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_renew (kim_credential_t *io_credential,
                                  kim_options_t     in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_credential == NULL) { err = param_error (1, "io_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_credential_validate (kim_credential_t *io_credential,
                                     kim_options_t     in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_credential == NULL) { err = param_error (1, "io_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_credential_free (kim_credential_t *io_credential)
{
    if (io_credential && *io_credential) {
        if ((*io_credential)->context != NULL) {
            if ((*io_credential)->creds != NULL) {
                krb5_free_creds ((*io_credential)->context, (*io_credential)->creds);
            }
            krb5_free_context ((*io_credential)->context);
        }
        free (*io_credential);
        *io_credential = NULL;
    }
}
