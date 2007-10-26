/*
 * Copyright 2005-2006 Massachusetts Institute of Technology.
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

kim_error_t kim_ccache_iterator_create (kim_ccache_iterator_t *out_ccache_iterator)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_ccache_iterator == NULL) { err = param_error (1, "out_ccache_iterator", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_iterator_next (kim_ccache_iterator_t  in_ccache_iterator,
                                      kim_ccache_t          *out_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache_iterator == NULL) { err = param_error (1, "in_ccache_iterator", "NULL"); }
    if (out_ccache         == NULL) { err = param_error (2, "out_ccache", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_ccache_iterator_free (kim_ccache_iterator_t *io_ccache_iterator)
{
    if (io_ccache_iterator && *io_ccache_iterator) {
        free (*io_ccache_iterator);
        *io_ccache_iterator = NULL;
    }
}

#pragma mark -

/* ------------------------------------------------------------------------ */

struct kim_ccache_opaque {
    krb5_context context;
    krb5_ccache ccache;
};

struct kim_ccache_opaque kim_ccache_initializer = { NULL, NULL };

/* ------------------------------------------------------------------------ */

static inline kim_error_t kim_ccache_allocate (kim_ccache_t *out_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_ccache_t ccache = NULL;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    
    if (!err) {
        ccache = malloc (sizeof (*ccache));
        if (!ccache) { err = os_error (errno); }
    }
    
    if (!err) {
        *ccache = kim_ccache_initializer;
        *out_ccache = ccache;
        ccache = NULL;
    }
    
    kim_ccache_free (&ccache);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

static kim_error_t kim_ccache_create_resolve_name (kim_string_t *out_resolve_name,
                                                   kim_string_t  in_name,
                                                   kim_string_t  in_type)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_resolve_name == NULL) { err = param_error (1, "out_resolve_name", "NULL"); }
    if (in_name          == NULL) { err = param_error (2, "in_name", "NULL"); }
    if (in_type          == NULL) { err = param_error (2, "in_type", "NULL"); }
    
    if (!err) {
        err = kim_string_create_from_format (out_resolve_name, "%s:%s", in_type, in_name);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_new (kim_ccache_t          *out_ccache,
                                   kim_identity_t         in_client_identity,
                                   kim_options_t          in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    kim_identity_t client_identity = NULL;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    
    if (!err) {
        err = kim_credential_create_new (&credential, in_client_identity, in_options);
    }
    
    if (!err) {
        err = kim_credential_get_client_identity (credential, &client_identity);
    }
        
    if (!err) {
        err = kim_credential_store (credential, client_identity, out_ccache);
    }
    
    kim_identity_free (&client_identity);
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_new_if_needed (kim_ccache_t   *out_ccache,
                                             kim_identity_t  in_client_identity,
                                             kim_options_t   in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_ccache         == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    if (in_client_identity == NULL) { err = param_error (2, "in_client_identity", "NULL"); }
    
    if (!err) {
        err = kim_ccache_create_from_client_identity (out_ccache, in_client_identity);
        if (err) {
            kim_error_free (&err);  /* toss error since we don't care */
            err = kim_ccache_create_new (out_ccache, in_client_identity, in_options);
        }
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_from_client_identity (kim_ccache_t   *out_ccache,
                                                    kim_identity_t  in_client_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_ccache         == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    if (in_client_identity == NULL) { err = param_error (2, "in_client_identity", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_from_keytab (kim_ccache_t    *out_ccache,
                                           kim_identity_t   in_identity,
                                           kim_options_t    in_options,
                                           kim_string_t     in_keytab)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    kim_identity_t client_identity = NULL;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    
    if (!err) {
        err = kim_credential_create_from_keytab (&credential, in_identity, in_options, in_keytab);
    }
    
    if (!err) {
        err = kim_credential_get_client_identity (credential, &client_identity);
    }
    
    if (!err) {
        err = kim_credential_store (credential, client_identity, out_ccache);
    }
    
    kim_identity_free (&client_identity);
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_from_default (kim_ccache_t *out_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_from_type_and_name (kim_ccache_t  *out_ccache,
                                                  kim_string_t   in_type,
                                                  kim_string_t   in_name)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_ccache_t ccache = NULL;
    kim_string_t resolve_name = NULL;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    if (in_name    == NULL) { err = param_error (2, "in_name", "NULL"); }
    if (in_type    == NULL) { err = param_error (2, "in_type", "NULL"); }
    
    if (!err) {
        err = kim_ccache_allocate (&ccache);
    }
    
    if (!err) {
        err = krb5_error (krb5_init_context (&ccache->context));
    }
    
    if (!err) {
        err = kim_ccache_create_resolve_name (&resolve_name, in_name, in_type);
    }
    
    if (!err) {
        err = krb5_error (krb5_cc_resolve (ccache->context, resolve_name, &ccache->ccache));
    }
    
    if (!err) {
        *out_ccache = ccache;
        ccache = NULL;
    }
    
    kim_ccache_free (&ccache);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_create_from_krb5_ccache (kim_ccache_t  *out_ccache,
                                                krb5_context   in_krb5_context,
                                                krb5_ccache    in_krb5_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (out_ccache      == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    if (in_krb5_ccache  == NULL) { err = param_error (2, "in_krb5_ccache", "NULL"); }
    if (in_krb5_context == NULL) { err = param_error (3, "in_krb5_context", "NULL"); }
    
    if (!err) {
        err = kim_ccache_create_from_type_and_name (out_ccache, 
                                                    krb5_cc_get_type (in_krb5_context, in_krb5_ccache), 
                                                    krb5_cc_get_name (in_krb5_context, in_krb5_ccache));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_copy (kim_ccache_t  *out_ccache,
                             kim_ccache_t   in_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t name = NULL;
    kim_string_t type = NULL;
    
    if (out_ccache == NULL) { err = param_error (1, "out_ccache", "NULL"); }
    if (in_ccache  == NULL) { err = param_error (2, "in_ccache", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_name (in_ccache, &name);
    }

    if (!err) {
        err = kim_ccache_get_type (in_ccache, &type);
    }
    
    if (!err) {
        err = kim_ccache_create_from_type_and_name (out_ccache, type, name);
    }
    
    kim_string_free (&name);
    kim_string_free (&type);

    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_krb5_ccache (kim_ccache_t  in_ccache,
                                        krb5_context  in_krb5_context,
                                        krb5_ccache  *out_krb5_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_string_t resolve_name = NULL;
    
    if (in_ccache       == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (in_krb5_context == NULL) { err = param_error (2, "in_krb5_context", "NULL"); }
    if (out_krb5_ccache == NULL) { err = param_error (2, "out_krb5_ccache", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_display_name (in_ccache, &resolve_name);
    }
    
    if (!err) {
        err = krb5_error (krb5_cc_resolve (in_krb5_context, resolve_name, out_krb5_ccache));
    }
    
    kim_string_free (&resolve_name);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_type (kim_ccache_t  in_ccache,
                                 kim_string_t *out_type)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_type  == NULL) { err = param_error (2, "out_type", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_type, krb5_cc_get_type (in_ccache->context, in_ccache->ccache));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_name (kim_ccache_t  in_ccache,
                                 kim_string_t *out_name)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_name  == NULL) { err = param_error (2, "out_name", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_name, krb5_cc_get_name (in_ccache->context, in_ccache->ccache));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_display_name (kim_ccache_t  in_ccache,
                                         kim_string_t *out_display_name)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache        == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_display_name == NULL) { err = param_error (2, "out_display_name", "NULL"); }
    
    if (!err) {
        err = kim_ccache_create_resolve_name (out_display_name, 
                                              krb5_cc_get_type (in_ccache->context, in_ccache->ccache), 
                                              krb5_cc_get_name (in_ccache->context, in_ccache->ccache));
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_client_identity (kim_ccache_t    in_ccache,
                                            kim_identity_t *out_client_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    krb5_principal principal = NULL;
    
    if (in_ccache           == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_client_identity == NULL) { err = param_error (2, "out_client_identity", "NULL"); }
    
    if (!err) {
        err = krb5_error (krb5_cc_get_principal (in_ccache->context, in_ccache->ccache, 
                                                 &principal));
    }
    
    if (!err) {
        err = kim_identity_create_from_krb5_principal (out_client_identity,
                                                       in_ccache->context, principal);
    }
    
    if (principal != NULL) { krb5_free_principal (in_ccache->context, principal); }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_valid_credential (kim_ccache_t      in_ccache,
                                             kim_credential_t *out_credential)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache      == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_credential == NULL) { err = param_error (2, "out_credential", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_start_time (kim_ccache_t  in_ccache,
                                       kim_time_t   *out_start_time)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (in_ccache      == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_start_time == NULL) { err = param_error (2, "out_start_time", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_get_start_time (credential, out_start_time);
    }
    
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_expiration_time (kim_ccache_t  in_ccache,
                                            kim_time_t   *out_expiration_time)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (in_ccache           == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_expiration_time == NULL) { err = param_error (2, "out_expiration_time", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_get_expiration_time (credential, out_expiration_time);
    }
    
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_get_renewal_expiration_time (kim_ccache_t  in_ccache,
                                                    kim_time_t   *out_renewal_expiration_time)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    
    if (in_ccache                   == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    if (out_renewal_expiration_time == NULL) { err = param_error (2, "out_renewal_expiration_time", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_get_renewal_expiration_time (credential, 
                                                          out_renewal_expiration_time);
    }
    
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_set_default (kim_ccache_t in_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    
    if (!err) {
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_verify (kim_ccache_t   in_ccache,
                               kim_identity_t in_service_identity,
                               kim_string_t   in_keytab,
                               kim_boolean_t  in_fail_if_no_service_key)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
        
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_verify (credential, in_service_identity, 
                                     in_keytab, in_fail_if_no_service_key);
    }
    
    kim_credential_free (&credential);

    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_renew (kim_ccache_t  in_ccache,
                              kim_options_t in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    kim_identity_t client_identity = NULL;
    
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_renew (&credential, in_options);
    }
    
    if (!err) {
        err = kim_ccache_get_client_identity (in_ccache, &client_identity);
    }
    
    if (!err) {
        err = kim_credential_store (credential, client_identity, NULL);
    }
    
    kim_identity_free (&client_identity);
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_validate (kim_ccache_t  in_ccache,
                                 kim_options_t in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_credential_t credential = NULL;
    kim_identity_t client_identity = NULL;
    
    if (in_ccache == NULL) { err = param_error (1, "in_ccache", "NULL"); }
    
    if (!err) {
        err = kim_ccache_get_valid_credential (in_ccache, &credential);
    }
    
    if (!err) {
        err = kim_credential_validate (&credential, in_options);
    }
    
    if (!err) {
        err = kim_ccache_get_client_identity (in_ccache, &client_identity);
    }
    
    if (!err) {
        err = kim_credential_store (credential, client_identity, NULL);
    }
    
    kim_identity_free (&client_identity);
    kim_credential_free (&credential);
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_ccache_destroy (kim_ccache_t *io_ccache)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_ccache && *io_ccache) {
        err = krb5_error (krb5_cc_destroy ((*io_ccache)->context, (*io_ccache)->ccache));
        
        if (!err) { 
            (*io_ccache)->ccache = NULL; 
            kim_ccache_free (io_ccache);
        }
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_ccache_free (kim_ccache_t *io_ccache)
{
    if (io_ccache && *io_ccache) {
        if ((*io_ccache)->context) { 
            if ((*io_ccache)->ccache) {
                krb5_cc_close ((*io_ccache)->context, (*io_ccache)->ccache);
            }
            krb5_free_context ((*io_ccache)->context); 
        }
        free (*io_ccache);
        *io_ccache = NULL;
    }
}

