/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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


                /* "More computing sins are committed in the name of
                 * efficiency (without necessarily achieving it) than
                 * for any other single reason - including blind
                 * stupidity."  -- W.A. Wulf
                 */



#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "snprintf.h"
#include "exitcode.h"



/**
 * @file
 *
 * Routines for naming, generating and removing temporary files.
 *
 * Temporary files are stored under $TMPDIR or /tmp.
 *
 * From 2.10 on, our lock and state files are not stored in there.
 *
 * It would be nice if we could use a standard function, but I don't
 * think any of them are adequate: we need to control the extension
 * and know the filename.  tmpfile() does not give back the filename.
 * tmpnam() is insecure.  mkstemp() does not allow us to set the
 * extension.
 *
 * It sucks that there is no standard function.  The implementation
 * below is inspired by the __gen_tempname() code in glibc; hopefully
 * it will be secure on all platforms.
 *
 * We need to touch the filename before running commands on it,
 * because we cannot be sure that the compiler will create it
 * securely.
 *
 * Even with all this, we are not necessarily secure in the presence
 * of a tmpreaper if the attacker can play timing tricks.  However,
 * since we are not setuid and since there is no completely safe way
 * to write tmpreapers, this is left alone for now.
 *
 * If you're really paranoid, you should just use per-user TMPDIRs.
 *
 * @sa http://www.dwheeler.com/secure-programs/Secure-Programs-HOWTO/avoid-race.html#TEMPORARY-FILES
 **/

/*
 Attempts to set the owner and group of path to be the same as the directory containing it.
 Returns 0 on success, nonzero on error.
 */
int dcc_set_owner(const char *path)
{
    int result = 1;
    char base[MAXPATHLEN];
    int i, lastSlash;
    struct stat st;
    
    for (i=0; path[i]!=0; i++) {
        base[i] = path[i];
        if (path[i] == '/')
            lastSlash = i;
    }
    base[lastSlash] = 0;
    if (stat(base, &st) == 0) {
        if (chown(path, st.st_uid, st.st_gid) == 0)
            result = 0;
    }
    return result;
}

/*
 Construct a path string in the temporary directory.
 This function constructs a path in the temporary directory by catenating several directory names and an optional file name.
 It can also optionally create the corresponding directories in the filesystem.
 path_ret returns the result by reference. This buffer is of size MAXPATHLEN should be freed when no longer needed.
 create_directories specifies whether to create directories in the filesystem
 file_part specifies an optional filename to append to the path. It may be NULL, in which case nothing is appended beyond the last directory. The file is not created in the filesystem.
 The variable arguments are all const char *, and are pointers to the individual directory names that should comprise the final path. This function handles directory names with leading or trailing slash characters.
 */
int dcc_make_tmpfile_path(char **path_ret, int create_directories, const char *file_part, ...)
{
    char *path;
    const char *tempdir;
    int result = 0;
    
    result = dcc_get_tmp_top(&tempdir);

    if (result == 0) {
        path = (char *)malloc(MAXPATHLEN);
        if (!path)
            result = -1;
    }

    if (result == 0) {
        int path_len, part_len;
        const char *path_part;
        va_list         va;

        strcpy(path, tempdir);
        path_len = strlen(path);
        
        va_start(va, file_part);
        for (path_part = va_arg(va, const char *); result == 0 && path_part != NULL; path_part = va_arg(va, const char *)) {
            part_len = strlen(path_part);
            if (path_len + part_len + 2 < MAXPATHLEN) {
                if (path_part[0] != '/')
                    path[path_len++] = '/';
                if (create_directories) {
                    int i;
                    for (i = 0; result == 0 && i<part_len; i++) {
                        if (i > 0 && path_part[i] == '/') {
                            path[path_len] = 0;
                            result = dcc_mkdir(path);
                        }
                        path[path_len++] = path_part[i];
                    }
                    path[path_len] = 0;
                } else {
                    strcpy(&path[path_len], path_part);
                    path_len += part_len;
                }
                if (path[path_len] == '/') {
                    path[path_len] = 0;
                    path_len--;
                } else {
                    if (create_directories) {
                        result = dcc_mkdir(path);
                    }
                }
            } else {
                result = -1;
            }
        }
        va_end(va);
        
        if (file_part) {
            part_len = strlen(file_part);
            if (path_len + part_len + 2 < MAXPATHLEN) {
                if (file_part[0] != '/')
                    path[path_len++] = '/';
                strcpy(&path[path_len], file_part);
            } else {
                result = -1;
            }
        }
    }
    if (result != 0 && path) {
        free(path);
        path = NULL;
    }
    *path_ret = path;
    return result;   
}

int dcc_get_tmp_top(const char **p_ret)
{
    static const char *d = NULL;
    
    if (d == NULL) {
        d = getenv("TMPDIR");
        
        if (!d || d[0] == '\0') {
            d = "/tmp/distcc";
            if (dcc_mkdir(d) == 0) {
                chmod(d, 0777);
            } else {
                d = "/tmp";
            }
        }
    }
    *p_ret = d;
    return 0;
}


