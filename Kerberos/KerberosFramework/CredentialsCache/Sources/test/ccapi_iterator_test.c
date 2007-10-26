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

#include "ccapi_test.h"
#include "ccs_list_internal.h"

#define obj_not_found_err  128
#define iter_not_found_err 129

cci_server_id_t g_server_id = "TEST";
cci_object_id_t g_next_object_id = 0;

/* ------------------------------------------------------------------------ */

typedef struct test_object_d {
    cci_identifier_t identifier;
    char *string;
} *test_object_t;

struct test_object_d test_object_initializer = { NULL, NULL };

/* ------------------------------------------------------------------------ */

cc_int32 ccs_server_new_identifier (cci_identifier_t *out_identifier);

static cc_int32 test_obj_new (ccs_object_t *out_object,
                              char         *in_string);
static cc_int32 test_obj_release (ccs_object_t in_object);
static cc_int32 test_obj_compare_identifier (ccs_object_t      in_object,
                                             cci_identifier_t  in_identifier,
                                             cc_uint32        *out_equal);

#pragma mark -

/* ------------------------------------------------------------------------ */
/* Used by cci_list_internal.c but we don't want to pull in the server code */

cc_int32 ccs_server_new_identifier (cci_identifier_t *out_identifier)
{
    cc_int32 err = ccNoError;
    
    if (!out_identifier) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = cci_identifier_new (out_identifier,
                                  g_server_id, 
                                  g_next_object_id++);
    }
    
    return cci_check_error (err);    
}

#pragma mark -

/* ------------------------------------------------------------------------ */

static cc_int32 test_obj_new (ccs_object_t *out_object,
                              char         *in_string) 
{
    cc_int32 err = 0;
    test_object_t object = NULL;
    
    if (!out_object) { err = ccErrBadParam; }
    
    if (!err) {
        object = malloc (sizeof (*object));
        if (object) { 
            *object = test_object_initializer;
        } else {
            err = ccErrNoMem; 
        }
    }
    
    if (!err) {
        err = ccs_server_new_identifier (&object->identifier);
    }
    
    if (!err) {
        object->string = strdup (in_string);
        if (!object->string) { err = ccErrNoMem; }
    }
    
    if (!err) {
        *out_object = (ccs_object_t) object;
        object = NULL;
    }
    
    test_obj_release ((ccs_object_t) object);
    
    return err;
}

/* ------------------------------------------------------------------------ */

static cc_int32 test_obj_release (ccs_object_t in_object)
{
    cc_int32 err = 0;
    test_object_t object = (test_object_t) in_object;
    
    if (!in_object) { err = ccErrBadParam; }
    
    if (!err) {
        printf ("Releasing object '%s'.\n", object->string);
        cci_identifier_release (object->identifier);
        free (object->string);
        free (in_object);
    }
    
    return err;
}

/* ------------------------------------------------------------------------ */

static cc_int32 test_obj_compare_identifier (ccs_object_t      in_object,
                                             cci_identifier_t  in_identifier,
                                             cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;
    test_object_t object = (test_object_t) in_object;
    
    if (!in_object    ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal    ) { err = cci_check_error (ccErrBadParam); }
    
    if (!err) {
        err = cci_identifier_compare (object->identifier, 
                                      in_identifier, 
                                      out_equal);
    }
    
    return cci_check_error (err);
}

#pragma mark -

/* ------------------------------------------------------------------------ */

static cc_int32 print_list (ccs_list_t in_list)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t iterator = NULL;
    
    if (!in_list) { err = ccErrBadParam; }
    
    if (!err) {
        err = ccs_list_new_iterator (in_list, &iterator);
    }
    
    if (!err) {
        int i = 0;
        printf ("List:\n");
        
        while (!err) {
            ccs_object_t object = NULL;
            
            err = ccs_list_iterator_next (iterator, &object);
            
            if (!err) {
                test_object_t tobj = (test_object_t) object;
                printf ("\t%d: \"%s\"\n", i++, tobj->string);
            }
        }
        if (err == ccIteratorEnd) { 
            printf ("\n");
            err = ccNoError; 
        }
    }
    
    ccs_list_iterator_release (iterator);
    
    return err;
}

/* ------------------------------------------------------------------------ */

int ccapi_iterator_test (void)
{
    cc_int32 err = 0;
    ccs_list_t list = NULL;
    char *obj_strings[] = { "FOO", "BAR", "BAZ", "QUUX", "BAR", NULL };
    int i;
    
    if (!err) {
        err = ccs_list_new (&list, 
                            obj_not_found_err,
                            iter_not_found_err,
                            test_obj_compare_identifier,
                            test_obj_release);
    }
    
    for (i = 0; !err && obj_strings[i]; i++) {
        ccs_object_t object = NULL;
        
        err = test_obj_new (&object, obj_strings[i]);
        
        if (!err) {
            err = ccs_list_add (list, object);
        }
        
        if (!err) {
            object = NULL;
        }
        
        test_obj_release (object);
    }
    
    if (!err) {
        err = print_list (list);
    }
    
    ccs_list_release (list);

    return err;
}

