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
 * Server functions for file indirection feature.
 **/


#if defined(DARWIN)


#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bulk.h"
#include "config.h"
#include "exitcode.h"
#include "indirect_server.h"
#include "indirect_util.h"
#include "io.h"
#include "rpc.h"
#include "tempfile.h"
#include "trace.h"


static const char message_terminator = '\n';


static int       childWrite[2];
static pthread_t indirectionThread = { 0 };
static int       parentWrite[2];


// from lock.c
int sys_lock(int fd, int block);


/**
 * Receive messages from a child process (typically GCC 3.3 and later).
 * These messages provide information from the child regarding file
 * indirection.
 * The message includes all bytes on the pipe up to the first message
 * terminator (newline) encountered.
 * Converts the message terminator into a string terminator.
 * Stores the message in <code>buffer</code>.
 *
 * Return <code>1</code> on success and <code>0</code> on failure.
 * Fails if the message is longer than <code>size</code>
 **/
static int read_from_child(char *buffer, const int size)
{
    int idx = 0;
    int result;

    if ( size <= 0 ) {
        return 0;
    }

    do {
        result = read(childWrite[0], &buffer[idx], 1);

        if ( result <= 0 || idx >= size ) {
            return 0;
        } else {
            idx += result;
        }
    } while ( buffer[idx - 1] != message_terminator );

    // Straighten out the string termination.
    buffer[idx - 1] = '\0';

    return 1;
}


/**
 * Write <code>message</code> to a child process (typically GCC 3.3 and later).
 * These messages provide information to the child regarding file
 * indirection.
 * The message includes all bytes up to the string terminator.
 * Converts the string terminator into a message terminator (newline).
 * Writes a single message terminator alone if <code>message</code> is
 * <code>NULL</code>.
 *
 * Return <code>1</code> on success and <code>0</code> on failure.
 **/
static int write_to_child(const char *message)
{
    int result;

    if ( message ) {
        const int length = strlen(message);
        int idx = 0;

        while ( idx < length ) {
            result = write(parentWrite[1], &message[idx], length - idx);

            if ( result < 0 ) {
                return 0;
            } else {
                idx += result;
            }
        }
    }

    result = write(parentWrite[1], &message_terminator, 1);

    if ( result < 0 ) {
        return 0;
    }

    return 1;
}


/**
 * Create <code>path_to_dir</code>, including any intervening directories.
 * Behaves similarly to <code>mkdir -p</code>.
 * Permissions are set to <code>ug=rwx,o-rwx</code>.
 *
 * <code>path_to_dir</code> must not be <code>NULL</code>
 **/
static int mkdir_p(const char *path_to_dir)
{
    char *path = strdup(path_to_dir);
    char current_path[MAXPATHLEN];
    char *latest_path_element;

    current_path[0] = '\0';
    latest_path_element = strtok(path, "/");

    while ( strlen(current_path) < MAXPATHLEN - MAXNAMLEN - 1 ) {
        if ( latest_path_element == NULL ) {
            break;
        } else {
            strcat(current_path, "/");
            strcat(current_path, latest_path_element);

            // 504 == \770 == ug=rwx,o-rwx
            if ( mkdir(current_path, 504) == 0 ) {
                rs_trace("Created directory %s", current_path);
            } else {
                if ( errno == EEXIST ) {
                    rs_trace("Directory exists: %s", current_path);
                } else {
                    rs_log_error("Unable to create directory %s: %s",
                                 current_path, strerror(errno));
                    return 0;
                }
            }

            latest_path_element = strtok(NULL, "/");
        }
    }

    free(path);

    if ( latest_path_element == NULL ) {
        return 1;
    } else {
        return 0;
    }
}


/**
 * Creates a temporary file at <code>tmp_path</code> to store
 * <code>checksum</code>.
 **/
