/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 * Copyright (C) 2003 by Apple Computer, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


                /* The lyf so short, the craft so long to lerne.
                 * -- Chaucer */


    
/**
 * @file
 *
 * Routines to parse <tt>$DISTCC_HOSTS</tt>.  Actual decisions about
 * where to run a job are in where.c.
 *
 * The grammar of this variable is, informally:
 *
 * DISTCC_HOSTS = HOST ...
 * HOSTID = SSH_HOST | TCP_HOST
 * SSH_HOST = USER@HOSTID[/MUL][:COMMAND]
 * TCP_HOST = HOSTID[/MUL][:PORT]
 * HOSTID = HOSTNAME | IPV4 | '['IPV6']'
 *
 * Any amount of whitespace may be present between hosts.
 *
 * The command specified for SSH defines the location of the remote
 * server, e.g. "/usr/local/bin/distccd".  This is provided as a
 * convenience who have trouble getting their PATH set correctly for
 * sshd to find distccd, and should not normally be needed.
 *
 * If you need to specify special options for ssh, they should be put
 * in ~/.ssh/config and referenced by the hostname.
 *
 * The TCP port defaults to 3632 and should not normally need to be
 * overridden.
 *
 * IPv6 literals are not supported yet.  They will need to be
 * surrounded by square brackets because they may contain a colon,
 * which would otherwise be ambiguous.  This is consistent with other
 * URL-like schemes.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "hosts.h"
#include "exitcode.h"
#include "zeroconf_client.h"

const int dcc_default_port = DISTCC_DEFAULT_PORT;

/* TODO: Write and use a test harness for this. */

/* XXX: Where is the right place to handle multi-A-records?  Should
 * they expand into multiple host definitions, or should they be
 * handled later on when locking?
 *
 * ssh is an interesting case because we probably want to open the
 * connection using the hostname, so that the ssh config's "Host"
 * sections can have the proper effect.
 *
 * Alternatively perhaps it would be better to avoid multi-A records
 * altogether and just use SRV records.  Simpler in a way...
 *
 * For every hostname occurring in the list, we need to check if it
 * has SRV records, and if so use that instead.  We need to handle the
 * special SRV record which says "canonically not available", by
 * ignoring that host.  This can be done for both SSH and TCP.
 * (Perhaps for SSH it needs to be "distcc.ssh.host.foo.com" rather
 * than "tcp"?)
 *
 * Sometimes people use multi A records for machines with several
 * routeable interfaces.  In that case it would be bad to assume the
 * machine can run multiple jobs, and it is better to let the resolver
 * work out which address to use.
 */



/**
 * @post *ret_nhosts>0 if the function succeeded.
 *
 * @post There are @p *ret_nhosts entries in the list at @p *ret_list.
 *
 * @return -1 on error (including no hosts defined), or 0
 **/
int dcc_parse_hosts_env(struct dcc_hostdef **ret_list,
                        int *ret_nhosts)
{
    char *where;

    where = getenv("DISTCC_HOSTS");

#if defined(DARWIN)
    if (!where) {
        dcc_zc_get_resolved_services_list(&where);
    }
#endif // DARWIN

    if (!where) {
#if defined(DARWIN)
        rs_log_error("Unable to use zeroconfig; can't distribute work");
#else
        rs_log_warning("$DISTCC_HOSTS is not defined; can't distribute work");
#endif // DARWIN
        return EXIT_BAD_HOSTSPEC;
    }

    return dcc_parse_hosts(where, ret_list, ret_nhosts);
}


/**
 * Duplicate the part of the string @p psrc up to a character in @p sep
 * (or end of string), storing the result in @p pdst.  @p psrc is updated to
 * point to the terminator.  (If the terminator is not found it will
 * therefore point to \0.
 *
 * If there is no more string, then @p pdst is instead set to NULL, no
 * memory is allocated, and @p psrc is not advanced.
 **/
static int dcc_dup_part(const char **psrc, char **pdst, const char *sep)
{
    int len;

    len = strcspn(*psrc, sep);
    if (len == 0) {
        *pdst = NULL;
    } else {    
        if (!(*pdst = malloc(len + 1))) {
            rs_log_error("failed to allocate string duplicate: %d", (int) len);
            return EXIT_OUT_OF_MEMORY;
        }
        strncpy(*pdst, *psrc, len);
        (*pdst)[len] = '\0';
        (*psrc) += len;
    }

    return 0;
}

static int dcc_parse_multiplier(const char **psrc, struct dcc_hostdef *hostdef)
{
    char *mul;
    const char *token = *psrc;
    int ret;

    if ((*psrc)[0] == '/') {
        (*psrc)++;
        if ((ret = dcc_dup_part(psrc, &mul, "/: \t\n\f")) != 0)
            return ret;
        if (!mul || atoi(mul) == 0) {
            rs_log_error("bad multiplier \"%s\" in host specification", token);
            return EXIT_BAD_HOSTSPEC;
        }
        hostdef->n_slots = atoi(mul);
    }
    return 0;
}

