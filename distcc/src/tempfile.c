/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
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


                /* "More computing sins are committed in the name of
                 * efficiency (without necessarily achieving it) than
                 * for any other single reason - including blind
                 * stupidity."  -- W.A. Wulf
                 */



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "tempfile.h"
#include "snprintf.h"


/**
 * @file
 *
 * Routines for naming, generating and removing temporary files and
 * fifos.
 *
 * All our temporary files are held in a single directory owned by the
 * user running the program.  I think this avoids any security or
 * symlink problems, and it keeps things kind of tidy.  I'm not
 * completely sure, though.
 *
 * It's fine for the files in that directory to be removed by the
 * tmpreaper, or indeed by hand if you're sure they're no longer
 * active.
 *
 * We also support generation of fifos (aka named pipes), which are
 * used for feeding data from the preprocessor and to the compiler.
 **/


/** A subdirectory of tmp to hold all our stuff. **/
static char *tempdir;

/**
 * A list of files that need to be cleaned up on exit.  The fixed-size
 * array is kind of cheap and nasty, but we're never going to use that
 * many.
 **/
#define N_CLEANUPS 20
char *cleanups[N_CLEANUPS];

static const char *dcc_get_tmp_top(void)
{
    const char *d;

    d = getenv("TMPDIR");
    /* some sanity checks */
    if (!d || d[0] == '\0' || d[0] != '/' || d[1] == '\0') {
        return "/tmp";
    } else {
        return d;
    }
}



/**
 * Create a new fifo node, replacing whatever previously had the name.
 * The fifo is not opened.
 *
 * The fifo is opened without blocking to wait for somebody to start
 * reading; however the file descriptor is immediately set back to
 * blocking mode, so the first write to it will block.
 *
 * @param mode Permission bits for new fifo.
 *
 * @returns 0 for OK, or -1 for error.
 **/
int dcc_mkfifo(const char *fifo_name)
{
    /* Just try to unlink once; don't worry if it doesn't exist.  This is to
     * handle the occasional temporary file which might get left behind from a
     * previous invocation with the same pid. */
    
    if (dcc_remove_if_exists(fifo_name))
        return -1;

    if (mkfifo(fifo_name, S_IRUSR|S_IWUSR) == -1) {
        rs_log_warning("failed to make fifo %s: %s", fifo_name,
                       strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * Create a temporary directory used for all our files.  May be shared
 * by multiple instances.
 **/
int dcc_setup_tempdir(void)
{
    struct stat buf;
    const char *tmp;
    int need_len;

    tmp = dcc_get_tmp_top();

    /* size includes trailing nul */
    need_len = strlen(tmp) + strlen("/distcc_") + 8 + 1;
    tempdir = malloc(need_len);
    if (snprintf(tempdir, need_len, "%s/distcc_%08x", tmp, (int) getuid())
        != need_len - 1) {
        rs_fatal("tempdir too small??");
    }
    
    if (mkdir(tempdir, 0755) == 0) {
        return 0;               /* great */
    } else if (errno == EEXIST) {
        /* If there's already a symbolic link of this name, then we
         * will have just failed with EEXIST.  We need to make sure
         * that if there was an existing file, then it is actually a
         * directory. */
        if (lstat(tempdir, &buf) == -1) {
            rs_log_error("lstat %s failed: %s", tempdir, strerror(errno));
            return -1;            
        } else if (!S_ISDIR(buf.st_mode)) {
            rs_log_error("%s is not a directory", tempdir);
            return -1;
        } else if (buf.st_mode & (S_IWGRP | S_IWOTH)) {
            rs_log_error("permissions on %s (%#o) seem insecure", tempdir,
                         (int) buf.st_mode);
            return -1;
        } else {
            return 0;
        }
    } else {
        rs_log_error("mkdir %s failed: %s", tempdir, strerror(errno));
        return -1;
    }
}


char *dcc_make_tmpnam(const char *prefix, const char *suffix)
{
    char *s;
    int i;
    int need_len;

    if (!tempdir)
        dcc_setup_tempdir();

    for (i = 0; cleanups[i]; i++)
        ;

    assert(i < N_CLEANUPS);

    /* NOTE: Make sure this lines up with the printf statement.  C sucks. */
    need_len = strlen(tempdir) + 1 + strlen(prefix) + 1 + 10 
        + strlen(suffix) + 1;

    /* PID field is 10 digits, enough to fit the highest number that
     * can be represented in a 32-bit pid_t. */
    
    if ((s = malloc(need_len)) == NULL) {
        rs_fatal("failed to allocate %d", need_len);
        /* ABORTS */
    }

    if ((snprintf(s, need_len, "%s/%s_%010ld%s", tempdir, prefix,
                  (long) getpid() & 0xffffffffUL, suffix)) != need_len - 1) {
        rs_fatal("string length was wrong??");
        /* ABORTS */
    }

    cleanups[i] = s;
    
    return s;
}


const char *dcc_get_tempdir(void)
{
    if (!tempdir)
        dcc_setup_tempdir();
    return tempdir;
}


/**
 * You can call this at any time, or hook it into atexit()
 *
 * If $DISTCC_SAVE_TEMPS is set to "1", then files are not actually
 * deleted -- good for debugging.
 **/
void dcc_cleanup_tempfiles(void)
{
    int i;
    int done = 0;

    if (dcc_getenv_bool("DISTCC_SAVE_TEMPS", 0)) /* tempus fugit */
        return;

    for (i = 0; i < N_CLEANUPS && cleanups[i]; i++) {
        if (unlink(cleanups[i]) == -1 && (errno != ENOENT)) {
            rs_log_notice("cleanup %s failed: %s", cleanups[i],
                          strerror(errno));
        }
        done++;
        free(cleanups[i]);
        cleanups[i] = NULL;
    }

    rs_trace("deleted %d temporary files", done);
}
