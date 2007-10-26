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

#include "kim_private.h"

/* ------------------------------------------------------------------------ */

struct kim_selection_hints_opaque {
    kim_string_t application_identifier;
    kim_string_t application_name;
    kim_string_t explanation;
    kim_options_t options;
    kim_boolean_t allow_user_interaction;
    kim_boolean_t use_cached_results;
    kim_identity_t service_identity;
    kim_string_t client_realm;
    kim_string_t user;
    kim_string_t service_realm;
    kim_string_t service;
    kim_string_t hostname;
};

struct kim_selection_hints_opaque kim_selection_hints_initializer = { 
    NULL,
    NULL,
    NULL,
    KIM_OPTIONS_DEFAULT,
    TRUE,
    TRUE,
    KIM_IDENTITY_ANY,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* ------------------------------------------------------------------------ */

static inline kim_error_t kim_selection_hints_allocate (kim_selection_hints_t *out_selection_hints)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_selection_hints_t selection_hints = NULL;
    
    if (out_selection_hints == NULL) { err = param_error (1, "out_selection_hints", "NULL"); }
    
    if (!err) {
        selection_hints = malloc (sizeof (*selection_hints));
        if (!selection_hints) { err = os_error (errno); }
    }
    
    if (!err) {
        *selection_hints = kim_selection_hints_initializer;
        *out_selection_hints = selection_hints;
        selection_hints = NULL;
    }
    
    kim_selection_hints_free (&selection_hints);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_create (kim_selection_hints_t *out_selection_hints,
                                        kim_string_t           in_application_identifier)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_selection_hints_t selection_hints = NULL;
    
    if (out_selection_hints       == NULL) { err = param_error (1, "out_selection_hints", "NULL"); }
    if (in_application_identifier == NULL) { err = param_error (1, "in_application_identifier", "NULL"); }
    
