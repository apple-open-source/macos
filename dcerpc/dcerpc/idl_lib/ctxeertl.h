/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      ctxeertl.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      Definitions for callee stub's tables of contexts and clients
**
**  VERSION: DCE 1.0
**
**
*/

/*  The structure of the callee client lookup table
**  is overflow hash with double chaining.
*/
typedef struct callee_client_entry_t {
    rpc_client_handle_t client;
    long int count;    /* Number of contexts for this client */
    struct callee_context_entry_t *first_context; /* First in chain of
                                                     contexts for this client */
    struct callee_context_entry_t *last_context;  /* Last in chain of
                                                     contexts for this client */
    struct callee_client_entry_t *prev_h_client;    /* Previous in chain of
                                                clients with same hash value */
    struct callee_client_entry_t *next_h_client;    /* Next in chain of
                                                clients with same hash value */
    long int ref_count;  /* The number of threads currently using contexts
                                of this client */
    RPC_SS_THREADS_CONDITION_T cond_var;  /* Used to signal when ref_count
                                                has been decremented */
    idl_boolean rundown_pending;    /* TRUE if a rundown request for this
                                        client has been received */
} callee_client_entry_t;

/*  The structure of the callee context lookup table
**  is overflow hash with chaining.
*/
typedef struct callee_context_entry_t {
    idl_uuid_t uuid;
    rpc_ss_context_t user_context;
    ctx_rundown_fn_p_t rundown; /* Pointer to rundown routine for context */
    callee_client_entry_t *p_client_entry;  /* Client this context belongs to */
    struct callee_context_entry_t *prev_in_client;  /* Previous in chain of
                                                        contexts for client */
    struct callee_context_entry_t *next_in_client;  /* Next in chain of
                                                        contexts for client */
    struct callee_context_entry_t *next_context;    /* Next in chain of
                                            contexts with the same hash value */
} callee_context_entry_t;

/**************** Function prototypes *******************************/

void rpc_ss_rundown_client
(
    rpc_client_handle_t failed_client
);

void rpc_ss_add_to_callee_client
(
    rpc_client_handle_t ctx_client,     /* Client for whom there is another context */
    callee_context_entry_t *p_context,  /* Pointer to the context */
    ndr_boolean *p_is_new_client,       /* Pointer to TRUE if new client */
    error_status_t *result         /* Function result */
);

void rpc_ss_take_from_callee_client
(
    callee_context_entry_t *p_context,  /* Pointer to the context */
    rpc_client_handle_t *p_close_client,
                                  /* Ptr to NULL or client to stop monitoring */
    error_status_t *result         /* Function result */
);

void rpc_ss_lkddest_callee_context
(
    idl_uuid_t *p_uuid,    /* Pointer to UUID of context to be destroyed */
    rpc_client_handle_t *p_close_client,
                         /* Ptr to NULL or client to stop monitoring */
    error_status_t *result         /* Function result */
);    /* Returns SUCCESS unless the UUID is not in the lookup table */

void rpc_ss_init_callee_client_table(
    void
);

void rpc_ss_create_callee_context
(
    rpc_ss_context_t callee_context, /* user's local form of the context */
    idl_uuid_t    *p_uuid,               /* pointer to the equivalent UUID */
    handle_t  h,                     /* Binding handle */
    ctx_rundown_fn_p_t ctx_rundown,  /* pointer to context rundown routine */
    error_status_t *result      /* Function result */
);

/* Returns status_ok unless the UUID is not in the lookup table */
void rpc_ss_update_callee_context
(
    rpc_ss_context_t callee_context, /* user's local form of the context */
    idl_uuid_t    *p_uuid,               /* pointer to the equivalent UUID */
    error_status_t *result      /* Function result */
);

/* Returns status_ok unless the UUID is not in the lookup table */
void rpc_ss_destroy_callee_context
(
    idl_uuid_t *p_uuid,          /* pointer to UUID of context to be destroyed */
    handle_t  h,                    /* Binding handle */
    error_status_t *result     /* Function result */
);
