/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gssdigest.h"

#ifdef ENABLE_SCRAM

static gssapi_mech_interface_desc scram_mech = {
    GMI_VERSION,
    "SCRAM-SHA1",
    {6, (void *)"\x2b\x06\x01\x05\x05\x0e"},
    0,
    _gss_scram_acquire_cred,
    _gss_scram_release_cred,
    _gss_scram_init_sec_context,
    _gss_scram_accept_sec_context,
    _gss_scram_process_context_token,
    _gss_scram_delete_sec_context,
    _gss_scram_context_time,
    NULL, /* get_mic */
    NULL, /* verify_mic */
    NULL, /* wrap */
    NULL, /* unwrap */
    _gss_scram_display_status,
    NULL,
    _gss_scram_compare_name,
    _gss_scram_display_name,
    _gss_scram_import_name,
    _gss_scram_export_name,
    _gss_scram_release_name,
    _gss_scram_inquire_cred,
    _gss_scram_inquire_context,
    NULL, /* wrap_size_limit */
    _gss_scram_add_cred,
    _gss_scram_inquire_cred_by_mech,
    _gss_scram_export_sec_context,
    _gss_scram_import_sec_context,
    _gss_scram_inquire_names_for_mech,
    _gss_scram_inquire_mechs_for_name,
    _gss_scram_canonicalize_name,
    _gss_scram_duplicate_name,
    _gss_scram_inquire_sec_context_by_oid,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* wrap_iov */
    NULL, /* unwrap_iov */
    NULL, /* wrap_iov_length */
    NULL,
    NULL,
    NULL,
    _gss_scram_acquire_cred_ext,
    _gss_scram_iter_creds_f,
    _gss_scram_destroy_cred,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

#endif

gssapi_mech_interface
__gss_scram_initialize(void)
{
#ifdef ENABLE_SCRAM
    return &scram_mech;
#else
    return NULL;
#endif
}