static char *dcc_create_checksum_tmpfile(const char *tmp_path,
                                         const char *checksum)
{
    char *checksum_tmpfile = malloc(MAXPATHLEN);
    int   fd;

    if ( checksum_tmpfile == NULL ) {
        rs_log_error("Unable to allocate space for checksum_tmpfile");
        return NULL;
    }

    strncpy(checksum_tmpfile, tmp_path, MAXPATHLEN);
    strcat(checksum_tmpfile, checksum_suffix);

    // do not need to mkdir_p, since this should always happen after
    // the files represented by the checksum are laid down

    fd = open(checksum_tmpfile, O_WRONLY|O_CREAT, 0777);

    if ( fd < 0 ) {
        rs_log_error("Unable to create %s", checksum_tmpfile);
        return NULL; 
    } else {
        size_t checksum_length = strlen(checksum);

        if ( write(fd, checksum, checksum_length) == (ssize_t)checksum_length ){
            rs_trace("Wrote checksum to %s", checksum_tmpfile);
            close(fd);
            return checksum_tmpfile;
        } else {
            rs_log_error("Unable to write checksum to %s", checksum_tmpfile);
            close(fd);
            remove(checksum_tmpfile);
            return NULL;
        }
    }
}


/**
 * Pulls a checksum for the previously requested file over the socket
 * specified by <code>netfd</code>.  Stores the checksum at
 * <code>tmp_path</code>.
 **/
static char *dcc_pull_checksum(int netfd, const char *tmp_path)
{
    int checksum_length = 0;

    if ( dcc_r_token_int(netfd, checksum_length_token, &checksum_length)
         || checksum_length <= 0 ) {
        rs_log_error("No incoming checksum for path %s", tmp_path);
        return NULL;
    } else {
        char *checksum = malloc(checksum_length + 1);

        if ( dcc_readx(netfd, checksum, checksum_length) ) {
            rs_log_error("Unable to read checksum for path %s", tmp_path);
            free(checksum);
            return NULL;
        } else {
            char *tmp_checksum_filename = dcc_create_checksum_tmpfile(tmp_path,
                                                                      checksum);
            free(checksum);
            return tmp_checksum_filename;
        }
    }
}


/**
 * Pushes the existing <code>checksum</code> for the previously requested file
 * over the socket specified by <code>netfd</code>.
 * <code>hostname</code> used strictly to disambiguate logging.
 **/
static int dcc_push_checksum(int netfd, const char *hostname,
                               const char *checksum)
{
    size_t checksum_length = ( checksum == NULL ) ? 0 : strlen(checksum) + 1;

    if ( dcc_x_token_int(netfd, checksum_length_token, checksum_length) ) {
        rs_log_error("Unable to transmit checksum length %u to %s",
                     checksum_length, hostname);
        return 0;
    } else if ( checksum_length > 0 ) {
        if ( dcc_writex(netfd, checksum, checksum_length) ) {
            rs_log_error("Unable to transmit checksum \"%s\" to %s",
                         checksum, hostname);
            return 0;
        }
    }

    return 1;
}


/**
 * Reads the contents of <code>cached_path_checksum</code>.
 * <code>cached_path_checksum_size</code> is used as a hint for the size
 * of the buffer to return.  The returned buffer must be freed by the caller.
 **/
static char *dcc_read_checksum(const char *cached_path_checksum,
                               size_t cached_path_checksum_size)
{
    int   remaining_buffer = cached_path_checksum_size + 1;
    char *checksum         = malloc(remaining_buffer);

    if ( checksum == NULL ) {
        rs_log_error("Unable to allocate space for checksum_tmpfile");
    } else {
        int checksum_file = open(cached_path_checksum, O_RDONLY, 0777);

        if ( checksum_file <= 0 ) {
            rs_log_error("Unable to open %s", cached_path_checksum);
            free(checksum);
            checksum = NULL;
        } else {
            int num_read = -1;

            while ( remaining_buffer > 0 &&
                    ( num_read = read(checksum_file, checksum,
                                      remaining_buffer) ) > 0 ) {
                remaining_buffer -= num_read;
            }

            checksum[cached_path_checksum_size] = '\0';

            if ( num_read < 0 ||
                 strlen(checksum) < cached_path_checksum_size ) {
                rs_log_error("Unable to read checksum from %s",
                             cached_path_checksum);
                free(checksum);
                checksum = NULL;
            }

            close(checksum_file);
        }
    }

    return checksum;
}


