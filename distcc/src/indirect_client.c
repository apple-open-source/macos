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


#if defined(DARWIN)


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
#include "bulk.h"
#include "indirect_client.h"
#include "indirect_util.h"
#include "io.h"
#include "rpc.h"
#include "trace.h"


/**
 * Compute the MD5 checksum of the file at <code>path</code>.
 * Assumes that <code>path</code> indicates a file, not a directory.
 * Returns <code>NULL</code> on error or if no file exists at <code>path</code>.
 **/
static char *dcc_compute_file_checksum(const char *path)
{
    int fd = open(path, O_RDONLY, 0777);

    if ( fd > 0 ) {
        char    buffer[1024];
        MD5_CTX ctx;
        int     numBytes;

        MD5_Init(&ctx);

        while ( ( numBytes = read(fd, buffer, 1024) ) && numBytes != -1 ) {
            MD5_Update(&ctx, buffer, numBytes);
        }

        close(fd);

        if ( numBytes == -1 ) {
            rs_log_error("Unable to read %s: %s", path, strerror(errno));
            return NULL;
        } else {
            char *checksum = (char *)malloc(MD5_DIGEST_LENGTH + 1);
            int   i;

            MD5_Final(checksum, &ctx);

            for ( i = 0; i < MD5_DIGEST_LENGTH; i++ ) {
                if ( checksum[i] == '\0' ) {
                    checksum[i] = (char) 1;
                }
            }

            checksum[MD5_DIGEST_LENGTH] = '\0';

            return checksum;
        }
    } else {
        return NULL;
    }
}


/**
 * Compute the MD5 checksum of the directory at <code>path</code>.
 * Assumes that <code>path</code> indicates a directory, not a file, and that
 * this directory contains no subdirectories.
 * Returns <code>NULL</code> on error or if no directory exists at
 * <code>path</code>.
 **/
static char *dcc_compute_directory_checksum(const char *path)
{
    char *checksum = NULL;
    int   cwd      = open(".", O_RDONLY, 0777);

    if ( chdir(path) ) {
        rs_log_error("Unable to chdir to %s", path);
    } else {
        int    numFiles;
        char **filenames = dcc_filenames_in_directory(path, &numFiles);

        if ( filenames != NULL ) {
            checksum = (char *) malloc(numFiles *
                                       (MAXNAMLEN + 1 + MD5_DIGEST_LENGTH + 1)
                                       + 1);

            if ( checksum != NULL ) {
                int i;
                char *fileChecksum;
                char *filename;

                for ( i = 0; i < numFiles; i++ ) {
                    filename = filenames[i];

                    strcat(checksum, filename);
                    strcat(checksum, "\n");

                    fileChecksum = dcc_compute_file_checksum(filename);

                    if ( fileChecksum == NULL ) {
                        rs_log_error("Unable to compute checksum for %s in %s",
                                     filename, path);
                        strcat(checksum, "UNKNOWN");
                    } else {
                        strcat(checksum, fileChecksum);
                        free(fileChecksum);
                    }

                    strcat(checksum, "\n");

                    free(filename);
                }
            }
        }

        free(filenames);
    }

    fchdir(cwd);
    close(cwd);

    return checksum;
}


/**
 * Compute a checksum for <code>path</code>, and compares this checksum to
 * <code>checksum</code>.
 * Provides the computed checksum via <code>currentChecksum</code>.
 * Indicates the file type of <code>path</code> via <code>isDirectory</code>.
 * This checksum is cached on a remote host, along with the file or
 * directory at <code>path</code>.
 * The remote host returns this checksum to the client to determine whether 
 * the cached file needs to be refreshed.
 * Checksum computation and comparison occurs only on the client.
 * As an optimization, a timestamp prefixes the checksum.
 * If the timestamp prefix matches the current timestamp of <code>path</code>, 
 * returns <code>1</code> and leaves <code>currentChecksum</code> and
 * <code>isDirectory</code> unset.
 * If the timestamp prefix does not match, computes the checksum for
 * <code>path</code> and sets <code>currentChecksum</code> and
 * <code>isDirectory</code>.
 * If that checksum matches (without timestamp), returns <code>2</code>.
 * If that checksum does not match, returns <code>0</code>.
 **/
