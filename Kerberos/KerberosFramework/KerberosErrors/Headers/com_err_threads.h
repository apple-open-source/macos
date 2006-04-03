/*
 * com_err_threads.h
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#include <Kerberos/com_err.h>

typedef struct error_tables {
    size_t count;
    size_t size;
    const struct error_table **ets;
} *error_table_array_t;


errcode_t com_err_thread_lock_error_tables (error_table_array_t *error_tables);
errcode_t com_err_thread_unlock_error_tables (error_table_array_t *error_tables);

errcode_t com_err_thread_lock_for_bundle_lookup ();
errcode_t com_err_thread_unlock_for_bundle_lookup ();

errcode_t com_err_thread_get_com_err_hook (com_err_handler_t *handler);
errcode_t com_err_thread_set_com_err_hook (com_err_handler_t handler);

errcode_t com_err_thread_get_error_message (char **out_string);
errcode_t com_err_thread_set_error_message (const char *in_string);

errcode_t com_err_thread_get_error_manager (char **out_string);
errcode_t com_err_thread_set_error_manager (const char *in_string);