    if (!err) {
        err = kim_selection_hints_allocate (&selection_hints);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->application_identifier, 
                               in_application_identifier);
    }
    
    if (!err) {
        *out_selection_hints = selection_hints;
        selection_hints = NULL;
    }
    
    kim_selection_hints_free (&selection_hints);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_copy (kim_selection_hints_t *out_selection_hints,
                                      kim_selection_hints_t  in_selection_hints)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_selection_hints_t selection_hints = NULL;
    
    if (out_selection_hints == NULL) { err = param_error (1, "out_selection_hints", "NULL"); }
    if (in_selection_hints  == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    
    if (!err) {
        err = kim_selection_hints_allocate (&selection_hints);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->application_identifier, 
                               in_selection_hints->application_identifier);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->application_name, 
                               in_selection_hints->application_name);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->explanation, 
                               in_selection_hints->explanation);
    }
    
    if (!err) {
        err = kim_options_copy (&selection_hints->options, 
                                in_selection_hints->options);
    }
    
    if (!err) {
        err = kim_identity_copy (&selection_hints->service_identity, 
                                 in_selection_hints->service_identity);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->client_realm, 
                               in_selection_hints->client_realm);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->user, 
                               in_selection_hints->user);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->service_realm, 
                               in_selection_hints->service_realm);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->service, 
                               in_selection_hints->service);
    }
    
    if (!err) {
        err = kim_string_copy (&selection_hints->hostname, 
                               in_selection_hints->hostname);
    }
    
    if (!err) {
        selection_hints->allow_user_interaction = in_selection_hints->allow_user_interaction;
        selection_hints->use_cached_results = in_selection_hints->use_cached_results;
        
        *out_selection_hints = selection_hints;
        selection_hints = NULL;
    }
    
    kim_selection_hints_free (&selection_hints);
    
    return check_error (err);    
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_service_identity (kim_selection_hints_t io_selection_hints,
                                                      kim_identity_t        in_service_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints  == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_service_identity == NULL) { err = param_error (2, "in_service_identity", "NULL"); }
    
    if (!err) {
        err = kim_identity_copy (&io_selection_hints->service_identity, in_service_identity);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_service_identity (kim_selection_hints_t  in_selection_hints,
                                                      kim_identity_t        *out_service_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints   == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_service_identity == NULL) { err = param_error (2, "out_service_identity", "NULL"); }
    
    if (!err) {
        err = kim_identity_copy (out_service_identity, in_selection_hints->service_identity);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_client_realm (kim_selection_hints_t io_selection_hints,
                                                  kim_string_t          in_client_realm)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_client_realm    == NULL) { err = param_error (2, "in_client_realm", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->client_realm, in_client_realm);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_client_realm (kim_selection_hints_t  in_selection_hints,
                                                  kim_string_t          *out_client_realm)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_client_realm   == NULL) { err = param_error (2, "out_client_realm", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_client_realm, in_selection_hints->client_realm);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_user (kim_selection_hints_t io_selection_hints,
                                          kim_string_t          in_user)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_user            == NULL) { err = param_error (2, "in_user", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->user, in_user);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_user (kim_selection_hints_t  in_selection_hints,
                                          kim_string_t          *out_user)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_user           == NULL) { err = param_error (2, "out_user", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_user, in_selection_hints->user);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_service_realm (kim_selection_hints_t io_selection_hints,
                                                   kim_string_t          in_service_realm)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_service_realm   == NULL) { err = param_error (2, "in_service_realm", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->service_realm, in_service_realm);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_service_realm (kim_selection_hints_t  in_selection_hints,
                                                   kim_string_t          *out_service_realm)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_service_realm  == NULL) { err = param_error (2, "out_service_realm", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_service_realm, in_selection_hints->service_realm);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_service (kim_selection_hints_t io_selection_hints,
                                             kim_string_t          in_service)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_service         == NULL) { err = param_error (2, "in_service", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->service, in_service);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_service (kim_selection_hints_t  in_selection_hints,
                                             kim_string_t          *out_service)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_service        == NULL) { err = param_error (2, "out_service", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_service, in_selection_hints->service);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_hostname (kim_selection_hints_t io_selection_hints,
                                              kim_string_t          in_hostname)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_hostname        == NULL) { err = param_error (2, "in_hostname", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->hostname, in_hostname);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_hostname (kim_selection_hints_t  in_selection_hints,
                                              kim_string_t          *out_hostname)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_hostname       == NULL) { err = param_error (2, "out_hostname", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_hostname, in_selection_hints->hostname);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_application_name (kim_selection_hints_t io_selection_hints,
                                                      kim_string_t          in_application_name)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints  == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_application_name == NULL) { err = param_error (2, "in_application_name", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->application_name, in_application_name);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_application_name (kim_selection_hints_t  in_selection_hints,
                                                      kim_string_t          *out_application_name)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints   == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_application_name == NULL) { err = param_error (2, "out_application_name", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_application_name, in_selection_hints->application_name);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_explanation (kim_selection_hints_t io_selection_hints,
                                                 kim_string_t          in_explanation)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_explanation     == NULL) { err = param_error (2, "in_explanation", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (&io_selection_hints->explanation, in_explanation);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_explanation (kim_selection_hints_t  in_selection_hints,
                                                 kim_string_t          *out_explanation)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_explanation    == NULL) { err = param_error (2, "out_explanation", "NULL"); }
    
    if (!err) {
        err = kim_string_copy (out_explanation, in_selection_hints->explanation);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_options (kim_selection_hints_t io_selection_hints,
                                             kim_options_t         in_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    if (in_options         == NULL) { err = param_error (2, "in_options", "NULL"); }
    
    if (!err) {
        err = kim_options_copy (&io_selection_hints->options, in_options);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_options (kim_selection_hints_t  in_selection_hints,
                                             kim_options_t         *out_options)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_options        == NULL) { err = param_error (2, "out_options", "NULL"); }
    
    if (!err) {
        err = kim_options_copy (out_options, in_selection_hints->options);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_allow_user_interaction (kim_selection_hints_t io_selection_hints,
                                                            kim_boolean_t         in_allow_user_interaction)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints  == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    
    if (!err) {
        io_selection_hints->allow_user_interaction = in_allow_user_interaction;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_allow_user_interaction (kim_selection_hints_t  in_selection_hints,
                                                            kim_boolean_t         *out_allow_user_interaction)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints         == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_allow_user_interaction == NULL) { err = param_error (2, "out_allow_user_interaction", "NULL"); }
    
    if (!err) {
        *out_allow_user_interaction = in_selection_hints->allow_user_interaction;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_set_use_cached_results (kim_selection_hints_t io_selection_hints,
                                                        kim_boolean_t         in_use_cached_results)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (io_selection_hints  == NULL) { err = param_error (1, "io_selection_hints", "NULL"); }
    
    if (!err) {
        io_selection_hints->use_cached_results = in_use_cached_results;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_use_cached_results (kim_selection_hints_t  in_selection_hints,
                                                        kim_boolean_t         *out_use_cached_results)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints     == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_use_cached_results == NULL) { err = param_error (2, "out_use_cached_results", "NULL"); }
    
    if (!err) {
        *out_use_cached_results = in_selection_hints->use_cached_results;
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_get_identity (kim_selection_hints_t in_selection_hints,
                                              kim_identity_t        *out_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    kim_identity_t identity = NULL;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (out_identity       == NULL) { err = param_error (2, "out_identity", "NULL"); }
    
    if (!err && in_selection_hints->use_cached_results) {
        err = kim_os_selection_hints_cache_lookup (in_selection_hints, &identity);
    }
    
    if (!err && !identity) {
        /* Search cache collection */
    }
    
    if (!err && !identity && in_selection_hints->allow_user_interaction) {
        /* pop up dialog to ask user for help selecting an identity */
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_cache_results (kim_selection_hints_t in_selection_hints,
                                               kim_identity_t        in_identity)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    if (in_identity        == NULL) { err = param_error (2, "in_identity", "NULL"); }
    
    if (!err) {
        err = kim_os_selection_hints_cache_results (in_selection_hints, in_identity);
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

kim_error_t kim_selection_hints_forget_cached_results (kim_selection_hints_t in_selection_hints)
{
    kim_error_t err = KIM_NO_ERROR;
    
    if (in_selection_hints == NULL) { err = param_error (1, "in_selection_hints", "NULL"); }
    
    if (!err) {
        
    }
    
    return check_error (err);
}

/* ------------------------------------------------------------------------ */

void kim_selection_hints_free (kim_selection_hints_t *io_selection_hints)
{
    if (io_selection_hints && *io_selection_hints) {
        kim_string_free   (&(*io_selection_hints)->application_identifier);
        kim_string_free   (&(*io_selection_hints)->application_name);
        kim_string_free   (&(*io_selection_hints)->explanation);
        kim_options_free  (&(*io_selection_hints)->options);
        kim_identity_free (&(*io_selection_hints)->service_identity);
        kim_string_free   (&(*io_selection_hints)->client_realm);
        kim_string_free   (&(*io_selection_hints)->user);
        kim_string_free   (&(*io_selection_hints)->service_realm);
        kim_string_free   (&(*io_selection_hints)->service);
        kim_string_free   (&(*io_selection_hints)->hostname);
        free (*io_selection_hints);
        *io_selection_hints = NULL;
    }
}

