/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org>
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
  DISTCC_HOSTS = HOSTSPEC ...
  HOSTSPEC = LOCAL_HOST | SSH_HOST | TCP_HOST | OLDSTYLE_TCP_HOST
  LOCAL_HOST = localhost[/LIMIT]
  SSH_HOST = [USER]@HOSTID[/LIMIT][:COMMAND][OPTIONS]
  TCP_HOST = HOSTID[:PORT][/LIMIT][OPTIONS]
  OLDSTYLE_TCP_HOST = HOSTID[/LIMIT][:PORT][OPTIONS]
  HOSTID = HOSTNAME | IPV4
  OPTIONS = ,OPTION[OPTIONS]
  OPTION = lzo
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


/*
       Alexandre Oliva writes

        I take this opportunity to plead people to consider such issues when
        proposing additional syntax for DISTCC_HOSTS: if it was possible to
        handle DISTCC_HOSTS as a single shell word (perhaps after turning
        blanks into say commas), without the risk of any shell active
        characters such as {, }, ~, $, quotes getting in the way, outputting
        distcc commands that override DISTCC_HOSTS would be far
        simpler.

  TODO: Perhaps entries in the host list that "look like files" (start
    with '/' or '~') should be read in as files?  This could even be
    recursive.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "hosts.h"
#include "exitcode.h"
#include "snprintf.h"

const int dcc_default_port = DISTCC_DEFAULT_PORT;


#ifndef HAVE_STRNDUP
/**
 * Copy at most @p size characters from @p src, plus a terminating nul.
 *
 * Really this needs to be in util.c, but it's only used here.
 **/
static char *strndup(const char *src, size_t size)
{
    char *dst;

    dst = malloc(size + 1);
    if (dst == NULL)
        return NULL;
    strncpy(dst, src, size);
    dst[size] = '\0';

    return dst;
}
#endif

/**
 * Get a list of hosts to use.
 *
 * Hosts are taken from DISTCC_HOSTS, if that exists.  Otherwise, they are
 * taken from $DISTCC_DIR/hosts, if that exists.  Otherwise, they are taken
 * from ${sysconfdir}/distcc/hosts, if that exists.  Otherwise, we fail.
 **/
int dcc_get_hostlist(struct dcc_hostdef **ret_list,
                     int *ret_nhosts)
{
    char *env;
    char *path, *top;
    int ret;

    if ((env = getenv("DISTCC_HOSTS")) != NULL) {
        rs_trace("read hosts from environment");
        return dcc_parse_hosts(env, "$DISTCC_HOSTS", ret_list, ret_nhosts);
    }

    /* $DISTCC_DIR or ~/.distcc */
    if ((ret = dcc_get_top_dir(&top)) == 0) {
        /* if we failed to get it, just warn */

        asprintf(&path, "%s/hosts", top);
        if (access(path, R_OK) == 0) {
            ret = dcc_parse_hosts_file(path, ret_list, ret_nhosts);
            free(path);
            return ret;
        } else {
            rs_trace("not reading %s: %s", path, strerror(errno));
            free(path);
        }
    }

    asprintf(&path, "%s/distcc/hosts", SYSCONFDIR);
    if (access(path, R_OK) == 0) {
        ret = dcc_parse_hosts_file(path, ret_list, ret_nhosts);
        free(path);
        return ret;
    } else {
        rs_trace("not reading %s: %s", path, strerror(errno));
        free(path);
    }
    
    /* FIXME: Clearer message? */
    rs_log_warning("no hostlist is set; can't distribute work");

    return EXIT_BAD_HOSTSPEC;
}


/**
 * Parse an optionally present multiplier.
 *
 * *psrc is the current parse cursor; it is advanced over what is read.
 *
 * If a multiplier is present, *psrc points to a substring starting with '/'.
 * The host defintion is updated to the numeric value following.  Otherwise
 * the hostdef is unchanged.
 **/
static int dcc_parse_multiplier(const char **psrc, struct dcc_hostdef *hostdef)
{
    const char *token = *psrc;

    if ((*psrc)[0] == '/') {
        int val;
        (*psrc)++;
        val = atoi(*psrc);
        if (val == 0) {
            rs_log_error("bad multiplier \"%s\" in host specification", token);
            return EXIT_BAD_HOSTSPEC;
        }
        while (isdigit(**psrc))
            (*psrc)++;
        hostdef->n_slots = val;
    }
    return 0;
}


/**
 * Parse an optionally present option string.
 *
 * At the moment the only option we have is "lzo" for compression, so this is
 * pretty damn simple.
 **/
static int dcc_parse_options(const char **psrc,
                             struct dcc_hostdef *host)
{
    const char *started = *psrc, *p = *psrc;

