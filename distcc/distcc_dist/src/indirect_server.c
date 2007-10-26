/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003, 2005 by Apple Computer, Inc.
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
 * Server functions for file indirection feature.
 **/

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/dirent.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <fts.h>
#include "distcc.h"
#include "bulk.h"
#include "config.h"
#include "exitcode.h"
#include "exec.h"
#include "indirect_util.h"
#include "rpc.h"
#include "trace.h"


static const char message_terminator = '\n';
static const char *pch_cache_dir = "pch_cache";
unsigned pullfile_cache_max_age = 36;
unsigned pullfile_max_cache_size = 0;
unsigned pullfile_min_free_space = 2048;

/**
 * Receive messages from a child process (typically GCC 3.3 and later).
 * These messages provide information from the child regarding file
 * indirection.
 * The message includes all bytes on the pipe up to the first message
 * terminator (newline) encountered.
 * Converts the message terminator into a string terminator.
 * Stores the message in buffer.
 *
 * Return 1 on success and 0 on failure.
 * Fails if the message is longer than size
 **/
static int read_from_child(dcc_indirection *indirect, char *buffer, const int size)
{
    int pos = 0;
    for (;;) {
        if (indirect->read_buf_pos >= indirect->read_buf_used) {
            indirect->read_buf_used = read(indirect->childWrite[0], &indirect->read_buf, INDIRECT_READ_BUFSZ);
            indirect->read_buf_pos = 0;
        }
        if (indirect->read_buf_used <= 0) {
            return 0;
        }
        while (pos < size && indirect->read_buf_pos < indirect->read_buf_used) {
            if (indirect->read_buf[indirect->read_buf_pos] == '\n') {
                buffer[pos] = 0;
                indirect->read_buf_pos++;
                return 1;
            } else {
                buffer[pos++] = indirect->read_buf[indirect->read_buf_pos++];
            }
        }
    }
}

/**
 * Write message to a child process (typically GCC 3.3 and later).
 * These messages provide information to the child regarding file
 * indirection.
 * The message includes all bytes up to the string terminator.
 * Converts the string terminator into a message terminator (newline).
 * Writes a single message terminator alone if message is
 * NULL.
 *
 * Return 1 on success and 0 on failure.
 **/
static int write_to_child(dcc_indirection *indirect, const char *message)
{
    int result;

    if ( message ) {
        const int length = strlen(message);
        int idx = 0;

        while ( idx < length ) {
            result = write(indirect->parentWrite[1], &message[idx], length - idx);

            if ( result < 0 ) {
                return 0;
            } else {
                idx += result;
            }
        }
    }

    result = write(indirect->parentWrite[1], &message_terminator, 1);

    if ( result < 0 ) {
        return 0;
    }

    return 1;
}

static int lock_pullfile()
{
    static char *pull_lock = NULL;
    if (pull_lock == NULL) {
        if (dcc_make_tmpfile_path(&pull_lock, 1, "pull_lockfile", NULL)) {
            return -1;
        }
    }
    return open(pull_lock, O_WRONLY|O_CREAT|O_EXLOCK, 0777);
}

typedef struct _CachedPullfile {
    char *path;
    time_t accessTime;
    off_t size; // size in kb
    struct _CachedPullfile *next;
} CachedPullfile;

/*
 This function walks the pullfile cache directory and builds up lists of all the files it contains.
 The files are sorted into an array of linked lists (cache_lists), where each list contains the files
 whose access times are in that age group. cache_lists[0] through cache_lists[5] contain a list of files
 that were accessed within the last hour, in 10 minute intervals. The remaining cache_list entries have a
 granularity of hours, so cache_lists[6] contains files last accessed between 1 and 2 hours ago, cache_lists[7]
 contains files last accesssed between 2 and 3 hours ago, etc. The last cache_list entry contains all
 files whose access times do not fall in any of the earlier buckets.
 If there are no files in a certain age period then the cache_lists entry
 will be NULL.
 Additionally, the total size of all cached files (in kb) is returned in total_file_size.
 */