/**
 * Pulls the previously requested directory over the socket specified by
 * <code>netfd</code>.  Currently, this directory cannot contain any
 * subdirectories.  <code>tmp_path</code> specifies the location on the
 * server where the directory will be stored.  <code>hostname</code> is
 * used only to disambiguate logging.
 **/
static void dcc_pull_directory_file(int netfd, const char *hostname,
                                   const char *tmp_path)
{
    int cwd = open(".", O_RDONLY, 0777);

    if ( chdir(tmp_path) ) {
        rs_log_error("Unable to chdir to %s", tmp_path);
    } else {
        int incomingBytes;

        if ( dcc_r_token_int(netfd, result_name_token, &incomingBytes)
             || incomingBytes <= 0 ) {
            rs_log_error("No incoming file from %s for directory %s",
                         hostname, tmp_path);
        } else {
            char filename[incomingBytes];

            if ( dcc_readx(netfd, filename, incomingBytes) ) {
                rs_log_error("Unable to read filename from %s for file in directory %s", hostname, tmp_path);
            } else {
                if (dcc_r_token_int(netfd, result_item_token, &incomingBytes)) {
                    rs_log_error("Unable to read length from %s for file %s in %s", hostname, filename, tmp_path);
                } else {
                    if ( dcc_r_file_timed(netfd, filename, incomingBytes) ) {
                        rs_log_error("Failed to retrieve from %s incoming file %s/%s", hostname, tmp_path, filename);
                    } else {
                        rs_log_info("Stored incoming file from %s at %s in %s",
                                    hostname, filename, tmp_path);
                    }
                }
            }
        }
    }

    fchdir(cwd);
    close(cwd);
}


/**
 * Remove <code>path</code> (not including intervening directories).
 * All files in <code>path</code> are removed, as well.
 * Does not remove subdirectories of <code>path</code>.
 **/
static int dcc_rmdir(const char *path) {
    int    numFiles;
    char **filenames = dcc_filenames_in_directory(path, &numFiles);

    if ( filenames != NULL ) {
        int dir = open(path, O_RDONLY, 0777);

        if ( dir > 0 ) {
            int cwd = open(".", O_RDONLY, 0777);

            if ( fchdir(dir) == 0 ) {
                int i;

                for ( i = 0; i < numFiles && filenames[i] != NULL; i++ ) {
                    if ( unlink(filenames[i]) ) {
                        rs_log_error("Unable to remove %s from %s",
                                     filenames[i], path);
                    } else {
                        rs_trace("Removed %s from %s", filenames[i], path);
                    }

                    free(filenames[i]);
                }
            }

            fchdir(cwd);
            close(cwd);
            close(dir);
        }

        free(filenames);
    }

    return rmdir(path);
}


#if ! defined(HAVE_FLOCK)
/**
 * Get a shared lock on a file using whatever method
 * is available on this system.
 *
 * @retval 0 if we got the lock
 * @retval -1 with errno set if the file is already locked.
 **/
static int sys_shlock(int fd, int block)
{
#if defined(F_SETLK) && ! defined(DARWIN)
    struct flock lockparam;

    lockparam.l_type = F_RDLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start = 0;
    lockparam.l_len = 0;        /* whole file */
    
    return fcntl(fd, block ? F_SETLKW : F_SETLK, &lockparam);
#elif defined(HAVE_FLOCK)
    return flock(fd, LOCK_SH | (block ? 0 : LOCK_NB));
#else
#  error "No supported lock method.  Please port this code."
#endif
}
#endif // ! defined(HAVE_FLOCK)


/**
 * Performs a read lock on the file at <code>checksum_path</code>.
 * Returns the file descriptor for the result.
 **/
static int dcc_shared_lock_on_checksum_file(const char *checksum_path)
{
    int fd;

    rs_trace("Taking read lock on %s", checksum_path);
#if defined(HAVE_FLOCK)
    fd = open(checksum_path, O_RDONLY | O_SHLOCK, 0777);
#else
    fd = open(checksum_path, O_RDONLY, 0777);
    sys_shlock(fd, 1);
#endif
    rs_trace("Got read lock (released automatically on process exit) on %s",
             checksum_path);

    return fd;
}


