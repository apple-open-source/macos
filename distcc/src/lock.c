/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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


                /* Power is nothing without control
                 *      -- Pirelli tyre advertisment. */


/**
 * @file
 *
 * @brief Manage lockfiles.
 *
 * distcc uses a simple disk-based lockfile system to keep track of how many
 * jobs are queued on various machines.  These locks might be used for
 * something else in the future.
 *
 * We use locks rather than e.g. a database or a central daemon because we
 * want to make sure that the lock will be removed if the client terminates
 * unexpectedly.  
 *
 * The files themselves (as opposed to the lock on them) are never cleaned up;
 * since locking & creation is nonatomic I can't think of a clean way to do
 * it.  There shouldn't be many of them, and dead ones will be caught by the
 * tmpreaper.  In any case they're zero bytes.
 *
 * Semaphores might work well here, but the interface is a bit ugly
 * and they have a reputation for being nonportable.
 *
 * @fixme Cygwin (really, Windows) can't handle recursive file locks in the
 * same way as POSIX.  We ought to avoid ever taking the same lock twice from
 * the same process.
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/file.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "hosts.h"
#include "tempfile.h"
#include "lock.h"
#include "io.h"
#include "exitcode.h"
#include "snprintf.h"


struct dcc_hostdef _dcc_local = {
    DCC_MODE_LOCAL,
    NULL,
    (char *) "localhost",
    0,
    NULL,
    4,                          /* number of tasks */
    NULL    
};

struct dcc_hostdef *dcc_hostdef_local = &_dcc_local;


static char * dcc_make_lock_filename(const char *lockname,
                                     const struct dcc_hostdef *host,
                                     int iter)
{
    char * buf;
    const char *tempdir;

    tempdir = dcc_get_tempdir();

    if (host->mode == DCC_MODE_LOCAL) {
        asprintf(&buf, "%s/lock_%s_localhost_%d", tempdir, lockname, iter);
    } else if (host->mode == DCC_MODE_TCP) {
        asprintf(&buf, "%s/lock_%s_tcp_%s_%d_%d", tempdir, lockname, host->hostname,
                 host->port, iter);
    } else if (host->mode == DCC_MODE_SSH) {
        asprintf(&buf, "%s/lock_%s_ssh_%s_%d", tempdir, lockname, host->hostname, iter);
    } else {
        rs_fatal("oops");
    }

    if (!buf)
        rs_fatal("failed to allocate string");

    return buf;
}


/**
 * Get an exclusive, non-blocking lock on a file using whatever method
 * is available on this system.
 *
 * @retval 0 if we got the lock
 * @retval -1 with errno set if the file is already locked.
 **/
int sys_lock(int fd, int block)
{
#if defined(F_SETLK) && ! defined(DARWIN)
    struct flock lockparam;

    lockparam.l_type = F_WRLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start = 0;
    lockparam.l_len = 0;        /* whole file */
    
    return fcntl(fd, block ? F_SETLKW : F_SETLK, &lockparam);
#elif defined(HAVE_FLOCK)
    return flock(fd, LOCK_EX | (block ? 0 : LOCK_NB));
#elif defined(HAVE_LOCKF)
    return lockf(fd, block ? F_LOCK : F_TLOCK, 0);
#else
#  error "No supported lock method.  Please port this code."
#endif
}



int dcc_unlock(int lock_fd)
{
    rs_trace("release lock");
    /* All our current locks can just be closed */
    if (close(lock_fd)) {
        rs_log_error("close failed: %s", strerror(errno));
        return EXIT_IO_ERROR;
    }
    return 0;
}


/**
 * Open a lockfile, creating if it does not exist.
 **/
static int dcc_open_lockfile(const char *fname, int *plockfd)
{
    /* Create if it doesn't exist.  We don't actually do anything with
     * the file except lock it.*/
    *plockfd = open(fname, O_WRONLY|O_CREAT, 0600);
    if (*plockfd == -1 && errno != EEXIST) {
        rs_log_error("failed to creat %s: %s", fname, strerror(errno));
        return EXIT_IO_ERROR;
    }

    return 0;
}


/**
 * Lock a server slot, in either blocking or nonblocking mode.
 *
 * In blocking mode, this function will not return until either the lock has
 * been acquired, or an error occured.  In nonblocking mode, it will instead
 * return EXIT_BUSY if some other process has this slot locked.
 *
 * @param slot 0-based index of available slots on this host.
 * @param block True for blocking mode.
 *
 * @param lock_fd On return, contains the lock file descriptor to allow
 * it to be closed.
 **/
int dcc_lock_host(const char *lockname,
                  const struct dcc_hostdef *host,
                  int slot, int block,
                  int *lock_fd)
{
    char *fname;
    int ret;

    fname = dcc_make_lock_filename(lockname, host, slot);

    if ((ret = dcc_open_lockfile(fname, lock_fd)) != 0) {
        free(fname);
        return ret;
    }        

    if (sys_lock(*lock_fd, block) == 0) {
        /* TODO: Print better readable form of hostdef. */
        rs_trace("got %s lock on %s slot %d", lockname, host->hostname, slot);
        free(fname);
        return 0;
    } else {
        switch (errno) {
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
        case EAGAIN:
        case EACCES: /* HP-UX gives this for exclusion */
            rs_trace("%s is busy", fname);
            ret = EXIT_BUSY;
            break;
        default:
            rs_log_error("lock %s failed: %s", fname, strerror(errno));
            ret = EXIT_IO_ERROR;
            break;
        }

        dcc_close(*lock_fd);
        free(fname);
        return ret;
    }
}
