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
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      epstatus.c
**
**  FACILITY:
**
**      Endpoint mapper status.
**
**  ABSTRACT:
**
**	EPSTATUS - a tool to display endpoint mapper registrations.
**
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <dce/rpc.h>
#include <dce/dce.h>
#include <dce/dce_error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#if HAVE_SYSEXITS_H
#include <sysexits.h>
#endif

#ifndef EX_OSERR
#define EX_OSERR 71
#endif

#ifndef EX_USAGE
#define EX_USAGE 64
#endif

struct ep_table_entry
{
    rpc_if_id_t		    te_interface;
    rpc_binding_handle_t    te_binding;
    idl_uuid_t		    te_uuid;
    idl_char *		    te_annotation;
};

static const char PROGNAME[] = "epstatus";
static boolean verbose = false;

#if __GNUC__
static void
status_message(
    FILE * file,
    error_status_t status,
    const char * fmt,
    ...) __attribute__((__format__ (__printf__, 3, 4)));
#endif

static void
status_message(
    FILE * file,
    error_status_t status,
    const char * fmt,
    ...)
{
    dce_error_string_t message;
    error_status_t error_status;

    va_list args;

    dce_error_inq_text(status, message,
                       (int *)&error_status);
    if (error_status != rpc_s_ok)
    {
        snprintf(message, sizeof(message), "RPC error %#x", status);
    }

    va_start(args, fmt);
    vfprintf(file, fmt, args);
    fprintf(file, " %s\n", message);
    va_end(args);
}

static void
epstatus_usage(void)
{
    fprintf(stderr, "usage: %s [-v]\n", PROGNAME);
    fprintf(stderr, "    -v: Print verbose status messages\n");
}

static rpc_binding_handle_t
ep_binding_from_protseq(
    const idl_char * protseq)
{
    error_status_t status;
    unsigned_char_p_t string_binding;
    rpc_binding_handle_t binding = NULL;

    rpc_string_binding_compose(NULL, (unsigned_char_p_t)protseq,
                               NULL, NULL, NULL, &string_binding, &status);
    if (status != rpc_s_ok)
    {
        status_message(stderr, status,
		"%s: failed to compose %s binding", PROGNAME, protseq);
	return NULL;
    }

    if (verbose)
    {
        printf("composed binding string '%s' from protoseq '%s'\n",
               string_binding, protseq);
    }

    rpc_binding_from_string_binding(string_binding, &binding, &status);
    if (status != rpc_s_ok)
    {
        status_message(stderr, status,
		"%s: failed to create %s binding handle",
		PROGNAME, protseq);

	rpc_string_free(&string_binding, &status);
	return NULL;
    }

    /* XXX Setting the timeout doesn't actually seem to allow ncadg_ip_udp
     * to timeout when the endpoint mapper is not there. Not really sure
     * why this  is.
     */
    rpc_mgmt_set_com_timeout(binding, rpc_c_binding_min_timeout, &status);

    rpc_string_free(&string_binding, &status);
    return binding;
}

static void
ep_table_entry_free(
    struct ep_table_entry * te)
{
    error_status_t status;

    if (!te)
    {
        return;
    }

    if (te->te_binding)
    {
        rpc_binding_free(&te->te_binding, &status);
    }

    if (te->te_annotation)
    {
        rpc_string_free(&te->te_annotation, &status);
    }
}

static void
ep_table_entry_display(
    const struct ep_table_entry * te)
{
    error_status_t status;

    unsigned_char_p_t rpc_string = NULL;

    printf("interface version: %u.%u\n",
           te->te_interface.vers_major,
           te->te_interface.vers_minor);

    uuid_to_string((uuid_p_t)&te->te_interface.uuid, &rpc_string, &status);
    if (status == rpc_s_ok)
    {
        printf("interface UUID: %s\n", rpc_string);
        rpc_string_free(&rpc_string, &status);
    }
    else
    {
        status_message(stdout, status, "interface UUID:");
    }

    if (te->te_binding)
    {
        rpc_binding_to_string_binding(te->te_binding,
                                      &rpc_string, &status);
        if (status == rpc_s_ok)
        {
            printf("binding: %s\n", rpc_string);
            rpc_string_free(&rpc_string, &status);
        }
	else
        {
            status_message(stdout, status, "binding:");
        }

    }
    else
    {
        printf("binding:\n");
    }

    uuid_to_string((uuid_p_t)&te->te_uuid, &rpc_string, &status);
    if (status == rpc_s_ok)
    {
        printf("object UUID: %s\n", rpc_string);
        rpc_string_free(&rpc_string, &status);
    }
    else
    {
        status_message(stdout, status, "object UUID:");
    }

    printf("annotation: %s\n",
           te->te_annotation ? (const char *)te->te_annotation : "");
}

static error_status_t
ep_status_display(
    const idl_char * protseq,
    rpc_binding_handle_t binding,
    unsigned * count)
{
    error_status_t status;
    rpc_ep_inq_handle_t inquiry;

    *count = 0;

    if (verbose)
    {
        printf("querying endpoints status over %s\n", protseq);
    }

    rpc_mgmt_ep_elt_inq_begin(binding, rpc_c_ep_all_elts,
                              NULL, rpc_c_vers_all, NULL, &inquiry, &status);
    if (status != rpc_s_ok)
    {
        return status;
    }

    for (;;)
    {
        struct ep_table_entry te;
        memset(&te, 0, sizeof(te));

        rpc_mgmt_ep_elt_inq_next(inquiry, &te.te_interface,
		&te.te_binding, &te.te_uuid, &te.te_annotation, &status);

        if (status == rpc_s_ok)
        {
            (*count)++;
            ep_table_entry_display(&te);
            ep_table_entry_free(&te);
        }
	else if (status == rpc_s_no_more_elements)
        {
            break;
        }
	else
        {
            error_status_t tmp;
            status_message(stderr, status,
		    "%s: endpoint mapper failed on %s,",
		    PROGNAME, protseq);
            rpc_mgmt_ep_elt_inq_done(&inquiry, &tmp);
            return status;
        }

    }

    rpc_mgmt_ep_elt_inq_done(&inquiry, &status);
    return rpc_s_ok;
}

int main(int argc, const char ** argv)
{
    error_status_t status;
    unsigned i;
    int opt;

    rpc_protseq_vector_p_t protseq;

    while ((opt = getopt(argc, (void *)argv, "v")) != -1)
    {
        switch (opt)
        {
        case 'v':
            verbose = true;
            break;
        default:
            epstatus_usage();
            exit(EX_USAGE);
        }
    }

    rpc_network_inq_protseqs(&protseq, &status);
    if (status != rpc_s_ok)
    {
        status_message(stderr, status,
		"%s: no installed protocols", PROGNAME);
	exit(EX_OSERR);
    }

    /* For all the installed protocols, try to dump the endpoint mapper
     * database.
     */
    for (i = 0; i < protseq->count; ++i)
    {
        rpc_binding_handle_t binding;
        unsigned count = 0;

        binding = ep_binding_from_protseq(protseq->protseq[i]);
        if (binding)
        {
            status = ep_status_display(protseq->protseq[i], binding, &count);

            if (status == rpc_s_ok)
            {
                rpc_binding_free(&binding, &status);
                printf("%u endpoints registered\n", count);
                break;
            }

            rpc_binding_free(&binding, &status);
        }
    }

    rpc_protseq_vector_free(&protseq, &status);

    return 0;
}