    while (p[0] == ',') {
        p++;
        if (str_startswith("lzo", p)) {
            rs_trace("got LZO option");
            host->protover = DCC_VER_2;
            host->compr = DCC_COMPRESS_LZO1X;
            p += 3;
        } else {
            rs_log_warning("unrecognized option in host specification %s",
                           started);
            return EXIT_BAD_HOSTSPEC;
        }
    }

    *psrc = p;
    
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

    if (token[0] != '@') {
        rs_log_error("expected '@' to start ssh token");
        return EXIT_BAD_HOSTSPEC;
    }

    token++;

    if ((ret = dcc_dup_part(&token, &hostdef->hostname, "/: \t\n\r\f,")) != 0)
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
        if ((ret = dcc_dup_part(&token, &hostdef->ssh_command, " \t\n\r\f,")))
            return ret;
    }
    
    if ((ret = dcc_parse_options(&token, hostdef)))
        return ret;

    hostdef->mode = DCC_MODE_SSH;
    return 0;
}


static int dcc_parse_tcp_host(struct dcc_hostdef *hostdef,
                              const char * const token_start)
{
    int ret;
    const char *token = token_start;
    
    if ((ret = dcc_dup_part(&token, &hostdef->hostname, "/: \t\n\r\f,")))
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
        char *tail;

        token++;

        hostdef->port = strtol(token, &tail, 10);
        if (*tail != '\0' && !isspace(*tail) && *tail != '/' && *tail != ',') {
            rs_log_error("invalid tcp port specification in \"%s\"", token);
            return EXIT_BAD_HOSTSPEC; 
        } else {
            token = tail;
        }
    }
        
    if ((ret = dcc_parse_multiplier(&token, hostdef)) != 0)
        return ret;

    if ((ret = dcc_parse_options(&token, hostdef)))
        return ret;

    hostdef->mode = DCC_MODE_TCP;
    return 0;
}


static int dcc_parse_localhost(struct dcc_hostdef *hostdef,
                               const char * token_start)
{
    const char *token = token_start + strlen("localhost");

    hostdef->mode = DCC_MODE_LOCAL;
    hostdef->hostname = strdup("localhost");

    /* Run only two tasks on localhost by default.
     *
     * It might be nice to run more if there are more CPUs, but determining
     * the number of CPUs on Linux is a bit expensive since it requires
     * examining mtab and /proc/stat.  Anyone lucky enough to have a >2 CPU
     * machine can specify a number in the host list.
     */
    hostdef->n_slots = 2;
    
    return dcc_parse_multiplier(&token, hostdef);
}


/**
 * @p where is the host list, taken either from the environment or file.
 *
 * @return 0 if parsed successfully; nonzero if there were any errors,
 * or if no hosts were defined.
 **/
int dcc_parse_hosts(const char *where, const char *source_name,
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
        
        if (where[0] == '\0')
            break;              /* end of string */
        
        /* skip over comments */
        if (where[0] == '#') {
            do
                where++;
            while (where[0] != '\n' && where[0] != '\r' && where[0] != '\0');
            continue;
        }

        if (isspace(where[0])) {
            where++;            /* skip space */
            continue;
        }

        token_start = where;
        token_len = strcspn(where, " #\t\n\f\r");

        /* Allocate new list item */
        curr = calloc(1, sizeof(struct dcc_hostdef));
        if (!curr) {
            rs_log_crit("failed to allocate host definition");
            return EXIT_OUT_OF_MEMORY;
        }

        /* Store verbatim hostname */
        if (!(curr->hostdef_string = strndup(token_start, (size_t) token_len))) {
            rs_log_crit("failed to allocate hostdef_string");
            return EXIT_OUT_OF_MEMORY;
        }

        /* Link into list */
        if (prev) {
            prev->next = curr;
        } else {
            *ret_list = curr;   /* first */
        }

        /* Default task limit.  A bit higher than the local limit to allow for
         * some files in transit. */
        curr->n_slots = 4;

        curr->protover = DCC_VER_1; /* default */
        curr->compr = DCC_COMPRESS_NONE;
            
        has_at = (memchr(token_start, '@', (size_t) token_len) != NULL);

        if (!strncmp(token_start, "localhost", 9)
            && (token_len == 9 || token_start[9] == '/')) {
            rs_trace("found localhost token \"%.*s\"", token_len, token_start);
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
        rs_log_warning("%s contained no hosts; can't distribute work", source_name); 
        return EXIT_BAD_HOSTSPEC;
    }
}



int dcc_free_hostdef(struct dcc_hostdef *host)
{
    /* ANSI C requires free() to accept NULL */
    
    free(host->user);
    free(host->hostname);
    free(host->ssh_command);
    free(host->hostdef_string);
    free(host->system_info);
    free(host->compiler_vers);
    memset(host, 0xf1, sizeof *host);
    free(host);

    return 0;
}