/**
 * Performs a write lock on the file at <code>checksum_path</code>.
 * Returns the file descriptor for the result.
 **/
static int dcc_exclusive_lock_on_checksum_file(const char *checksum_path)
{
    int fd = -1;

    // Use the checksum file as a lock file.
    rs_trace("Taking a write lock on %s", checksum_path);

#if defined(HAVE_FLOCK)
    fd = open(checksum_path, O_WRONLY|O_CREAT|O_EXLOCK, 0777);
#else
    fd = open(checksum_path, O_WRONLY|O_CREAT, 0777);
    sys_lock(fd, 1);
#endif
    rs_trace("Got write lock on %s", checksum_path);

    return fd;
}


/**
 * Forcibly removes a lock on the file described by <code>lock_fd</code>.
 * Returns non-zero on error.
 **/
static int dcc_really_unlock(int lock_fd)
{
#if defined(F_SETLK) && ! defined(DARWIN)
    struct flock lockparam;

    lockparam.l_type = F_UNLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start = 0;
    lockparam.l_len = 0;        /* whole file */
    
    if (fcntl(lock_fd, F_SETLK, &lockparam))
#elif defined(HAVE_FLOCK)
    if (flock(lock_fd, LOCK_UN))
#elif defined(HAVE_LOCKF)
    if (lockf(lock_fd, F_ULOCK, 0))
#else
#  error "No supported lock method.  Please port this code."
#endif
    {
        rs_log_error("unlock failed: %s", strerror(errno));
        return EXIT_IO_ERROR;
    }

    rs_trace("really released lock");

    return 0;
}


/**
 * Releases the lock held on <code>fd</code>.  <code>checksum_path</code>
 * provided for logging only.  Also closes <code>fd</code>.
 **/
static void dcc_unlock_checksum_file(int fd, const char *checksum_path)
{
    dcc_really_unlock(fd);
    rs_trace("Released write lock on %s", checksum_path);
          
    if ( fd >= 0 ) {
        close(fd);
    }
}


/**
 * Handles a pull operation, as specified by the server's child (typically
 * gcc 3.3).  <code>netfd</code> is the file descriptor for the socket
 * connection between the client and server.  <code>hostname</code> specifies
 * the hostname of the client.  Pulled files are stored in a per-host cache.
 **/
