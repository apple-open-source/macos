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

                /* "A new contraption to capture a dandelion in one
                 * piece has been put together by the crew."
                 *      -- Boards of Canada, "Geogaddi" */


/**
 * @file
 *
 * Bulk file transfer, used for sending .i, .o files etc.
 *
 * Files are always sent in the standard IO format: stream name,
 * length, bytes.  This implies that we can deliver to a fifo (just
 * keep writing), but we can't send from a fifo, because we wouldn't
 * know how many bytes were coming.
 *
 * @todo Optionally support lzo compression
 **/ 

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/time.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "rpc.h"
#include "bulk.h"
#include "time.h"

#include "timeval.h"

#ifndef O_BINARY
#  define O_BINARY 0
#endif


/**
 * Open a file for read, and also put its size into @p fsize.
 *
 * If the file does not exist, then returns 0, but @p ifd is -1 and @p
 * fsize is zero.  If @p fsize is zero, the caller should not try to
 * read from the file.
 *
 * This strange behaviour for ENOENT is useful because if there is
 * e.g. no output file from the compiler, we don't want to abort, but
 * rather just send nothing.  The receiver has the corresponding
 * behaviour of not creating zero-length files.
 *
 * Using fstat() helps avoid a race condition -- not a security issue,
 * but possibly a failure.  Shouldn't be very likely though.
 *
 * The caller is responsible for closing @p ifd.
 **/
int dcc_open_read(const char *fname, int *ifd, off_t *fsize)
{
    struct stat buf;
    
    *ifd = open(fname, O_RDONLY|O_BINARY);
    if (*ifd == -1) {
        int save_errno = errno;
        if (save_errno == ENOENT) {
            /* that's OK, just assume it's empty */
            *fsize = 0;
            return 0;
        } else {
            rs_log_error("failed to open %s: %s", fname, strerror(save_errno));
            return -1;
        }
    }

    if (fstat(*ifd, &buf) == -1) {
	rs_log_error("fstat %s failed: %s", fname, strerror(errno));
        close(*ifd);
        return -1;
    }

    *fsize = buf.st_size;

    return 0;
}


static void dcc_calc_rate(size_t size_out,
                          struct timeval *before,
                          struct timeval *after,
                          double *secs,
                          double *rate)
{
    struct timeval delta;
    
    timeval_subtract(&delta, after, before);

    *secs = (double) delta.tv_sec + (double) delta.tv_usec / 1e6;

    *rate = ((double) size_out / *secs) / 1024.0;
}


/**
 * Transmit a file and print timing statistics.  Only used for big files.
 *
 * Wrapper around dcc_x_file(). 
 **/
int dcc_x_file_timed(int ofd, const char *fname, const char *token,
                     size_t *size_out)
{
    struct timeval before, after;
    int ret;
    size_t size;

    if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    ret = dcc_x_file(ofd, fname, token, &size);
    if (size_out)
        *size_out = size;           /* separate to allow for size_out NULL */

    if (gettimeofday(&after, NULL)) {
        rs_log_warning("gettimeofday failed");
    } else {
        double secs, rate;
        
        dcc_calc_rate(size, &before, &after, &secs, &rate);
        rs_log_info("%ld bytes sent in %.3fs, rate %.0fkB/s",
                    (long) size, secs, rate);
    }

    return ret;
}


/**
 * Transmit from a local file to the network.
 *
 * @param ofd File descriptor for the network connection.
 * @param fname Name of the file to send.
 * @param token Token for this file, e.g. "DOTO".
 *
 * @param size_out If non-NULL, set on return to the number of bytes
 * transmitted.
 **/
int dcc_x_file(int ofd, const char *fname, const char *token,
               size_t *size_out)
{
    int ifd;
    off_t f_size; 

    if (dcc_open_read(fname, &ifd, &f_size))
        return -1;

    rs_trace("send %ld byte file %s with token %s",
             (long) f_size, fname, token);

    if (size_out)
        *size_out = (size_t) f_size;

    if (dcc_x_token_int(ofd, token, f_size))
        goto failed;

    if (f_size) {
#ifdef HAVE_SENDFILE
        if (dcc_pump_sendfile(ofd, ifd, (size_t) f_size))
            goto failed;
#else
        if (dcc_pump_readwrite(ofd, ifd, (size_t) f_size))
            goto failed;
#endif
    }

    if (ifd != -1)
        close(ifd);
    return 0;

  failed:
    if (ifd != -1)
        close(ifd);
    return -1;
}


/**
 * Receive a file stream from the network into a local file.  
 *
 * This is only used for receipt into a regular file.  The file is
 * always removed before creation.
 *
 * @param filename local filename to create.  
 **/
int dcc_r_file(int ifd, const char *filename, size_t len)
{
    int ofd;
    int ret, close_ret;

    if (unlink(filename) && errno != ENOENT) {
        rs_log_error("failed to remove %s: %s", filename, strerror(errno));
        return -1;
    }

    ofd = open(filename, O_TRUNC|O_WRONLY|O_CREAT|O_EXCL|O_BINARY, 0666);
    if (ofd == -1) {
        rs_log_error("failed to create %s: %s", filename, strerror(errno));
        return -1;
    }

    ret = dcc_r_file_body(ofd, ifd, len);
    close_ret = dcc_close(ofd);

    if (!ret && !close_ret) {
        rs_log_info("received %d bytes to file %s", len, filename);
        return 0;
    }

    rs_trace("failed to receive %s, removing it", filename);
    if (unlink(filename)) {
        rs_log_error("failed to unlink %s after failed transfer: %s",
                     filename, strerror(errno));
    }
    return -1;
}


/**
 * Common routine for reading a file body used by both fifos and
 * regular files.
 *
 * The output file descriptor is not closed, because this routine is
 * also used for copying onto stderr.
 **/
int dcc_r_file_body(int ofd, int ifd, size_t len)
{
    return dcc_pump_readwrite(ofd, ifd, (size_t) len);
}


/**
 * Receive a file and print timing statistics.  Only used for big files.
 *
 * Wrapper around dcc_r_file(). 
 **/
int dcc_r_file_timed(int ifd, const char *fname, size_t size)
{
    struct timeval before, after;
    int ret;

    if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    ret = dcc_r_file(ifd, fname, size);

    if (gettimeofday(&after, NULL)) {
        rs_log_warning("gettimeofday failed");
    } else {
        double secs, rate;
        
        dcc_calc_rate(size, &before, &after, &secs, &rate);
        rs_log_info("%ld bytes received in %.3fs, rate %.0fkB/s",
                    (long) size, secs, rate);
    }

    return ret;
}