static int dcc_parse_ssh_host(struct dcc_hostdef *hostdef,
                              const char *token_start)
{
    int ret;
    const char *token = token_start;
    
    /* Everything up to '@' is the username */
    if ((ret = dcc_dup_part(&token, &hostdef->user, "@")) != 0)
        return ret;

    assert(token[0] == '@');
    token++;

    if ((ret = dcc_dup_part(&token, &hostdef->hostname, "/: \t\n\f")) != 0)
        return ret;

    if (!hostdef->hostname) {
        rs_log_error("hostname is required in SSH host specification \"%s\"",
                     token_start);
        return EXIT_BAD_HOSTSPEC;
    }

    if ((ret = dcc_parse_multiplier(&token, hostdef)) != 0)
        return ret;

    if (token[0] == ':') {
        token++;
        if ((ret = dcc_dup_part(&token, &hostdef->ssh_command, " \t\n\f")) != 0)
            return ret;
    }
    
    hostdef->mode = DCC_MODE_SSH;
    return 0;
}


static int dcc_parse_tcp_host(struct dcc_hostdef *hostdef,
                              const char * const token_start)
{
    char *port_str;
    int ret;
    const char *token = token_start;
    
    if ((ret = dcc_dup_part(&token, &hostdef->hostname, "/: \t\n\f")) != 0)
        return ret;

    if (!hostdef->hostname) {
        rs_log_error("hostname is required in tcp host specification \"%s\"",
                     token_start);
        return EXIT_BAD_HOSTSPEC;
    }

    if ((ret = dcc_parse_multiplier(&token, hostdef)) != 0)
        return ret;

    hostdef->port = dcc_default_port;
    if (token[0] == ':') {
        token++;

        if ((ret = dcc_dup_part(&token, &port_str, " \t\n\f")) != 0)
            return ret;
        
        if (port_str) {
            char *tail;
            hostdef->port = strtol(port_str, &tail, 10);
            if (*tail != '\0' && !isspace(*tail)) {
                rs_log_error("invalid tcp port specification in \"%s\"", port_str);
                return EXIT_BAD_HOSTSPEC;
            }
            free(port_str);
        }
    }
        
    hostdef->mode = DCC_MODE_TCP;
    return 0;
}


static int dcc_parse_localhost(struct dcc_hostdef *hostdef,
                               const char * token_start)
{
    const char *token = token_start + strlen("localhost");

    hostdef->mode = DCC_MODE_LOCAL;
    hostdef->hostname = strdup("localhost");
    hostdef->n_slots = 1;
    
    return dcc_parse_multiplier(&token, hostdef);
}


/**
 * @return 0 if parsed successfully; nonzero if there were any errors,
 * or if no hosts were defined.
 **/
int dcc_parse_hosts(const char *where,
                    struct dcc_hostdef **ret_list,
                    int *ret_nhosts)
{
    int ret;
    struct dcc_hostdef *prev, *curr;

    /* TODO: Check for '/' in places where it might cause trouble with
     * a lock file name. */

    prev = NULL;
    *ret_list = NULL;
    *ret_nhosts = 0;
    /* A simple, hardcoded scanner.  Some of the GNU routines might be
     * useful here, but they won't work on less capable systems.
     *
     * We repeatedly attempt to extract a whitespace-delimited host
     * definition from the string until none remain.  Allocate an
     * entry; hook to previous entry.  We then determine if there is a
     * '@' in it, which tells us whether it is an SSH or TCP
     * definition.  We then duplicate the relevant subcomponents into
     * the relevant fields. */
    while (1) {
        int token_len;
        const char *token_start;
        int has_at;
        
        while (isspace(where[0]))
            where++;            /* skip space */
        if (where[0] == '\0')
            break;              /* end of string */

        token_start = where;
        token_len = strcspn(where, " \t\n\f");

        /* Allocate new list item */
        curr = calloc(1, sizeof(struct dcc_hostdef));
        if (!curr) {
            rs_log_crit("failed to allocate host definition");
            return EXIT_OUT_OF_MEMORY;
        }

        /* Link into list */
        if (prev) {
            prev->next = curr;
        } else {
            *ret_list = curr;   /* first */
        }

        /* Default task limit */
        curr->n_slots = 4;
            
        has_at = (memchr(token_start, '@', token_len) != NULL);
        /* TODO: Call a separate function to split each type up, then
         * link the result into the list. */
        if (!strncmp(token_start, "localhost", token_len)
            || !strncmp(token_start, "localhost/", strlen("localhost/"))) {
            if ((ret = dcc_parse_localhost(curr, token_start)) != 0)
                return ret;
        } else if (has_at) {
            rs_trace("found ssh token \"%.*s\"", token_len, token_start);
            if ((ret = dcc_parse_ssh_host(curr, token_start)) != 0)
                return ret;
        } else {
            rs_trace("found tcp token \"%.*s\"", token_len, token_start);
            if ((ret = dcc_parse_tcp_host(curr, token_start)) != 0)
                return ret;
        }

        /* continue to next token if any */
        where = token_start + token_len;
        prev = curr;
        (*ret_nhosts)++;
    }
    
    if (*ret_nhosts) {
        return 0;
    } else {
        rs_log_warning("$DISTCC_HOSTS is empty; can't distribute work"); 
        return EXIT_BAD_HOSTSPEC;
    }
}

