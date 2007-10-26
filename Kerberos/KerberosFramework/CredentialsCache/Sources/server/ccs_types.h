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

#ifndef CCS_TYPES_H
#define CCS_TYPES_H

#include "cci_types.h"

/* ccs_os_port_t is IPC-specific so it's special cased here */

#if TARGET_OS_MAC
#include <mach/mach_types.h>
typedef mach_port_t ccs_os_port_t;
#define CCS_OS_PORT_INITIALIZER MACH_PORT_NULL
#else
typedef int ccs_os_port_t;
#define CCS_OS_PORT_INITIALIZER -1
#endif

#pragma mark -

struct cci_array_d;
typedef struct cci_array_d *ccs_client_array_t;

#pragma mark -

struct ccs_list_d;
struct ccs_list_iterator_d;

typedef struct ccs_list_d *ccs_cache_collection_list_t;

typedef struct ccs_list_d *ccs_ccache_list_t;
typedef struct ccs_list_iterator_d *ccs_ccache_list_iterator_t;

typedef struct ccs_list_d *ccs_credentials_list_t;
typedef struct ccs_list_iterator_d *ccs_credentials_list_iterator_t;

#pragma mark -

struct ccs_client_d;
typedef struct ccs_client_d *ccs_client_t;

struct ccs_credentials_d;
typedef struct ccs_credentials_d *ccs_credentials_t;

typedef ccs_credentials_list_iterator_t ccs_credentials_iterator_t;

struct ccs_ccache_d;
typedef struct ccs_ccache_d *ccs_ccache_t;

typedef ccs_ccache_list_iterator_t ccs_ccache_iterator_t;

struct ccs_cache_collection_d;
typedef struct ccs_cache_collection_d *ccs_cache_collection_t;

#endif /* CCS_TYPES_H */