static void dcc_all_cached_files(char *root, CachedPullfile **cache_lists, int cache_lists_size, long *total_file_size)
{
    CachedPullfile *result = NULL;
    char *cache_root[2];
    time_t current_time = time(NULL), age;
    cache_root[0] = root;
    cache_root[1] = NULL;
    *total_file_size = 0;
    bzero(cache_lists, cache_lists_size * sizeof(CachedPullfile *));
    FTS *fts_handle = fts_open(cache_root, FTS_NOCHDIR, NULL);
    if (fts_handle != NULL) {
        FTSENT *file;
        int entry_index;
        do {
            file = fts_read(fts_handle);
            if ((file != NULL) && (file->fts_info == FTS_F)) {
                CachedPullfile *newNode = (CachedPullfile *)malloc(sizeof(CachedPullfile));
                newNode->path = (char *)malloc(file->fts_pathlen + 1);
                bcopy(file->fts_path, newNode->path, file->fts_pathlen);
                newNode->path[file->fts_pathlen] = 0;
                newNode->accessTime = file->fts_statp->st_mtimespec.tv_sec;
                newNode->size = (file->fts_statp->st_size + 1023) / 1024;
                // Figure out what cache_lists bucket to put the file into.
                age = current_time - file->fts_statp->st_atimespec.tv_sec;
                if (age > 60 * 60) {
                    // it's at least one hour old so calculate time in hours starting with bucket 6
                    entry_index = 5 + age / (60 * 60);
                } else {
                    // it's less than one hour old so calculate time in 10 minute intervals starting with bucket 0
                    entry_index = age / (10 * 60);
                }
                if (entry_index >= cache_lists_size)
                    entry_index = cache_lists_size - 1;
                newNode->next = cache_lists[entry_index];
                cache_lists[entry_index] = newNode;
                *total_file_size += newNode->size;
            }
        } while (file != NULL);
        fts_close(fts_handle);
    }
}

static long dcc_free_cached_files(CachedPullfile **cache_lists, int cache_lists_size, int delete_files)
{
    CachedPullfile *next;
    int i;
    long removed_size = 0;
    for (i=0; i<cache_lists_size; i++) {
        while (cache_lists[i] != NULL) {
            if (delete_files) {
                rs_log_info("removing cached pull file: %s", cache_lists[i]->path);
                unlink(cache_lists[i]->path);
                removed_size += cache_lists[i]->size;
            }
            next = cache_lists[i]->next;
            free(cache_lists[i]->path);
            free(cache_lists[i]);
            cache_lists[i] = next;
        }
    }
    return removed_size;
}

static void dcc_emit_cache_warning(char *fmt)
{
    char hostbuf[_POSIX_HOST_NAME_MAX+1], *host;
    if (gethostname(hostbuf, _POSIX_HOST_NAME_MAX+1)) {
        host = "UNKNOWN";
        rs_log_warning("Failed to get host name: %s", strerror(errno));
    } else {
        host = hostbuf;
    }
    rs_log_warning(fmt, host);
}    

/*
 Traverses the indirection file cache and removes cached files.
 max_age - any files not accessed withing max_age hours are removed. max_age has a hardcoded maximum constraint (currently 72), and zero is interpreted as max.
 max_used - files are removed until the total size occupied is max_used or less (Mb). Zero is interpreted as no restriction on total cache size.
 min_free - files are removed until at least min_free (Mb) is available on the volume. min_free has a hardcoded minimum (currently 512).
 */