static int dcc_compare_checksum(const char *path, const char *checksum,
                                char **currentChecksum, int *isDirectory)
{
    int         comparison_value  = -1;
    char       *first             = NULL;
    time_t      currentTimestamp;
    time_t      previousTimestamp;
    struct stat sb;

    *currentChecksum  = (char *) "UNKNOWN";
    *isDirectory      = 0;
     currentTimestamp = -1;

    if ( stat(path, &sb) != 0 ) {
        rs_log_error("Unable to stat %s", path);
        return -1;
    }

    currentTimestamp = sb.st_ctime;

    if ( checksum != NULL ) {
        first = strchr(checksum, '\n');

        if ( first == NULL ) {
            rs_log_error("Invalid checksum");
            comparison_value = -1;
        } else {
            first[0] = '\0';
            previousTimestamp = strtoul(checksum, (char**)NULL, 10);
            first[0] = '\n';

            if ( currentTimestamp == previousTimestamp ) {
                return 1;
            }
        }
    }

    if ( sb.st_mode & S_IFDIR ) {
        *isDirectory = 1;
        *currentChecksum = dcc_compute_directory_checksum(path);
    } else {
        *currentChecksum = dcc_compute_file_checksum(path);
    }

    if ( *currentChecksum == NULL ) {
        *currentChecksum = (char *) "UNKNOWN";
        comparison_value = -1;
    } else {
        // Prepend the new timestamp to the new checksum.
        // 50 + 1 for the prepended timestamp + newline
        char *tstamped = (char *)malloc(50 + 1 + strlen(*currentChecksum) + 1);

        comparison_value = -1;

        snprintf(tstamped, 50 + 1, "%lu\n", currentTimestamp);
        strcat(tstamped, *currentChecksum);

        if ( first != NULL ) {
            comparison_value = ( strcmp(&(first[1]), *currentChecksum) == 0 );
        }

        free(*currentChecksum);
        *currentChecksum = tstamped;

        if ( comparison_value == 1 ) {
            comparison_value = 2;
        }
    }

    return comparison_value;
}


/**
 * Transmits the contents of <code>name</code> to a remote host via
 * <code>netfd</code>.
 * Returns <code>0</code> on error, <code>1</code> otherwise.
 **/
static int dcc_push_directory_file(int netfd, const char *name) {
    size_t name_length = strlen(name) + 1;
    size_t bytes;

    if ( dcc_x_token_int(netfd, result_name_token, name_length) ) {
        rs_log_error("Unable to transmit name length for %s", name);
        return 0;
    } else if ( dcc_writex(netfd, name, name_length) ) {
        rs_log_error("Unable to transmit name \"%s\"", name);
        return 0;
    } else if ( dcc_x_file_timed(netfd, name, result_item_token, &bytes) ) {
        rs_log_error("Unable to transmit %s", name);
        return 0;
    } else {
        return 1;
    }
}


/**
 * Transmits <code>checksum</code> to a remote host via <code>netfd</code>.
 * Returns <code>0</code> on error, <code>1</code> otherwise.
 **/
static int dcc_send_new_checksum(int netfd, const char *path,
                                 const char *checksum)
{
    size_t checksum_length = strlen(checksum) + 1;

    if ( dcc_x_token_int(netfd, checksum_length_token, checksum_length) ||
         dcc_writex(netfd, checksum, checksum_length) ) {
        rs_log_error("Unable to transmit checksum for %s", path);
        return 0;
    } else {
        rs_log_info("Successfully transmitted checksum for %s", path);
        return 1;
    }
}


