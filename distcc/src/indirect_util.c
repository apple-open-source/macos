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
 * Utility functions for file indirection feature.
 **/


#if defined(DARWIN)


#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "indirect_util.h"


/**
 * Provide an array of filenames that represents all files in
 * <code>dir_path</code>.
 * Caller must release the returned array.
 **/
char **dcc_filenames_in_directory(const char *dir_path, int *numFiles)
{
    long   basep;
    int    count = 0;
    char   dirge[8192];
    int    fd;
    char **filenames;
    int    numread;

    *numFiles = 0;

    if ( dir_path == NULL ) {
        return NULL;
    }

    fd = open(dir_path, O_RDONLY, 0777);

    if ( fd < 0 ) {
        return NULL;
    }

    filenames = (char **) calloc(20, sizeof(char *));

    while ( (numread = getdirentries(fd, dirge, sizeof(dirge), &basep)) > 0 ) {
        struct dirent *dent;

        for ( dent = (struct dirent *)dirge;
              dent < (struct dirent *)(dirge + numread);
              dent = (struct dirent *)((char *)dent + dent->d_reclen) ) {

            // skip . & ..
            if ( dent->d_fileno == 0 ||
                 ( dent->d_name[0] == '.' &&
                   ( dent->d_namlen == 1 ||
                     ( dent->d_namlen == 2 && dent->d_name[1] == '.') ) ) ) {
                continue;
            }

            filenames[count++] = strdup(dent->d_name);
        }
    }

    filenames[count] = NULL;

    close(fd);

    if  ( numread == -1 || count == 0 ) {
        int i;

        for ( i = 0; i < count; i++ ) {
            free(filenames[i]);
        }

        free(filenames);

        filenames = NULL;
    }

    if ( filenames != NULL ) {
        *numFiles = count;
    }

    return filenames;
}


#endif // DARWIN