int dcc_prune_indirection_cache(unsigned max_age, unsigned max_used, unsigned min_free)
{
    int result = 0, pullfile_lock, age, done, cached_files_size;
    long total_cache_size, initial_total_size; // size in kb
    struct statfs fs_stat;
    char *cache_root;
    
    if (max_age == 0 || max_age > 72)
        max_age = 72;
    if (min_free < 512)
        min_free = 512;
    cached_files_size = max_age + 5;
    CachedPullfile *cached_files[cached_files_size];
    
    /*
     The strategy is to build a list of files in the cache up front.
     Then we traverse the list and remove any files that exceed the age limit.
     If we have then satisfied the max_used and min_free requirements then we are done.
     If we have not then we reduce the max_age and traverse the list again.
     */
    if (dcc_make_tmpfile_path(&cache_root, 0, NULL, pch_cache_dir, NULL) == 0) {
        dcc_all_cached_files(cache_root, cached_files, cached_files_size, &total_cache_size);
        initial_total_size = total_cache_size;
        age = cached_files_size - 1;

        do {
            /* removed the next least recently accessed chunk of files */
            /* note that the first time through the loop we remove all the files that are too old */
            /* subsequent passes continue removing files until the space requirements are satisfied */
            total_cache_size -= dcc_free_cached_files(&cached_files[age], 1, 1);
            
            /* if we still exceed the max size there is no reason to do the statfs */
            if (max_used == 0 || (total_cache_size + 1023) / 1024 < max_used) {

                /* Check the free space on the filesystem. */
                if (statfs(cache_root, &fs_stat) == 0) {
		     if (fs_stat.f_bavail < 1024 * 1024 / fs_stat.f_bsize * min_free) {
                        /* Insufficient free space. If will wind up removing all the files emit a warning. */
                        if (age == 1)
                            dcc_emit_cache_warning("Emptied distcc pch cache on %s due to insufficient free disk space. Build times may be slower.");
                            done = 0;
                    } else {
                        done = 1;
                    }
                } else {
                    rs_log_warning("(dcc_all_cached_files) failed to get free space: %s", strerror(errno));
                    done = 1;
                }
            } else {
                /* Still too much cached data. If we will wind up removing all the files emit a warning */
                if (age == 1)
                    dcc_emit_cache_warning("Exceeded distcc pch cache size on %s within a short time interval. Cache size may be too small for the client load. Build times may be slower.");
                done = 0;
            }
            age--;
        } while (age >= 0 && !done);
        free(cache_root);
        dcc_free_cached_files(cached_files, max_age, 0);
    } else {
        rs_log_warning("(dcc_all_cached_files) failed to construct cache path");
    }
    return result;
}

/*  Ensure that the available space on the PCH cache volume is greater than pullfile_min_free_space
 *  (settable from the command line with --min-disk-free).  If the available space is below the threshold,
 *  prune the PCH cache and re-check the space, returning failure if pruning didn't free enough space.  */
int dcc_ensure_free_space()
{ 
	char *cache_root;	    // the pch cache root directory.
	struct statfs fs_stat;  // filesystem stats for the cache volume.

	/* The pch cache directory isn't created until the first indirection request,
	 * so if we can't access it we still return success.  */
	if (dcc_make_tmpfile_path(&cache_root, 0, NULL, pch_cache_dir, NULL) != 0) 
		return 0;
	if (access(cache_root, R_OK) != 0) 
		return 0;
	
	/* If the pch cache directory exists, but we can't stat it, something is wrong.
	 * Return failure to dcc_service_job, so the job is recompiled locally on the 
	 * recruiter machine.  */
	if (statfs(cache_root, &fs_stat) != 0) { 
		dcc_emit_cache_warning("dcc_cache_free_mb: unable to determine free space for cache");
		return -1;
	}	

	/* The pch cache dir exists.  If the free space on the volume is greater than
     * pullfile_min_free_space (--min-disk-free), return success.  */
	if (fs_stat.f_bavail > 1024 * 1024 / fs_stat.f_bsize * pullfile_min_free_space)
		return 0;
	
	/* We're below the minimum free  space on the cache volume.  Prune the pch cache
	 * directory  */
	int pull_lockfd = lock_pullfile();
	dcc_prune_indirection_cache(0, 0, pullfile_min_free_space);
    if (pull_lockfd != -1)
        close(pull_lockfd);
	/* Something bad happened to the cache volume during the prune, return failure to
	 * and refuse the job  */
	if (statfs(cache_root, &fs_stat) != 0) { 
		dcc_emit_cache_warning("dcc_cache_free_mb: unable to determine free space for cache");
		return -1;
	}
	/* return (available space > minimum)  */
	return (fs_stat.f_bavail > 1024 * 1024 / fs_stat.f_bsize * pullfile_min_free_space);
}


