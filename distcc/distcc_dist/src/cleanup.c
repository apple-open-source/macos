/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool 
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

        /* "I have not come through fire and death to bandy crooked
         * words with a serving-man until the lightning falls!"
         *      -- Gandalf (BBC LoTR radio play) */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "distcc.h"
#include "exitcode.h"
#include "trace.h"
#include "util.h"

/**
 * A list of files that need to be cleaned up on exit.  The fixed-size
 * array is kind of cheap and nasty, but we're never going to use that
 * many.
 *
 * Volatile because it can be read from signal handlers.
 **/
#define N_CLEANUPS 20
volatile char *cleanups[N_CLEANUPS];



/*
 * You can call this at any time, or hook it into atexit().  It is
 * safe to call repeatedly.
 *
 * If $DISTCC_SAVE_TEMPS is set to "1", then files are not actually
 * deleted, which can be good for debugging.  However, we still need
 * to remove them from the list, otherwise it will eventually overflow
 * in prefork mode.
 */
void dcc_cleanup_tempfiles(void)
{
    int i;
    int done = 0;
    int save = dcc_getenv_bool("DISTCC_SAVE_TEMPS", 0);

     /* tempus fugit */
    for (i = 0; i < N_CLEANUPS && cleanups[i]; i++) {
        if (save) {
            rs_trace("skip cleanup of %s", cleanups[i]);
        } else { 
            if (unlink((char *) cleanups[i]) == -1 && (errno != ENOENT)) {
                rs_log_notice("cleanup %s failed: %s", cleanups[i],
                              strerror(errno));
            }
            done++;
        }
        free((char *) cleanups[i]);
        cleanups[i] = NULL;
    }

    rs_trace("deleted %d temporary files", done);
}


/**
 * Add to the list of files to delete on exit.
 *
 * The string pointer must remain valid until exit.
 */
int dcc_add_cleanup(char *filename)
{
    int i;
    
    for (i = 0; cleanups[i]; i++)
        ;

    if (i >= N_CLEANUPS) {
        rs_log_crit("too many cleanups");
        return EXIT_OUT_OF_MEMORY;
    }
    cleanups[i] = filename;
    
    return 0;
}