/**
 * Invoked by the client executable when a token received from the server
 * indicates that there may be a remote indirection request.
 * <code>ifd</code> specifies the file descriptor for the socket the client
 * and server are using to communicate.  <code>operation</code> is an integer
 * value indicating the operation requested by the server.
 **/
int dcc_handle_remote_indirection_request(int ifd, int operation)
{
    if ( operation == indirection_request_pull ) {
        int length;
        char path[MAXPATHLEN];

        if ( dcc_r_token_int(ifd, operation_pull_token, &length) ||
             dcc_readx(ifd, path, length) ) {
            rs_log_error("Unable to receive %s request", operation_pull_token);
            dcc_x_token_int(ifd, result_type_token, result_type_nothing);
            return 0;
        } else {
            char *checksum        = NULL;
            int   checksumLength;
            char *currentChecksum = NULL;
            int   exitVal         = 0;
            int   isDirectory;

            rs_log_info("Server desires %s", path);

            if (dcc_r_token_int(ifd, checksum_length_token, &checksumLength)) {
                rs_log_error("Unable to receive checksum length for %s", path);
            } else {
                if ( checksumLength > 0 ) {
                    checksum = malloc(checksumLength + 1);

                    if ( dcc_readx(ifd, checksum, checksumLength) ) {
                        rs_log_error("Unable to receive checksum for %s", path);
                    }
                }
            }

            exitVal = dcc_compare_checksum(path, checksum, &currentChecksum,
                                           &isDirectory);

            if ( checksum != NULL ) {
                free(checksum);
            }

            if ( exitVal == 1 ) {
                rs_log_info("Checksum matched for %s", path);
                dcc_x_token_int(ifd, result_type_token, result_type_nothing);
            } else if ( exitVal == 2 ) {
                rs_log_info("Checksum matched, but timestamp didn't for %s",
                            path);
                dcc_x_token_int(ifd, result_type_token,
                                result_type_checksum_only);
                exitVal = dcc_send_new_checksum(ifd, path, currentChecksum);
            } else {
                if ( exitVal == -1 ) {
                    rs_log_error("Unable to compare checksums");
                }

                if ( isDirectory ) {
                    int    cwd = open(".", O_RDONLY, 0777);
                    int    fileCount;
                    char **filenames = dcc_filenames_in_directory(path,
                                                                  &fileCount);
                    int    i;

                    if ( filenames == NULL || fileCount <= 0 || chdir(path) ) {
                        rs_log_error("Unable to read files in %s", path);
                        dcc_x_token_int(ifd, result_type_token,
                                        result_type_nothing);
                    } else {
                        if ( dcc_x_token_int(ifd, result_type_token,
                                             result_type_dir) ) {
                            rs_log_error("Unable to transmit dir result type");
                        }

                        if ( dcc_x_token_int(ifd, result_count_token,
                                             fileCount) ) {
                            rs_log_error("Unable to dir file count");
                        }

                        for ( i = 0; i < fileCount; i++ ) {
                            dcc_push_directory_file(ifd, filenames[i]);
                            free(filenames[i]);
                        }

                        free(filenames);

                        exitVal = dcc_send_new_checksum(ifd, path,
                                                        currentChecksum);
                    }

                    fchdir(cwd);
                    close(cwd);
                } else {
                    size_t bytes;

                    if ( dcc_x_token_int(ifd, result_type_token,
                                         result_type_file) ) {
                        rs_log_error("Unable to transmit file result type");
                    }

                    if ( dcc_x_file_timed(ifd, path, result_item_token,
                         &bytes) ) {
                        rs_log_error("Unable to transmit file %s", path);
                    } else {
                        exitVal = dcc_send_new_checksum(ifd, path,
                                                        currentChecksum);
                    }
                }
            }

            if ( currentChecksum != NULL && currentChecksum != "UNKNOWN" ) {
                free(currentChecksum);
            }

            return exitVal;
        }
    } else {
        rs_log_error("Unsupported indirection operation: %d", operation);
        return 0;
    }

    return 1;
}


#endif // DARWIN