static int use_pullfile(char *pullfile, char **path_in_use)
{
    char file[64];
    sprintf(file, "pch_%d", getpid());
    if (dcc_make_tmpfile_path(path_in_use, 1, file, NULL))
        return -1;
    if (dcc_add_cleanup(*path_in_use)) {
        rs_log_warning("Unable to add cleanup file: %s", *path_in_use);
    }
    unlink(*path_in_use);
    return link(pullfile, *path_in_use);
}


/**
 * Handles a pull operation, as specified by the server's child (typically
 * gcc 3.3).  Pulled files are stored in a per-host cache.
 **/
static int dcc_handle_pull(dcc_indirection *indirect)
{
    char  *indirection_path     = NULL;
    size_t hostname_length      = strlen(indirect->hostname) + 1;
    char   pullfile[MAXPATHLEN];
    int result = 0;
    unsigned has_local_file = 0, pullfile_len, pull_response;
    struct stat st_pch;
    char *pch_cache_filename = NULL, *file_part_separator, *actual_path;
    
    int pull_lockfd = lock_pullfile();
    
    /* Fetch the path and construct the corresponding path in the file cache. */
    if ( read_from_child(indirect, pullfile, MAXPATHLEN) ) {
        file_part_separator = strrchr(pullfile, '/');
        if (file_part_separator) {
            // we temporarily strip the filename off of the path
            *file_part_separator = 0;
            pch_cache_filename = NULL;
            result = dcc_make_tmpfile_path(&pch_cache_filename, 0, &file_part_separator[1], pch_cache_dir, indirect->hostname, pullfile, NULL);
        } else {
            rs_log_error("Unable to resolve indirection request for relative path.");
            result = -1;
        }
    } else {
        result = -1;
        rs_log_error("Unable to receive pull request");
    }
    
    /* Check if the cached path exists. If not, construct the directory tree it will live in. */
    if (result == 0) {
        if (stat(pch_cache_filename, &st_pch) != 0) {
            if (errno == ENOENT) {
                if (pch_cache_filename) {
                    free(pch_cache_filename);
                    pch_cache_filename = NULL;
                }
                result = dcc_make_tmpfile_path(&pch_cache_filename, 1, &file_part_separator[1], pch_cache_dir, indirect->hostname, pullfile, NULL);
            } else {
                result = -1;
            }
        } else {
            has_local_file = 1;
        }
    }
    
    /* At this point we send the query to the client */
    if (result == 0) {
        // restore the filename
        *file_part_separator = '/';
        pullfile_len = strlen(pullfile);
        if (dcc_x_token_int(indirect->out_fd, indirection_request_token, indirection_request_pull) ||
            dcc_x_token_int(indirect->out_fd, indirection_path_length_token, pullfile_len) ||
            dcc_writex(indirect->out_fd, pullfile, pullfile_len)) {
            result = -1;
        } else {
            /* if we have a file cached locally send the stat info, otherwise send the flag that we don't have a file */
            if (has_local_file) {
                if (result == 0) result = dcc_x_token_int(indirect->out_fd, indirection_file_stat_token, indirection_file_stat_info_present);
                if (result == 0) result = dcc_writex(indirect->out_fd, &st_pch.st_size, sizeof(st_pch.st_size));
                if (result == 0) result = dcc_writex(indirect->out_fd, &st_pch.st_mtimespec, sizeof(st_pch.st_mtimespec));
            } else {
                if (dcc_x_token_int(indirect->out_fd, indirection_file_stat_token, indirection_no_file_stat_info))
                    result = -1;
            }
        }
    }
    
    /* read the client's response */
    if (result == 0) {
        if (dcc_r_token_int(indirect->out_fd, indirection_pull_response_token, &pull_response)) {
            result = -1;
        } else {
            struct timespec mod_time;
            struct timeval mod_timeval[2];
            
            switch (pull_response) {
                case indirection_pull_response_file_ok:
                    break;
                case indirection_pull_response_file_download:
                    if (has_local_file)
                        unlink(pch_cache_filename);
                    result = dcc_r_token_file(indirect->out_fd, indirection_pull_file, pch_cache_filename, DCC_COMPRESS_LZO1X);
                    if (result == 0) {
                        result = dcc_readx(indirect->out_fd, &mod_time, sizeof(mod_time));
                        if (result == 0) {
                            mod_timeval[0].tv_sec = time(NULL);
                            mod_timeval[0].tv_usec = 0;
                            mod_timeval[1].tv_sec = mod_time.tv_sec;
                            mod_timeval[1].tv_usec = mod_time.tv_nsec / 1000;
                            if (utimes(pch_cache_filename, mod_timeval))
                                rs_log_warning("Failed to set modification time on pull file: %s - %s", pch_cache_filename, strerror(errno));
                        } else {
                            dcc_emit_cache_warning("Failed to transfer pch file to build machine %s.");
                        }
                    }
                    break;
                case indirection_pull_response_file_missing:
                    if (has_local_file) {
                        unlink(pch_cache_filename);
                    }
                    result = -1;
                    break;
                default:
                    rs_log_error("unknown indirection pull response: %d", pull_response);
                    result = -1;
                    break;
            }
        }
    }
    
    if (result != 0) {
        rs_log_error("indirection request failed, substituting /dev/null");
        actual_path = (char *) "/dev/null";
    } else {
        result = use_pullfile(pch_cache_filename, &actual_path);
    }
    
    if ( write_to_child(indirect, actual_path) ) {
        rs_log_info("Using %s (linked to %s)", pch_cache_filename, actual_path);
    } else {
        rs_log_error("Unable to contact child for path %s", actual_path);
    }
    
    // cleanup
    if (pch_cache_filename)
        free(pch_cache_filename);
    
    if (pull_response == indirection_pull_response_file_download)
        dcc_prune_indirection_cache(pullfile_cache_max_age, pullfile_max_cache_size, pullfile_min_free_space);
        
    if (pull_lockfd != -1)
        close(pull_lockfd);
    return result;
}