static void dcc_handle_pull(int netfd, const char *hostname)
{
          char  *actual_path          = NULL;
          size_t hostname_length      = strlen(hostname) + 1;
          char   response[MAXPATHLEN];
    const char  *tempdir              = dcc_get_tempdir();
          size_t tempdir_length       = strlen(tempdir) + 1;
          int    fd;

    if ( read_from_child(response, MAXPATHLEN) ) {
        size_t response_length = strlen(response) + 1;
        size_t total_length  = tempdir_length + hostname_length
                                              + response_length;
        char *cached_path = malloc(total_length);
        char *cached_path_checksum = malloc(total_length
                                            + checksum_suffix_length);
        int    cached_path_exists;
        int    cached_path_checksum_exists;
        size_t cached_path_checksum_size;
        char  *checksum = NULL;
        struct stat sb;

        rs_trace("Attempting to pull %s", response);

        strcat(cached_path, tempdir);
        strcat(cached_path, "/");
        strcat(cached_path, hostname);
        strcat(cached_path, "/");
        strcat(cached_path, response);

        strncpy(cached_path_checksum, cached_path, total_length);
        strncat(cached_path_checksum, checksum_suffix, checksum_suffix_length);

        cached_path_exists = ( stat(cached_path, &sb) == 0 );
        cached_path_checksum_exists = ( stat(cached_path_checksum, &sb) == 0 );
        cached_path_checksum_size = sb.st_size;

        // Lock the file using a reader lock, so that multiple processes
        // may read the same file, but only one file at a time may write it.
        // The lock is released when the process exits.  Do not explicitly
        // release it here, as the subordinate process will need to use
        // the file safely.

        fd = dcc_shared_lock_on_checksum_file(cached_path_checksum);

        if ( cached_path_exists && cached_path_checksum_exists &&
             cached_path_checksum_size > 0 ) {
            checksum = dcc_read_checksum(cached_path_checksum,
                                         cached_path_checksum_size);
        }

        if ( dcc_x_token_int(netfd, indirection_request_token,
                             indirection_request_pull)
             || dcc_x_token_int(netfd, operation_pull_token, response_length)
             || dcc_writex(netfd, response, response_length) ) {
            rs_log_error("Unable to transmit filename to %s", hostname);
            actual_path = response;
        } else {
            if ( ! dcc_push_checksum(netfd, hostname, checksum) ) {
                actual_path = response;
            }
        }

        dcc_unlock_checksum_file(fd, cached_path_checksum);

        if ( checksum != NULL ) {
            free(checksum);
            checksum = NULL;
        }

        if ( actual_path == NULL ) {
            int result_type = result_type_nothing;

            if ( dcc_r_token_int(netfd, result_type_token, &result_type) ) {
                rs_log_error("Unable to read result type from %s", hostname);
                actual_path = response;
            } else if ( result_type == result_type_nothing ||
                        result_type == result_type_checksum_only ) {
                // the client says that it has nothing or nothing newer
                // than what the server has 

                if ( cached_path_exists && cached_path_checksum_exists ) {
                    rs_log_info("Using cached version: %s", cached_path);
                    actual_path = cached_path;
                } else {
                    rs_log_error("Unable to find %s on %s", response, hostname);
                    actual_path = response;
                }

                if ( result_type == result_type_checksum_only ) {
                    char *tmpName         = dcc_make_tmpnam(hostname, response);
                    char *tmpChecksumName = NULL;
                    char *lastSlash       = strrchr(tmpName, '/');

                    lastSlash[0] = '\0';

                    if ( mkdir_p(tmpName) ) {
                        lastSlash[0] = '/';

                        tmpChecksumName = dcc_pull_checksum(netfd, tmpName);

                        if ( tmpChecksumName != NULL ) {
                            int xfd = dcc_exclusive_lock_on_checksum_file(cached_path_checksum);

                            if ( rename(tmpChecksumName, cached_path_checksum)){
                                rs_log_error("Failed to move %s to %s: %s",
                                             tmpChecksumName,
                                             cached_path_checksum,
                                             strerror(errno));
                            } else {
                                rs_trace("Moved %s to %s", tmpChecksumName,
                                         cached_path_checksum);
                            }

                            dcc_unlock_checksum_file(xfd, cached_path_checksum);

                            free(tmpChecksumName);
                        }
                    } else {
                        rs_log_error("Unable to create directory %s", tmpName);
                    }

                    free(tmpName);
                    free(cached_path_checksum);
                }
            } else if ( result_type == result_type_file ) {
                char *tmpChecksumName = NULL;
                char *tmpName         = dcc_make_tmpnam(hostname, response);
                char *lastSlash       = strrchr(tmpName, '/');
                int   incomingBytes;

                if ( dcc_r_token_int(netfd, result_item_token, &incomingBytes)
                     || incomingBytes <= 0 ) {
                    rs_log_error("No incoming file for path %s", response);
                    actual_path = response;
                } else {
                    lastSlash[0] = '\0';

                    if ( mkdir_p(tmpName) ) {
                        lastSlash[0] = '/';
                        if ( dcc_r_file_timed(netfd, tmpName, incomingBytes)
                             == 0 ) {
                            rs_log_info("Stored incoming file at %s", tmpName);
                            tmpChecksumName = dcc_pull_checksum(netfd, tmpName);
                        } else {
                            rs_log_error("Failed to retrieve incoming file %s",
                                         tmpName);
                            actual_path = response;
                        }
                    } else {
                        rs_log_error("Unable to create directory %s", tmpName);
                        actual_path = response;
                    }

                }

                if ( actual_path == NULL ) {
                    // move the temp files into the cached location
                    int xfd;

                    lastSlash = strrchr(cached_path, '/');

                    lastSlash[0] = '\0';

                    if ( ! mkdir_p(cached_path) ) {
                        rs_log_error("Unable to create directory %s",
                                     cached_path);
                    }

                    lastSlash[0] = '/';

                    xfd = dcc_exclusive_lock_on_checksum_file(cached_path_checksum);

                    if ( rename(tmpName, cached_path) ) {
                        rs_log_error("Failed to move %s to %s: %s", tmpName,
                                     cached_path, strerror(errno));
                        actual_path = response;
                    } else {
                        rs_trace("Moved %s to %s", tmpName, cached_path);
                        actual_path = cached_path;

                        if ( tmpChecksumName != NULL ) {                  
                            if ( rename(tmpChecksumName, cached_path_checksum)){
                                rs_log_error("Failed to move %s to %s: %s",
                                             tmpChecksumName,
                                             cached_path_checksum,
                                             strerror(errno));
                            } else {
                                rs_trace("Moved %s to %s", tmpChecksumName,
                                         cached_path_checksum);
                            }
                        }
                    }

                    dcc_unlock_checksum_file(xfd, cached_path_checksum);
                }

                // cleanup

                free(cached_path_checksum);

                if ( tmpChecksumName != NULL ) {
                    free(tmpChecksumName);
                }

                free(tmpName);
            } else if ( result_type == result_type_dir ) {
                char *tmpChecksumName = NULL;
                char *tmpName         = dcc_make_tmpnam(hostname, response);
                int   fileCount;

                if ( dcc_r_token_int(netfd, result_count_token, &fileCount)
                     || fileCount <= 0 ) {
                    rs_log_error("No incoming files for path %s", response);
                    actual_path = response;
                } else {
                    if ( mkdir_p(tmpName) ) {
                        int i;

                        for ( i = 0; i < fileCount; i++ ) {
                            dcc_pull_directory_file(netfd, hostname, tmpName);
                        }

                        tmpChecksumName = dcc_pull_checksum(netfd, tmpName);
                    } else {
                        rs_log_error("Unable to create directory %s", tmpName);
                        actual_path = response;
                    }
                }

                if ( actual_path == NULL ) {
                    // move the temp files into the cached location
                    int xfd;

                    if ( dcc_rmdir(cached_path) ) {
                        char *lastSlash = strrchr(cached_path, '/');

                        rs_log_error("Unable to remove %s", cached_path);

                        lastSlash[0] = '\0';

                        if ( ! mkdir_p(cached_path) ) {
                            rs_log_error("Unable to create directory %s",
                                         cached_path);
                            actual_path = response;
                        }

                        lastSlash[0] = '/';
                    }

                    xfd = dcc_exclusive_lock_on_checksum_file(cached_path_checksum);

                    if ( rename(tmpName, cached_path) ) {
                        rs_log_error("Failed to move %s to %s: %s", tmpName,
                                     cached_path, strerror(errno));
                        actual_path = response;
                    } else {
                        rs_trace("Moved %s to %s", tmpName, cached_path);

                        actual_path = cached_path;

                        if ( tmpChecksumName != NULL ) {                  
                            if ( rename(tmpChecksumName, cached_path_checksum)){
                                rs_log_error("Failed to move %s to %s: %s",
                                             tmpChecksumName,
                                             cached_path_checksum,
                                             strerror(errno));
                            } else {
                                rs_trace("Moved %s to %s", tmpChecksumName,
                                         cached_path_checksum);
                            }
                        }
                    }

                    dcc_unlock_checksum_file(xfd, cached_path_checksum);
                }

                // cleanup

                free(cached_path_checksum);

                if ( tmpChecksumName != NULL ) {
                    free(tmpChecksumName);
                }

                free(tmpName);
            }
        }

        fd = dcc_shared_lock_on_checksum_file(cached_path_checksum);  
    } else {
        rs_log_error("Unable to receive %s request", operation_pull_token);
        actual_path = (char *) "UNKNOWN";
    }

    if ( write_to_child(actual_path) ) {
        rs_log_info("Using %s", actual_path);
    } else {
        rs_log_error("Unable to contact child for path %s", actual_path);
    }

    // cleanup

    if ( actual_path != response ) {
        free(actual_path);
    }
}