/**
 * Create the directory @p path.  If it already exists as a directory
 * we succeed.
 **/
int dcc_mkdir(const char *path)
{
    if ((mkdir(path, 0777) == -1) && (errno != EEXIST)) {
        rs_log_error("mkdir %s failed: %s", path, strerror(errno));
        return EXIT_IO_ERROR;
    }

    dcc_set_owner(path);
    return 0;
}


/**
 * Return a static string holding DISTCC_DIR, or ~/.distcc.
 * The directory is created if it does not exist.
 **/
int dcc_get_top_dir(char **path_ret)
{
    char *env;
    static char *cached;
    int ret;

    if (cached) {
        *path_ret = cached;
        return 0;
    }

    if ((env = getenv("DISTCC_DIR"))) {
        if ((cached = strdup(env)) == NULL) {
            return EXIT_OUT_OF_MEMORY;
        } else {
            *path_ret = cached;
            return 0;
        }
    }

    /* We want all the lock files to reside on a local filesystem. */
    if (asprintf(path_ret, "/var/tmp/distcc.%d", getuid()) == -1) {
        rs_log_error("asprintf failed");
        return EXIT_OUT_OF_MEMORY;
    }

    /*
    if ((env = getenv("HOME")) == NULL) {
        rs_log_warning("HOME is not set; can't find distcc directory");
        return EXIT_BAD_ARGUMENTS;
    }

    if (asprintf(path_ret, "%s/.distcc", env) == -1) {
        rs_log_error("asprintf failed");
        return EXIT_OUT_OF_MEMORY;
    }
    */

    ret = dcc_mkdir(*path_ret);
    if (ret == 0)
        cached = *path_ret;
    return ret;
}


/**
 * Return a subdirectory of the DISTCC_DIR of the given name, making
 * sure that the directory exists.
 **/
static int dcc_get_subdir(const char *name,
                          char **dir_ret)
{
    int ret;
    char *topdir;

    if ((ret = dcc_get_top_dir(&topdir)))
        return ret;

    if (asprintf(dir_ret, "%s/%s", topdir, name) == -1) {
        rs_log_error("asprintf failed");
        return EXIT_OUT_OF_MEMORY;
    }

    return dcc_mkdir(*dir_ret);
}


int dcc_get_lock_dir(char **dir_ret)
{
    static char *cached;
    int ret;

    if (cached) {
        *dir_ret = cached;
        return 0;
    } else {
        ret = dcc_get_subdir("lock", dir_ret);
        if (ret == 0)
            cached = *dir_ret;
        return ret;
    }
}



int dcc_get_state_dir(char **dir_ret)
{
    static char *cached;
    int ret;

    if (cached) {
        *dir_ret = cached;
        return 0;
    } else {
        ret = dcc_get_subdir("state", dir_ret);
        if (ret == 0)
            cached = *dir_ret;
        return ret;
    }
}



/**
 * Create a file inside the temporary directory and register it for
 * later cleanup, and return its name.
 *
 * The file will be reopened later, possibly in a child.  But we know
 * that it exists with appropriately tight permissions.
 **/
int dcc_make_tmpnam(const char *prefix,
                    const char *suffix,
                    char **name_ret)
{
    char *s = NULL;
    const char *tempdir;
    int ret;
    unsigned long random_bits;
    int fd;

    if ((ret = dcc_get_tmp_top(&tempdir)))
        return ret;

    if (access(tempdir, W_OK|X_OK) == -1) {
        rs_log_error("can't use TMPDIR \"%s\": %s", tempdir, strerror(errno));
        return EXIT_IO_ERROR;
    }

    random_bits = (unsigned long) getpid() << 16;

# if HAVE_GETTIMEOFDAY
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_bits ^= tv.tv_usec << 16;
        random_bits ^= tv.tv_sec;
    }
# else
    random_bits ^= time(NULL);
# endif

#if 0
    random_bits = 0;            /* FOR TESTING */
#endif

    do {
        free(s);
        
        if (asprintf(&s, "%s/%s_%08lx%s",
                     tempdir,
                     prefix,
                     random_bits & 0xffffffffUL,
                     suffix) == -1)
            return EXIT_OUT_OF_MEMORY;

        /* Note that if the name already exists as a symlink, this
         * open call will fail.
         *
         * The permissions are tight because nobody but this process
         * and our children should do anything with it. */
        fd = open(s, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd == -1) {
            /* try again */
            rs_trace("failed to create %s: %s", s, strerror(errno));
            random_bits += 7777; /* fairly prime */
            continue;
        }
        
        if (close(fd) == -1) {  /* huh? */
            rs_log_warning("failed to close %s: %s", s, strerror(errno));
            return EXIT_IO_ERROR;
        }
        
        break;
    } while (1);

    if ((ret = dcc_add_cleanup(s))) {
        free(s);
        return ret;
    }

    *name_ret = s;
    return 0;
}