/**
 * Thread task for handling indirection requests from a child process
 * (typically gcc 3.3).  Communication between the two processes occurs over
 * a parent-child pipe pair.  Only "pull" operations are currently implemented.
 * Invokes dcc_handle_pull to do so.
 **/
static void *dcc_handle_indirection(dcc_indirection *indirect)
{
    struct sockaddr_in sain;
    size_t sain_length = sizeof(sain);
    size_t actual_length = sain_length;
    int result;
    
    // Grab the incoming hostname for use in creating a cache path on this
    // host for the desired files.

    if (getpeername(indirect->in_fd, (struct sockaddr *) &sain, (socklen_t *) &actual_length)
         == 0 && actual_length <= sain_length ) {
        indirect->hostname = inet_ntoa(sain.sin_addr);
    } else {
        rs_log_error("Unable to determine remote hostname; using \"unknown\"");
        indirect->hostname = "unknown";
    }

    do {
        char response[10];

        if ( (result = read_from_child(indirect, response, 10)) ) {
            if ( strcmp(operation_pull_token, response) == 0 ) {
                dcc_handle_pull(indirect);
            } else if ( strcmp(operation_push_token, response) == 0 ) {
                rs_log_error("Unsupported operation: %s", response);
            } else if ( strcmp(operation_both_token, response) == 0 ) {
                rs_log_error("Unsupported operation: %s", response);
            } else if ( strcmp(operation_version_token, response) == 0 ) {
                if ( read_from_child(indirect, response, 10) ) {
                    if ( strcmp(indirection_protocol_version, response) == 0 ) {
                        write_to_child(indirect, "OK");
                        continue;
                    }
                }

                write_to_child(indirect, "NO");
            } else {
                rs_log_error("Unsupported operation: %s", response);
            }
        }
    } while (result);
}