/**
 * Thread task for handling indirection requests from a child process
 * (typically gcc 3.3).  Communication between the two processes occurs over
 * a parent-child pipe pair.  Only "pull" operations are currently implemented.
 * Invokes <code>dcc_handle_pull</code> to do so.
 * <code>ifd</code> is a pointer to the file descriptor for the socket
 * that the client and server use to communicate.
 **/
static void *dcc_handle_indirection(void *ifd)
{
    int netfd = *((int *)ifd);
    char *hostname = (char *) "unknown";
    struct sockaddr_in sain;
    size_t sain_length = sizeof(sain);
    size_t actual_length = sain_length;

    // Grab the incoming hostname for use in creating a cache path on this
    // host for the desired files.

    if ( getpeername(netfd, (struct sockaddr *) &sain, (int *) &actual_length)
         == 0 && actual_length <= sain_length ) {
        hostname = inet_ntoa(sain.sin_addr);
    } else {
        rs_log_error("Unable to determine remote hostname; using \"unknown\"");
    }

    // Handle requests from the child process until this process terminates.

    while (1) {
        char response[10];

        if ( read_from_child(response, 10) ) {
            if ( strcmp(operation_pull_token, response) == 0 ) {
                dcc_handle_pull(netfd, hostname);
            } else if ( strcmp(operation_push_token, response) == 0 ) {
                rs_log_error("Unsupported operation: %s", response);
            } else if ( strcmp(operation_both_token, response) == 0 ) {
                rs_log_error("Unsupported operation: %s", response);
            } else if ( strcmp(operation_version_token, response) == 0 ) {
                if ( read_from_child(response, 10) ) {
                    if ( strcmp(indirection_protocol_version, response) == 0 ) {
                        write_to_child("OK");
                        continue;
                    }
                }

                write_to_child("NO");
            } else {
                rs_log_error("Unsupported operation: %s", response);
            }
        }
    }
}


