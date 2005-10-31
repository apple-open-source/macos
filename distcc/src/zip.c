/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 * $Header: /cvs/karma/distcc/src/zip.c,v 1.1.1.1 2005/05/06 05:09:42 deatley Exp $ 
 *
 * Copyright (C) 2002 by Martin Pool <mbp@samba.org>
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


		      /* Perfection is acheived only on the point of collapse.
                       *                         -- C. N. Parkinson
                       */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <zlib.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "rpc.h"
#include "exitcode.h"

/**
 * @file
 *
 * Support for zlib compression in distcc.
 *
 * Use of compression is enabled by a special "GZIP" token in the
 * protocol stream; after that all file bodies are transferred with
 * compression rather than through the usual means.
 *
 * As for the uncompressed case, files are preceeded by an identifying
 * token (e.g. "DOTI"), and then their length.  The length is the wire
 * (i.e. compressed) length.
 *
 * Transmitting the length first keeps the read process fairly simple,
 * but it means that we must do all compression in memory first before
 * beginning to send the file.
 *
 * zlib.h says that to do all compression in a single step, the output
 * buffer must be at least 0.1% larger than avail_in plus 12 bytes.
 * In this case, we expect to see Z_FINISH set on the first call to
 * deflate().
 *
 * These methods more or less parallel those in bulk.c.
 **/


/**
 * Send a file, compressed.
 **/
int dcc_x_file_gz(int ofd, const char *fname, const char *token,
                  size_t *size_out)
{
    z_stream strm;

    /* use default allocator */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    deflateInit(&strm, 3);
    deflate(Z_FINISH);
    deflateEnd();
}