/**
 * Closes the child (typically gcc 3.3) end of the parent-child pipe pair used
 * to describe file indirection requests and results.
 **/
void dcc_close_pipe_end_child(dcc_indirection *indirect)
{
    if (indirect->parentWrite[1] != -1) {
        close(indirect->parentWrite[1]);
        indirect->parentWrite[1] = -1;
    }
    if (indirect->childWrite[0] != -1) {
        close(indirect->childWrite[0]);
        indirect->childWrite[0] = -1;
    }
}


/**
 * Closes the parent end of the parent-child pipe pair used to describe file
 * indirection requests and results.
 **/
void dcc_close_pipe_end_parent(dcc_indirection *indirect)
{
    if (indirect->parentWrite[0] != -1) {
        close(indirect->parentWrite[0]);
        indirect->parentWrite[0] = -1;
    }
    if (indirect->childWrite[1] != -1) {
        close(indirect->childWrite[1]);
        indirect->childWrite[1] = -1;
    }
}


/**
 * Creates a parent-child pipe pair to handle communication between the parent
 * (distccd) and the child (typically gcc 3.3).  
 **/
int dcc_prepare_indirect(dcc_indirection *indirect)
{
    indirect->read_buf_used = indirect->read_buf_pos = 0;
    indirect->childWrite[0] = -1;
    indirect->childWrite[1] = -1;
    
    int result = pipe(indirect->parentWrite);
    if ( result != 0 ) {
        rs_log_error("Unable to create file indirection pipe set #1: %s", strerror(errno));
        indirect->parentWrite[0] = -1;
        indirect->parentWrite[1] = -1;
    } else {
        result = pipe(indirect->childWrite);
        if ( result != 0 ) {
            rs_log_error("Unable to create file indirection pipe set #2: %s", strerror(errno));
            indirect->childWrite[0] = -1;
            indirect->childWrite[1] = -1;
        }
    }
    if (result != 0) {
        dcc_close_pipe_end_child(indirect);
        dcc_close_pipe_end_parent(indirect);
    }
    return result == 0 ? 0 : EXIT_OUT_OF_MEMORY;
}

void dcc_indirect_child(dcc_indirection *indirect)
{
    char *fdString;
    int result;
    
    if (indirect->parentWrite[0] != -1 && indirect->childWrite[1] != -1) {
        result = asprintf(&fdString, "%d, %d", indirect->parentWrite[0], indirect->childWrite[1]);
        
        if ( result <= 0 ) {
            rs_log_error("Unable to create file indirection pipe description: %s", strerror(errno));
            dcc_close_pipe_end_child(indirect);
            return;
        }
        
        result = setenv("GCC_INDIRECT_FILES", fdString, 1);
        
        if ( result != 0 ) {
            rs_log_error("Unable to set indirection environment variable");
            dcc_close_pipe_end_child(indirect);
            free(fdString);
            return;
        }
    }
}

int dcc_indirect_parent(dcc_indirection *indirect)
{
    int ret;
    dcc_close_pipe_end_parent(indirect);
    // FIXME: need to check result
    dcc_handle_indirection(indirect);
    ret = dcc_x_token_int(indirect->out_fd, indirection_request_token, indirection_complete);
    dcc_close_pipe_end_child(indirect);
    return ret;
}