/**
 * Closes the child (typically gcc 3.3) end of the parent-child pipe pair used
 * to describe file indirection requests and results.
 **/
void dcc_close_pipe_end_child(void)
{
    close(parentWrite[1]);
    close(childWrite[0]);
}


/**
 * Closes the parent end of the parent-child pipe pair used to describe file
 * indirection requests and results.
 **/
void dcc_close_pipe_end_parent(void)
{
    close(parentWrite[0]);
    close(childWrite[1]);
}


/**
 * Creates a parent-child pipe pair to handle communication between the parent
 * (distccd) and the child (typically gcc 3.3).  Creates a thread to handle
 * file indirection requests.  This thread persists over the life of the parent
 * process.
 **/
void dcc_support_indirection(int *ifd)
{
    char *fdString;
    int result = pipe(parentWrite);

    if ( result != 0 ) {
        rs_log_error("Unable to create file indirection pipe set #1: %s", strerror(errno));
        return;
    }

    result = pipe(childWrite);

    if ( result != 0 ) {
        rs_log_error("Unable to create file indirection pipe set #2: %s", strerror(errno));
        close(parentWrite[0]);
        close(parentWrite[1]);
        return;
    }

    result = asprintf(&fdString, "%d, %d", parentWrite[0], childWrite[1]);

    if ( result <= 0 ) {
        rs_log_error("Unable to create file indirection pipe description: %s", strerror(errno));
        dcc_close_pipe_end_child();
        dcc_close_pipe_end_parent();
        return;
    }

    result = setenv("GCC_INDIRECT_FILES", fdString, 1);

    if ( result != 0 ) {
        rs_log_error("Unable to set indirection environment variable");
        dcc_close_pipe_end_child();
        dcc_close_pipe_end_parent();
        free(fdString);
        return;
    }

    result = pthread_create(&indirectionThread, NULL, dcc_handle_indirection, ifd);

    if ( result == 0 ) {
        rs_log_info("Created thread to handle file indirection");
    } else {
        rs_log_error("Unable to create thread to handle file indirection");
        dcc_close_pipe_end_child();
        dcc_close_pipe_end_parent();
        free(fdString);
        unsetenv("GCC_INDIRECT_FILES");
        return;
    }
}


#endif // DARWIN
