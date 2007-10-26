/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
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


/**
 * @file
 *
 * Client functions for file indirection feature.
 **/


#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "distcc.h"
#include "bulk.h"
#include "indirect_client.h"
#include "indirect_util.h"
#include "rpc.h"
#include "trace.h"


/* 
 Handle a pull request.
 
 In response, we send a file size to transfer.
 If the file size is zero it indicates we have nothing to send. If the 
 */
static int dcc_handle_remote_indirection_pull(int ifd)
{
    unsigned path_length;
    char path[MAXPATHLEN];
    int result = 0;
    unsigned file_stat_info_flag, pull_response;
    off_t file_size, send_size;
    struct timespec mod_time;
    struct stat st_pullfile;
    
    /*
     Fetch the pull info from the build machine.
     It will start by sending us a path length, and the path.
     */     
    if ((result = dcc_r_token_int(ifd, indirection_path_length_token, &path_length)) != 0) {
        rs_log_error("unable to fetch indirection path length");
    }
    
    if (result == 0) {
        if ((result = dcc_readx(ifd, path, path_length)) != 0)
            rs_log_error("unable to fetch indirection path");
        path[path_length] = 0;
    }
    
    if (result == 0) {
        if ((result = dcc_r_token_int(ifd, indirection_file_stat_token, &file_stat_info_flag)) != 0) {
            rs_log_error("unable to fetch indirection file stat token");
        } else {
            if (file_stat_info_flag == indirection_file_stat_info_present) {
                // build machine has a cached file and will send us the mod time
                if (result == 0) result = dcc_readx(ifd, &file_size, sizeof(file_size));
                if (result == 0) result = dcc_readx(ifd, &mod_time, sizeof(mod_time));
                if (result != 0) rs_log_error("unable to fetch indirection file stat info");
            }
        }
    }
        
    /* validate the size and mod time of the file against the local filesystem. */
    if (result == 0) {
        if (stat(path, &st_pullfile) != 0) {
            result = -1;
            pull_response = indirection_pull_response_file_missing;
            rs_log_warning("Unable to send pull file: %s - %s", path, strerror(errno));
        } else {
            if (file_stat_info_flag == indirection_file_stat_info_present && st_pullfile.st_size == file_size && st_pullfile.st_mtimespec.tv_sec == mod_time.tv_sec && st_pullfile.st_mtimespec.tv_nsec == mod_time.tv_nsec) {
                pull_response = indirection_pull_response_file_ok;
                rs_log_info("using cached pull file");
            } else {
                if (file_stat_info_flag == indirection_file_stat_info_present) {
                    rs_log_info("pull file changed, sending");
                    rs_log_info("my size = %d, my seconds = %d, my nsec = %d\nhis size = %d, his seconds = %d, his nsec = %d", (int)st_pullfile.st_size, st_pullfile.st_mtimespec.tv_sec, st_pullfile.st_mtimespec.tv_nsec, (int)file_size, mod_time.tv_sec, mod_time.tv_nsec);
                } else {
                    rs_log_info("pull file missing, sending");
                }
                pull_response = indirection_pull_response_file_download;
            }
        }
        if ((result = dcc_x_token_int(ifd, indirection_pull_response_token, pull_response)) != 0) {
            rs_log_error("unable to send pull response code");
        } else {
            if (pull_response == indirection_pull_response_file_download) {
                if (dcc_x_file(ifd, path, indirection_pull_file, DCC_COMPRESS_LZO1X, &send_size) ||
                    dcc_writex(ifd, &st_pullfile.st_mtimespec, sizeof(st_pullfile.st_mtimespec))) {
                    rs_log_error("failure sending pull file");
                }
            }
        }
    } else {
        // we failed somewhere reading the request
    }
    return result;
}


/**
 * Invoked by the client executable when a token received from the server
 * indicates that there may be a remote indirection request.
 * ifd specifies the file descriptor for the socket the client
 * and server are using to communicate.  operation is an integer
 * value indicating the operation requested by the server.
 **/
int dcc_handle_remote_indirection_request(int ifd, int operation)
{
    int result;
    if ( operation == indirection_request_pull ) {
        result = dcc_handle_remote_indirection_pull(ifd);
    } else {
        rs_log_error("Unsupported indirection operation: %d", operation);
        result = -1;
    }
    return result;
}
